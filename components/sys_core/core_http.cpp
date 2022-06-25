/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "core_config.hpp"
#include "core_http.hpp"

#include "cxx_espnow_peer.hpp"

#include "esp_app_format.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_system.h"

#include <cstring>
#include <fcntl.h>
#include <stdexcept>

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)


#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

namespace Core {

const char *REST_TAG = "http";

std::unique_ptr<HTTPServer> http;

esp_err_t httpd_resp_json(httpd_req_t *req, const json& j)
{
	auto ret = httpd_resp_set_type(req, "application/json");
	if (ret != ESP_OK) {
		ESP_LOGE(REST_TAG, "Failed to set response type");
		return ret;
	}
	return httpd_resp_sendstr(req, j.dump().c_str());
}

namespace {

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
	const char *type = "text/plain";
	if (CHECK_FILE_EXTENSION(filepath, ".html")) {
		type = "text/html";
	} else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
		type = "application/javascript";
	} else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
		type = "text/css";
	} else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
		type = "image/png";
	} else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
		type = "image/x-icon";
	} else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
		type = "text/xml";
	}
	return httpd_resp_set_type(req, type);
}

/* Simple handler for getting system handler */
static esp_err_t system_info_get_handler(httpd_req_t *req)
{
	const esp_app_desc_t *app_desc = esp_ota_get_app_description();

	uint8_t mac[8];
	using MacAddr = esp_now::PeerAddress;

	esp_read_mac(mac, ESP_MAC_WIFI_STA);
	MacAddr mac_sta(mac);

	esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
	MacAddr mac_ap(mac);

	esp_read_mac(mac, ESP_MAC_BT);
	MacAddr mac_bt(mac);

	json resp = {
		{"app_name", app_desc->project_name},
		{"app_version", app_desc->version},
		{"idf_version", app_desc->idf_ver},
		{"hostname", config.hostname},
		{"heap_free", esp_get_free_heap_size()},
		{"heap_internal_free", esp_get_free_internal_heap_size()},
		{"heap_minimum_free", esp_get_minimum_free_heap_size()},
		{"mac_wifi_sta", to_string(mac_sta)},
		{"mac_wifi_ap", to_string(mac_ap)},
		{"mac_bt", to_string(mac_bt)},
	};
	return httpd_resp_json(req, resp);
}

} // namespace

HTTPServer::HTTPServer(const char *base_path)
{
	if (!base_path)
		throw std::runtime_error("Wrong base path for HTTP server");

	auto context = std::make_unique<server_context>();
	strlcpy(context->base_path, base_path, sizeof(context->base_path));

	config.uri_match_fn = httpd_uri_match_wildcard;

	ESP_LOGI(REST_TAG, "Starting HTTP Server");
	auto err = httpd_start(&server, &config);
	if (err != ESP_OK)
		throw std::runtime_error("HTTP server start failed");

	on("/api/v1/system/info", HTTP_GET, system_info_get_handler);

	/* URI handler for getting web server files */
#if 0
	httpd_uri_t common_get_uri = {
		.uri = "/*",
		.method = HTTP_GET,
		.handler = common_get_handler,
		.user_ctx = context.get(),
	};
	httpd_register_uri_handler(server, &common_get_uri);
#endif
	srv_ctx = std::move(context);
}

void HTTPServer::on(
		const std::string& uri,
		httpd_method_t method,
		HTTPHandler handler)
{
	auto r = new route {
		.uri = {
			.uri = uri.c_str(),
			.method = method,
			.handler = route_handler,
			.user_ctx = nullptr
		},
		.handler = handler,
	};
	r->uri.user_ctx = r;
	ESP_LOGI(REST_TAG, "Route %s %s",
			(r->uri.method == HTTP_GET) ? "GET"
			: (r->uri.method == HTTP_POST) ? "POST"
			: "",
			r->uri.uri);
	httpd_register_uri_handler(server, &r->uri);
}

esp_err_t HTTPServer::route_handler(httpd_req_t *req)
{
	try {
		auto *ctx = static_cast<route *>(req->user_ctx);
		return ctx->handler(req);
	}
	catch (std::exception& e)
	{
		ESP_LOGE(REST_TAG, "Handler failed: %s", e.what());
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, e.what());
	}
	return ESP_OK;
}

/* Send HTTP response with the contents of the requested file */
esp_err_t HTTPServer::common_get_handler(httpd_req_t *req)
{
	char filepath[FILE_PATH_MAX];

	auto context = static_cast<server_context *>(req->user_ctx);
	strlcpy(filepath, context->base_path, sizeof(filepath));
	if (req->uri[strlen(req->uri) - 1] == '/') {
		strlcat(filepath, "/index.html", sizeof(filepath));
	} else {
		strlcat(filepath, req->uri, sizeof(filepath));
	}
	int fd = open(filepath, O_RDONLY, 0);
	if (fd == -1) {
		ESP_LOGE(REST_TAG, "Failed to open file : %s", filepath);
		/* Respond with 500 Internal Server Error */
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
		return ESP_FAIL;
	}

	set_content_type_from_file(req, filepath);

	char *chunk = context->scratch;
	ssize_t read_bytes;
	do {
		/* Read file in chunks into the scratch buffer */
		read_bytes = read(fd, chunk, server_context::SCRATCH_BUFSIZE);
		if (read_bytes == -1) {
			ESP_LOGE(REST_TAG, "Failed to read file : %s", filepath);
		} else if (read_bytes > 0) {
			/* Send the buffer contents as HTTP response chunk */
			if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
				close(fd);
				ESP_LOGE(REST_TAG, "File sending failed!");
				/* Abort sending file */
				httpd_resp_sendstr_chunk(req, NULL);
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
				return ESP_FAIL;
			}
		}
	} while (read_bytes > 0);
	/* Close file after sending complete */
	close(fd);
	ESP_LOGI(REST_TAG, "File sending complete");
	/* Respond with an empty chunk to signal HTTP response completion */
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

} // namespace Core
