idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES 
		pthread spi_flash nvs_flash
		sys_console wifi ota_server vesc r1
	)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(nlohmann_json)
target_link_libraries(${COMPONENT_LIB} PUBLIC nlohmann_json::nlohmann_json)
