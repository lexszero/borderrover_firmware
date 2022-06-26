#pragma once

#include "cxx_espnow_message.hpp"

using esp_now::MessageType;
using esp_now::GenericMessage;

enum MessageId : MessageType {
	RoverRemoteState = 0x90,
	RoverBodyState = 0xA0,
};

enum RemoteButton : uint32_t
{
	_First		= 0,
	Joystick	= 0,
	Left		= 1,
	Right		= 2,
	Up			= 3,
	Down		= 4,
	SwitchRed	= 5,
	SwitchBlue	= 6,
	_Count
};

std::ostream& operator<<(std::ostream& os, const RemoteButton& btn);

struct RemoteJoystickState
{
	int8_t x;
	int8_t y;
} __attribute__((__packed__));

struct RemoteState
{
	uint32_t btn_mask;
	RemoteJoystickState joy;

	bool is_pressed(RemoteButton button_id) const
	{
		return btn_mask & (1 << to_underlying(button_id));
	}
} __attribute__((__packed__));

std::ostream& operator<<(std::ostream& os, const RemoteState& st);

using MessageRoverRemoteState = GenericMessage<RoverRemoteState, RemoteState>;

enum OutputId
{
	Valve0,
	Valve1,
	Valve2,
	Pump,
	Igniter,
	Aux0,
	Aux1,
	Lockout,
	_TotalCount,
	_PulsedCount = 7
};

OutputId from_string(const std::string& s);

struct BodyPackedState
{
	bool lockout;
	uint32_t outputs;

	bool is_on(OutputId id) const
	{
		return outputs & (1 << to_underlying(id));
	}
} __attribute__((__packed__));

using MessageRoverBodyState = GenericMessage<RoverBodyState, BodyPackedState>;

