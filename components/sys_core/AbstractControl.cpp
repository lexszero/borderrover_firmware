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

	http->on("/api/v1/control/"s + name, HTTP_GET, [this](httpd_req_t *req) {
		auto ret = httpd_resp_set_type(req, "text/plain");
		if (ret != ESP_OK) {
			ESP_LOGE(REST_TAG, "Failed to set response type");
			return ret;
		}
		return httpd_resp_sendstr(req, this->to_string().c_str());
	});

	if (!readonly) {
		http->on("/api/v1/control/"s + name, HTTP_POST, [this](httpd_req_t *req) {
			char buf[16];
			auto ret = httpd_req_recv(req, buf, sizeof(buf)-1);
			if (ret <= 0)
				return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad request");
			buf[ret] = 0;

			this->from_string(buf);
			return httpd_resp_sendstr(req, this->to_string().c_str());
		});
	}
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
