#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"

#include "cmd_system.h"
#include "cmd_nvs.h"

static void console_task(void *pvParameter) {
	const char *prompt = LOG_COLOR_I "esp32> " LOG_RESET_COLOR;

	while (true) {
		/* Get a line using linenoise.
		 * The line is returned when ENTER is pressed.
		 */
		char *line = linenoise(prompt);
		if (line == NULL) { /* Ignore empty lines */
			continue;
		}
		/* Add the command to the history */
		linenoiseHistoryAdd(line);

		/* Try to run the command */
		int ret;
		esp_err_t err = esp_console_run(line, &ret);
		if (err == ESP_ERR_NOT_FOUND) {
			printf("Unrecognized command\n");
		} else if (err == ESP_OK && ret != ESP_OK) {
			printf("Command returned non-zero error code: 0x%x\n", ret);
		} else if (err != ESP_OK) {
			printf("Internal error: %s\n", esp_err_to_name(err));
		}
		/* linenoise allocates line buffer on the heap, so need to free it */
		linenoiseFree(line);
	}
}

void __attribute__((weak)) sys_console_register_commands() {}

void sys_console_init() {
	/* Disable buffering on stdin */
	setvbuf(stdin, NULL, _IONBF, 0);

	/* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
	esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
	/* Move the caret to the beginning of the next line on '\n' */
	esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

	/* Install UART driver for interrupt-driven reads and writes */
	ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
				256, 0, 0, NULL, 0) );

	/* Tell VFS to use UART driver */
	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

	/* Initialize the console */
	esp_console_config_t console_config = {
		.max_cmdline_args = 32,
		.max_cmdline_length = 256,
		.hint_color = atoi(LOG_COLOR_CYAN)
	};
	ESP_ERROR_CHECK( esp_console_init(&console_config) );

	/* Configure linenoise line completion library */
	/* Enable multiline editing. If not set, long commands will scroll within
	 * single line.
	 */
	linenoiseSetMultiLine(1);

	/* Tell linenoise where to get command completions and hints */
	linenoiseSetCompletionCallback(&esp_console_get_completion);
	linenoiseSetHintsCallback((linenoiseHintsCallback *) &esp_console_get_hint);

	/* Set command history size */
	linenoiseHistorySetMaxLen(100);

	/* Register commands */
	esp_console_register_help_command();
	cmd_system_register();
	cmd_nvs_register();
	sys_console_register_commands();

	xTaskCreate(&console_task, "console_task", 16384, NULL, 5, NULL);
}
