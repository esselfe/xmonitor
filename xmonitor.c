#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <X11/Xlib.h>

#include "xmonitor.h"

Display *display;
Window window;
GC gc, gc_buffers;
time_t t0, tp;
// May require changes on other systems:
char *temp_file = "/sys/class/hwmon/hwmon2/temp3_input";
char *km_file = "/abin/km.log"; // Here, contains us or ca, set by a custom script
FILE *temp_fp;
FILE *km_fp;
XTextItem text_temp;
XTextItem text_km;
int text_temp_len;
int text_km_len;
// local screen dimensions: 1366x768
unsigned int text_temp_pos_x = 1278, text_temp_pos_y = 696;
unsigned int text_km_pos_x = 1346, text_km_pos_y = 696;
unsigned int mem_pos_x = 1277, mem_pos_y = 700,
	mem_width = 40, mem_height = 40,
	swap_pos_x = 1322, swap_pos_y = 700,
	swap_width = 40, swap_height = 40;

struct history mem_history, swap_history;

void XmonitorSignal(int signum) {
	Display *dsp = XOpenDisplay(NULL);
	XClearArea(dsp, window, mem_pos_x, mem_pos_y, mem_width+4, mem_height+1, False);
	XClearArea(dsp, window, swap_pos_x, swap_pos_y, swap_width+4, swap_height+1, False);
	XClearArea(dsp, window, text_temp_pos_x, text_temp_pos_y-10, 120, 10, False);
	XFlush(dsp);
	exit(0);
}

void Draw(void) {
	XClearArea(display, window, mem_pos_x, mem_pos_y, mem_width+4, mem_height, False);
	XClearArea(display, window, swap_pos_x, swap_pos_y, swap_width+4, swap_height, False);

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
		line[strlen(line)-1] = '\0';
		sprintf(text_km.chars, "%s", line);
		text_km.nchars = strlen(text_km.chars);
	}

	fseek(km_fp, 0, SEEK_SET);
	fflush(km_fp);

	free(line);
}

int main(int argc, char **argv) {
	printf("xmonitor started\n");

	signal(SIGINT, XmonitorSignal);
	signal(SIGTERM, XmonitorSignal);
	signal(SIGQUIT, XmonitorSignal);

	tp = t0 = time(NULL);

	temp_fp = fopen(temp_file, "r");
	if (temp_fp == NULL)
		printf("xmonitor error: Cannot open %s: %s\n", temp_file, strerror(errno));

	km_fp = fopen(km_file, "r");
	if (km_fp == NULL)
		printf("xmonitor error: Cannot open %s: %s\n", km_file, strerror(errno));

	display = XOpenDisplay(NULL);
	if (display == NULL) { printf("xmonitor error: Cannot open X display!\n");
		return 1;
	}

	window = XDefaultRootWindow(display);

	XGCValues gcv;
	gcv.foreground = 0xa0a8af;
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

	text_km_len = 4;
	text_km.chars = malloc(text_km_len);
	sprintf(text_km.chars, "  ");
	text_km.nchars = strlen(text_km.chars);
	text_km.delta = 0;

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

