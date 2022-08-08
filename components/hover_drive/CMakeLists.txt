idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES cxx_utils driver
	)

set_source_files_properties(
	hover_drive.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17)
