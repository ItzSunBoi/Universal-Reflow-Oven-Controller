#include "ProfileStore.h"

#include <cstddef>
#include <cstring>

namespace {
ReflowProfile emptyProfile() {
  ReflowProfile profile{};
  profile.uid = 1;
  strlcpy(profile.name, "Profile", sizeof(profile.name));
  profile.factoryProfile = false;
  profile.stageCount = 1;
  profile.liquidusC = 180.0f;
  profile.maxTemperatureC = 230.0f;
  profile.maxRampCPerSecond = 2.0f;
  profile.targetTimeAboveLiquidusS = 45;
  strlcpy(profile.stages[0].name, "Ramp", sizeof(profile.stages[0].name));
  profile.stages[0].mode = StageMode::RAMP;
  profile.stages[0].targetC = 200.0f;
  profile.stages[0].durationS = 120;
  return profile;
}
}  // namespace

bool ProfileStore::begin() {
  if (!preferences_.begin(NVS_NAMESPACE, false)) {
    resetDefaults();
    return false;
  }

  const size_t storedSize = preferences_.getBytesLength(NVS_KEY);
  if (storedSize == sizeof(Database)) {
    preferences_.getBytes(NVS_KEY, &database_, sizeof(Database));
  }

  if (!databaseValid()) {
    resetDefaults();
    return save();
  }

  // v1.2 reserved this byte and initialized it to zero. Reusing that byte
  // preserves the NVS database layout and existing custom profiles.
  if (database_.settings.backlightPercent < TFT_BACKLIGHT_MIN_PERCENT ||
      database_.settings.backlightPercent > 100U) {
    database_.settings.backlightPercent = TFT_BACKLIGHT_DEFAULT_PERCENT;
    return save();
  }
  return true;
}

bool ProfileStore::save() {
  database_.magic = DATABASE_MAGIC;
  database_.version = PROFILE_STORE_VERSION;
  database_.crc32 = calculateCrc(database_);
  return preferences_.putBytes(NVS_KEY, &database_, sizeof(Database)) ==
         sizeof(Database);
}

void ProfileStore::resetDefaults() {
  database_ = Database{};
  database_.magic = DATABASE_MAGIC;
  database_.version = PROFILE_STORE_VERSION;
  database_.settings.temperatureOffsetC = 0.0f;
  database_.settings.buzzerEnabled = true;
  database_.settings.fanDuringCool = true;
  database_.settings.backlightPercent = TFT_BACKLIGHT_DEFAULT_PERCENT;
  createFactoryProfiles();
  database_.selectedIndex = 0;
  database_.crc32 = calculateCrc(database_);
}

bool ProfileStore::selectProfile(uint8_t index) {
  if (index >= database_.profileCount) {
    return false;
  }
  database_.selectedIndex = index;
  return save();
}

const ReflowProfile &ProfileStore::profile(uint8_t index) const {
  static const ReflowProfile fallback = emptyProfile();
  if (index >= database_.profileCount) {
    return fallback;
  }
  return database_.profiles[index];
}

ReflowProfile &ProfileStore::profileMutable(uint8_t index) {
  if (index >= database_.profileCount) {
    index = 0;
  }
  return database_.profiles[index];
}

const ReflowProfile &ProfileStore::selectedProfile() const {
  return profile(database_.selectedIndex);
}

bool ProfileStore::updateProfile(uint8_t index,
                                 const ReflowProfile &profileValue) {
  if (index >= database_.profileCount || !validateProfile(profileValue)) {
    return false;
  }
  database_.profiles[index] = profileValue;
  return save();
}

int8_t ProfileStore::addDuplicate(uint8_t sourceIndex) {
  if (database_.profileCount >= MAX_PROFILES ||
      sourceIndex >= database_.profileCount) {
    return -1;
  }

  const uint8_t newIndex = database_.profileCount++;
  database_.profiles[newIndex] = database_.profiles[sourceIndex];
  ReflowProfile &copy = database_.profiles[newIndex];
  copy.uid = nextProfileUid();
  copy.factoryProfile = false;

  char generatedName[sizeof(copy.name)];
  snprintf(generatedName, sizeof(generatedName), "Custom %u",
           static_cast<unsigned>(newIndex - 2U));
  strlcpy(copy.name, generatedName, sizeof(copy.name));

  database_.selectedIndex = newIndex;
  save();
  return static_cast<int8_t>(newIndex);
}

bool ProfileStore::deleteProfile(uint8_t index) {
  if (database_.profileCount <= 1 || index >= database_.profileCount) {
    return false;
  }

  for (uint8_t i = index; i + 1U < database_.profileCount; ++i) {
    database_.profiles[i] = database_.profiles[i + 1U];
  }
  database_.profiles[database_.profileCount - 1U] = ReflowProfile{};
  --database_.profileCount;

  if (database_.selectedIndex >= database_.profileCount) {
    database_.selectedIndex = database_.profileCount - 1U;
  } else if (database_.selectedIndex > index) {
    --database_.selectedIndex;
  }
  return save();
}

void ProfileStore::addRunLog(const RunSummary &summary) {
  database_.logs[database_.nextLogIndex] = summary;
  database_.nextLogIndex =
      static_cast<uint8_t>((database_.nextLogIndex + 1U) % MAX_RUN_LOGS);
  if (database_.runLogCount < MAX_RUN_LOGS) {
    ++database_.runLogCount;
  }
  save();
}

const RunSummary &ProfileStore::runLogNewest(uint8_t newestOffset) const {
  static const RunSummary empty{};
  if (newestOffset >= database_.runLogCount) {
    return empty;
  }
  int index = static_cast<int>(database_.nextLogIndex) - 1 - newestOffset;
  while (index < 0) {
    index += MAX_RUN_LOGS;
  }
  return database_.logs[index];
}

