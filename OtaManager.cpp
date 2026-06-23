#include "OtaManager.h"

#include <Update.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include <cstring>

OtaManager::OtaManager(HeaterController &heater) : heater_(heater) {}

void OtaManager::begin() {
  configureRoutes();
  bootResetReason_ = esp_reset_reason();

  Preferences preferences;
  if (preferences.begin("reflow_ota", false)) {
    previousSessionInterrupted_ = preferences.getBool("active", false);
    if (previousSessionInterrupted_) {
      preferences.putBool("active", false);
    }
    preferences.end();
  }

  Serial.printf("Boot reset reason: %s (%d)%s\n",
                resetReasonName(bootResetReason_),
                static_cast<int>(bootResetReason_),
                previousSessionInterrupted_ ? " during OTA session" : "");
}

bool OtaManager::start(uint32_t nowMs) {
  if (active()) return true;
  if (esp_ota_get_next_update_partition(nullptr) == nullptr) {
    setError("No OTA app partition; select an OTA partition scheme");
    return false;
  }

  heater_.forceOff();
  Update.abort();
  progressPercent_ = 0;
  uploadTotalBytes_ = 0;
  uploadWrittenBytes_ = 0;
  uploadStarted_ = false;
  uploadAuthorized_ = false;
  restartPending_ = false;

  const uint32_t randomA = esp_random();
  const uint32_t randomB = esp_random();
  char suffix[9];
  makeHex(suffix, sizeof(suffix), randomA, 4);
  snprintf(ssid_, sizeof(ssid_), "Reflow-%s", suffix);

  snprintf(password_, sizeof(password_), "R%04lX%06lX",
           static_cast<unsigned long>(randomA & 0xFFFFUL),
           static_cast<unsigned long>(randomB & 0xFFFFFFUL));
  makeHex(csrfToken_, sizeof(csrfToken_),
          (static_cast<uint64_t>(randomA) << 32U) | randomB, 16);

  freeHeapBeforeWifi_ = ESP.getFreeHeap();
  freeHeapAfterWifi_ = 0;
  const uint32_t largestBlockBeforeWifi =
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  Serial.printf("OTA start: free heap=%lu, largest block=%lu\n",
                static_cast<unsigned long>(freeHeapBeforeWifi_),
                static_cast<unsigned long>(largestBlockBeforeWifi));
  if (freeHeapBeforeWifi_ < OTA_MIN_FREE_HEAP_BYTES ||
      largestBlockBeforeWifi < OTA_MIN_LARGEST_HEAP_BLOCK_BYTES) {
    setError("Not enough contiguous heap for Wi-Fi");
    return false;
  }

  setSessionMarker(true);
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
  delay(50);
  if (!WiFi.mode(WIFI_AP)) {
    setSessionMarker(false);
    setError("Could not enable AP mode");
    return false;
  }
  delay(OTA_WIFI_START_SETTLE_MS);

  wifi_power_t txPower = WIFI_POWER_8_5dBm;
  if (OTA_WIFI_TX_POWER_DBM <= 2) {
    txPower = WIFI_POWER_2dBm;
  } else if (OTA_WIFI_TX_POWER_DBM <= 5) {
    txPower = WIFI_POWER_5dBm;
  } else if (OTA_WIFI_TX_POWER_DBM <= 7) {
    txPower = WIFI_POWER_7dBm;
  } else if (OTA_WIFI_TX_POWER_DBM <= 9) {
    txPower = WIFI_POWER_8_5dBm;
  } else if (OTA_WIFI_TX_POWER_DBM <= 11) {
    txPower = WIFI_POWER_11dBm;
  } else {
    txPower = WIFI_POWER_13dBm;
  }
  WiFi.setTxPower(txPower);

  if (!WiFi.softAP(ssid_, password_, OTA_WIFI_CHANNEL, false,
                   OTA_MAX_CLIENTS)) {
    WiFi.mode(WIFI_OFF);
    setSessionMarker(false);
    setError("Could not start update Wi-Fi");
    return false;
  }
  WiFi.setTxPower(txPower);
  delay(OTA_WIFI_START_SETTLE_MS);

  freeHeapAfterWifi_ = ESP.getFreeHeap();
  Serial.printf("OTA AP ready: free heap=%lu, min heap=%lu, largest block=%lu, tx=%ddBm\n",
                static_cast<unsigned long>(freeHeapAfterWifi_),
                static_cast<unsigned long>(ESP.getMinFreeHeap()),
                static_cast<unsigned long>(
                    heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
                static_cast<int>(OTA_WIFI_TX_POWER_DBM));

  const IPAddress ip = WiFi.softAPIP();
  snprintf(address_, sizeof(address_), "http://%u.%u.%u.%u",
           ip[0], ip[1], ip[2], ip[3]);
  server_.begin();
  state_ = State::READY;
  sessionStartedMs_ = nowMs;
  strlcpy(detail_, "Connect and upload firmware.bin", sizeof(detail_));
  return true;
}

void OtaManager::stop() {
  if (uploading()) Update.abort();
  server_.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  setSessionMarker(false);
  heater_.forceOff();
  state_ = State::OFF;
  uploadStarted_ = false;
  uploadAuthorized_ = false;
  restartPending_ = false;
  progressPercent_ = 0;
  strlcpy(detail_, "Disabled", sizeof(detail_));
}

void OtaManager::update(uint32_t nowMs) {
  if (!active()) return;

  heater_.forceOff();
  server_.handleClient();

  if (restartPending_ && static_cast<int32_t>(nowMs - restartAtMs_) >= 0) {
    ESP.restart();
  }

  if (!uploading() && !restartPending_ &&
      (nowMs - sessionStartedMs_) >= OTA_SESSION_TIMEOUT_MS) {
    stop();
  }
}

const char *OtaManager::stateName() const {
  switch (state_) {
    case State::OFF: return "OFF";
    case State::READY: return "READY";
    case State::UPLOADING: return "UPLOAD";
    case State::SUCCESS: return "SUCCESS";
    case State::ERROR: return "ERROR";
    default: return "?";
  }
}

uint32_t OtaManager::secondsRemaining(uint32_t nowMs) const {
  if (!active() || uploading() || restartPending_) return 0;
  const uint32_t elapsed = nowMs - sessionStartedMs_;
  if (elapsed >= OTA_SESSION_TIMEOUT_MS) return 0;
  return (OTA_SESSION_TIMEOUT_MS - elapsed + 999UL) / 1000UL;
}

void OtaManager::configureRoutes() {
  if (routesConfigured_) return;
  routesConfigured_ = true;

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on(
      "/update", HTTP_POST,
      [this]() { handleUploadComplete(); },
      [this]() { handleUploadChunk(); });
  server_.onNotFound([this]() { handleNotFound(); });
}

void OtaManager::handleRoot() {
  if (!active()) {
    server_.send(503, "text/plain", "OTA session is not active");
    return;
  }
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "text/html", buildUploadPage());
}

