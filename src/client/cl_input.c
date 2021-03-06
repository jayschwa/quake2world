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

#include "cl_local.h"

cvar_t *cl_run;
static cvar_t *cl_forward_speed;
static cvar_t *cl_pitch_speed;
static cvar_t *cl_right_speed;
static cvar_t *cl_up_speed;
static cvar_t *cl_yaw_speed;

static cvar_t *m_grab;
cvar_t *m_interpolate;
cvar_t *m_invert;
static cvar_t *m_pitch;
cvar_t *m_sensitivity_zoom;
cvar_t *m_sensitivity;
static cvar_t *m_yaw;

// key strokes queued per frame, power of 2
#define MAX_KEY_QUEUE 64

typedef struct {
	SDLKey key;
	uint16_t unicode;
	_Bool down;
} cl_key_queue_t;

static cl_key_queue_t cl_key_queue[MAX_KEY_QUEUE];

static int32_t cl_key_queue_head = 0;
static int32_t cl_key_queue_tail = 0;

#define EVENT_ENQUEUE(keyNum, keyUnicode, keyDown) \
	if(keyNum > 0){ \
		cl_key_queue[cl_key_queue_head].key = (SDLKey) (keyNum); \
		cl_key_queue[cl_key_queue_head].unicode = (uint16_t) (keyUnicode); \
		cl_key_queue[cl_key_queue_head].down = (_Bool) (keyDown); \
		cl_key_queue_head = (cl_key_queue_head + 1) & (MAX_KEY_QUEUE - 1); \
	}

/*
 * KEY BUTTONS
 *
 * Continuous button event tracking is complicated by the fact that two different
 * input sources (say, mouse button 1 and the control key) can both press the
 * same button, but the button should only be released when both of the
 * pressing key have been released.
 *
 * When a key event issues a button command (+forward, +attack, etc), it appends
 * its key number as a parameter to the command so it can be matched up with
 * the release.
 *
 * state bit 0 is the current state of the key
 * state bit 1 is edge triggered on the up to down transition
 * state bit 2 is edge triggered on the down to up transition
 */

typedef struct {
	SDLKey down[2]; // keys holding it down
	uint32_t down_time; // msec timestamp
	uint32_t msec; // msec down this frame
	byte state;
} cl_button_t;

static cl_button_t cl_buttons[12];
#define in_left cl_buttons[0]
#define in_right cl_buttons[1]
#define in_forward cl_buttons[2]
#define in_back cl_buttons[3]
#define in_look_up cl_buttons[4]
#define in_look_down cl_buttons[5]
#define in_move_left cl_buttons[6]
#define in_move_right cl_buttons[7]
#define in_speed cl_buttons[8]
#define in_attack cl_buttons[9]
#define in_up cl_buttons[10]
#define in_down cl_buttons[11]

/*
 * @brief
 */
static void Cl_KeyDown(cl_button_t *b) {
	SDLKey k;

	const char *c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = SDLK_MLAST; // typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return; // repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else {
		Com_Debug("3 keys down for button\n");
		return;
	}

	if (b->state & 1)
		return; // still down

	// save timestamp
	const char *t = Cmd_Argv(2);
	b->down_time = atoi(t);
	if (!b->down_time)
		b->down_time = cls.real_time;

	b->state |= 1;
}

/*
 * @brief
 */
