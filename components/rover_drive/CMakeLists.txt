
idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES cxx_utils nlohmann_json sys_console sys_core vesc websocket
	)

set_source_files_properties(
	app.cpp
	motion_control.cpp
	motion_control_monitor.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)

target_link_libraries(${COMPONENT_LIB} PUBLIC nlohmann_json::nlohmann_json)

set(JSON_BuildTests OFF CACHE INTERNAL "")
