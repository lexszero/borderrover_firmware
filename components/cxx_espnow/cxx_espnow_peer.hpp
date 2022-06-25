#pragma once

#include <esp_now.h>

#include <array>
#include <cstdint>
#include <ostream>
#include <string>

namespace esp_now {

	class PeerAddressHasher;

class PeerAddress
{
public:
	PeerAddress();
	PeerAddress(const std::string& str);
	PeerAddress(const uint8_t *addr);
	PeerAddress(uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5);

	const uint8_t * bytes() const;
	bool operator==(const PeerAddress& other) const;
	friend std::ostream& operator<<(std::ostream& os, const PeerAddress& addr);

private:
	std::array<uint8_t, ESP_NOW_ETH_ALEN> addr;
	friend PeerAddressHasher;
};

class PeerAddressHasher
{
public:
	std::size_t operator()(const PeerAddress& a) const {
		std::size_t h = 0;
		for (const auto& b : a.addr) {
			h ^= std::hash<uint8_t>{}(b)  + 0x9e3779b9 + (h << 6) + (h >> 2); 
		}
		return h;
	}
};
}