static void Cl_KeyUp(cl_button_t *b) {
	SDLKey k;

	const char *c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else { // typed manually at the console, assume for un-sticking, so clear all
		b->down[0] = b->down[1] = 0;
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return; // key up without corresponding down

	if (b->down[0] || b->down[1])
		return; // some other key is still holding it down

	if (!(b->state & 1))
		return; // still up (this should not happen)

	// save timestamp
	const char *t = Cmd_Argv(2);
	const uint32_t uptime = atoi(t);
	if (uptime)
		b->msec += uptime - b->down_time;
	else
		b->msec += 10;

	b->state &= ~1; // now up
}

static void Cl_Up_down_f(void) {
	Cl_KeyDown(&in_up);
}
static void Cl_Up_up_f(void) {
	Cl_KeyUp(&in_up);
}
static void Cl_Down_down_f(void) {
	Cl_KeyDown(&in_down);
}
static void Cl_Down_up_f(void) {
	Cl_KeyUp(&in_down);
}
static void Cl_Left_down_f(void) {
	Cl_KeyDown(&in_left);
}
static void Cl_Left_up_f(void) {
	Cl_KeyUp(&in_left);
}
static void Cl_Right_down_f(void) {
	Cl_KeyDown(&in_right);
}
static void Cl_Right_up_f(void) {
	Cl_KeyUp(&in_right);
}
static void Cl_Forward_down_f(void) {
	Cl_KeyDown(&in_forward);
}
static void Cl_Forward_up_f(void) {
	Cl_KeyUp(&in_forward);
}
static void Cl_Back_down_f(void) {
	Cl_KeyDown(&in_back);
}
static void Cl_Back_up_f(void) {
	Cl_KeyUp(&in_back);
}
static void Cl_LookUp_down_f(void) {
	Cl_KeyDown(&in_look_up);
}
static void Cl_LookUp_up_f(void) {
	Cl_KeyUp(&in_look_up);
}
static void Cl_LookDown_down_f(void) {
	Cl_KeyDown(&in_look_down);
}
static void Cl_LookDown_up_f(void) {
	Cl_KeyUp(&in_look_down);
}
static void Cl_MoveLeft_down_f(void) {
	Cl_KeyDown(&in_move_left);
}
static void Cl_MoveLeft_up_f(void) {
	Cl_KeyUp(&in_move_left);
}
static void Cl_MoveRight_down_f(void) {
	Cl_KeyDown(&in_move_right);
}
static void Cl_MoveRight_up_f(void) {
	Cl_KeyUp(&in_move_right);
}
static void Cl_Speed_down_f(void) {
	Cl_KeyDown(&in_speed);
}
static void Cl_Speed_up_f(void) {
	Cl_KeyUp(&in_speed);
}
static void Cl_Attack_down_f(void) {
	Cl_KeyDown(&in_attack);
}
static void Cl_Attack_up_f(void) {
	Cl_KeyUp(&in_attack);
}
static void Cl_CenterView_f(void) {
	cl.angles[PITCH] = 0;
}

/*
 * @brief Returns the fraction of the command interval for which the key was down.
 */
static vec_t Cl_KeyState(cl_button_t *key, uint32_t cmd_msec) {
	uint32_t msec;
	vec_t v;

	msec = key->msec;
	key->msec = 0;

	if (key->state) { // still down, reset downtime for next frame
		msec += cls.real_time - key->down_time;
		key->down_time = cls.real_time;
	}

	v = (msec * 1000.0) / (cmd_msec * 1000.0);

	if (v > 1.0)
		v = 1.0;
	else if (v < 0.0)
		v = 0.0;

	return v;
}

/*
 * @brief
 */
static void Cl_HandleEvent(SDL_Event *event) {
	SDLButton b;

	if (cls.key_state.dest == KEY_UI) { // let the menus handle events
		if (Ui_Event(event))
			return;
	}

	switch (event->type) {
	case SDL_MOUSEBUTTONUP:
	case SDL_MOUSEBUTTONDOWN:
		b = SDLK_MOUSE1 + (event->button.button - 1) % 8;
		EVENT_ENQUEUE(b, b, (event->type == SDL_MOUSEBUTTONDOWN))
		break;

	case SDL_KEYDOWN:
	case SDL_KEYUP:
		EVENT_ENQUEUE(event->key.keysym.sym, event->key.keysym.unicode, (event->type == SDL_KEYDOWN))
		break;

	case SDL_QUIT:
		Cmd_ExecuteString("quit");
		break;

	case SDL_VIDEORESIZE:
		Cvar_SetValue("r_windowed_width", event->resize.w);
		Cvar_SetValue("r_windowed_height", event->resize.h);
		Cbuf_AddText("r_restart\n");
		break;
	}
}

/*
 * @brief
 */
static void Cl_MouseMove(int32_t mx, int32_t my) {

	if (m_sensitivity->modified) { // clamp sensitivity
		m_sensitivity->value = Clamp(m_sensitivity->value, 0.1, 20.0);
		m_sensitivity->modified = false;
	}

	if (m_interpolate->value) { // interpolate movements
		cls.mouse_state.x = (mx + cls.mouse_state.old_x) * 0.5;
		cls.mouse_state.y = (my + cls.mouse_state.old_y) * 0.5;
	} else {
		cls.mouse_state.x = mx;
		cls.mouse_state.y = my;
	}

	cls.mouse_state.old_x = mx;
	cls.mouse_state.old_y = my;

	// for active, connected players, add it to their move
	if (cls.state == CL_ACTIVE && cls.key_state.dest == KEY_GAME) {

		cls.mouse_state.x -= r_context.width / 2; // first normalize to center
		cls.mouse_state.y -= r_context.height / 2;

		cls.mouse_state.x *= m_sensitivity->value; // then amplify
		cls.mouse_state.y *= m_sensitivity->value;

		if (m_invert->value) // and finally invert
			cls.mouse_state.y = -cls.mouse_state.y;

		// add horizontal and vertical movement
		cl.angles[YAW] -= m_yaw->value * cls.mouse_state.x;
		cl.angles[PITCH] += m_pitch->value * cls.mouse_state.y;
	}

	if (cls.key_state.dest != KEY_UI && cls.mouse_state.grabbed) {
		// warp the cursor back to the center of the screen
		SDL_WarpMouse(r_context.width / 2, r_context.height / 2);
	}
}

/*
 * @brief
 */
void Cl_HandleEvents(void) {
	static cl_key_dest_t prev_key_dest;
	SDL_Event event;
	int32_t mx, my;

	if (!SDL_WasInit(SDL_INIT_VIDEO))
		return;

	// set key repeat based on key destination
	if (cls.key_state.dest == KEY_GAME) {
		if (cls.key_state.repeat) {
			SDL_EnableKeyRepeat(0, 0);
			cls.key_state.repeat = false;
		}
	} else {
		if (!cls.key_state.repeat) {
			SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
			cls.key_state.repeat = true;
		}
	}

	// send key-up events to previous destination before handling new events
	if (prev_key_dest != cls.key_state.dest) {
		cl_key_dest_t dest = cls.key_state.dest;
		cls.key_state.dest = prev_key_dest;

		SDLKey i;

		for (i = SDLK_FIRST; i < (SDLKey) SDLK_MLAST; i++) {
			if (cls.key_state.down[i]) {
				if (cls.key_state.binds[i] && cls.key_state.binds[i][0] == '+') {
					Cl_KeyEvent(i, i, false, cls.real_time);
				}
			}
		}

		cls.key_state.dest = dest;
	}

	// handle new key events
	while (true) {

		memset(&event, 0, sizeof(event));

		if (SDL_PollEvent(&event))
			Cl_HandleEvent(&event);
		else
			break;
	}

	// ignore mouse position after SDL re-grabs mouse
	// http://bugzilla.libsdl.org/show_bug.cgi?id=341
	_Bool invalid_mouse_state = false;

	// force a mouse grab when changing video modes
	if (r_view.update) {
		cls.mouse_state.grabbed = false;
	}

	if (cls.key_state.dest == KEY_CONSOLE || cls.key_state.dest == KEY_UI || !m_grab->integer) {
		if (!r_context.fullscreen) { // allow cursor to move outside window
			SDL_WM_GrabInput(SDL_GRAB_OFF);
			cls.mouse_state.grabbed = false;
		}
	} else {
		if (!cls.mouse_state.grabbed) { // grab it for everything else
			SDL_WM_GrabInput(SDL_GRAB_ON);
			cls.mouse_state.grabbed = true;
			invalid_mouse_state = true;
		}
	}

	prev_key_dest = cls.key_state.dest;

	SDL_GetMouseState(&mx, &my);

	if (invalid_mouse_state) {
		mx = r_context.width / 2;
		my = r_context.height / 2;
	}

	Cl_MouseMove(mx, my);

	while (cl_key_queue_head != cl_key_queue_tail) { // then check for keys
		cl_key_queue_t *k = &cl_key_queue[cl_key_queue_tail];

		Cl_KeyEvent(k->key, k->unicode, k->down, cls.real_time);

		cl_key_queue_tail = (cl_key_queue_tail + 1) & (MAX_KEY_QUEUE - 1);
	}
}

/*
 * @brief
 */
static void Cl_ClampPitch(void) {
	const pm_state_t *s = &cl.frame.ps.pm_state;

	// ensure our pitch is valid
	vec_t pitch = UnpackAngle(s->delta_angles[PITCH] + s->kick_angles[PITCH]);

	if (pitch > 180.0)
		pitch -= 360.0;

	if (cl.angles[PITCH] + pitch < -360.0)
		cl.angles[PITCH] += 360.0; // wrapped
	if (cl.angles[PITCH] + pitch > 360.0)
		cl.angles[PITCH] -= 360.0; // wrapped

	if (cl.angles[PITCH] + pitch > 89.0)
		cl.angles[PITCH] = 89.0 - pitch;
	if (cl.angles[PITCH] + pitch < -89.0)
		cl.angles[PITCH] = -89.0 - pitch;
}

/*
 * @brief Accumulate this frame's movement-related inputs and assemble a movement
 * command to send to the server. This may be called several times for each
 * command that is transmitted if the client is running asynchronously.
 */
void Cl_Move(user_cmd_t *cmd) {

	if (cmd->msec < 1) // save key states for next move
		return;

	// keyboard move forward / back
	cmd->forward += cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_forward, cmd->msec);
	cmd->forward -= cl_forward_speed->value * cmd->msec * Cl_KeyState(&in_back, cmd->msec);

	// keyboard strafe left / right
	cmd->right += cl_right_speed->value * cmd->msec * Cl_KeyState(&in_move_right, cmd->msec);
	cmd->right -= cl_right_speed->value * cmd->msec * Cl_KeyState(&in_move_left, cmd->msec);

	// keyboard jump / crouch
	cmd->up += cl_up_speed->value * cmd->msec * Cl_KeyState(&in_up, cmd->msec);
	cmd->up -= cl_up_speed->value * cmd->msec * Cl_KeyState(&in_down, cmd->msec);

	// keyboard turn left / right
	cl.angles[YAW] -= cl_yaw_speed->value * cmd->msec * Cl_KeyState(&in_right, cmd->msec);
	cl.angles[YAW] += cl_yaw_speed->value * cmd->msec * Cl_KeyState(&in_left, cmd->msec);

	// keyboard look up / down
	cl.angles[PITCH] -= cl_pitch_speed->value * cmd->msec * Cl_KeyState(&in_look_up, cmd->msec);
	cl.angles[PITCH] += cl_pitch_speed->value * cmd->msec * Cl_KeyState(&in_look_down, cmd->msec);

	Cl_ClampPitch(); // clamp, accounting for frame delta angles

	// pack the angles into the command
	PackAngles(cl.angles, cmd->angles);

	// set any button hits that occurred since last frame
	if (in_attack.state & 3)
		cmd->buttons |= BUTTON_ATTACK;

	in_attack.state &= ~2;

	if (cl_run->value) { // run by default, walk on speed toggle
		if (in_speed.state & 1)
			cmd->buttons |= BUTTON_WALK;
	} else { // walk by default, run on speed toggle
		if (!(in_speed.state & 1))
			cmd->buttons |= BUTTON_WALK;
	}
}

