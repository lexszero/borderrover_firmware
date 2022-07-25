#include "body_control_message.hpp"
#include "util_misc.hpp"

template <size_t N, typename ButtonEnum>
using button_name_table = lookup_table<N, ButtonEnum, const char *>;

static constexpr auto remote_button_lookup_table = make_array<>(
		std::tuple(RemoteButton::Joystick, "Joystick"),
		std::tuple(RemoteButton::Left, "Left"),
		std::tuple(RemoteButton::Right, "Right"),
		std::tuple(RemoteButton::Up, "Up"),
		std::tuple(RemoteButton::Down, "Down"),
		std::tuple(RemoteButton::SwitchRed, "SwRed"),
		std::tuple(RemoteButton::SwitchBlue, "SwBlue")
		);

template <typename ButtonEnum, typename ButtonNameTable>
static const char * button_name(ButtonNameTable table, ButtonEnum id)
{
	return std::get<1>(table[to_underlying(id)]);
}

std::ostream& operator<<(std::ostream& os, const RemoteButton& btn)
{
	return os << button_name(remote_button_lookup_table, btn);
//	return os << lookup<>(remote_button_lookup_table,
//			btn,
//			"UNKNOWN");
}

std::ostream& operator<<(std::ostream& os, const RemoteState& st)
{
	os << "Remote buttons: " << std::hex << std::setw(2) << st.btn_mask << " [ ";
	for (int btn_num = to_underlying(RemoteButton::_First); btn_num < to_underlying(RemoteButton::_Count); btn_num++) {
		if (st.btn_mask & (1 << btn_num)) {
			os << static_cast<RemoteButton>(btn_num) << " ";
		}
	}
	os << "], joystick: " << static_cast<int>(st.joy.x) << ", " << static_cast<int>(st.joy.y);
	return os;
}

static constexpr auto joypad_button_lookup_table = make_array<>(
		std::tuple(JoypadButton::L_Shift, "L_Shift"),
		std::tuple(JoypadButton::L_Joystick, "L_Joystick"),
		std::tuple(JoypadButton::L_Center, "L_Center"),
		std::tuple(JoypadButton::R_Shift, "R_Shift"),
		std::tuple(JoypadButton::R_Joystick, "R_Joystick"),
		std::tuple(JoypadButton::R_Center, "R_Center"),
		std::tuple(JoypadButton::L_Up, "L_Up"),
		std::tuple(JoypadButton::L_Down, "L_Down"),
		std::tuple(JoypadButton::L_Left, "L_Left"),
		std::tuple(JoypadButton::L_Right, "L_Right"),
		std::tuple(JoypadButton::R_Up, "R_Up"),
		std::tuple(JoypadButton::R_Down, "R_Down"),
		std::tuple(JoypadButton::R_Left, "R_Left"),
		std::tuple(JoypadButton::R_Right, "R_Right")
		);

std::ostream& operator<<(std::ostream& os, const JoypadButton& btn)
{
	return os << button_name(joypad_button_lookup_table, btn);
//	return os << lookup<>(remote_button_lookup_table,
//			btn,
//			"UNKNOWN");
}

std::ostream& operator<<(std::ostream& os, const JoypadState& st)
{
	os << "Joypad buttons: " << std::hex << std::setw(2) << st.btn_mask << std::dec << " [ ";
	for (int btn_num = to_underlying(JoypadButton::_First); btn_num < to_underlying(JoypadButton::_Count); btn_num++) {
		if (st.btn_mask & (1 << btn_num)) {
			os << static_cast<JoypadButton>(btn_num) << " ";
		}
	}
	os << "], joy_left: " << static_cast<int>(st.joy_left.x) << ", " << static_cast<int>(st.joy_left.y) <<
		", joy_right: " << static_cast<int>(st.joy_right.x) << ", " << static_cast<int>(st.joy_right.y);

	return os;
}
