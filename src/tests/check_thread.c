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

#include "tests.h"
#include "thread.h"

typedef struct {
	_Bool ready;
} critical_section_t;

static critical_section_t cs;

/*
 * @brief Setup fixture.
 */
void setup(void) {

	Mem_Init();

	Cmd_Init();

	Cvar_Init();

	Thread_Init();

	memset(&cs, 0, sizeof(cs));
}

/*
 * @brief Teardown fixture.
 */
void teardown(void) {

	Thread_Shutdown();

	Cvar_Shutdown();

	Cmd_Shutdown();

	Mem_Shutdown();
}

/*
 * @brief Populates the critical section.
 */
static void produce(void *data __attribute__((unused))) {
	cs.ready = true; // set the CS to ready
}

/*
 * @brief Consumes the critical section.
 */
static void consume(void *data) {

	Thread_Wait((thread_t *) data); // wait for the producer

	ck_assert(cs.ready); // ensure the CS was made ready

	cs.ready = false; // and reset it
}

START_TEST(check_Thread_Wait)
	{
		Cvar_SetValue("threads", 4);

		thread_t *p = Thread_Create(produce, NULL);

		thread_t *c = Thread_Create(consume, p);

		Thread_Wait(c);

		ck_assert(!cs.ready);

	}END_TEST

/*
 * @brief Test entry point.
 */
int32_t main(int32_t argc, char **argv) {

	Test_Init(argc, argv);

	TCase *tcase = tcase_create("check_Thread_Wait");
	tcase_add_checked_fixture(tcase, setup, teardown);

	tcase_add_test(tcase, check_Thread_Wait);

	Suite *suite = suite_create("check_threads");
	suite_add_tcase(suite, tcase);

	int32_t failed = Test_Run(suite);

	Test_Shutdown();
	return failed;
}
