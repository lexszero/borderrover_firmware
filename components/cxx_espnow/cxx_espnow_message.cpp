#include "cxx_espnow_message.hpp"

namespace esp_now {

std::ostream& operator<<(std::ostream& os, const MessageInterface& m)
{
	os << m.to_string();
	return os;
}

}
