/*
* Copyright(c) 1997-2001 Id Software, Inc.
* Copyright(c) 2002 The Quakeforge Project.
* Copyright(c) 2006 Quake2World.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or(at your option) any later version.
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

#include <SDL/SDL.h>
#include "q2wmap.h"

char mapname[MAX_OSPATH];
char bspname[MAX_OSPATH];
char outbase[MAX_OSPATH];

qboolean verbose;
qboolean debug;
qboolean legacy;

/** BSP */
extern qboolean noprune;
extern qboolean nodetail;
extern qboolean fulldetail;
extern qboolean onlyents;
extern qboolean nomerge;
extern qboolean nowater;
extern qboolean nofill;
extern qboolean nocsg;
extern qboolean noweld;
extern qboolean noshare;
extern qboolean nosubdivide;
extern qboolean notjunc;
extern qboolean noopt;
extern qboolean leaktest;
extern qboolean verboseentities;

extern int block_xl, block_xh, block_yl, block_yh;
extern vec_t microvolume;
extern int subdivide_size;

/** VIS */
extern char inbase[32];
extern qboolean fastvis;
extern qboolean nosort;
extern int testlevel;

/** LIGHT */
extern qboolean extrasamples;
extern float light_scale;
extern float saturation;
extern float contrast;
extern float surface_scale;
extern float entity_scale;

#ifdef _WIN32

static HANDLE Console;							//windows console
static FILE *output_file;						//file output
#define HORIZONTAL	45							//console size and buffer
#define VERTICAL		70						//console size and buffer
static int input_index_h, input_index_v;		//pdcurses print location
static char title[64];							//window bar title (updates to show status)

#ifdef HAVE_CURSES
#include <ncurses.h>

static void PDCursesInit(void){
	stdscr = initscr();		// initialize the ncurses window
	resize_term(HORIZONTAL, VERTICAL);	// resize the console
	cbreak();					// disable input line buffering
	noecho();					// don't show type characters
	keypad(stdscr, TRUE);	// enable special keys
	nodelay(stdscr, TRUE); 	// non-blocking input
	curs_set(1);				// enable the cursor

	if(has_colors() == TRUE){
		start_color();
		// this is ncurses-specific
		use_default_colors();
		// COLOR_PAIR(0) is terminal default
		init_pair(1, COLOR_RED, -1);
		init_pair(2, COLOR_GREEN, -1);
		init_pair(3, COLOR_YELLOW, -1);
		init_pair(4, COLOR_BLUE, -1);
		init_pair(5, COLOR_CYAN, -1);
		init_pair(6, COLOR_MAGENTA, -1);
		init_pair(7, -1, -1);
	}
}
#endif


static void OpenWin32Console(void){
	AllocConsole();
	Console = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0,0,CONSOLE_TEXTMODE_BUFFER,0);
	SetConsoleActiveScreenBuffer(Console);

	snprintf(title, sizeof(title), "Q2W Map Compiler");
	SetConsoleTitle(title);

	freopen("CON", "wt", stdout);
	freopen("CON", "wt", stderr);
	freopen("CON", "rt", stdin);

	if((output_file = fopen ("bsp_output.txt","w")) == NULL)
		Error("Could not open bsp_compiler.txt\n");

	input_index_h = 0;	// zero the index counters
	input_index_v = 0;	// zero the index counters
#ifdef HAVE_CURSES
	PDCursesInit();		// initialize the pdcurses window
#endif
}

static void Close32WinConsole(void){
	Fs_CloseFile(output_file);		//close the open file stream
	CloseHandle(Console);
	FreeConsole();
}

/*
Debug
*/
void Debug(const char *fmt, ...){
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	unsigned long cChars;

	if(!debug)
		return;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);	//copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);			//output to a file
	va_end(argptr);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}

/*
Verbose
*/
void Verbose(const char *fmt, ...){
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	unsigned long cChars;

	if(!verbose)
		return;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);	//copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);				//output to a file
	va_end(argptr);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
}


/*
Error
*/
void Error(const char *fmt, ...){
	va_list argptr;
	char msg[MAX_PRINT_MSG];
	unsigned long cChars;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);	//copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);				//output to a file
	va_end(argptr);

	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);

	strcpy(msg, "************ ERROR ************");
	fprintf(output_file, msg, argptr);				//output to a file
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
#ifdef HAVE_CURSES
	do {	//Don't quit, leave compiler error on screen until a key is pressed
	} while (getch() == ERR);

	endwin();					// shutdown pdcurses
