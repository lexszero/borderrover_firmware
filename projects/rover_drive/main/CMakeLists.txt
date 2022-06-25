idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES 
		sys_core
		rover_drive
	)

set_source_files_properties(
	main.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
