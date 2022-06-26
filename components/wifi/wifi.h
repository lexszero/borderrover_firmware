/* Console example — declarations of command registration functions.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_WIFI_CHANNEL 1
esp_err_t wifi_init();
void wifi_set_hostname(const char *hostname);
void wifi_set_reconnect(bool reconnect);
void wifi_register_commands();
void wifi_wait_for_ip();

#ifdef __cplusplus
}
#endif