#endif
	Close32WinConsole();		// close the console

	Z_FreeTags(0);
	exit(1);
}

/*
Print
*/
void Print(const char *fmt, ...){
	va_list argptr;

	char msg[MAX_PRINT_MSG];
	unsigned long cChars;
#ifdef HAVE_CURSES
	int n;
	char copymsg[2];
#endif

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);	//copy fmt into a friendly formatted string (msg)
	fprintf(output_file, msg, argptr);				//output to a file
	va_end(argptr);

#ifdef HAVE_CURSES
	if(verbose || debug)	//verbose and debug output doesn't need fancy output - use WriteConsole() instead
		WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);

	else {	//fancy colored pdcurses output for normal compiling
		for(n = 0; n < lstrlen(msg); n++){	//process the entire string (msg)
			if (msg[n] == '\n'){
				input_index_h++;	//start a new line
				input_index_v = 0;	//start at the beginning of the new line
			}
			else {	//otherwise continue processing the current line
				copymsg[0] = msg[n];	//copy a character
				if(input_index_h == 0){	//highlight the first line (q2wmap version, date, mingw32 build)
					attron(COLOR_PAIR(3));	//bright yellow
					attron(A_BOLD);
				}
				//highlight compiler progression (1... 2... 3... 4... 5... 6... 7... 8... 9... 10...)
				else if(input_index_h == 4 || input_index_h == 5 || input_index_h == 11 || input_index_h == 12 ||
					input_index_h == 21 || input_index_h == 22 || input_index_h == 23){
					attron(COLOR_PAIR(1));	//bright red
					attron(A_BOLD);
				}
				else
					attroff(COLOR_PAIR(3));	//turn off attributes

				//finally print our processed character on the console
				mvwprintw(stdscr, input_index_h, input_index_v, copymsg);
				refresh();


				input_index_v++;	//advance to the next character position
			}
		}
	}
#else
	WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), msg, lstrlen(msg), &cChars, NULL);
#endif
}

#else // _WIN32

/*
Debug
*/
void Debug(const char *fmt, ...){
	va_list argptr;

	if(!debug)
		return;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

/*
Verbose
*/
void Verbose(const char *fmt, ...){
	va_list argptr;

	if(!verbose)
		return;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}


/*
Error
*/
void Error(const char *fmt, ...){
	va_list argptr;

	printf("************ ERROR ************\n");

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);

	Z_FreeTags(0);
	exit(1);
}


/*
Print
*/
void Print(const char *fmt, ...){
	va_list argptr;

	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

#endif // _WIN32

/*
Check_BSP_Options
*/
static int Check_BSP_Options(int argc, char **argv){
	int i;

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-noweld")){
			Verbose("noweld = true\n");
			noweld = true;
		} else if(!strcmp(argv[i], "-nocsg")){
			Verbose("nocsg = true\n");
			nocsg = true;
		} else if(!strcmp(argv[i], "-noshare")){
			Verbose("noshare = true\n");
			noshare = true;
		} else if(!strcmp(argv[i], "-notjunc")){
			Verbose("notjunc = true\n");
			notjunc = true;
		} else if(!strcmp(argv[i], "-nowater")){
			Verbose("nowater = true\n");
			nowater = true;
		} else if(!strcmp(argv[i], "-noopt")){
			Verbose("noopt = true\n");
			noopt = true;
		} else if(!strcmp(argv[i], "-noprune")){
			Verbose("noprune = true\n");
			noprune = true;
		} else if(!strcmp(argv[i], "-nofill")){
			Verbose("nofill = true\n");
			nofill = true;
		} else if(!strcmp(argv[i], "-nomerge")){
			Verbose("nomerge = true\n");
			nomerge = true;
		} else if(!strcmp(argv[i], "-nosubdivide")){
			Verbose("nosubdivide = true\n");
			nosubdivide = true;
		} else if(!strcmp(argv[i], "-nodetail")){
			Verbose("nodetail = true\n");
			nodetail = true;
		} else if(!strcmp(argv[i], "-fulldetail")){
			Verbose("fulldetail = true\n");
			fulldetail = true;
		} else if(!strcmp(argv[i], "-onlyents")){
			Verbose("onlyents = true\n");
			onlyents = true;
		} else if(!strcmp(argv[i], "-micro")){
			microvolume = atof(argv[i + 1]);
			Verbose("microvolume = %f\n", microvolume);
			i++;
		} else if(!strcmp(argv[i], "-leaktest")){
			Verbose("leaktest = true\n");
			leaktest = true;
		} else if(!strcmp(argv[i], "-verboseentities")){
			Verbose("verboseentities = true\n");
			verboseentities = true;
		} else if(!strcmp(argv[i], "-subdivide")){
			subdivide_size = atoi(argv[i + 1]);
			Verbose("subdivide_size = %d\n", subdivide_size);
			i++;
		} else if(!strcmp(argv[i], "-block")){
			block_xl = block_xh = atoi(argv[i + 1]);
			block_yl = block_yh = atoi(argv[i + 2]);
			Verbose("block: %i,%i\n", block_xl, block_yl);
			i += 2;
		} else if(!strcmp(argv[i], "-blocks")){
			block_xl = atoi(argv[i + 1]);
			block_yl = atoi(argv[i + 2]);
			block_xh = atoi(argv[i + 3]);
			block_yh = atoi(argv[i + 4]);
			Verbose("blocks: %i,%i to %i,%i\n",
			           block_xl, block_yl, block_xh, block_yh);
			i += 4;
		} else if(!strcmp(argv[i], "-tmpout")){
			strcpy(outbase, "/tmp");
		} else
			break;
	}
	return 0;
}


