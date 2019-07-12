#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <string.h>
#include "vesc.hpp"
#include "crc.h"

struct buffer {
	uint8_t data;
	size_t len;
};
#define TAG (this->name)

extern "C" void vesc_uart_task(void *arg)
{
	VescUartInterface *vesc = (VescUartInterface *)arg;
	vesc->taskFunc();
}

extern "C" void vesc_timer_callback(void *arg)
{
	VescUartInterface *vesc = (VescUartInterface *)arg;
	vesc->timerCb();
}

VescUartInterface::VescUartInterface(const char *name, uart_port_t uart_port) :
	VescInterface(name) {
	this->uart_port = uart_port;

	esp_timer_create_args_t vesc_timer_args = {};
	vesc_timer_args.callback = &vesc_timer_callback;
	vesc_timer_args.name = name;

	ESP_ERROR_CHECK(esp_timer_create(&vesc_timer_args, &timer));

	rxStop();

	xTaskCreate(vesc_uart_task, name, 8192, this, 5, &task);
}

void VescUartInterface::taskFunc() {
/*
	char uart_path[16];
	int uart_fd;
	
	snprintf(uart_path, sizeof(uart_path), "/dev/uart/%d", uart_port);
	if ((uart_fd = open(uart_path, O_RDWR | O_NONBLOCK)) == -1) {
		ESP_LOGE(TAG, "Cannot open UART");
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}

	esp_vfs_dev_uart_use_driver(uart_port);*/

	while (1) {
		doRx();
		if (!rx_state.receiving) {
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}
}

void VescUartInterface::timerCb() {
	ESP_LOGI(TAG, "timer callback");
}

void VescUartInterface::rxStart() {
	rx_state.receiving = true;
	rx_state.len = 0;
	rx_state.end_message = 256;
	rx_state.payload = NULL;
	rx_state.payload_len = 0;
	rx_state.deadline = xTaskGetTickCount() + 100 * portTICK_PERIOD_MS; // Defining the timestamp for timeout (100ms before timeout)
}

void VescUartInterface::rxStop() {
	rx_state.receiving = false;
}

void VescUartInterface::doRx() {
	esp_err_t err;
	int length;

	err = uart_get_buffered_data_len(uart_port, (size_t *)&length);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "RX: uart_get_buffered_data_len failed: %s", esp_err_to_name(err));
		vTaskDelay(1000 / portTICK_PERIOD_MS);
		return;
	}

	if (!rx_state.receiving) {
		uart_flush(uart_port);
		return;
	}
	
	if (xTaskGetTickCount() > rx_state.deadline) {
		ESP_LOGW(TAG, "RX: timeout");
		rxStop();
		return;
	}

	if (length == 0)
		return;

	// Messages <= 255 starts with "2", 2nd byte is length
	// Messages > 255 starts with "3" 2nd and 3rd byte is length combined with 1st >>8 and then &0xFF

	length = uart_read_bytes(uart_port, rx_state.buf + rx_state.len, length, 100 / portTICK_PERIOD_MS);
	if (length < 0) {
		ESP_LOGE(TAG, "RX: uart_read_bytes failed");
		return;
	}
	rx_state.len += length;

	if (rx_state.len == 2) {
		switch (rx_state.buf[0]) {
			case 2:
				rx_state.end_message = rx_state.buf[1] + 5; //Payload size + 2 for sice + 3 for SRC and End.
				break;

			case 3:
				// ToDo: Add Message Handling > 255 (starting with 3)
				ESP_LOGW(TAG, "RX: Message is larger than 256 bytes - not supported");
				rxStop();
				break;

			default:
				ESP_LOGE(TAG, "RX: Invalid start bit");
				rxStop();
				break;
		}
	}

	if (rx_state.len >= sizeof(rx_state.buf)) {
		ESP_LOGW(TAG, "RX: buffer overflow");
		rxStop();
		return;
	}

	if (rx_state.len == rx_state.end_message && rx_state.buf[rx_state.end_message - 1] == 3) {
		rx_state.buf[rx_state.end_message] = 0;
		ESP_LOGD(TAG, "RX: End of message reached!");

		/* Unpack payload */
		int len_msg = rx_state.end_message;

		// Rebuild crc:
		uint16_t crc_msg = (rx_state.buf[len_msg - 3] << 8) | 
			(rx_state.buf[len_msg - 2]);
		
		uint16_t crc_calc = crc16(rx_state.buf + 2, rx_state.buf[1]);

		ESP_LOGD(TAG, "RX: CRC rcvd %04x, calc %04x", crc_msg, crc_calc);

		if (crc_msg == crc_calc) {
			rx_state.payload = rx_state.buf + 2;
			rx_state.payload_len = rx_state.buf[1];
			ESP_LOGD(name, "RX: ");
			ESP_LOG_BUFFER_HEXDUMP(name, rx_state.payload, rx_state.payload_len, ESP_LOG_DEBUG);
			if (rx_callback) {
				rx_callback(rx_state.payload);
				rx_callback = NULL;
			}
		}
		else {
			ESP_LOGW(TAG, "RX: CRC check failed: rcvd %04x != calc %04x", crc_msg, crc_calc);
		}
	}
}

int VescUartInterface::sendPacket(uint8_t * payload, int payload_len) {
	uint16_t crc = crc16(payload, payload_len);
	int count = 0;
	uint8_t buf[256];

	if (payload_len <= 256) {
		buf[count++] = 2;
		buf[count++] = payload_len;
	}
	else {
		buf[count++] = 3;
		buf[count++] = (uint8_t)(payload_len >> 8);
		buf[count++] = (uint8_t)(payload_len & 0xFF);
	}

	memcpy(&buf[count], payload, payload_len);

	count += payload_len;
	buf[count++] = (uint8_t)(crc >> 8);
	buf[count++] = (uint8_t)(crc & 0xFF);
	buf[count++] = 3;
	buf[count] = '\0';

	ESP_LOGD(TAG, "TX: ");
	ESP_LOG_BUFFER_HEXDUMP(name, buf, count, ESP_LOG_DEBUG);

	// Sending package
	int ret = uart_write_bytes(uart_port, (const char *)buf, count);
	if (ret < 0) {
		ESP_LOGE(TAG, "TX: uart_write_bytes failed");
		return 0;
	}

	// Returns number of send bytes
	return ret;
}

void VescUartInterface::onPacketCallback(VescInterface::ReceivePacketCb cb) {
	rx_callback = std::move(cb);
	rxStart();
}
