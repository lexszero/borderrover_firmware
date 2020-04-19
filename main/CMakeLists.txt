set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_SRCS
	"main.c"
	"app.cpp"
	"motion_control.cpp"
	"uart_tcp_server.c"
	"ui.c"
	)
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component()
