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
 * packet.h
 *
 *  Created on: 21 mar 2013
 *      Author: benjamin
 */

#ifndef PACKET_H_
#define PACKET_H_

#include <stdint.h>

// Settings
#define PACKET_RX_TIMEOUT		2
#define PACKET_MAX_PL_LEN		512

typedef struct bldc_packet_state {
	volatile unsigned char rx_state;
	volatile unsigned char rx_timeout;
	void(*send_func)(unsigned char *data, unsigned int len, void *arg);
	void(*process_func)(unsigned char *data, unsigned int len, void *arg);
	void *callback_arg;
	unsigned int payload_length;
	unsigned char rx_buffer[PACKET_MAX_PL_LEN];
	unsigned char tx_buffer[PACKET_MAX_PL_LEN + 6];
	unsigned int rx_data_ptr;
	unsigned char crc_low;
	unsigned char crc_high;
} bldc_packet_state_t;

// Functions
void packet_init(bldc_packet_state_t *state, void (*s_func)(unsigned char *data, unsigned int len, void *arg),
		void (*p_func)(unsigned char *data, unsigned int len, void *arg), void *arg);
void packet_process_byte(bldc_packet_state_t *state, uint8_t rx_data);
void packet_timerfunc(bldc_packet_state_t *state);
void packet_send_packet(bldc_packet_state_t *state, unsigned char *data, unsigned int len);

#endif /* PACKET_H_ */
