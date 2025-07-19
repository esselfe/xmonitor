#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <getopt.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>

#include "xmonitor.h"

const char *xmonitor_version_string = "0.2.1";

static const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'V'},
	{"position-x", required_argument, NULL, 'X'},
	{"position-y", required_argument, NULL, 'Y'},
	{"width", required_argument, NULL, 'W'},
	{"height", required_argument, NULL, 'H'},
	{NULL, 0, NULL, 0}
};
static const char *short_options = "hVX:Y:W:H:";

Display *display;
Window window, window_root;
GC gc, gc_buffers;
time_t t0, tp;
// May require changes on other systems:
char *temp_file = "/sys/class/hwmon/hwmon2/temp3_input";
char *km_file = "/abin/km.log"; // Contains en or ca, set by a custom script
FILE *temp_fp;
FILE *km_fp;
XTextItem text_temp;
XTextItem text_km;
int text_temp_len;
int text_km_len;
int winX = 10, winY = 10, winW = 120, winH = 68;
unsigned int text_temp_pos_x = 4, text_temp_pos_y = 12;
unsigned int text_km_pos_x = 70, text_km_pos_y = 12;
unsigned int mem_pos_x = 4, mem_pos_y = 20,
	mem_width = 40, mem_height = 40,
	swap_pos_x = 48, swap_pos_y = 20,
	swap_width = 40, swap_height = 40;

struct history mem_history, swap_history;

void ShowHelp(void) {
	printf("Usage: xmonitor { -h/--help | -V/--version\n");
	printf("                  -X/--position-x NUM | -Y/--position-y NUM\n");
	printf("                  -W/--width NUM | -H/--height NUM }\n");
}

void XmonitorSignal(int signum) {
	Display *dsp = XOpenDisplay(NULL);
	XClearArea(dsp, window, mem_pos_x, mem_pos_y, mem_width+1, mem_height+1, False);
	XClearArea(dsp, window, swap_pos_x, swap_pos_y, swap_width+1, swap_height+1, False);
	XClearArea(dsp, window, text_temp_pos_x, text_temp_pos_y-10, 120, 10, False);
	XFlush(dsp);
	exit(0);
}

void Draw(void) {
	XClearArea(display, window, mem_pos_x, mem_pos_y, mem_width+1, mem_height, False);
	XClearArea(display, window, swap_pos_x, swap_pos_y, swap_width+1, swap_height, False);

	XClearArea(display, window, text_temp_pos_x, text_temp_pos_y-10, 120, 10, False);
	XDrawText(display, window, gc, text_temp_pos_x, text_temp_pos_y, &text_temp, 1);
	if (km_fp != NULL)
		XDrawText(display, window, gc, text_km_pos_x, text_km_pos_y, &text_km, 1);

	// Draw contour lines
	// top
	XDrawLine(display, window, gc, mem_pos_x, mem_pos_y,
		mem_pos_x + mem_width, mem_pos_y);
	// right
	XDrawLine(display, window, gc, mem_pos_x + mem_width, mem_pos_y,
		mem_pos_x + mem_width, mem_pos_y + mem_height);
	// bottom
	XDrawLine(display, window, gc, mem_pos_x, mem_pos_y + mem_height,
		mem_pos_x + mem_width, mem_pos_y + mem_height);
	// left
	XDrawLine(display, window, gc, mem_pos_x, mem_pos_y,
		mem_pos_x, mem_pos_y + mem_height);

	// top
	XDrawLine(display, window, gc, swap_pos_x, swap_pos_y,
		swap_pos_x + swap_width, swap_pos_y);
	// right
	XDrawLine(display, window, gc, swap_pos_x + swap_width, swap_pos_y,
		swap_pos_x + swap_width, swap_pos_y + swap_height);
	// bottom
	XDrawLine(display, window, gc, swap_pos_x, swap_pos_y + swap_height,
		swap_pos_x + swap_width, swap_pos_y + swap_height);
	// left
	XDrawLine(display, window, gc, swap_pos_x, swap_pos_y,
		swap_pos_x, swap_pos_y + swap_height);

	int cnt;
	struct history_frame *frame = mem_history.first_frame;
	for (cnt = 0; cnt < mem_width; cnt++) {
		if (frame->used > 0 || frame->buffered > 0) {
			XDrawLine(display, window, gc, 
				mem_pos_x + cnt, mem_pos_y + mem_height,
				mem_pos_x + cnt, mem_pos_y + (mem_height - frame->used));
			XDrawLine(display, window, gc_buffers,
				mem_pos_x + cnt, mem_pos_y + mem_height - frame->used,
				mem_pos_x + cnt, mem_pos_y + mem_height - frame->used - frame->buffered);
		}
		
		if (frame->next)
			frame = frame->next;
		else
			break;
	}

	frame = swap_history.first_frame;
	for (cnt = 0; cnt < swap_width; cnt++) {
		if (frame->used > 0 || frame->buffered > 0) {
			XDrawLine(display, window, gc, 
				swap_pos_x + cnt, swap_pos_y + swap_height,
				swap_pos_x + cnt, swap_pos_y + (swap_height - frame->used));
		}
		
		if (frame->next)
			frame = frame->next;
		else
			break;
	}
}

