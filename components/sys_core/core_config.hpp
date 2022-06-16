#pragma once

namespace Core {

#define CONFIG_PARAMS \
	CONFIG_PARAM_STR(		hostname,	64,		"rover-body",	"Hostname") \
	CONFIG_PARAM_STR(		mqtt_host,	64,		"192.168.0.10",	"MQTT host") \
	CONFIG_PARAM_STR(		mqtt_port,	6,		"1883",			"MQTT port") \
	CONFIG_PARAM_STR(		mqtt_user,	32,		"",				"MQTT username") \
	CONFIG_PARAM_STR(		mqtt_pass,	32,		"",				"MQTT password") \
	CONFIG_PARAM_STR(		mqtt_name,	64,		"rover-body",	"MQTT device name") \

struct Config {
	Config();

#define CONFIG_PARAM_STR(name, len, defval, desc) char name[len] = defval;
#define CONFIG_PARAM(type, name, defval, desc) type name = defval;
	CONFIG_PARAMS
#undef CONFIG_PARAM_STR
#undef CONFIG_PARAM

	static void load();
	static void save();
};
extern Config config;

}  // namespace
