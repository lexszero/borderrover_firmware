set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/cxx/experimental/experimental_cpp_component)

idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES
		cxx_utils experimental_cpp_component
		nlohmann_json
		sys_console
		sys_core
		leds
		cxx_espnow
	)

set_source_files_properties(
	body_control.cpp
	body_control_console.cpp
	body_control_message.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
