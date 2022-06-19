#pragma once

#include <string>
#include "esp_local_ctrl.h"
#include "nlohmann/json.hpp"

#include "core_config.hpp"
#include "core_http.hpp"

namespace Core {

using nlohmann::json;

class AbstractControl {
public:
	AbstractControl(const std::string& _name, const char *_desc = nullptr, bool _readonly = false, int _order = 0);

	virtual std::string to_string() = 0;
	virtual void from_string(const std::string& newval) = 0;
	virtual std::string show() = 0;

	/*
	virtual void to_mqtt(char *buf);
	virtual void from_mqtt(const char *newval);
	*/
	virtual void append_json(json& j) = 0;

	const char *name;
	const char *description;
	const bool readonly;
	const int order;
protected:
	esp_local_ctrl_prop_t local_ctrl;
	std::string mqttPath;

	/*
	uint16_t mqttPublish();
	uint16_t mqttMetaAttr(const char *meta, const char *value);
	uint16_t mqttMetaAttr(const char *meta, const String& value);
	void mqttMetaDefault(const char *type);

	virtual void mqttMeta() = 0;
	*/
};

} // namespace
