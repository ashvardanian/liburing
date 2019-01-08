/*
 * Will go away once libc support is there
 */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include "io_uring.h"

#if defined(__x86_64)
#ifndef __NR_sys_io_uring_setup
#define __NR_sys_io_uring_setup	335
#endif
#ifndef __NR_sys_io_uring_enter
#define __NR_sys_io_uring_enter	336
#endif
#else
#error "Arch not supported yet"
#endif

int io_uring_setup(unsigned int entries, struct iovec *iovecs,
		   struct io_uring_params *p)
{
	return syscall(__NR_sys_io_uring_setup, entries, iovecs, p);
}

int io_uring_enter(int fd, unsigned int to_submit, unsigned int min_complete,
		   unsigned int flags)
{
	return syscall(__NR_sys_io_uring_enter, fd, to_submit, min_complete,
			flags);
}
