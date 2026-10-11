#include "SlotGuard.h"

#include <Logging.h>

#ifndef SIMULATOR
#include <esp_app_desc.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#endif

#include <cstring>

namespace inkhome {

bool otherSlotHoldsOtherFirmware() {
#ifdef SIMULATOR
  return false;
#else
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
  if (!other || other == running) return false;

  uint8_t magic = 0;
  if (esp_partition_read(other, 0, &magic, 1) != ESP_OK || magic != 0xE9) return false;

  esp_app_desc_t desc = {};
  if (esp_ota_get_partition_description(other, &desc) != ESP_OK) {
    LOG_INF("SLOT", "%s holds an image without a readable description; protecting it", other->label);
    return true;
  }
  const esp_app_desc_t* self = esp_app_get_description();
  const bool same = strncmp(desc.project_name, self->project_name, sizeof(desc.project_name)) == 0;
  if (!same) LOG_INF("SLOT", "%s holds %.32s %.32s; protecting it", other->label, desc.project_name, desc.version);
  return !same;
#endif
}

}  // namespace inkhome
