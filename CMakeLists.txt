cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(BorderRover)

add_custom_target(ota
	curl $ENV{ESP32_IP}:8032 --data-binary @- < ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
	DEPENDS app
	)