void OtaManager::handleUploadChunk() {
  HTTPUpload &upload = server_.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadAuthorized_ = requestAuthorized();
    uploadStarted_ = false;
    progressPercent_ = 0;
    uploadWrittenBytes_ = 0;
    uploadTotalBytes_ = upload.totalSize;

    if (!uploadAuthorized_) {
      setError("Upload token rejected");
      return;
    }
    if (!upload.filename.endsWith(".bin")) {
      setError("Select an Arduino firmware .bin file");
      return;
    }

    heater_.forceOff();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      setError(Update.errorString());
      return;
    }
    uploadStarted_ = true;
    state_ = State::UPLOADING;
    strlcpy(detail_, "Receiving firmware", sizeof(detail_));
    return;
  }

  if (!uploadAuthorized_ || !uploadStarted_) return;

  if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadWrittenBytes_ == 0 && upload.currentSize > 0 &&
        upload.buf[0] != 0xE9) {
      Update.abort();
      uploadStarted_ = false;
      setError("Not an ESP32 application image");
      return;
    }

    const size_t written = Update.write(upload.buf, upload.currentSize);
    if (written != upload.currentSize) {
      Update.abort();
      uploadStarted_ = false;
      setError(Update.errorString());
      return;
    }
    uploadWrittenBytes_ += written;
    if (upload.totalSize > 0) uploadTotalBytes_ = upload.totalSize;
    if (uploadTotalBytes_ > 0) {
      const size_t calculated = uploadWrittenBytes_ * 100U / uploadTotalBytes_;
      progressPercent_ = static_cast<uint8_t>(calculated < 99U ? calculated : 99U);
    }
    return;
  }

  if (upload.status == UPLOAD_FILE_END) {
    if (!Update.end(true)) {
      uploadStarted_ = false;
      setError(Update.errorString());
      return;
    }
    uploadStarted_ = false;
    progressPercent_ = 100;
    state_ = State::SUCCESS;
    strlcpy(detail_, "Update verified; restarting", sizeof(detail_));
    setSessionMarker(false);
    restartPending_ = true;
    restartAtMs_ = millis() + OTA_RESTART_DELAY_MS;
    return;
  }

  if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    uploadStarted_ = false;
    setError("Upload aborted");
  }
}

