#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
typedef enum { WIFI_IF_STA = 0, WIFI_IF_AP = 1 } wifi_interface_t;
typedef union { struct { uint8_t ssid[32]; uint8_t password[64]; } sta; } wifi_config_t;
inline esp_err_t esp_wifi_get_config(wifi_interface_t, wifi_config_t *conf)
{
    conf->sta.ssid[0] = 'C'; conf->sta.ssid[1] = 0; // a saved network exists
    return ESP_OK;
}
