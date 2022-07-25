#pragma once

#include "cxx_espnow_message.hpp"

using esp_now::MessageType;
using esp_now::GenericMessage;

enum MessageId : MessageType {
	RoverRemoteState = 0x90,
	RoverJoypadState = 0x91,
	RoverBodyState = 0xA0,
};

struct JoystickState
{
	int16_t x;
	int16_t y;
} __attribute__((__packed__));

template <typename ButtonEnum>
struct ButtonState
{
	uint32_t btn_mask;

	bool is_pressed(ButtonEnum button_id) const
	{
		return btn_mask & (1 << to_underlying(button_id));
	}

	bool operator==(const ButtonState& other) {
		return btn_mask == other.btn_mask;
	}
} __attribute__((__packed__));

enum class RemoteButton : uint32_t
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

enum class JoypadButton : uint32_t
{
	_First		= 0,
	L_Shift		= 0,
	L_Joystick,
	L_Center,
	R_Shift,
	R_Joystick,
	R_Center,
	L_Up		= 3,
	L_Down		= 4,
	L_Left		= 5,
	L_Right		= 6,
	R_Up		= 10,
	R_Down		= 11,
	R_Left		= 12,
	R_Right		= 13,
	_Count,
	_CountGpio = 6
};

std::ostream& operator<<(std::ostream& os, const JoypadButton& btn);

struct RemoteState :
	public ButtonState<RemoteButton>
{
	using ButtonState::operator==;
	JoystickState joy;
} __attribute__((__packed__));
std::ostream& operator<<(std::ostream& os, const RemoteState& st);
using MessageRoverRemoteState = GenericMessage<MessageId::RoverRemoteState, RemoteState>;

struct JoypadState :
	public ButtonState<JoypadButton>
{
	using ButtonState::operator==;
	JoystickState joy_left;
	JoystickState joy_right;
} __attribute__((__packed__));

std::ostream& operator<<(std::ostream& os, const JoypadState& st);
using MessageRoverJoypadState = GenericMessage<RoverJoypadState, JoypadState>;

enum OutputId : uint32_t
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