/*
 * @brief
 */
void Cl_ClearInput(void) {

	memset(cl_key_queue, 0, sizeof(cl_key_queue));

	cl_key_queue_head = 0;
	cl_key_queue_tail = 0;

	memset(cl_buttons, 0, sizeof(cl_buttons));
}

/*
 * @brief
 */
void Cl_InitInput(void) {

	Cmd_Add("center_view", Cl_CenterView_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_up", Cl_Up_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_up", Cl_Up_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_down", Cl_Down_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_down", Cl_Down_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+left", Cl_Left_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-left", Cl_Left_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+right", Cl_Right_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-right", Cl_Right_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+forward", Cl_Forward_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-forward", Cl_Forward_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+back", Cl_Back_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-back", Cl_Back_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+look_up", Cl_LookUp_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-look_up", Cl_LookUp_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+look_down", Cl_LookDown_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-look_down", Cl_LookDown_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_left", Cl_MoveLeft_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_left", Cl_MoveLeft_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+move_right", Cl_MoveRight_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-move_right", Cl_MoveRight_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+speed", Cl_Speed_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-speed", Cl_Speed_up_f, CMD_CLIENT, NULL);
	Cmd_Add("+attack", Cl_Attack_down_f, CMD_CLIENT, NULL);
	Cmd_Add("-attack", Cl_Attack_up_f, CMD_CLIENT, NULL);

	cl_run = Cvar_Get("cl_run", "1", CVAR_ARCHIVE, NULL);
	cl_forward_speed = Cvar_Get("cl_forward_speed", "100.0", 0, NULL);
	cl_pitch_speed = Cvar_Get("cl_pitch_speed", "0.15", 0, NULL);
	cl_right_speed = Cvar_Get("cl_right_speed", "100.0", 0, NULL);
	cl_up_speed = Cvar_Get("cl_up_speed", "100.0", 0, NULL);
	cl_yaw_speed = Cvar_Get("cl_yaw_speed", "0.2", 0, NULL);

	m_grab = Cvar_Get("m_grab", "1", 0, NULL);
	m_interpolate = Cvar_Get("m_interpolate", "0", CVAR_ARCHIVE, NULL);
	m_invert = Cvar_Get("m_invert", "0", CVAR_ARCHIVE, "Invert the mouse");
	m_pitch = Cvar_Get("m_pitch", "0.022", 0, NULL);
	m_sensitivity = Cvar_Get("m_sensitivity", "3.0", CVAR_ARCHIVE, NULL);
	m_sensitivity_zoom = Cvar_Get("m_sensitivity_zoom", "1.0", CVAR_ARCHIVE, NULL);
	m_yaw = Cvar_Get("m_yaw", "0.022", 0, NULL);

	Cl_ClearInput();

	cls.mouse_state.grabbed = true;
}

