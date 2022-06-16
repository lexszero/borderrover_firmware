idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES esp_http_server esp_local_ctrl spiffs nvs_flash
		cxx_utils nlohmann_json ota_server sys_console wifi
	)

set_source_files_properties(
	AbstractControl.cpp
	sys_core.cpp
	core_controls.cpp
	core_http.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
