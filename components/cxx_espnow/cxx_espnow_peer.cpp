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

PeerAddress::PeerAddress(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5) :
	addr{a0, a1, a2, a3, a4, a5}
{}

const uint8_t * PeerAddress::bytes() const
{
	return addr.data();
}

bool PeerAddress::operator==(const PeerAddress& other) const
{
	return addr == other.addr;
}

std::ostream& operator<<(std::ostream& os, const PeerAddress& addr)
{
	using namespace std;
	size_t i = 0;
	for (const auto& b : addr.addr) {
		os << setfill('0') << setw(2) << right << hex << b;
		if (i < ESP_NOW_ETH_ALEN-1) {
			os << ":";
			i++;
		}
	}
	return os;
}


}
