#include "body_control.hpp"

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"

#define TAG "bc"

static struct {
	struct arg_str *out;
	struct arg_str *action;
	struct arg_int *length;
	struct arg_end *end;
} cmd_out_args;

int handle_cmd_out(int argc, char **argv)
{
	int ret = arg_parse(argc, argv, (void **)&cmd_out_args);
	if (ret) {
		arg_print_errors(stderr, cmd_out_args.end, argv[0]);
		return 1;
	}

	if (!cmd_out_args.out->count) {
		ESP_LOGE(TAG, "Output missing");
		return 1;
	}

	OutputId out;
	try {
		out = from_string(cmd_out_args.out->sval[0]);
	}
	catch (std::exception& e) {
		ESP_LOGE(TAG, "%s", e.what());
		return 1;
	}

	if (!cmd_out_args.action->count) {
		ESP_LOGE(TAG, "Action missing");
		return 1;
	}

	const char *action = cmd_out_args.action->sval[0];
	auto& body = BodyControl::instance();
	if (strcmp(action, "on") == 0) {
		body.set_output(out, true);
	}
	else if (strcmp(action, "off") == 0) {
		body.set_output(out, false);
	}
	else if (strcmp(action, "pulse") == 0) {
		if (!cmd_out_args.length->count) {
			ESP_LOGE(TAG, "Pulse length missing");
			return 1;
		}
		const auto length = cmd_out_args.length->ival[0];
		body.pulse_output(out, length);
	}
	return 0;
}

static struct {
	struct arg_str *action;
	struct arg_end *end;
} cmd_body_args;

int handle_cmd_body(int argc, char **argv)
{
	int ret = arg_parse(argc, argv, (void **)&cmd_body_args);
	if (ret) {
		arg_print_errors(stderr, cmd_body_args.end, argv[0]);
		return 1;
	}

	if (!cmd_body_args.action->count) {
		ESP_LOGE(TAG, "Action missing");
		return 1;
	}

	const char *action = cmd_body_args.action->sval[0];
	auto& body = BodyControl::instance();
	if (strcmp(action, "reset") == 0) {
		// TODO
	}
	else if (strcmp(action, "test") == 0) {
		body.test_outputs();
	}
	else if (strcmp(action, "show") == 0) {
		body.print_state();
	}
	else {
		ESP_LOGD(TAG, "Invalid action");
	}
	return 0;
}

void BodyControl::register_console_cmd() {
	cmd_out_args.out = arg_str0(NULL, NULL, "<output>", "Output to control");
	cmd_out_args.action = arg_str0(NULL, NULL, "<on|off|pulse>", "on/off/pulse");
	cmd_out_args.length = arg_int0("l", "length", "<length>", "Pulse length (ms)");
	cmd_out_args.end = arg_end(1);
	static const esp_console_cmd_t cmd_out = {
		.command = "out",
		.help = "Output control",
		.hint = NULL,
		.func = &handle_cmd_out,
		.argtable = &cmd_out_args,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_out));

	cmd_body_args.action = arg_str0(NULL, NULL, "<reset|test|show>", "Action to run");
	cmd_body_args.end = arg_end(1);
	static const esp_console_cmd_t cmd_body = {
		.command = "body",
		.help = "Body control",
		.hint = NULL,
		.func = &handle_cmd_body,
		.argtable = &cmd_body_args,
	};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd_body));
}
