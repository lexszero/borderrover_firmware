#pragma once

#define OTA_LISTEN_PORT 8032
#define OTA_BUFF_SIZE 1024

#ifdef __cplusplus
extern "C" {
#endif

void ota_server_init();
extern void sys_console_register_commands();

#ifdef __cplusplus
};
#endif

