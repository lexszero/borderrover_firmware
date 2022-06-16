#include "core_controls.hpp"
#include "core_http.hpp"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

namespace Core {

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
	return 0;
}

Controls::Controls()
{
	register_console_cmd();
	http->on("/api/v1/control/_all", HTTP_GET, [this](httpd_req_t *req) {
		json j;
		for (const auto& item : controls) {
			item.second->append_json(j);
		}
		return httpd_resp_json(req, j);
	});
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
		std::cout << std::setfill(' ') << std::setw(10) << name << " : "
			<< ctl->to_string() << std::endl;
	}
	std::cout << std::endl;

}

void Controls::set(const char *name, const char *value)
{
	controls[name]->from_string(value);
}

}
