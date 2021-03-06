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

#include <errno.h>

#ifndef _WIN32
#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/socket.h>
#endif

#include "net.h"

in_addr_t net_lo;

/*
 * @return A printable error string for the most recent OS-level network error.
 */
const char *Net_GetErrorString(void) {
	return strerror(Net_GetError());
}

/*
 * @brief Initializes the specified sockaddr_in according to the net_addr_t.
 */
void Net_NetAddrToSockaddr(const net_addr_t *a, struct sockaddr_in *s) {

	memset(s, 0, sizeof(*s));
	s->sin_family = AF_INET;

	if (a->type == NA_BROADCAST) {
		*(uint32_t *) &s->sin_addr = -1;
	} else if (a->type == NA_DATAGRAM) {
		*(in_addr_t *) &s->sin_addr = a->addr;
	}

	s->sin_port = a->port;
}

/*
 * @return True if the addresses share the same base and port.
 */
_Bool Net_CompareNetaddr(const net_addr_t *a, const net_addr_t *b) {
	return a->addr == b->addr && a->port == b->port;
}

/*
 * @return True if the addresses share the same type and base.
 */
_Bool Net_CompareClientNetaddr(const net_addr_t *a, const net_addr_t *b) {
	return a->type == b->type && a->addr == b->addr;
}

/*
 * @brief
 */
const char *Net_NetaddrToString(const net_addr_t *a) {
	static char s[64];

	g_snprintf(s, sizeof(s), "%s:%i", inet_ntoa(*(struct in_addr *) &a->addr), ntohs(a->port));

	return s;
}

/*
 * @brief Resolve internet hostnames to sockaddr. Examples:
 *
 * localhost
 * idnewt
 * idnewt:28000
 * 192.246.40.70
 * 192.246.40.70:28000
 */
_Bool Net_StringToSockaddr(const char *s, struct sockaddr_in *saddr) {
	char *colon;
	char copy[128];

	memset(saddr, 0, sizeof(*saddr));
	saddr->sin_family = AF_INET;

	g_strlcpy(copy, s, sizeof(copy));

	// strip off a trailing :port if present
	for (colon = copy; *colon; colon++) {
		if (*colon == ':') {
			*colon = '\0';
			saddr->sin_port = htons((int16_t) atoi(colon + 1));
		}
	}

	if (copy[0] >= '0' && copy[0] <= '9') {
		*(in_addr_t *) &saddr->sin_addr = inet_addr(copy);
	} else {
		struct hostent *h;
		if (!(h = gethostbyname(copy)))
			return false;
		*(in_addr_t *) &saddr->sin_addr = *(in_addr_t *) h->h_addr_list[0];
	}

	return true;
}

/*
 * @brief Parses the hostname and port into the specified net_addr_t.
 */
_Bool Net_StringToNetaddr(const char *s, net_addr_t *a) {
	struct sockaddr_in saddr;

	if (!Net_StringToSockaddr(s, &saddr))
		return false;

	a->addr = saddr.sin_addr.s_addr;

	if (a->addr == net_lo) {
		a->port = 0;
		a->type = NA_LOOP;
	} else {
		a->port = saddr.sin_port;
		a->type = NA_DATAGRAM;
	}

	return true;
}

/*
 * @brief Creates and binds a new network socket for the specified protocol.
 */
int32_t Net_Socket(net_addr_type_t type, const char *iface, in_port_t port) {
	int32_t sock, i = 1;

	switch (type) {
		case NA_BROADCAST:
		case NA_DATAGRAM:
			if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
				Com_Error(ERR_DROP, "socket: %s\n", Net_GetErrorString());
			}

			if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const void *) &i, sizeof(i)) == -1) {
				Com_Error(ERR_DROP, "setsockopt: %s\n", Net_GetErrorString());
			}
			break;

		case NA_STREAM:
			if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
				Com_Error(ERR_DROP, "socket: %s", Net_GetErrorString());
			}

			if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const void *) &i, sizeof(i)) == -1) {
				Com_Error(ERR_DROP, "setsockopt: %s", Net_GetErrorString());
			}
			break;

		default:
			Com_Error(ERR_DROP, "Invalid socket type: %d", type);
	}

	// make all sockets non-blocking
	if (ioctl(sock, FIONBIO, (void *) &i) == -1) {
		Com_Error(ERR_DROP, "ioctl: %s\n", Net_GetErrorString());
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	if (iface) {
		Net_StringToSockaddr(iface, &addr);
	} else {
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
	}

	addr.sin_port = htons(port);

	if (bind(sock, (void *) &addr, sizeof(addr)) == -1) {
		Com_Error(ERR_DROP, "bind: %s\n", Net_GetErrorString());
	}

	return sock;
}

/*
 * @brief
 */
void Net_Init(void) {

#ifdef _WIN32
	WORD v;
	WSADATA d;

	v = MAKEWORD(2, 2);
	WSAStartup(v, &d);
#endif

	Cvar_Get("net_interface", "", CVAR_NO_SET, NULL);
	Cvar_Get("net_port", va("%i", PORT_SERVER), CVAR_NO_SET, NULL);

	// assign a small random number for the qport
	Cvar_Get("net_qport", va("%d", Sys_Milliseconds() & 255), CVAR_NO_SET, NULL);

	net_lo = inet_addr("127.0.0.1");
}

/*
 * @brief
 */
void Net_Shutdown(void) {

#ifdef _WIN32
	WSACleanup();
#endif

}
