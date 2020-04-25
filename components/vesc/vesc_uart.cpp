//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/uart.h"

#include "vesc.hpp"
#include "crc.h"

#define TAG (this->name)

static const auto RX_BUF_SIZE = 512;

VescUartInterface::VescUartInterface(const char *name, uart_port_t _uart_port) :
	VescInterface(name),
	Task::Task(name, 16*1024, 3),
	uart_port(_uart_port)
{
	this->uart_port = uart_port;

	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE
	};
	
	ESP_LOGI(TAG, "Initializing UART %d", uart_port);
	uart_driver_install(uart_port, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 20, &uart_queue, 0);
	uart_param_config(uart_port, &uart_config);

	rxStop();
	Task::start();
}

void VescUartInterface::run() {
	uart_event_t event;
	while (1) {
		if (!xQueueReceive(uart_queue, (void *)&event, (portTickType)portMAX_DELAY))
			continue;
		
		switch (event.type) {
			case UART_DATA:
				receive(event.size);
				break;

			case UART_FIFO_OVF:
				ESP_LOGW(TAG, "uart fifo overflow");
				rxStart();
				break;

			case UART_BUFFER_FULL:
				ESP_LOGI(TAG, "ring buffer full");
				rxStart();
				break;

			case UART_BREAK:
				ESP_LOGI(TAG, "uart rx break");
				break;

			case UART_PARITY_ERR:
				ESP_LOGI(TAG, "uart parity error");
				break;

			case UART_FRAME_ERR:
				ESP_LOGI(TAG, "uart frame error");
				break;

			default:
				ESP_LOGI(TAG, "uart event type: %d", event.type);
				break;
		}
	}
}

void VescUartInterface::rxStart() {
	ESP_LOGD(TAG, "rx start");
	rx_state.receiving = true;
	rx_state.len = 0;
	rx_state.end_message = 256;
	rx_state.payload = NULL;
	rx_state.payload_len = 0;
	rx_state.deadline = xTaskGetTickCount() + 100 * portTICK_PERIOD_MS; // Defining the timestamp for timeout (100ms before timeout)

	uart_flush(uart_port);
	xQueueReset(uart_queue);
}

void VescUartInterface::rxStop() {
	ESP_LOGD(TAG, "rx stop");
	rx_state.receiving = false;
}

void VescUartInterface::receive(int len) {
	// Messages <= 255 starts with "2", 2nd byte is length
	// Messages > 255 starts with "3" 2nd and 3rd byte is length combined with 1st >>8 and then &0xFF

	int length = uart_read_bytes(uart_port, rx_state.buf + rx_state.len, len, 100 / portTICK_PERIOD_MS);
	if (length < 0) {
		ESP_LOGE(TAG, "RX: uart_read_bytes failed");
		return;
	}
	rx_state.len += length;
	ESP_LOGD(TAG, "rx %d bytes", length);

	if (rx_state.len >= 2) {
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
				ESP_LOGE(TAG, "RX: Invalid start byte 0x%x", rx_state.buf[0]);
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
			//ESP_LOGD(name, "RX: ");
			//ESP_LOG_BUFFER_HEXDUMP(name, rx_state.payload, rx_state.payload_len, ESP_LOG_DEBUG);
			if (rx_callback) {
				rx_callback(rx_state.payload);
				//rx_callback = NULL;
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
/*
	ESP_LOGD(TAG, "TX: ");
	ESP_LOG_BUFFER_HEXDUMP(name, buf, count, ESP_LOG_DEBUG);
*/

	// Sending package
	int ret = uart_write_bytes(uart_port, (const char *)buf, count);
	if (ret < 0) {
		ESP_LOGE(TAG, "TX: uart_write_bytes failed");
		return 0;
	}

	// Returns number of send bytes
	return ret;
}

void VescUartInterface::onPacketCallback(VescInterface::ReceivePacketCb&& cb) {
	rx_callback = std::move(cb);
}
