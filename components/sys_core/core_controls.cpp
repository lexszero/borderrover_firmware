#include "core_controls.hpp"
#include "core_http.hpp"
#include "core_messages.hpp"

#include "cxx_espnow.hpp"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

namespace Core {

using esp_now::espnow;
using namespace std::string_literals;

static const char *TAG = "controls";

std::unique_ptr<Controls> controls;

static struct {
	struct arg_str *action;
	struct arg_str *ctl;
	struct arg_str *value;
	struct arg_end *end;
} cmd_ctl_args;

int handle_cmd_ctl(int argc, char **argv)
{
	int ret = arg_parse(argc, argv, (void **)&cmd_ctl_args);
	if (ret) {
		arg_print_errors(stderr, cmd_ctl_args.end, argv[0]);
		return 1;
	}

	if (!cmd_ctl_args.action->count) {
		ESP_LOGE(TAG, "Action missing");
		return 1;
	}

	const char *action = cmd_ctl_args.action->sval[0];
	try {
		if (strcmp(action, "show") == 0) {
			controls->show();
		}
		else if (strcmp(action, "set") == 0) {
			if (!cmd_ctl_args.ctl->count) {
				ESP_LOGE(TAG, "Control name missing");
				return 1;
			}
			if (!cmd_ctl_args.value->count) {
				ESP_LOGE(TAG, "Value missing");
				return 1;
			}
			controls->set(cmd_ctl_args.ctl->sval[0], cmd_ctl_args.value->sval[0]);
		}
		else {
			ESP_LOGD(TAG, "Invalid action");
		}
	}
	catch (const std::exception& e) {
		ESP_LOGE(TAG, "%s", e.what());
		return 1;
	}
	return 0;
}

Controls::Controls()
{
	register_console_cmd();
	http->on(std::string(URL_PREFIX) + "*", HTTP_GET, [this](httpd_req_t *req) {
		return http_get_handler(req);
	});
	http->on(std::string(URL_PREFIX) + "*", HTTP_POST, [this](httpd_req_t *req) {
		return http_post_handler(req);
	});
	/*
	espnow->on_recv(static_cast<esp_now::MessageType>(MessageId::CtlGet), [this](Message&& _msg) {
			try {
				auto req_msg = MessageGet(_msg);
				auto req = req_msg.content();

				auto& ctl = *controls.at(req.key);

				CtlKeyValue resp;
				strncpy(resp.key, req.key, KEY_LENGTH);
				strncpy(resp.value, ctl.to_string().c_str(), VALUE_LENGTH);
				espnow->send(MessageUpdate(req_msg.peer(), resp));
			}
			catch (const std::exception& e) {
				ESP_LOGE(TAG, "MessageGet request failed: %s", e.what());
			}
		});
	*/
}

void Controls::register_console_cmd() {
	cmd_ctl_args.action = arg_str0(NULL, NULL, "<show|set>", "Action");
	cmd_ctl_args.ctl = arg_str0(NULL, NULL, "<ctl>", "Control name");
	cmd_ctl_args.value = arg_str0(NULL, NULL, "<value>", "Value to set");
	cmd_ctl_args.end = arg_end(1);
	static const esp_console_cmd_t cmd_ctl = {
		.command = "ctl",
		.help = "Controls",
		.hint = NULL,
		.func = &handle_cmd_ctl,
		.argtable = &cmd_ctl_args,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_ctl));
}

void Controls::add(const std::string& name, std::shared_ptr<AbstractControl> control)
{
	controls[name] = control;
}

void Controls::show()
{
	for (const auto& item : controls) {
		const auto& [name, ctl] = item;
		std::cout << std::setfill(' ') << std::setw(4) << ctl->order
			<< std::setfill(' ') << std::setw(15) << name << " : "
			<< ctl->show() << std::endl;
	}
	std::cout << std::endl;

}

void Controls::set(const char *name, const char *value)
{
	controls.at(name)->from_string(value);
}

esp_err_t Controls::http_get_handler(httpd_req_t *req)
{
	auto path = req->uri + strlen(URL_PREFIX);
	try {
		if (strncmp(path, "_all", 4) == 0) {
			json j;
			for (const auto& item : controls) {
				item.second->append_json(j);
			}
			return httpd_resp_json(req, j);
		}
		else {
			auto& ctl = *controls.at(path);
			ESP_LOGD(REST_TAG, "Found control %s", ctl.name);
	
			auto ret = httpd_resp_set_type(req, "text/plain");
			if (ret != ESP_OK) {
				ESP_LOGE(REST_TAG, "Failed to set response type");
				return ret;
			}
			return httpd_resp_sendstr(req, ctl.to_string().c_str());
		}
	}
	catch (const std::exception& e)
	{
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, e.what());
	}

	return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Shit happened");
}

esp_err_t Controls::http_post_handler(httpd_req_t *req)
{
	auto path = req->uri + strlen(URL_PREFIX);
	try {
		auto& ctl = *controls.at(path);
		ESP_LOGD(REST_TAG, "Found control %s", ctl.name);

		if (ctl.readonly)
			return httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "Readonly control");

		char buf[16];
		auto ret = httpd_req_recv(req, buf, sizeof(buf)-1);
		if (ret <= 0)
			return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
		buf[ret] = 0;

		ctl.from_string(buf);
		return httpd_resp_sendstr(req, ctl.to_string().c_str());
	}
	catch (const std::exception& e)
	{
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, e.what());
	}

	return httpd_resp_send_err(req, HTTPD_501_METHOD_NOT_IMPLEMENTED, "Not implemented");
}

AbstractControl& Controls::from_request(httpd_req_t *req)
{
	auto path = req->uri + strlen(URL_PREFIX);
	ESP_LOGD(TAG, "path=%s", path);
	return *controls[path];
}

}
