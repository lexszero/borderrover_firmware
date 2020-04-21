#pragma once

#include <functional>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_timer.h"

#include "packet.h"

class VescInterface {
public:
	using ReceivePacketCb = std::function<void(uint8_t *packet)>;

	VescInterface(const char *_name) : name(_name) {}
	const char *name;
	ReceivePacketCb rx_callback;
	virtual int sendPacket(uint8_t *packet, int len) = 0;
	virtual void onPacketCallback(ReceivePacketCb cb) = 0;
private:
};

class VescUartInterface : public VescInterface {
public:
	VescUartInterface(const char *name, uart_port_t uart_port);
	virtual int sendPacket(uint8_t *packet, int len);
	virtual void onPacketCallback(VescInterface::ReceivePacketCb cb);

	void taskFunc(void);
	void timerCb(void);

private: 
	TaskHandle_t task;

	uart_port_t uart_port;
	esp_timer_handle_t timer;

	struct {
		bool receiving;
		uint8_t buf[256];
		uint16_t len;
		uint32_t deadline;

		uint16_t end_message;
		uint8_t *payload;
		uint16_t payload_len;
		
	} rx_state;

	void doRx();
	void rxStart();
	void rxStop();
};

class VescForwardCANInterface : public VescInterface {
public:
	VescForwardCANInterface(const char *_name, VescInterface& _interface, uint8_t _id);
	virtual int sendPacket(uint8_t *packet, int len);
	virtual void onPacketCallback(VescInterface::ReceivePacketCb cb);

private:
	VescInterface& interface;
	uint8_t id;
};


class Vesc {
public:
	Vesc(VescInterface& _interface);

	struct vescData {
		float avgMotorCurrent;
		float avgInputCurrent;
		float dutyCycleNow;
		long rpm;
		float inpVoltage;
		float ampHours;
		float ampHoursCharged;
		long tachometer;
		long tachometerAbs;
	} data;

	void getValues(void);
	void printValues(void);

	void sendMsg(uint8_t *payload, uint16_t payload_len);
	void setCurrent(float current);
	void setBrakeCurrent(float brakeCurrent);
	void setRPM(float rpm);
	void setDuty(float duty);

	void kill();
	void brake();

private:
	VescInterface& interface;
	bool processReadPacket(uint8_t * message);

	uint8_t last_message[256];
	uint16_t last_message_len;
};
