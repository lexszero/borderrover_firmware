#include "cxx_espnow_message.hpp"

std::ostream& operator<<(std::ostream& os, const esp_now::MessageInterface& m)
{
	os << m.to_string();
	return os;
}
