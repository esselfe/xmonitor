#ifndef XMONITOR_H
#define XMONITOR_H 1

#define _NET_WM_STATE_REMOVE        0	// remove/unset property
#define _NET_WM_STATE_ADD           1	// add/set property
#define _NET_WM_STATE_TOGGLE        2	// toggle property

struct history_frame {
	struct history_frame *prev, *next;
	unsigned long used, buffered;
};

struct history {
	struct history_frame *first_frame, *last_frame;
};

extern struct history mem_history, swap_history;

void HistoryInit(struct history *hist, unsigned int size);
void HistoryAdd(struct history *hist, unsigned long used, unsigned long buffered);
void HistoryPopFirst(struct history *hist);

#endif /* XMONITOR_H */
