#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>

#include "Config.h"
#include "HeaterController.h"

class OtaManager {
 public:
  enum class State : uint8_t {
    OFF,
    READY,
    UPLOADING,
    SUCCESS,
    ERROR,
  };

  explicit OtaManager(HeaterController &heater);

  void begin();
  bool start(uint32_t nowMs);
  void stop();
  void update(uint32_t nowMs);

  bool active() const { return state_ != State::OFF; }
  bool uploading() const { return state_ == State::UPLOADING; }
  State state() const { return state_; }
  const char *stateName() const;
  const char *ssid() const { return ssid_; }
  const char *password() const { return password_; }
  const char *address() const { return address_; }
  const char *detail() const { return detail_; }
  uint8_t progressPercent() const { return progressPercent_; }
  uint32_t secondsRemaining(uint32_t nowMs) const;
  const char *bootResetReasonName() const;
  bool previousSessionInterrupted() const { return previousSessionInterrupted_; }
  uint32_t freeHeapBeforeWifi() const { return freeHeapBeforeWifi_; }
  uint32_t freeHeapAfterWifi() const { return freeHeapAfterWifi_; }

 private:
  HeaterController &heater_;
  WebServer server_{80};
  State state_ = State::OFF;
  bool routesConfigured_ = false;
  bool uploadAuthorized_ = false;
  bool uploadStarted_ = false;
  bool restartPending_ = false;
  char ssid_[24] = "";
  char password_[16] = "";
  char address_[24] = "";
  char csrfToken_[24] = "";
  char detail_[72] = "Disabled";
  uint8_t progressPercent_ = 0;
  uint32_t sessionStartedMs_ = 0;
  uint32_t restartAtMs_ = 0;
  size_t uploadTotalBytes_ = 0;
  size_t uploadWrittenBytes_ = 0;
  esp_reset_reason_t bootResetReason_ = ESP_RST_UNKNOWN;
  bool previousSessionInterrupted_ = false;
  uint32_t freeHeapBeforeWifi_ = 0;
  uint32_t freeHeapAfterWifi_ = 0;

  void configureRoutes();
  void handleRoot();
  void handleUploadChunk();
  void handleUploadComplete();
  void handleNotFound();
  bool requestAuthorized();
  void setError(const char *message);
  String buildUploadPage() const;
  static void makeHex(char *out, size_t capacity, uint64_t value,
                      uint8_t digits);
  static const char *resetReasonName(esp_reset_reason_t reason);
  static void setSessionMarker(bool active);
};
