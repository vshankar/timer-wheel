#ifndef __TIMER_WHEEL_H
#define __TIMER_WHEEL_H

#include <pthread.h>

#include "list.h"

#define TVR_BITS  8
#define TVN_BITS  6
#define TVR_SIZE  (1 << TVR_BITS)
#define TVN_SIZE  (1 << TVN_BITS)
#define TVR_MASK  (TVR_SIZE - 1)
#define TVN_MASK  (TVN_SIZE - 1)

#define BITS_PER_LONG  64

struct tvec {
        struct list_head vec[TVN_SIZE];
};

struct tvec_root {
        struct list_head vec[TVR_SIZE];
};

struct tvec_base {
        pthread_spinlock_t lock;      /* base lock */

        unsigned long timer_sec;      /* time counter */

        struct tvec_root tv1;
        struct tvec tv2;
        struct tvec tv3;
        struct tvec tv4;
        struct tvec tv5;
};

struct timer_list {
        struct list_head entry;

        unsigned long expires;

        void (*function)(struct timer_list *, void *, unsigned long);
        void *data;
};

int init_timers ();
void add_timer (struct timer_list *);

#endif
