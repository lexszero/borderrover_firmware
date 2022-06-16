idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES 
		sys_core
		rover_body
	)

set_source_files_properties(
	app.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
