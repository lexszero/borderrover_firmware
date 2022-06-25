cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS
	$ENV{IDF_PATH}/examples/cxx/experimental/experimental_cpp_component
	../../components
	)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(RoverBody)

add_custom_target(ota
	curl $ENV{ESP32_IP}:8032 --data-binary @- < ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
	DEPENDS app
	)
