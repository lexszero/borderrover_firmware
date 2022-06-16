#include "sys_core.hpp"
#include "body_control.hpp"

#include "app.hpp"

#include <memory>

std::unique_ptr<BodyControl> bc;

void app_init()
{
	core_init();
	bc = std::make_unique<BodyControl>();
}
