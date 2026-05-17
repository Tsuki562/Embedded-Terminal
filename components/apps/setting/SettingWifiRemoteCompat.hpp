#pragma once

// Some project configurations enable esp_wifi_remote through esp_hosted, but
// the generated SLAVE_IDF_TARGET_* Kconfig symbols do not always reach this
// translation unit. The injected esp_wifi.h still expects the corresponding
// CONFIG_WIFI_RMT_* defaults to exist when WIFI_INIT_CONFIG_DEFAULT() is used.
//
// When that happens, infer the slave target from esp_hosted's selected
// coprocessor target and then pull in esp_wifi_remote's generated defaults
// before including esp_wifi.h.

#if CONFIG_ESP_WIFI_REMOTE_ENABLED && \
    (!defined(CONFIG_WIFI_RMT_STATIC_RX_BUFFER_NUM) || \
     !defined(CONFIG_WIFI_RMT_DYNAMIC_RX_BUFFER_NUM) || \
     !defined(CONFIG_WIFI_RMT_TX_BUFFER_TYPE) || \
     !defined(CONFIG_WIFI_RMT_DYNAMIC_RX_MGMT_BUF) || \
     !defined(CONFIG_WIFI_RMT_ESPNOW_MAX_ENCRYPT_NUM))

#if !defined(CONFIG_SLAVE_IDF_TARGET_ESP32) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32S2) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32C3) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32S3) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32C2) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32C6) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32H2) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32P4) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32C5) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32C61) && \
    !defined(CONFIG_SLAVE_IDF_TARGET_ESP32H21)

#if CONFIG_ESP_HOSTED_CP_TARGET_ESP32
#define CONFIG_SLAVE_IDF_TARGET_ESP32 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32S2
#define CONFIG_SLAVE_IDF_TARGET_ESP32S2 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32C3
#define CONFIG_SLAVE_IDF_TARGET_ESP32C3 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32S3
#define CONFIG_SLAVE_IDF_TARGET_ESP32S3 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32C2
#define CONFIG_SLAVE_IDF_TARGET_ESP32C2 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6
#define CONFIG_SLAVE_IDF_TARGET_ESP32C6 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32H2
#define CONFIG_SLAVE_IDF_TARGET_ESP32H2 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32C5
#define CONFIG_SLAVE_IDF_TARGET_ESP32C5 1
#elif CONFIG_ESP_HOSTED_CP_TARGET_ESP32C61
#define CONFIG_SLAVE_IDF_TARGET_ESP32C61 1
#endif

#endif

#include "esp_wifi_default_config.h"

#endif

#include "esp_wifi.h"
