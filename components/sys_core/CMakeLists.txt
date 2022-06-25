idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES
		esp_http_server
		esp_local_ctrl
		spiffs nvs_flash
		experimental_cpp_component
		cxx_utils
		cxx_espnow
		nlohmann_json
		ota_server
		sys_console
		wifi
	)

set_source_files_properties(
	AbstractControl.cpp
	sys_core.cpp
	core_controls.cpp
	core_http.cpp
	core_status_led.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