void UpdateTemp(void) {
	if (temp_fp == NULL)
		return;

	char *line = malloc(64);
	memset(line, 0, 64);
	size_t line_size = 63;
	ssize_t nread = getline(&line, &line_size, temp_fp);
	unsigned int val = 0;
	if (nread > 0) {
		val = atoi(line)/1000;
		sprintf(text_temp.chars, "%u", val);
		text_temp.nchars = strlen(text_temp.chars);
	}

	fseek(temp_fp, 0, SEEK_SET);
	fflush(temp_fp);

	free(line);
}

void UpdateKM(void) {
	if (km_fp == NULL)
		return;

	char *line = malloc(64);
	memset(line, 0, 64);
	size_t line_size = 63;
	ssize_t nread = getline(&line, &line_size, km_fp);
	if (nread > 0) {
		if (line[strlen(line)-1] == '\n')
			line[strlen(line)-1] = '\0';

		sprintf(text_km.chars, "%s", line);
		text_km.nchars = strlen(text_km.chars);
	}

	fseek(km_fp, 0, SEEK_SET);
	fflush(km_fp);

	free(line);
}

int main(int argc, char **argv) {
	signal(SIGINT, XmonitorSignal);
	signal(SIGTERM, XmonitorSignal);
	signal(SIGQUIT, XmonitorSignal);

	// Parse program options and arguments
	int c;
	while (1) {
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			ShowHelp();
			exit(0);
			break;
		case 'V':
			printf("xmonitor %s\n", xmonitor_version_string);
			exit(0);
			break;
		case 'X':
			if (optarg != NULL && strlen(optarg) > 0)
				winX = atoi(optarg);

			break;
		case 'Y':
			if (optarg != NULL && strlen(optarg) > 0)
				winY = atoi(optarg);

			break;
		case 'W':
			if (optarg != NULL && strlen(optarg) > 0)
				winW = atoi(optarg);

			break;
		case 'H':
			if (optarg != NULL && strlen(optarg) > 0)
				winH = atoi(optarg);

			break;
		}
	}

	printf("xmonitor started\n");

	tp = t0 = time(NULL);

	temp_fp = fopen(temp_file, "r");
	if (temp_fp == NULL)
		printf("xmonitor error: Cannot open %s: %s\n", temp_file, strerror(errno));

	km_fp = fopen(km_file, "r");
	if (km_fp == NULL)
		printf("xmonitor error: Cannot open '%s': %s\n", km_file, strerror(errno));

	display = XOpenDisplay(NULL);
	if (display == NULL) { printf("xmonitor error: Cannot open X display!\n");
		return 1;
	}

	Screen *screen = XDefaultScreenOfDisplay(display);
	int screen_num = XScreenNumberOfScreen(screen);
	window_root = XDefaultRootWindow(display);
	XVisualInfo vinfo;
	if (!XMatchVisualInfo(display, screen_num, 32, TrueColor, &vinfo)) {
		fprintf(stderr, "xmonitor error: 32-bit visual not found, exiting.\n");
		exit(1);
	}

	XSetWindowAttributes wattr;
	memset(&wattr, 0, sizeof(XSetWindowAttributes));
	wattr.colormap = XCreateColormap(display, window_root, vinfo.visual, AllocNone);
	wattr.background_pixel = 0;
	wattr.border_pixel = 0;
	window = XCreateWindow(display, window_root, winX, winY, winW, winH,
		0, 32, InputOutput, vinfo.visual,
		CWBackPixel | CWBorderPixel | CWColormap, &wattr);

	XSizeHints wmsize;
	wmsize.flags = USPosition | USSize;
	XSetWMNormalHints(display, window, &wmsize);

	XWMHints wmhint;
	wmhint.initial_state = NormalState;
	wmhint.flags = StateHint;
	XSetWMHints(display, window, &wmhint);

	XGCValues gcv;
	gcv.foreground = 0xababab;
	gcv.background = 0x101010;
	gcv.line_width = 1;
	gcv.line_style = LineSolid;
	gc = XCreateGC(display, window,
				GCForeground|GCBackground|GCLineWidth|GCLineStyle,
				&gcv);
	if (gc == NULL) {
		printf("xmonitor error: Cannot create graphical context!\n");
		return 1;
	}

	gcv.foreground = 0x808080;
	gcv.background = 0x101010;
	gcv.line_width = 1;
	gcv.line_style = LineSolid;
	gc_buffers = XCreateGC(display, window,
				GCForeground|GCBackground|GCLineWidth|GCLineStyle,
				&gcv);
	if (gc_buffers == NULL) {
		printf("xmonitor error: Cannot create graphical context!\n");
		return 1;
	}

	text_temp_len = 4;
	text_temp.chars = malloc(text_temp_len);
	sprintf(text_temp.chars, "000");
	text_temp.nchars = strlen(text_temp.chars);
	text_temp.delta = 0;

	text_km_len = 64;
	text_km.chars = malloc(text_km_len);
	sprintf(text_km.chars, "  ");
	text_km.nchars = strlen(text_km.chars);
	text_km.delta = 0;

	XMapWindow(display, window);

	Atom window_transparency = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
	long opacity_value = 0xFFFFFFFF;
	XChangeProperty(display, window, window_transparency,
		XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&opacity_value, 1);

	Atom window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
	long value = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);
	XChangeProperty(display, window, window_type,
		XA_ATOM, 32, PropModeReplace, (unsigned char *)&value, 1);

	Atom wmStateSticky = XInternAtom(display, "_NET_WM_STATE_STICKY", 1);
	Atom wmNetWmState = XInternAtom(display, "_NET_WM_STATE", 1);
	XClientMessageEvent xclient;
	memset(&xclient, 0, sizeof(xclient));
	xclient.type = ClientMessage;
	xclient.window = window;
	xclient.message_type = wmNetWmState;
	xclient.format = 32;
	xclient.data.l[0] = _NET_WM_STATE_ADD;
	xclient.data.l[1] = wmStateSticky;
	xclient.data.l[3] = 0;
	xclient.data.l[4] = 0;
	XSendEvent(display, window_root, False,
		SubstructureRedirectMask | SubstructureNotifyMask, (XEvent *)&xclient);

	HistoryInit(&mem_history, mem_width);
	HistoryInit(&swap_history, swap_width);

	struct sysinfo sinfo;
	while (1) {
		t0 = time(NULL);
		if (t0 > tp) {
			tp = t0;

			UpdateTemp();
			UpdateKM();
			
			sysinfo(&sinfo);
			unsigned int bytes_per_pixel = sinfo.totalram / mem_height;
			unsigned long value = 
				(sinfo.totalram - sinfo.freeram - sinfo.bufferram) / bytes_per_pixel,
				value2 = sinfo.bufferram / bytes_per_pixel;
			HistoryPopFirst(&mem_history);
			HistoryAdd(&mem_history, value, value2);

			if (sinfo.totalswap > 0) {
				bytes_per_pixel = sinfo.totalswap / swap_height;
				value = (sinfo.totalswap - sinfo.freeswap) / bytes_per_pixel;
				HistoryPopFirst(&swap_history);
				HistoryAdd(&swap_history, value, 0);
			}
			else {
				HistoryPopFirst(&swap_history);
				HistoryAdd(&swap_history, 0, 0);
			}

			Draw();
			XFlush(display);
		}

		usleep(10000);
	}

	XCloseDisplay(display);
	return 0;
}

