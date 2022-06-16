idf_component_register(
	SRCS leds.cpp
        "NeoPixelBus/src/internal/HsbColor.cpp"
        "NeoPixelBus/src/internal/HslColor.cpp"
        "NeoPixelBus/src/internal/HtmlColor.cpp"
        "NeoPixelBus/src/internal/HtmlColorNames.cpp"
        "NeoPixelBus/src/internal/HtmlColorNameStrings.cpp"
        "NeoPixelBus/src/internal/HtmlColorShortNames.cpp"
        "NeoPixelBus/src/internal/NeoEsp32RmtMethod.cpp"
        "NeoPixelBus/src/internal/NeoGamma.cpp"
        "NeoPixelBus/src/internal/NeoPixelAnimator.cpp"
        "NeoPixelBus/src/internal/Rgb48Color.cpp"
        "NeoPixelBus/src/internal/RgbColor.cpp"
        "NeoPixelBus/src/internal/RgbColorBase.cpp"
        "NeoPixelBus/src/internal/RgbwColor.cpp"
        "NeoPixelBus/src/internal/SegmentDigit.cpp"
		"WS2812FX/src/WS2812FX.cpp"
	INCLUDE_DIRS . WS2812FX/src
	REQUIRES cxx_utils sys_core
	)

target_compile_definitions(${COMPONENT_TARGET} PRIVATE 
    "-DARDUINO_ARCH_ESP32"
	)

set_source_files_properties(
	leds.cpp
	PROPERTIES COMPILE_FLAGS -std=gnu++17
	)
