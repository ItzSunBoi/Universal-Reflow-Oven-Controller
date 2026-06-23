#pragma once

#include <Preferences.h>

#include "Config.h"
#include "Types.h"

class ProfileStore {
 public:
  bool begin();
  bool save();
  void resetDefaults();

  uint8_t profileCount() const { return database_.profileCount; }
  uint8_t selectedIndex() const { return database_.selectedIndex; }
  bool selectProfile(uint8_t index);

  const ReflowProfile &profile(uint8_t index) const;
  ReflowProfile &profileMutable(uint8_t index);
  const ReflowProfile &selectedProfile() const;

  bool updateProfile(uint8_t index, const ReflowProfile &profile);
  int8_t addDuplicate(uint8_t sourceIndex);
  bool deleteProfile(uint8_t index);

  SystemSettings &settings() { return database_.settings; }
  const SystemSettings &settings() const { return database_.settings; }

  void addRunLog(const RunSummary &summary);
  uint8_t runLogCount() const { return database_.runLogCount; }
  const RunSummary &runLogNewest(uint8_t newestOffset) const;

  bool validateProfile(const ReflowProfile &profile) const;

 private:
  struct Database {
    uint32_t magic;
    uint16_t version;
    uint8_t profileCount;
    uint8_t selectedIndex;
    ReflowProfile profiles[MAX_PROFILES];
    SystemSettings settings;
    RunSummary logs[MAX_RUN_LOGS];
    uint8_t runLogCount;
    uint8_t nextLogIndex;
    uint8_t reserved[2];
    uint32_t crc32;
  };

  static constexpr uint32_t DATABASE_MAGIC = 0x52464C57UL; // "RFLW"
  static constexpr const char *NVS_NAMESPACE = "reflow";
  static constexpr const char *NVS_KEY = "database";

  Preferences preferences_;
  Database database_{};

  uint32_t calculateCrc(const Database &database) const;
  bool databaseValid() const;
  void createFactoryProfiles();
  uint32_t nextProfileUid() const;
  static void setStage(ReflowStage &stage, const char *name, StageMode mode,
                       float targetC, uint16_t durationS);
};
