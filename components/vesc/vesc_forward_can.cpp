#include "vesc.hpp"

VescForwardCANInterface::VescForwardCANInterface(const char *_name, VescInterface& _interface, uint8_t _id) :
		VescInterface(_name),
		interface(_interface),
		id(_id) {};
