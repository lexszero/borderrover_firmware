#include "body_control_message.hpp"
#include "util_misc.hpp"

static constexpr auto button_lookup_table = make_array<>(
		std::tuple(RemoteButton::Joystick, "Joystick"),
		std::tuple(RemoteButton::Left, "Left"),
		std::tuple(RemoteButton::Right, "Right"),
		std::tuple(RemoteButton::Up, "Up"),
		std::tuple(RemoteButton::Down, "Down"),
		std::tuple(RemoteButton::SwitchRed, "SwRed"),
		std::tuple(RemoteButton::SwitchBlue, "SwBlue")
		);


std::ostream& operator<<(std::ostream& os, const RemoteButton& btn)
{
	return os << lookup<>(button_lookup_table,
			btn,
			"UNKNOWN");
}

std::ostream& operator<<(std::ostream& os, const RemoteState& st)
{
	os << "Remote buttons: " << std::hex << std::setw(2) << st.btn_mask << " [ ";
	for (int btn_num = RemoteButton::_First; btn_num < RemoteButton::_Count; btn_num++) {
		if (st.btn_mask & (1 << btn_num)) {
			os << static_cast<RemoteButton>(btn_num) << " ";
		}
	}
	os << "], joystick: " << static_cast<int>(st.joy.x) << ", " << static_cast<int>(st.joy.y);
	return os;
}