/*
Check_VIS_Options
*/
static int Check_VIS_Options(int argc, char **argv){
	int i;

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-fast")){
			Verbose("fastvis = true\n");
			fastvis = true;
		} else if(!strcmp(argv[i], "-level")){
			testlevel = atoi(argv[i + 1]);
			Verbose("testlevel = %i\n", testlevel);
			i++;
		} else if(!strcmp(argv[i], "-nosort")){
			Verbose("nosort = true\n");
			nosort = true;
		} else
			break;
	}

	return 0;
}


/*
Check_LIGHT_Options
*/
static int Check_LIGHT_Options(int argc, char **argv){
	int i;

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-extra")){
			extrasamples = true;
			Verbose("extrasamples = true\n");
		} else if(!strcmp(argv[i], "-scale")){
			light_scale = atof(argv[i + 1]);
			Verbose("light scale at %f\n", light_scale);
			i++;
		} else if(!strcmp(argv[i], "-saturation")){
			saturation = atof(argv[i + 1]);
			Verbose("saturation at %f\n", saturation);
			i++;
		} else if(!strcmp(argv[i], "-contrast")){
			contrast = atof(argv[i + 1]);
			Verbose("contrast at %f\n", contrast);
			i++;
		} else if(!strcmp(argv[i], "-surface")){
			surface_scale *= atof(argv[i + 1]);
			Verbose("surface light scale at %f\n", surface_scale);
			i++;
		} else if(!strcmp(argv[i], "-entity")){
			entity_scale *= atof(argv[i + 1]);
			Verbose("entity light scale at %f\n", entity_scale);
			i++;
		} else
			break;
	}

	return 0;
}


/*
Check_PAK_Options
*/
static int Check_PAK_Options(int argc, char **argv){
	return 0;
}

static void PrintHelpMessage(void){
	Print("General options\n");
	Print("-v -verbose\n");
	Print("-l -legacy            Compile a legacy Quake2 map\n");
	Print("-d -debug\n");
	Print("-t -threads <int>\n");

	Print("\n");
	Print("-bsp               Binary space partitioning (BSPing) options:\n");
	Print(" -block <int> <int>\n");
	Print(" -blocks <int> <int> <int> <int>\n");
	Print(" -fulldetail\n");
	Print(" -leaktest\n");
	Print(" -micro <float>\n");
	Print(" -nocsg\n");
	Print(" -nodetail\n");
	Print(" -nofill\n");
	Print(" -nomerge\n");
	Print(" -noopt\n");
	Print(" -noprune\n");
	Print(" -noshare\n");
	Print(" -nosubdivide\n");
	Print(" -notjunc\n");
	Print(" -nowater\n");
	Print(" -noweld\n");
	Print(" -onlyents\n");
	Print(" -subdivide <int>\n");
	Print(" -tmpout\n");
	Print(" -verboseentities\n");
	Print("\n");
	Print("-vis               VIS stage options:\n");
	Print(" -fast\n");
	Print(" -level\n");
	Print(" -nosort\n");
	Print("\n");
	Print("-light             Lighting stage options:\n");
	Print(" -contrast <float>\n");
	Print(" -entity <float>\n");
	Print(" -extra\n");
	Print(" -scale <float>\n");
	Print(" -saturation <float>\n");
	Print(" -surface <float>\n");
	Print("\n");
	Print("-pak               PAK file options:\n");
	Print("\n");
	Print("Examples:\n");
	Print("Standard full compile:\n q2wmap -bsp -vis -light maps/my.map\n");
	Print("Fast vis, extra light, two threads:\n q2wmap -t 2 -bsp -vis -fast -light -extra maps/my.map\n");
	Print("\n");
}