bool ProfileStore::validateProfile(const ReflowProfile &profileValue) const {
  if (profileValue.name[0] == '\0' || profileValue.stageCount == 0 ||
      profileValue.stageCount > MAX_PROFILE_STAGES) {
    return false;
  }
  if (profileValue.liquidusC < 60.0f ||
      profileValue.liquidusC > GLOBAL_MAX_TEMPERATURE_C) {
    return false;
  }
  if (profileValue.maxTemperatureC <= profileValue.liquidusC ||
      profileValue.maxTemperatureC > GLOBAL_MAX_TEMPERATURE_C) {
    return false;
  }
  if (profileValue.maxRampCPerSecond < 0.2f ||
      profileValue.maxRampCPerSecond > 5.0f) {
    return false;
  }

  for (uint8_t i = 0; i < profileValue.stageCount; ++i) {
    const ReflowStage &stage = profileValue.stages[i];
    if (stage.durationS < 5 || stage.durationS > 900 ||
        stage.targetC < 20.0f ||
        stage.targetC > profileValue.maxTemperatureC) {
      return false;
    }
  }
  return true;
}

uint32_t ProfileStore::calculateCrc(const Database &databaseValue) const {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&databaseValue);
  const size_t length = offsetof(Database, crc32);
  uint32_t crc = 0xFFFFFFFFUL;
  for (size_t i = 0; i < length; ++i) {
    crc ^= bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const uint32_t mask = -(crc & 1UL);
      crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
    }
  }
  return ~crc;
}

bool ProfileStore::databaseValid() const {
  if (database_.magic != DATABASE_MAGIC ||
      database_.version != PROFILE_STORE_VERSION ||
      database_.profileCount == 0 ||
      database_.profileCount > MAX_PROFILES ||
      database_.selectedIndex >= database_.profileCount ||
      database_.crc32 != calculateCrc(database_)) {
    return false;
  }
  for (uint8_t i = 0; i < database_.profileCount; ++i) {
    if (!validateProfile(database_.profiles[i])) {
      return false;
    }
  }
  return true;
}

uint32_t ProfileStore::nextProfileUid() const {
  uint32_t greatest = 0;
  for (uint8_t i = 0; i < database_.profileCount; ++i) {
    greatest = max(greatest, database_.profiles[i].uid);
  }
  return greatest + 1U;
}

void ProfileStore::setStage(ReflowStage &stage, const char *name,
                            StageMode mode, float targetC,
                            uint16_t durationS) {
  stage = ReflowStage{};
  strlcpy(stage.name, name, sizeof(stage.name));
  stage.mode = mode;
  stage.targetC = targetC;
  stage.durationS = durationS;
}

void ProfileStore::createFactoryProfiles() {
  database_.profileCount = 3;

  ReflowProfile &sac = database_.profiles[0];
  sac = ReflowProfile{};
  sac.uid = 1001;
  strlcpy(sac.name, "SAC305 217C", sizeof(sac.name));
  sac.factoryProfile = true;
  sac.stageCount = 5;
  sac.liquidusC = 217.0f;
  sac.maxTemperatureC = 255.0f;
  sac.maxRampCPerSecond = 2.5f;
  sac.targetTimeAboveLiquidusS = 60;
  setStage(sac.stages[0], "Preheat", StageMode::RAMP, 150.0f, 90);
  setStage(sac.stages[1], "Soak", StageMode::RAMP, 180.0f, 90);
  setStage(sac.stages[2], "Reflow", StageMode::RAMP, 245.0f, 45);
  setStage(sac.stages[3], "Peak", StageMode::HOLD, 245.0f, 20);
  setStage(sac.stages[4], "Cool", StageMode::COOL, 120.0f, 150);

  ReflowProfile &low = database_.profiles[1];
  low = ReflowProfile{};
  low.uid = 1002;
  strlcpy(low.name, "SnBi 138C", sizeof(low.name));
  low.factoryProfile = true;
  low.stageCount = 5;
  low.liquidusC = 138.0f;
  low.maxTemperatureC = 175.0f;
  low.maxRampCPerSecond = 2.0f;
  low.targetTimeAboveLiquidusS = 50;
  setStage(low.stages[0], "Preheat", StageMode::RAMP, 100.0f, 60);
  setStage(low.stages[1], "Soak", StageMode::RAMP, 120.0f, 60);
  setStage(low.stages[2], "Reflow", StageMode::RAMP, 165.0f, 35);
  setStage(low.stages[3], "Peak", StageMode::HOLD, 165.0f, 20);
  setStage(low.stages[4], "Cool", StageMode::COOL, 100.0f, 120);

  ReflowProfile &mid = database_.profiles[2];
  mid = ReflowProfile{};
  mid.uid = 1003;
  strlcpy(mid.name, "Mid-temp 180C", sizeof(mid.name));
  mid.factoryProfile = true;
  mid.stageCount = 5;
  mid.liquidusC = 180.0f;
  mid.maxTemperatureC = 225.0f;
  mid.maxRampCPerSecond = 2.3f;
  mid.targetTimeAboveLiquidusS = 55;
  setStage(mid.stages[0], "Preheat", StageMode::RAMP, 140.0f, 75);
  setStage(mid.stages[1], "Soak", StageMode::RAMP, 170.0f, 75);
  setStage(mid.stages[2], "Reflow", StageMode::RAMP, 210.0f, 40);
  setStage(mid.stages[3], "Peak", StageMode::HOLD, 210.0f, 20);
  setStage(mid.stages[4], "Cool", StageMode::COOL, 110.0f, 130);
}
