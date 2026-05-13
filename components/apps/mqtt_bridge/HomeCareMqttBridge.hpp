#pragma once

#include "esp_err.h"
#include "HomeCareMqttProtocol.hpp"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t homecare_mqtt_bridge_init(void);
bool homecare_mqtt_bridge_is_connected(void);
bool homecare_mqtt_bridge_receive(HomeCareMqttInboundMessage *out);
esp_err_t homecare_mqtt_bridge_publish_action(homecare_mqtt_action_t action);
esp_err_t homecare_mqtt_bridge_publish_mode(homecare_mqtt_mode_t mode);
esp_err_t homecare_mqtt_bridge_publish_event(const HomeCareMqttEvent *event,
                                             homecare_mqtt_mode_t mode);

#ifdef __cplusplus
}
#endif
