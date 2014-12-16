#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/select.h>

#include "timer-wheel.h"

#define COUNT (1 << 20)

#ifdef DEBUG
pthread_spinlock_t debug_lock;
unsigned long incoming = 0;
unsigned long dispatched = 0;

typedef struct expire_ctx {
        unsigned long expiry;
        unsigned long insert_time;
} expire_ctx_t;

expire_ctx_t ectx[COUNT];
#endif

void
func (struct timer_list *timer, void *data, unsigned long call_time)
{
#ifdef DEBUG
        expire_ctx_t *ctx = data;
        struct timeval tv;
        pthread_spin_lock (&debug_lock);
        {
                ++incoming;
                /* printf ("expiry: %lu (%lu), expired: %lu (%lu)\n",
                        ctx->expiry, ctx->insert_time,
                        (call_time- ctx->insert_time), call_time); */

        }
        pthread_spin_unlock (&debug_lock);

        free (timer);
#endif
}

int
internal_add_timer (int j, unsigned long expiry)
{
        int ret = 0;
        struct timer_list *timer;
#ifdef DEBUG
        struct timeval tv;
#endif

        timer = malloc (sizeof (*timer));
        if (!timer)
                return -1;

        INIT_LIST_HEAD (&timer->entry);

        timer->function = func;
        timer->data = NULL;

#ifdef DEBUG
        expire_ctx_t *ctx = &ectx[j];
        (void) gettimeofday (&tv, 0);
        ctx->expiry = expiry;
        ctx->insert_time = tv.tv_sec;

        timer->data = (void *) ctx;
#endif
        timer->expires = expiry;

        add_timer (timer);
        return 0;
}

void *
thread_proc (void *arg)
{
        while(1) {
                run_timers ();
                sleep (1);
        }
}

#ifdef DEBUG
void *monitor_proc (void *arg)
{
        while (1) {

                sleep (2);

                pthread_spin_lock (&debug_lock);
                {
                        printf ("%lu/%lu done\n", incoming, dispatched);
                        
                }
                pthread_spin_unlock (&debug_lock);
        }
}
#endif

int
main (int argc, char **argv)
{
        int j;
        int ret;
        pthread_t thread;

#ifdef DEBUG
        ret = pthread_spin_init (&debug_lock, 0);
        if (ret)
                exit (-1);
#endif

        ret = init_timers ();
        if (ret) {
                printf ("timer initialization failed!\n");
                exit (-1);
        }

        printf ("Successfully initialized timer wheel\n");

#ifdef DEBUG
        pthread_t monitor;
        ret = pthread_create (&monitor, NULL, monitor_proc, NULL);
        if (ret != 0)
                return -1;
#endif

        ret = pthread_create (&thread, NULL, thread_proc, NULL);
        if (ret != 0)
                return -1;

        unsigned int seed;
        FILE *urandom = fopen ("/dev/urandom", "r");

        fread (&seed, sizeof(int), 1, urandom);
        srand(seed);

        for (j = 0; j < COUNT; j++) {
                usleep (3000);
                (void) internal_add_timer (j, rand() % 50);
                ++dispatched;
        }

        fclose (urandom);

        /*
        internal_add_timer (0, 4);
        internal_add_timer (1, 3);
        internal_add_timer (2, 10);
        internal_add_timer (3, 1);
        internal_add_timer (4, 23);
        internal_add_timer (5, 33);
        internal_add_timer (6, 2);
        internal_add_timer (7, 3);
        internal_add_timer (8, 1);
        internal_add_timer (9, 11);
        */


        select (0, NULL, NULL, NULL, NULL);

        exit (0);
}
