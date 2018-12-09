#ifndef __M_TIMER_H__
#define __M_TIMER_H__

#include "global.h"
#include <signal.h>
#include <time.h>

/* Referred to:
 * http://www.cplusplus.com/reference/csignal/signal/
 * http://man7.org/linux/man-pages/man2/timer_create.2.html
 */

extern sig_atomic_t timer_expired;

void timer_handler(int sig, siginfo_t *si, void *uc);

class m_timer {
private:
    static timer_t timerid;
    static struct itimerspec its;

public:
    static void m_timer_init();

    static void m_timer_start();

    static void m_timer_pause();

    static void m_timer_restart();
};

#endif
