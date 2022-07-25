idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES
		cxx_utils
		cxx_espnow
		leds
		rover_body
		sys_core
		esp_adc_cal
	)

set_source_files_properties(
	main.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
