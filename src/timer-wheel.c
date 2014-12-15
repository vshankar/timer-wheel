#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "timer-wheel.h"

struct tvec_base *base;

int
init_timers()
{
        int j;
        int ret;
        struct timeval tv;

        base = malloc (sizeof (*base));
        if (!base)
                goto error_return;

        ret = pthread_spin_init (&base->lock, 0);
        if (ret != 0)
                goto error_dealloc;

        for (j = 0; j < TVN_SIZE; j++) {
                INIT_LIST_HEAD (base->tv5.vec + j);
                INIT_LIST_HEAD (base->tv4.vec + j);
                INIT_LIST_HEAD (base->tv3.vec + j);
                INIT_LIST_HEAD (base->tv2.vec + j);
        }

        for (j = 0; j < TVR_SIZE; j++) {
                INIT_LIST_HEAD (base->tv1.vec + j);
        }

        ret = gettimeofday (&tv, 0);
        if (ret < 0)
                goto error_clean_lock;

        base->timer_sec = tv.tv_sec;

#ifdef DEBUG
        printf ("timer_sec: %lu\n", base->timer_sec);
#endif

        return 0;

 error_clean_lock:
        (void) pthread_spin_destroy (&base->lock);
 error_dealloc:
        free (base);
 error_return:
        return -1;
}

void
__add_timer (struct tvec_base *base, struct timer_list *timer)
{
        int i;
        unsigned long idx;
        unsigned long expires;
        struct list_head *vec;

        expires = timer->expires;

        idx = expires - base->timer_sec;

        if (idx < TVR_SIZE) {
                i = expires & TVR_MASK;
                vec = base->tv1.vec + i;
        } else if (idx < 1 << (TVR_BITS + TVN_BITS)) {
                i = (expires >> TVR_BITS) & TVN_MASK;
                vec = base->tv2.vec + i;
        } else if (idx < 1 << (TVR_BITS + 2*TVN_BITS)) {
                i = (expires >> (TVR_BITS + TVN_BITS)) & TVN_MASK;
                vec = base->tv3.vec + i;
        } else if (idx < 1 << (TVR_BITS + 3*TVN_BITS)) {
                i = (expires >> (TVR_BITS + 2*TVN_BITS)) & TVN_MASK;
                vec = base->tv4.vec + i;
        } else if (idx < 0) {
                vec = base->tv1.vec + (base->timer_sec & TVR_MASK);
        } else {
                i = (expires >> (TVR_BITS + 3*TVN_BITS)) & TVN_MASK;
                vec = base->tv5.vec + i;
        }

        list_add_tail (&timer->entry, vec);
}

void
add_timer (struct timer_list *timer)
{
        pthread_spin_lock (&base->lock);
        {
                timer->expires += base->timer_sec;
                __add_timer (base, timer);
        }
        pthread_spin_unlock (&base->lock);
}

void
detach_timer (struct timer_list *timer)
{
        struct list_head *entry = &timer->entry;

        list_del (entry);
}

int
cascade (struct tvec_base *base, struct tvec *tv, int index)
{
        struct timer_list *timer, *tmp;
        struct list_head tv_list;

        list_replace_init (tv->vec + index, &tv_list);

        list_for_each_entry_safe (timer, tmp, &tv_list, entry) {
                __add_timer (base, timer);
        }

        return index;
}

#define INDEX(N)  ((base->timer_sec >> (TVR_BITS + N * TVN_BITS)) & TVN_MASK)

void
run_timers ()
{
        unsigned long index;
        struct timer_list *timer;

        struct list_head work_list;
        struct list_head *head = &work_list;

        pthread_spin_lock (&base->lock);
        {
                index  = base->timer_sec & TVR_MASK;

                if (!index &&
                    (!cascade (base, &base->tv2, INDEX(0))) &&
                    (!cascade (base, &base->tv3, INDEX(1))) &&
                    (!cascade (base, &base->tv4, INDEX(2))))
                        cascade (base, &base->tv5, INDEX(3));

                ++base->timer_sec;
                list_replace_init (base->tv1.vec + index, head);
                while (!list_empty(head)) {
                        void (*fn)(unsigned long);
                        unsigned long data;

                        timer = list_first_entry (head, struct timer_list, entry);
                        fn = timer->function;
                        data = timer->data;

                        detach_timer (timer);
                        fn (data);
                }
        }
        pthread_spin_unlock (&base->lock);

}
