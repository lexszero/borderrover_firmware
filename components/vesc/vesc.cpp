//#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <functional>
#include <string.h>
#include "vesc.hpp"
#include "datatypes.h"
#include "buffer.h"

#define TAG (this->interface.name)

Vesc::Vesc(VescInterface& _interface) : interface(_interface) {
	ESP_LOGI(TAG, "initializing");
	interface.onPacketCallback(std::move(std::bind(&Vesc::processReadPacket, this, std::placeholders::_1)));
};

void Vesc::printValues() {
	ESP_LOGI(TAG, "Im %f, duty %f, rpm %ld, U %f, ∑ %ld",
			data.avgMotorCurrent,
			data.dutyCycleNow,
			data.rpm,
			data.inpVoltage,
			data.tachometerAbs);
}

bool Vesc::processReadPacket(uint8_t * message) {
	ESP_LOGD(TAG, "processReadPacket, this = %p, this->interface = %p", this, &this->interface);

	COMM_PACKET_ID packetId;
	int32_t ind = 0;

	packetId = (COMM_PACKET_ID)message[0];
	message++; // Removes the packetId from the actual message (payload)

	switch (packetId){
		case COMM_GET_VALUES: // Structure defined here: https://github.com/vedderb/bldc/blob/43c3bbaf91f5052a35b75c2ff17b5fe99fad94d1/commands.c#L164
			ind = 4; // Skip the first 4 bytes 
			data.avgMotorCurrent 	= buffer_get_float32(message, 100.0, &ind);
			data.avgInputCurrent 	= buffer_get_float32(message, 100.0, &ind);
			ind += 8; // Skip the next 8 bytes
			data.dutyCycleNow 		= buffer_get_float16(message, 1000.0, &ind);
			data.rpm 				= buffer_get_int32(message, &ind);
			data.inpVoltage 		= buffer_get_float16(message, 10.0, &ind);
			data.ampHours 			= buffer_get_float32(message, 10000.0, &ind);
			data.ampHoursCharged 	= buffer_get_float32(message, 10000.0, &ind);
			ind += 8; // Skip the next 8 bytes 
			data.tachometer 		= buffer_get_int32(message, &ind);
			data.tachometerAbs 		= buffer_get_int32(message, &ind);

			data.timestamp = xTaskGetTickCount();
			if (cb_values) {
				cb_values(*this);
			}
			return true;
			break;

		default:
			ESP_LOGW(TAG, "received unexpected packet type: 0x%x", packetId);
			return false;
			break;
	}
}

void Vesc::getValues(void) {
	uint8_t command[1] = { COMM_GET_VALUES };

	interface.rxStart();
	interface.sendPacket(command, 1);
}

void Vesc::onValues(CallbackFn&& cb) {
	ESP_LOGD(TAG, "set onValues cb, this = %p", this);
	cb_values = cb;
}

void Vesc::sendMsg(uint8_t *msg, uint16_t len) {
	interface.sendPacket(msg, len);
	memcpy(&last_message, msg, len);
	last_message_len = len;
}

void Vesc::setCurrent(float current) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_CURRENT;
	buffer_append_int32(payload, (int32_t)(current * 1000), &index);

	interface.sendPacket(payload, 5);
}

void Vesc::setBrakeCurrent(float brakeCurrent) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_CURRENT_BRAKE;
	buffer_append_int32(payload, (int32_t)(brakeCurrent * 1000), &index);

	interface.sendPacket(payload, 5);
}

void Vesc::setRPM(float rpm) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_RPM ;
	buffer_append_int32(payload, (int32_t)(rpm), &index);

	interface.sendPacket(payload, 5);
}

void Vesc::setDuty(float duty) {
	int32_t index = 0;
	uint8_t payload[5];

	payload[index++] = COMM_SET_DUTY;
	buffer_append_int32(payload, (int32_t)(duty * 100000), &index);

	interface.sendPacket(payload, 5);
}

void Vesc::kill() {
	setCurrent(0);
}

void Vesc::brake() {
	setRPM(0);
}