/*
main
*/
int main(int argc, char **argv){
	int i;
	int r = 0;
	int total_time;
	time_t start, end;
	int alt_argc;
	char *c, **alt_argv;
	qboolean do_bsp = false;
	qboolean do_vis = false;
	qboolean do_light = false;
	qboolean do_pak = false;

#ifdef _WIN32
	OpenWin32Console();		//	initialize the windows console
#endif

	Print("Quake2World Map %s %s %s\n", VERSION, __DATE__, BUILDHOST);

	// init memory management facilities
	Z_Init();

	// init thread state
	memset(&threadstate, 0, sizeof(threadstate));

	if(argc < 2)
		PrintHelpMessage();

	// general options
	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "-verbose")){
			verbose = true;
			continue;
		}

		if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "-debug")){
			debug = true;
			continue;
		}

		if(!strcmp(argv[i], "-t") || !strcmp(argv[i], "-threads")){
			threadstate.numthreads = atoi(argv[++i]);
			continue;
		}

		if(!strcmp(argv[i], "-l") || !strcmp(argv[i], "-legacy")){
			legacy = true;
			continue;
		}
	}

	// read compiling options
	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help")){
			PrintHelpMessage();
			return 0;
		}

		if(!strcmp(argv[i], "-bsp")){
			do_bsp = true;
			alt_argc = argc - i;
			alt_argv = (char **)(argv + i);
			Check_BSP_Options(alt_argc, alt_argv);
		}

		if(!strcmp(argv[i], "-vis")){
			do_vis = true;
			alt_argc = argc - i;
			alt_argv = (char **)(argv + i);
			Check_VIS_Options(alt_argc, alt_argv);
		}

		if(!strcmp(argv[i], "-light")){
			do_light = true;
			alt_argc = argc - i;
			alt_argv = (char **)(argv + i);
			Check_LIGHT_Options(alt_argc, alt_argv);
		}

		if(!strcmp(argv[i], "-pak")){
			do_pak = true;
			alt_argc = argc - i;
			alt_argv = (char **)(argv + i);
			Check_PAK_Options(alt_argc, alt_argv);
		}
	}

	if(!do_bsp && !do_vis && !do_light && !do_pak)
		Error("No action specified.\n"
				"Please specify at least one of -bsp -vis -light.\n");

	// ugly little hack to localize global paths to game paths
	// for e.g. GtkRadiant
	c = strstr(argv[argc - 1], "/maps/");
	c = c ? c + 1 : argv[argc - 1];

	Com_StripExtension(c, mapname);
	strcpy(bspname, mapname);
	strcat(mapname, ".map");
	strcat(bspname, ".bsp");

	// init core facilities
	Swap_Init();
	Fs_InitFilesystem();

	// start timer
	start = time(NULL);
	srand(0);

	if(do_bsp)
		BSP_Main();
	if(do_vis)
		VIS_Main();
	if(do_light)
		LIGHT_Main();
	if(do_pak)
		PAK_Main();

	Z_FreeTags(0);
	Z_Shutdown();

	// emit time
	end = time(NULL);
	total_time = (int)(end - start);
	Print("\nTotal Time: ");
	if(total_time > 59)
		Print("%d Minutes ", total_time / 60);
	Print("%d Seconds\n", total_time % 60);

#ifdef _WIN32
	snprintf(title, sizeof(title), "Q2WMap [Finished]");
	SetConsoleTitle(title);

	Print("\n-----------------------------------------------------------------\n");
	Print("%s has been compiled sucessfully! \n\nPress any key to quit\n", bspname);
	Print("-----------------------------------------------------------------");

#ifdef HAVE_CURSES
	do {	//Don't quit, leave compile statistics on screen until a key is pressed
	} while (getch() == ERR);

	endwin();					// shutdown pdcurses
#endif

	Close32WinConsole();		// close the console
#endif

	// exit with error code
	return r;
}