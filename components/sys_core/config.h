#pragma once

/* Hardware setup */
#define LED_COLOR_ORDER GRB
#define LED_COUNT 32

	CONFIG_PARAM(uint16_t,	artnet_universe,	0,				"ArtNet Universe") \
	CONFIG_PARAM(uint16_t,	artnet_offset,		0,				"ArtNet start offset")


