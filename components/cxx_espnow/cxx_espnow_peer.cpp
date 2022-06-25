#include "cxx_espnow_peer.hpp"

#include <iomanip>

namespace esp_now {

PeerAddress::PeerAddress() :
	addr()
{}

PeerAddress::PeerAddress(const std::string& str)
{
	size_t sz = 0;
	for (auto i = 0; i < ESP_NOW_ETH_ALEN; i++) {
		addr[i] = static_cast<uint8_t>(std::stoi(str.substr(sz), &sz, 16));
		if (sz <= str.length())
			sz++;
	}
}

PeerAddress::PeerAddress(const uint8_t *_addr)
{
	for (auto i = 0; i < ESP_NOW_ETH_ALEN; i++) {
		addr[i] = _addr[i];
	}
}

const uint8_t * PeerAddress::bytes() const
{
	return addr.data();
}

bool PeerAddress::operator==(const PeerAddress& other) const
{
	return addr == other.addr;
}

} // namespace esp_now

std::ostream& operator<<(std::ostream& os, const esp_now::PeerAddress& addr)
{
	using namespace std;
	size_t i = 0;
	for (const auto& b : addr.addr) {
		os << setfill('0') << setw(2) << right << hex << static_cast<unsigned>(b);
		if (i < ESP_NOW_ETH_ALEN-1) {
			os << ":";
			i++;
		}
	}
	return os;
}
