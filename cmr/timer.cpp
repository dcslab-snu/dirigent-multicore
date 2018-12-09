#include "timer.h"

timer_t m_timer::timerid;
struct itimerspec m_timer::its;

void timer_handler(int sig, siginfo_t *si, void *uc) {
    timer_expired = 1;
}

void m_timer::m_timer_init() {
    struct sigaction sa;
    struct sigevent sev;

    /* Establish handler for timer signal */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGRTMIN, &sa, NULL) == -1)
        errExit("sigaction");

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
        errExit("timer_create");

    /* Start the timer */
    its.it_value.tv_sec = SAMPLE_DURATION / 1000000000;
    its.it_value.tv_nsec = SAMPLE_DURATION % 1000000000;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
}

void m_timer::m_timer_start() {
    if (timer_settime(timerid, 0, &its, NULL) == -1)
        errExit("timer_settime");
}

void m_timer::m_timer_pause() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1)
        errExit("sigprocmask");
}

void m_timer::m_timer_restart() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGRTMIN);
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
        errExit("sigprocmask");
}

