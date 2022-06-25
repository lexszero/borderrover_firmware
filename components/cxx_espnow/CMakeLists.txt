idf_component_register(
	SRC_DIRS .
	INCLUDE_DIRS .
	REQUIRES cxx_utils
	)

set_source_files_properties(
	cxx_espnow.cpp
	cxx_espnow_message.cpp
	cxx_espnow_peer.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17
	)
