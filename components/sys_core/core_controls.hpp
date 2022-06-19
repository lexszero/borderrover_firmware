#pragma once

#include <functional>
#include <memory>
#include <map>
#include "AbstractControl.hpp"

namespace Core {

class Controls {
public:
	Controls();

	void add(const std::string& name, std::shared_ptr<AbstractControl> control);
	void show();
	void set(const char *name, const char *value);

private:
	static constexpr char URL_PREFIX[] = "/api/v1/ctl/";
	std::map<std::string, std::shared_ptr<AbstractControl>> controls;

	void register_console_cmd();
	esp_err_t http_get_handler(httpd_req_t *req);
	esp_err_t http_post_handler(httpd_req_t *req);
	AbstractControl& from_request(httpd_req_t *req);
};

extern std::unique_ptr<Controls> controls;

}  // namespace
