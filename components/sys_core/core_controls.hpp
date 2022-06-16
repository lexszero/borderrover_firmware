#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include "AbstractControl.hpp"

namespace Core {

class Controls {
public:
	Controls();

	void add(const std::string& name, std::shared_ptr<AbstractControl> control);
	void show();
	void set(const char *name, const char *value);

private:
	void register_console_cmd();

	std::unordered_map<std::string, std::shared_ptr<AbstractControl>> controls;
};

extern std::unique_ptr<Controls> controls;

}  // namespace
