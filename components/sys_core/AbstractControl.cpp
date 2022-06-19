#include "AbstractControl.hpp"
#include "core_controls.hpp"
#include "core_http.hpp"

#include <cstddef>
#include "esp_log.h"

namespace Core {

static const size_t MQTT_BUF_SIZE = 16;

using namespace std::string_literals;

AbstractControl::AbstractControl(const std::string& _name, const char *_desc, bool _readonly, int _order) :
	name(strdup(_name.c_str())),
	description(_desc),
	readonly(_readonly),
	order(_order),
	mqttPath(std::string("/devices/") + config.hostname + std::string("/controls/") + std::string(_name.c_str()))
{
	controls->add(_name, std::shared_ptr<AbstractControl>(this));
}

/*
uint16_t AbstractControl::mqttPublish() {
	char buf[MQTT_BUF_SIZE];
	toMqtt(buf);
	debug("%s: publish value %s", name, buf);

	auto ret = MQTT::mqtt.publish(mqttPath.c_str(), buf, true);
	if (!ret) {
		log("%s: publish failed", mqttPath.c_str());
	}

	return 1;
}

uint16_t AbstractControl::mqttMetaAttr(const char *meta, const char *value) {
	debug("%s: publish meta %s = %s", name, meta, value ? value : "NULL");

	char topic[128];
	snprintf(topic, sizeof(topic), "%s/meta/%s", mqttPath.c_str(), meta);

	auto ret = MQTT::mqtt.publish(topic, (uint8_t *)value, (value ? strlen(value) : 0), true);
	if (!ret) {
		log("%s: publish failed", topic);
	}
	return 1;
}

uint16_t AbstractControl::mqttMetaAttr(const char *meta, const String& value) {
	return mqttMetaAttr(meta, value.c_str());
}

void AbstractControl::mqttMetaDefault(const char *type) {
	mqttMetaAttr("type", type);
	mqttMetaAttr("order", String(order));
	mqttMetaAttr("readonly", readonly ? "1" : NULL);
}
*/

} // namespace
