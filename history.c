#include <stdio.h>
#include <stdlib.h>

#include "xmonitor.h"

void HistoryInit(unsigned int size) {
	struct history_frame *frame = malloc(sizeof(struct history_frame));
	frame->next = NULL;
	frame->prev = NULL;
	frame->used = 0;
	frame->buffered = 0;
	mem_history.first_frame = frame;
	mem_history.last_frame = frame;

	unsigned int cnt;
	for (cnt = 0; cnt < size; cnt++) {
		frame->next = malloc(sizeof(struct history_frame));
		frame->next->prev = frame;
		frame->next->next = NULL;
		frame = frame->next;
		frame->used = 0;
		frame->buffered = 0;
		mem_history.last_frame = frame;
	}
}

void HistoryPopFirst(void) {
	struct history_frame *frame = mem_history.first_frame;
	frame->next->prev = NULL;
	mem_history.first_frame = frame->next;
	free(frame);
}

void HistoryAdd(unsigned int used, unsigned int buffered) {
	struct history_frame *frame = malloc(sizeof(struct history_frame));
	frame->prev = mem_history.last_frame;
	frame->next = NULL;
	frame->used = used;
	frame->buffered = buffered;
	mem_history.last_frame->next = frame;
	mem_history.last_frame = frame;
}

