//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "hover_drive.hpp"

#include "esp_log.h"

#define TAG "hover"

static constexpr auto RX_BUF_SIZE = 512;

HoverDrive::HoverDrive(uart_port_t uart_port) :
	Task(TAG, 4*1024, 20),
	uart_port(uart_port),
	uart_queue()
{
	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 0,
		.source_clk = UART_SCLK_APB,
	};
	
	ESP_LOGI(TAG, "Initializing UART %d", uart_port);
	uart_driver_install(uart_port, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 20, &uart_queue, 0);
	uart_param_config(uart_port, &uart_config);

	Task::start();
}

void HoverDrive::go(int16_t speed, int16_t steer)
{
	send(speed, steer);
}

void HoverDrive::run()
{
	uart_event_t event;
	while (1) {
		if (!xQueueReceive(uart_queue, (void *)&event, (TickType_t)portMAX_DELAY))
			continue;
		
		switch (event.type) {
			case UART_DATA:
				receive(event.size);
				break;

			case UART_FIFO_OVF:
				ESP_LOGW(TAG, "uart fifo overflow");
				break;

			case UART_BUFFER_FULL:
				ESP_LOGI(TAG, "ring buffer full");
				reset();
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

void HoverDrive::reset() {
	ESP_LOGD(TAG, "reset");
	uart_flush(uart_port);
	xQueueReset(uart_queue);
}

void HoverDrive::receive(size_t len) {
	uint8_t buf[16];
	int length = uart_read_bytes(uart_port, buf, std::min(sizeof(buf), len), 100 / portTICK_PERIOD_MS);
	if (length < 0) {
		ESP_LOGE(TAG, "RX: uart_read_bytes failed");
		return;
	}
	ESP_LOGD(TAG, "rx %d bytes", length);
}

void HoverDrive::send(int16_t speed, int16_t steer)
{
	HoverControlMsg msg = {
		.magic = 0xABCD,
		.steer = steer,
		.speed = speed,
		.checksum = 0
	};
	msg.checksum = msg.magic ^ msg.steer ^ msg.speed;
	const uint8_t *msg_bytes = reinterpret_cast<uint8_t *>(&msg);
	/*for (int i = 0; i < sizeof(msg)-2; i++) {
		msg.checksum ^= msg_bytes[i];
	}*/

    const int sent_bytes = uart_write_bytes(uart_port, msg_bytes, sizeof(HoverControlMsg));
	ESP_LOGD(TAG, "send command, %d bytes", sent_bytes);
}
