/* SPDX-License-Identifier: GPL-3.0-only */
/* Instructions retired by this process, user space only: the host CPU budget of the tests.
 * macOS: proc_pid_rusage. Linux: perf_event_open (perf_event_paranoid <= 2 is enough for
 * one's own process). 0 = no counter here (a VM without PMU, a locked-down kernel): the
 * tests then skip the budget. The counter is opened per process, so forked jobs count their own. */
#ifndef INSTR_H
#define INSTR_H
#include <stdint.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#elif defined(__linux__)
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#endif

static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#elif defined(__linux__)
    static pid_t owner;
    static int fd = -1;
    uint64_t n;
    if (owner != getpid()) {                 /* first call in this process (or a forked child) */
        struct perf_event_attr a;
        if (fd >= 0)
            close(fd);                       /* the parent's counter, inherited by fork */
        memset(&a, 0, sizeof a);
        a.type = PERF_TYPE_HARDWARE;
        a.size = sizeof a;
        a.config = PERF_COUNT_HW_INSTRUCTIONS;
        a.exclude_kernel = 1;
        a.exclude_hv = 1;
        fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
        owner = getpid();
    }
    if (fd >= 0 && read(fd, &n, sizeof n) == (ssize_t)sizeof n)
        return n ? n : 1;
#endif
    return 0;
}
#endif
