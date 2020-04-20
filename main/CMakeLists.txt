idf_component_register(
	SRC_DIRS "."
	INCLUDE_DIRS "."
	REQUIRES 
		pthread spi_flash nvs_flash
		sys_console wifi ota_server vesc r1
	)

