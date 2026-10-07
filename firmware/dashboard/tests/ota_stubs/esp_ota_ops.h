// Host stand-in for the OTA partition API of ESP-IDF.
#pragma once
#include <stdint.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
typedef enum {
    ESP_OTA_IMG_NEW = 0x0U,
    ESP_OTA_IMG_PENDING_VERIFY = 0x1U,
    ESP_OTA_IMG_VALID = 0x2U,
    ESP_OTA_IMG_INVALID = 0x3U,
    ESP_OTA_IMG_ABORTED = 0x4U,
    ESP_OTA_IMG_UNDEFINED = 0xFFFFFFFFU,
} esp_ota_img_states_t;
typedef struct { uint32_t address; uint32_t size; const char *label; } esp_partition_t;

extern esp_partition_t *g_runningPartition;
extern esp_partition_t *g_nextPartition;
extern esp_ota_img_states_t g_runningState;
extern int g_markedValid;
inline const esp_partition_t *esp_ota_get_running_partition() { return g_runningPartition; }
inline const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *) { return g_nextPartition; }
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t *, esp_ota_img_states_t *state)
{
    *state = g_runningState;
    return ESP_OK;
}
inline esp_err_t esp_ota_mark_app_valid_cancel_rollback()
{
    ++g_markedValid;
    g_runningState = ESP_OTA_IMG_VALID;
    return ESP_OK;
}
