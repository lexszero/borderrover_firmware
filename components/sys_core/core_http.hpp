#pragma once

#include "nlohmann/json.hpp"
#include "esp_http_server.h"
#include "esp_vfs.h"
#include "util_misc.hpp"

#include <memory>

namespace Core {

extern const char *REST_TAG;

using nlohmann::json;

esp_err_t httpd_resp_json(httpd_req_t *req, const json& j);

using HTTPHandler = std::function<esp_err_t(httpd_req_t *)>;

class HTTPServer {
	public:
		HTTPServer(const char *base_path);

		void on(
				const std::string& uri,
				httpd_method_t method,
				HTTPHandler handler);

	private:
		httpd_handle_t server = NULL;
		httpd_config_t config = HTTPD_DEFAULT_CONFIG();

		struct server_context {
			static constexpr auto SCRATCH_BUFSIZE = 10240;

			char base_path[ESP_VFS_PATH_MAX + 1];
			char scratch[SCRATCH_BUFSIZE];
		};
		std::unique_ptr<server_context> srv_ctx;

		struct route {
			httpd_uri_t uri;
			HTTPHandler handler;
		};

		static esp_err_t route_handler(httpd_req_t *req);
		static esp_err_t common_get_handler(httpd_req_t *req);
};

extern std::unique_ptr<HTTPServer> http;

}