void OtaManager::handleUploadComplete() {
  if (!requestAuthorized()) {
    server_.send(403, "text/plain", "Forbidden");
    return;
  }

  server_.sendHeader("Cache-Control", "no-store");
  if (state_ == State::SUCCESS) {
    server_.send(200, "text/html",
                 "<!doctype html><meta name=viewport content='width=device-width'>"
                 "<style>body{font-family:sans-serif;background:#0a0c12;color:#ebf1fa;"
                 "max-width:34rem;margin:4rem auto;padding:1rem}h1{color:#4edc97}</style>"
                 "<h1>Update installed</h1><p>The controller is restarting.</p>");
  } else {
    String body = "Update failed: ";
    body += detail_;
    server_.send(500, "text/plain", body);
  }
}

void OtaManager::handleNotFound() {
  server_.send(404, "text/plain", "Not found");
}

bool OtaManager::requestAuthorized() {
  return active() && server_.hasArg("token") &&
         server_.arg("token") == csrfToken_;
}

void OtaManager::setError(const char *message) {
  heater_.forceOff();
  state_ = State::ERROR;
  progressPercent_ = 0;
  strlcpy(detail_, message ? message : "OTA error", sizeof(detail_));
}

String OtaManager::buildUploadPage() const {
  String html;
  html.reserve(1700);
  html += F("<!doctype html><html><head><meta name=viewport "
            "content='width=device-width,initial-scale=1'><title>Reflow OTA</title>"
            "<style>body{font-family:system-ui,sans-serif;background:#0a0c12;"
            "color:#ebf1fa;max-width:36rem;margin:3rem auto;padding:1rem}"
            ".card{background:#161b26;border:1px solid #414c60;border-radius:14px;"
            "padding:1.3rem}h1{color:#4ad3ee}input,button{font:inherit;margin-top:1rem}"
            "button{background:#4edc97;border:0;border-radius:9px;padding:.8rem 1.2rem;"
            "font-weight:700}small{color:#9ba6b8}</style></head><body><div class=card>"
            "<h1>Reflow Controller Update</h1><p>Upload the compiled Arduino "
            "application <b>.bin</b> file.</p><form method=POST enctype='multipart/form-data' action='/update?token=");
  html += csrfToken_;
  html += F("'><input type=file name=firmware accept='.bin,application/octet-stream' "
            "required><br><button type=submit>Install update</button></form>"
            "<p><small>The heater remains disabled throughout this session. "
            "Do not remove power during upload or verification.</small></p>"
            "</div></body></html>");
  return html;
}

void OtaManager::makeHex(char *out, size_t capacity, uint64_t value,
                         uint8_t digits) {
  static constexpr char HEX_DIGITS[] = "0123456789ABCDEF";
  if (capacity == 0) return;
  const uint8_t maxDigits = static_cast<uint8_t>(capacity - 1U);
  if (digits > maxDigits) digits = maxDigits;
  if (digits > 16U) digits = 16U;
  for (uint8_t i = 0; i < digits; ++i) {
    const uint8_t shift = static_cast<uint8_t>((digits - 1U - i) * 4U);
    const uint8_t nibble = static_cast<uint8_t>((value >> shift) & 0x0FU);
    out[i] = HEX_DIGITS[nibble];
  }
  out[digits] = '\0';
}

const char *OtaManager::bootResetReasonName() const {
  return resetReasonName(bootResetReason_);
}

const char *OtaManager::resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWER ON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT WDT";
    case ESP_RST_TASK_WDT: return "TASK WDT";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_DEEPSLEEP: return "DEEP SLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    case ESP_RST_EFUSE: return "EFUSE";
    case ESP_RST_PWR_GLITCH: return "POWER GLITCH";
    case ESP_RST_CPU_LOCKUP: return "CPU LOCKUP";
    case ESP_RST_UNKNOWN:
    default: return "UNKNOWN";
  }
}

void OtaManager::setSessionMarker(bool active) {
  Preferences preferences;
  if (preferences.begin("reflow_ota", false)) {
    preferences.putBool("active", active);
    preferences.end();
  }
}
