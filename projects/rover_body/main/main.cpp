#include "sys_core.hpp"
#include "body_control.hpp"

#include <memory>

std::unique_ptr<BodyControl> bc;

extern "C" void app_main() {
	core_init();
	bc = std::make_unique<BodyControl>();
}
