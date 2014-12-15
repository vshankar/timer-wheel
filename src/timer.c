#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "timer-wheel.h"

void
func (unsigned long data)
{
        printf ("expires: %lu\n", data);
}

int
internal_add_timer (unsigned long expiry)
{
        int ret = 0;
        struct timer_list *timer;

        timer = malloc (sizeof (*timer));
        if (!timer)
                return -1;

        INIT_LIST_HEAD (&timer->entry);
        timer->function = func;
        timer->data = expiry;

        timer->expires = expiry;

        add_timer (timer);
        return 0;
}

void *
thread_proc (void *arg)
{
        sleep (3);

        while(1) {
                run_timers ();
                sleep (1);
        }
}

int
main (int argc, char **argv)
{
        int j;
        int ret;
        pthread_t thread;

        ret = init_timers ();
        if (ret) {
                printf ("timer initialization failed!\n");
                exit (-1);
        }

        printf ("Successfully initialized timer wheel\n");

        ret = pthread_create (&thread, NULL, thread_proc, NULL);
        if (ret != 0)
                return -1;

        (void) internal_add_timer (5);
        (void) internal_add_timer (10);
        (void) internal_add_timer (11);
        (void) internal_add_timer (15);
        (void) internal_add_timer (30);

        sleep (50);

        /**
        for (j = 0; j < 1<<30; j++) {
                usleep(100);
                srand(time(NULL));
                (void) internal_add_timer (rand() % 10);
                } */

        exit (0);
}
