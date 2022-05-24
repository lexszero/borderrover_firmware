/* Console example — WiFi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
   */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
//#include "esp_event_loop.h"
#include "wifi.h"

#define JOIN_TIMEOUT_MS (10000)

typedef struct {
	struct arg_str *ssid;
	struct arg_str *password;
	struct arg_end *end;
} wifi_args_t;

typedef struct {
	struct arg_str *ssid;
	struct arg_end *end;
} wifi_scan_arg_t;

static wifi_args_t sta_args;
static wifi_scan_arg_t scan_args;
static wifi_args_t ap_args;
static bool reconnect = true;
static const char *TAG="cmd_wifi";

static EventGroupHandle_t wifi_event_group = NULL;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;
static esp_netif_t *netif_sta = NULL;
static esp_netif_t *netif_ap = NULL;


static esp_ip4_addr_t s_ip_addr;
static char s_ssid[32];

static void on_scan_done(void* arg, esp_event_base_t event_base,
                      int32_t event_id, void* event_data)
{
	uint16_t sta_number = 0;
	uint8_t i;
	wifi_ap_record_t *ap_list_buffer;

	esp_wifi_scan_get_ap_num(&sta_number);
	ap_list_buffer = malloc(sta_number * sizeof(wifi_ap_record_t));
	if (ap_list_buffer == NULL) {
		ESP_LOGE(TAG, "Failed to malloc buffer to print scan results");
		return;
	}

	if (esp_wifi_scan_get_ap_records(&sta_number,(wifi_ap_record_t *)ap_list_buffer) == ESP_OK) {
		for(i=0; i<sta_number; i++) {
			ESP_LOGI(TAG, "[%s][rssi=%d]", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
		}
	}
	free(ap_list_buffer);
}

static void on_got_ip(void* arg, esp_event_base_t event_base,
                      int32_t event_id, void* event_data)
{
	ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
	memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
	ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&s_ip_addr));
	xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
	xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
}

static void on_wifi_connect(void *arg, esp_event_base_t event_base,
		int32_t event_id, void *event_data)
{
	wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
	memcpy(s_ssid, &event->ssid, event->ssid_len);
	ESP_LOGI(TAG, "Connected to %s", s_ssid);
}
static void on_wifi_disconnect(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "Disconnected");
	s_ssid[0] = 0;
	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
	xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
	if (reconnect) {
		ESP_LOGI(TAG, "Trying to reconnect...");
		ESP_ERROR_CHECK( esp_wifi_connect() );
	}
}

esp_err_t wifi_connect(wifi_config_t *config) {
	ESP_LOGI(TAG, "Connecting...");
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	if (config) {
		ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, config));
	}
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_connect());

	return ESP_OK;
}

esp_err_t wifi_init(void)
{
	esp_log_level_set("wifi", ESP_LOG_INFO);
	if (wifi_event_group != NULL) {
		return ESP_ERR_INVALID_STATE;
	}

	wifi_event_group = xEventGroupCreate();

	netif_sta = esp_netif_create_default_wifi_sta();
	assert(netif_sta);
	netif_ap = esp_netif_create_default_wifi_ap();
	assert(netif_ap);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &on_scan_done, NULL));

	wifi_config_t wifi_cfg;
	ESP_ERROR_CHECK(esp_wifi_get_config(ESP_IF_WIFI_STA, &wifi_cfg));
	if (strlen((char *)wifi_cfg.sta.ssid) > 0) {
		return wifi_connect(NULL);
	}

	return ESP_OK;
}

void wifi_wait_for_ip()
{
    ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}

static bool wifi_cmd_sta_join(const char* ssid, const char* pass)
{
	int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);

	wifi_config_t wifi_config = { 0 };

	strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	if (pass) {
		strncpy((char*) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
	}

	if (bits & CONNECTED_BIT) {
		reconnect = false;
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		ESP_ERROR_CHECK( esp_wifi_disconnect() );
		xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 0, 1, portTICK_PERIOD_MS);
	}

	reconnect = true;
	wifi_connect(&wifi_config);

	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 5000/portTICK_PERIOD_MS);

	return true;
}

static int wifi_cmd_sta(int argc, char** argv)
{
	int nerrors = arg_parse(argc, argv, (void**) &sta_args);

	if (nerrors != 0) {
		arg_print_errors(stderr, sta_args.end, argv[0]);
		return 1;
	}

	ESP_LOGI(TAG, "sta connecting to '%s'", sta_args.ssid->sval[0]);
	wifi_cmd_sta_join(sta_args.ssid->sval[0], sta_args.password->sval[0]);
	return 0;
}

static bool wifi_cmd_sta_scan(const char* ssid)
{
	wifi_scan_config_t scan_config = { 0 };
	scan_config.ssid = (uint8_t *) ssid;

	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_scan_start(&scan_config, false) );

	return true;
}

