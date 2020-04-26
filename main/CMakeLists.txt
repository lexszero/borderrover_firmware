idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES 
		spi_flash nvs_flash
		cxx_utils
		sys_console wifi ota_server vesc r1
	)

set_source_files_properties(
	app.cpp
	motion_control.cpp
	motion_control_monitor.cpp
	PROPERTIES COMPILE_FLAGS -std=c++17)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(nlohmann_json)
target_link_libraries(${COMPONENT_LIB} PUBLIC nlohmann_json::nlohmann_json)
