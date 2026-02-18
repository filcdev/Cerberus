#include "dz_db.h"
#include "dz_state.h"

#include <algorithm>

namespace {
bool sameUids(const std::vector<UidEntry>& left, const std::vector<UidEntry>& right)
{
  if (left.size() != right.size()) return false;

  std::vector<UidEntry> a = left;
  std::vector<UidEntry> b = right;

  auto sorter = [](const UidEntry& lhs, const UidEntry& rhs) {
    if (lhs.uid == rhs.uid) return lhs.name < rhs.name;
    return lhs.uid < rhs.uid;
  };

  std::sort(a.begin(), a.end(), sorter);
  std::sort(b.begin(), b.end(), sorter);

  for (size_t i = 0; i < a.size(); ++i) {
    if (a[i].uid != b[i].uid || a[i].name != b[i].name) {
      return false;
    }
  }

  return true;
}
} // namespace

DZDBControl dbControl;

DZDBControl::DZDBControl() : logger("DB") {}

void DZDBControl::begin()
{
  logger.info("Initializing DB");
  if (!SPIFFS.begin(true)) {
    logger.error("SPIFFS mount failed");
    stateControl.setError(ErrorSource::DB, true, "SPIFFS mount");
    return;
  }
  loadUIDs();
}

bool DZDBControl::saveUIDs()
{
  logger.info("Saving UIDs to file");
  File file = SPIFFS.open(uidsFilePath, FILE_WRITE);
  if (!file) {
    logger.error("Failed to open UIDs file for writing");
    stateControl.setError(ErrorSource::DB, true, "UID Save Fail");
    return false;
  }
  
  JsonDocument doc;
  for (const auto& entry : uids) {
    doc[entry.uid] = entry.name;
  }

  if (serializeJson(doc, file) == 0) {
    logger.error("Failed to serialize UIDs");
    stateControl.setError(ErrorSource::DB, true, "UID Save Fail");
    file.close();
    return false;
  }
  
  logger.info("UIDs saved successfully");
  stateControl.setError(ErrorSource::DB, false);
  file.close();
  return true;
}

bool DZDBControl::isAuthorized(const std::string& uid, std::string& nameOut)
{
  for (const auto& entry : uids) {
    if (entry.uid == uid) {
      nameOut = entry.name;
      return true;
    }
  }
  return false;
}

bool DZDBControl::loadUIDs()
{
  logger.info("Loading UIDs from file");
  if (!SPIFFS.exists(uidsFilePath)) {
    logger.info("UIDs file not found, creating new one");
    return saveUIDs();
  }
  uids.clear();

  File file = SPIFFS.open(uidsFilePath, FILE_READ);
  if (!file) {
    logger.error("Failed to open UIDs file for reading");
    stateControl.setError(ErrorSource::DB, true, "UID File Fail");
    return false;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    logger.error("Failed to parse UIDs file: %s", error.c_str());
    stateControl.setError(ErrorSource::DB, true, "UID Load Fail");
    return false;
  }

  uids.clear();
  JsonObject root = doc.as<JsonObject>();
  for (JsonPair kv : root) {
    std::string uid = kv.key().c_str();
    if(kv.value().is<const char*>()) {
      std::string name = kv.value().as<std::string>();
      uids.push_back({uid, name});
    }
  }

  logger.info("Loaded %d UIDs", uids.size());
  stateControl.setError(ErrorSource::DB, false);
  return true;
}

void DZDBControl::updateFromJSON(JsonArray root)
{
  logger.info("Updating UIDs from JSON");
  std::vector<UidEntry> nextUids;
  for (JsonVariant v : root) {
    if (!v.is<JsonObject>()) continue;
    JsonObject obj = v.as<JsonObject>();
    if (obj["uid"].is<const char*>() && obj["name"].is<const char*>()) {
      std::string uid = obj["uid"].as<std::string>();
      std::string name = obj["name"].as<std::string>();
      nextUids.push_back({uid, name});
    }
  }

  if (sameUids(uids, nextUids)) {
    logger.info("UIDs unchanged; skipping SPIFFS write");
    return;
  }

  uids = std::move(nextUids);
  saveUIDs();
}