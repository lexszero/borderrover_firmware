idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES cxx_utils nlohmann_json sys_console sys_core
	)

set_source_files_properties(
	body_control.cpp
	body_control_console.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
