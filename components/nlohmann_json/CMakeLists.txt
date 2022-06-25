idf_component_register(
	INCLUDE_DIRS "json/include"
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(json)
#target_link_libraries(${COMPONENT_LIB} PUBLIC nlohmann_json::nlohmann_json)