static int wifi_cmd_scan(int argc, char** argv)
{
	int nerrors = arg_parse(argc, argv, (void**) &scan_args);

	if (nerrors != 0) {
		arg_print_errors(stderr, scan_args.end, argv[0]);
		return 1;
	}

	ESP_LOGI(TAG, "sta start to scan");
	if ( scan_args.ssid->count == 1 ) {
		wifi_cmd_sta_scan(scan_args.ssid->sval[0]);
	} else {
		wifi_cmd_sta_scan(NULL);
	}
	return 0;
}

static bool wifi_cmd_ap_set(const char* ssid, const char* pass)
{
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = "",
			.ssid_len = 0,
			.max_connection = 4,
			.password = "",
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};

	reconnect = false;
	strncpy((char*) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
	if (pass) {
		if (strlen(pass) != 0 && strlen(pass) < 8) {
			reconnect = true;
			ESP_LOGE(TAG, "password less than 8");
			return false;
		}
		strncpy((char*) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
	}

	if (strlen(pass) == 0) {
		wifi_config.ap.authmode = WIFI_AUTH_OPEN;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
	return true;
}

static int wifi_cmd_ap(int argc, char** argv)
{
	int nerrors = arg_parse(argc, argv, (void**) &ap_args);

	if (nerrors != 0) {
		arg_print_errors(stderr, ap_args.end, argv[0]);
		return 1;
	}

	wifi_cmd_ap_set(ap_args.ssid->sval[0], ap_args.password->sval[0]);
	ESP_LOGI(TAG, "AP mode, %s %s", ap_args.ssid->sval[0], ap_args.password->sval[0]);
	return 0;
}

static int wifi_cmd_query(int argc, char** argv)
{
	wifi_config_t cfg;
	wifi_mode_t mode;

	esp_wifi_get_mode(&mode);
	if (WIFI_MODE_AP == mode) {
		esp_wifi_get_config(WIFI_IF_AP, &cfg);
		ESP_LOGI(TAG, "AP mode, %s %s", cfg.ap.ssid, cfg.ap.password);
	} else if (WIFI_MODE_STA == mode) {
		int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
		if (bits & CONNECTED_BIT) {
			esp_wifi_get_config(WIFI_IF_STA, &cfg);
			ESP_LOGI(TAG, "sta mode, connected %s", cfg.ap.ssid);
		} else {
			ESP_LOGI(TAG, "sta mode, disconnected");
		}
	} else {
		ESP_LOGI(TAG, "NULL mode");
		return 0;
	}

	return 0;
}

static uint32_t wifi_get_local_ip(void)
{
	int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
	esp_netif_t *ifx = netif_ap;
	esp_netif_ip_info_t ip_info;
	wifi_mode_t mode;

	esp_wifi_get_mode(&mode);
	if (WIFI_MODE_STA == mode) {
		bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
		if (bits & CONNECTED_BIT) {
			ifx = netif_sta;
		} else {
			ESP_LOGE(TAG, "sta has no IP");
			return 0;
		}
	}

	esp_netif_get_ip_info(ifx, &ip_info);
	return ip_info.ip.addr;
}

void wifi_register_commands()
{
	sta_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
	sta_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
	sta_args.end = arg_end(2);

	const esp_console_cmd_t sta_cmd = {
		.command = "wifi_sta",
		.help = "WiFi is station mode, join specified soft-AP",
		.hint = NULL,
		.func = &wifi_cmd_sta,
		.argtable = &sta_args
	};

	ESP_ERROR_CHECK( esp_console_cmd_register(&sta_cmd) );

	scan_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP want to be scanned");
	scan_args.end = arg_end(1);

	const esp_console_cmd_t scan_cmd = {
		.command = "wifi_scan",
		.help = "WiFi is station mode, start scan ap",
		.hint = NULL,
		.func = &wifi_cmd_scan,
		.argtable = &scan_args
	};

	ap_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
	ap_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
	ap_args.end = arg_end(2);


	ESP_ERROR_CHECK( esp_console_cmd_register(&scan_cmd) );

	const esp_console_cmd_t ap_cmd = {
		.command = "wifi_ap",
		.help = "AP mode, configure ssid and password",
		.hint = NULL,
		.func = &wifi_cmd_ap,
		.argtable = &ap_args
	};

	ESP_ERROR_CHECK( esp_console_cmd_register(&ap_cmd) );

	const esp_console_cmd_t query_cmd = {
		.command = "wifi_query",
		.help = "query WiFi info",
		.hint = NULL,
		.func = &wifi_cmd_query,
	};
	ESP_ERROR_CHECK( esp_console_cmd_register(&query_cmd) );
}
