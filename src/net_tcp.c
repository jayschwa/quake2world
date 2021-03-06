/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _WIN32
#include <sys/select.h>
#endif

#include "net_tcp.h"
#include "net_message.h"

/*
 * @brief Establishes a TCP connection to the specified host.
 *
 * @param host The host to connect to. See Net_StringToNetaddr.
 */
int32_t Net_Connect(const char *host, struct timeval *timeout) {
	struct sockaddr_in addr;

	int32_t sock = Net_Socket(NA_STREAM, NULL, 0);

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;

	struct sockaddr_in to;
	Net_StringToSockaddr(host, &to);

	if (connect(sock, (const struct sockaddr *) &to, sizeof(to)) == -1) {

		if (Net_GetError() == EINPROGRESS) { // non-blocking sockets do this
			fd_set fdset;

			FD_ZERO(&fdset);
			FD_SET((uint32_t) sock, &fdset);

			if (select(sock + 1, NULL, &fdset, NULL, timeout) < 1) {
				Com_Error(ERR_DROP, "%s", Net_GetErrorString());
			}
		} else {
			Com_Error(ERR_DROP, "%s", Net_GetErrorString());
		}
	}

	return sock;
}

/*
 * @brief Send data to the specified TCP stream.
 */
_Bool Net_SendStream(int32_t sock, const void *data, size_t len) {
	mem_buf_t buf;
	byte buffer[MAX_MSG_SIZE];

	Mem_InitBuffer(&buf, buffer, sizeof(buffer));

	// write the packet length
	Net_WriteLong(&buf, (int32_t) len);

	// and copy the payload
	Net_WriteData(&buf, data, len);

	ssize_t sent;
	while ((sent = send(sock, (void *) buf.data, buf.size, 0)) == -1) {

		if (Net_GetError() != EWOULDBLOCK) {
			Com_Warn("%s\n", Net_GetErrorString());
			return false;
		}
	}

	return sent == (ssize_t) buf.size;
}

/*
 * @brief Receive data from the specified TCP stream.
 */
_Bool Net_ReceiveStream(int32_t sock, mem_buf_t *buf) {

	buf->size = buf->read = 0;

	const ssize_t received = recv(sock, (void *) buf->data, buf->max_size, 0);
	if (received == -1) {

		if (Net_GetError() == EWOULDBLOCK)
			return false; // no data, don't crap our pants

		Com_Warn("%s\n", Net_GetErrorString());
		return false;
	}

	buf->size = received;

	// check the packet length
	return Net_ReadLong(buf) > -1;
}
