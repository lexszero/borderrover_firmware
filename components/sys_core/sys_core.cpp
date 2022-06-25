#include "sys_core.hpp"
#include "core_controls.hpp"
#include "core_status_led.hpp"
#include "cxx_espnow.hpp"
#include "utils.h"

#include "ota_server.h"
#include "sys_console.h"
#include "wifi.h"

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_spiffs.h"

#define TAG "Core"

namespace Core {

using esp_now::espnow;

void sys_console_register_commands() {
	wifi_register_commands();
}

void ota_server_start_callback() {
	status_led->blink(200, 200, 3, 500);
}

esp_err_t fs_init() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/web",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}


void init() {
	status_led = std::make_unique<StatusLed>(
		"led_status",
		OutputGPIO("led_status", GPIO_NUM_27));

	status_led->blink(200);

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// 1.OTA app partition table has a smaller NVS partition size than the non-OTA
		// partition table. This size mismatch may cause NVS initialization to fail.
		// 2.NVS partition contains data in new format and cannot be recognized by this version of code.
		// If this happens, we erase NVS partition and initialize NVS again.
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK( err );

	fs_init();
	esp_netif_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	wifi_init();
	sys_console_init();
	ota_server_init();

	http = std::make_unique<HTTPServer>("");
	espnow = std::make_unique<esp_now::ESPNow>();
	controls = std::make_unique<Controls>();
}

Config config;

enum ConfigParamId {
#define CONFIG_PARAM(type, name, ...) __cat(ConfigParam_, name),
#define CONFIG_PARAM_STR(name, ...) CONFIG_PARAM(_, name)
	CONFIG_PARAMS
#undef CONFIG_PARAM
#undef CONFIG_PARAM_STR
	_ConfigParamCount
};

static const uint16_t CONFIG_VERSION = (_ConfigParamCount << 8);
static const uint32_t CONFIG_MAGIC =   (0xdead << 16) | CONFIG_VERSION;

Config::Config()
{
}

void Config::load() {
	/*
	size_t offset = EEPROM_OFFSET;
	uint32_t magic;

	log("loading config");
	offset += eeprom_read(offset, &magic, sizeof(magic));
	if (magic == CONFIG_MAGIC) {
		log("magic ok, version %d", magic & 0xFF);

		#define CONFIG_PARAM(type, name, defval, desc) \
			offset += eeprom_read(offset, &config.name, sizeof(config.name)); \
			logval(desc, config.name);
		#define CONFIG_PARAM_STR(name, len, defval, desc) \
			offset += eeprom_read(offset, config.name, len); \
			logval(desc, config.name);
		CONFIG_PARAMS
		#undef CONFIG_PARAM
		#undef CONFIG_PARAM_STR
	}
	else {
		log("bad magic 0x%x, saving default config", magic);
		Config::save();
	}
	*/
}

void Config::save() {
	/*
	size_t offset = EEPROM_OFFSET;
	log("saving config");

	offset += eeprom_write(offset, &CONFIG_MAGIC, sizeof(CONFIG_MAGIC));
	#define CONFIG_PARAM(type, name, defval, desc) \
		offset += eeprom_write(offset, &config.name, sizeof(config.name));
	#define CONFIG_PARAM_STR(name, len, defval, desc) \
		offset += eeprom_write_str(offset, config.name, len);
	CONFIG_PARAMS
	#undef CONFIG_PARAM
	#undef CONFIG_PARAM_STR

	EEPROM.commit();
	*/
}
} // namespace Core

void core_init() {
	Core::init();
}
