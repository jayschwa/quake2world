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

#include "ui_local.h"
#include "client.h"

extern cl_static_t cls;

/*
 * @brief Callback which connects to the server specified in data.
 */
static TW_CALL void Ui_Servers_Connect(void *data) {
	cl_server_info_t *s = (cl_server_info_t *) data;

	Cbuf_AddText(va("connect %s\n", Net_NetaddrToString(&s->addr)));
}

/*
 * @brief Callback to refresh the servers list.
 */
static TW_CALL void Ui_Servers_Refresh(void *data __attribute__((unused))) {
	Cl_Servers_f();
}

/*
 * @brief A new server status message has just been parsed. Rebuild the servers menu.
 */
void Ui_NewServer(void) {
	TwBar *bar = ui.servers;
	const GList *e = cls.servers;

	TwRemoveAllVars(bar);

	while (e) {
		cl_server_info_t *s = (cl_server_info_t *) e->data;

		if (s->max_clients) {
			char button[128], *group;

			memset(button, 0, sizeof(button));

			g_snprintf(button, sizeof(button), "%-40.40s %-16.16s %-24.24s %02d/%02d %5d",
					s->hostname, s->name, s->gameplay, s->clients, s->max_clients, s->ping);

			switch (s->source) {
				case SERVER_SOURCE_BCAST:
					group = "Local";
					break;
				case SERVER_SOURCE_USER:
					group = "Favorites";
					break;
				case SERVER_SOURCE_INTERNET:
				default:
					group = "Internet";
					break;
			}

			TwAddButton(bar, button, Ui_Servers_Connect, s, va("group=%s", group));
		}
		e = e->next;
	}

	TwAddSeparator(bar, NULL, NULL);
	TwAddButton(bar, "Refresh", Ui_Servers_Refresh, bar, NULL);
}

/*
 * @brief
 */
TwBar *Ui_Servers(void) {

	TwBar *bar = TwNewBar("Servers");

	TwAddSeparator(bar, NULL, NULL);
	TwAddButton(bar, "Refresh", Ui_Servers_Refresh, bar, NULL);

	TwDefine("Servers size='850 400' alpha=200 iconifiable=false visible=false");

	// force a refresh, so pings aren't 999 by default
	Ui_Servers_Refresh(NULL);

	return bar;
}
