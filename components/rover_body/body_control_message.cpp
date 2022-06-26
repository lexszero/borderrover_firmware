#include "body_control_message.hpp"
#include "util_misc.hpp"

static constexpr auto button_lookup_table = make_array<>(
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
	os << "Remote buttons: [ ";
	for (int i = 0; i < RemoteButton::_Count; i++) {
		if (st.btn_mask & (1 << i)) {
			os << static_cast<RemoteButton>(i) << " ";
		}
	}
	os << "], joystick: " << st.joy.x << ", " << st.joy.y;
	return os;
}
