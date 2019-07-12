/*
	Copyright 2012-2014 Benjamin Vedder	benjamin@vedder.se

	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    */

/*
 * packet.c
 *
 *  Created on: 21 mar 2013
 *      Author: benjamin
 */

#include <string.h>
#include "packet.h"
#include "crc.h"

void packet_init(bldc_packet_state_t *state, void (*s_func)(unsigned char *data, unsigned int len, void *arg),
		void (*p_func)(unsigned char *data, unsigned int len, void *arg), void *arg) {
	state->send_func = s_func;
	state->process_func = p_func;
	state->callback_arg = arg;
}

void packet_send_packet(bldc_packet_state_t *state, unsigned char *data, unsigned int len) {
	if (len > PACKET_MAX_PL_LEN) {
		return;
	}

	int b_ind = 0;

	if (len <= 256) {
		state->tx_buffer[b_ind++] = 2;
		state->tx_buffer[b_ind++] = len;
	} else {
		state->tx_buffer[b_ind++] = 3;
		state->tx_buffer[b_ind++] = len >> 8;
		state->tx_buffer[b_ind++] = len & 0xFF;
	}

	memcpy(state->tx_buffer + b_ind, data, len);
	b_ind += len;

	unsigned short crc = crc16(data, len);
	state->tx_buffer[b_ind++] = (uint8_t)(crc >> 8);
	state->tx_buffer[b_ind++] = (uint8_t)(crc & 0xFF);
	state->tx_buffer[b_ind++] = 3;

	if (state->send_func) {
		state->send_func(state->tx_buffer, b_ind, state->callback_arg);
	}
}

/**
 * Call this function every millisecond.
 */
void packet_timerfunc(bldc_packet_state_t *state) {
	if (state->rx_timeout) {
		state->rx_timeout--;
	} else {
		state->rx_state = 0;
	}
}

void packet_process_byte(bldc_packet_state_t *state, uint8_t rx_data) {
	switch (state->rx_state) {
	case 0:
		if (rx_data == 2) {
			// 1 byte PL len
			state->rx_state += 2;
			state->rx_timeout = PACKET_RX_TIMEOUT;
			state->rx_data_ptr = 0;
			state->payload_length = 0;
		} else if (rx_data == 3) {
			// 2 byte PL len
			state->rx_state++;
			state->rx_timeout = PACKET_RX_TIMEOUT;
			state->rx_data_ptr = 0;
			state->payload_length = 0;
		} else {
			state->rx_state = 0;
		}
		break;

	case 1:
		state->payload_length = (unsigned int)rx_data << 8;
		state->rx_state++;
		state->rx_timeout = PACKET_RX_TIMEOUT;
		break;

	case 2:
		state->payload_length |= (unsigned int)rx_data;
		if (state->payload_length > 0 &&
				state->payload_length <= PACKET_MAX_PL_LEN) {
			state->rx_state++;
			state->rx_timeout = PACKET_RX_TIMEOUT;
		} else {
			state->rx_state = 0;
		}
		break;

	case 3:
		state->rx_buffer[state->rx_data_ptr++] = rx_data;
		if (state->rx_data_ptr == state->payload_length) {
			state->rx_state++;
		}
		state->rx_timeout = PACKET_RX_TIMEOUT;
		break;

	case 4:
		state->crc_high = rx_data;
		state->rx_state++;
		state->rx_timeout = PACKET_RX_TIMEOUT;
		break;

	case 5:
		state->crc_low = rx_data;
		state->rx_state++;
		state->rx_timeout = PACKET_RX_TIMEOUT;
		break;

	case 6:
		if (rx_data == 3) {
			if (crc16(state->rx_buffer, state->payload_length)
					== ((unsigned short)state->crc_high << 8
							| (unsigned short)state->crc_low)) {
				// Packet received!
				if (state->process_func) {
					state->process_func(state->rx_buffer,
							state->payload_length,
							state->callback_arg);
				}
			}
		}
		state->rx_state = 0;
		break;

	default:
		state->rx_state = 0;
		break;
	}
}
