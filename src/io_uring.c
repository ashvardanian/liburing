#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "io_uring.h"
#include "liburing.h"
#include "barrier.h"

/*
 * Return an IO completion, waiting for it it necessary.
 */
int io_uring_get_completion(int fd, struct io_uring_cq *cq,
			    struct io_uring_event **ev_ptr)
{
	const unsigned mask = *cq->kring_mask;
	struct io_uring_event *ev = NULL;
	unsigned head;
	int ret;

	head = *cq->khead;
	do {
		read_barrier();
		if (head != *cq->ktail) {
			ev = &cq->events[head & mask];
			break;
		}
		ret = io_uring_enter(fd, 0, 1, IORING_ENTER_GETEVENTS);
		if (ret < 0)
			return -errno;
	} while (1);

	if (ev) {
		*cq->khead = head + 1;
		write_barrier();
	}

	*ev_ptr = ev;
	return 0;
}

/*
 * Submit iocbs acquired from io_uring_get_iocb() to the kernel.
 *
 * Returns number of iocbs submitted
 */
int io_uring_submit(int fd, struct io_uring_sq *sq)
{
	const unsigned mask = *sq->kring_mask;
	unsigned ktail, ktail_next, submitted;

	/*
	 * If we have pending IO in the kring, submit it first
	 */
	read_barrier();
	if (*sq->khead != *sq->ktail) {
		submitted = *sq->kring_entries;
		goto submit;
	}

	if (sq->iocb_head == sq->iocb_tail)
		return 0;

	/*
	 * Fill in iocbs that we have queued up, adding them to the kernel ring
	 */
	submitted = 0;
	ktail = ktail_next = *sq->ktail;
	while (sq->iocb_head < sq->iocb_tail) {
		ktail_next++;
		read_barrier();
		if (ktail_next == *sq->khead)
			break;

		sq->array[ktail & mask] = sq->iocb_head & mask;
		ktail = ktail_next;

		sq->iocb_head++;
		submitted++;
	}

	if (!submitted)
		return 0;

	if (*sq->ktail != ktail) {
		write_barrier();
		*sq->ktail = ktail;
		write_barrier();
	}

submit:
	return io_uring_enter(fd, submitted, 0, IORING_ENTER_GETEVENTS);
}

/*
 * Return an iocb to fill. Application must later call io_uring_submit()
 * when it's ready to tell the kernel about it. The caller may call this
 * function multiple times before calling io_uring_submit().
 *
 * Returns a vacant iocb, or NULL if we're full.
 */
struct io_uring_iocb *io_uring_get_iocb(struct io_uring_sq *sq)
{
	unsigned next = sq->iocb_tail + 1;
	struct io_uring_iocb *iocb;

	/*
	 * All iocbs are used
	 */
	if (next - sq->iocb_head > *sq->kring_entries)
		return NULL;

	iocb = &sq->iocbs[sq->iocb_tail & *sq->kring_mask];
	sq->iocb_tail = next;
	return iocb;
}

static int io_uring_mmap(int fd, struct io_uring_params *p,
			 struct io_uring_sq *sq, struct io_uring_cq *cq)
{
	size_t size;
	void *ptr;
	int ret;

	sq->ring_sz = p->sq_off.array + p->sq_entries * sizeof(unsigned);
	ptr = mmap(0, sq->ring_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_SQ_RING);
	if (ptr == MAP_FAILED)
		return -errno;
	sq->khead = ptr + p->sq_off.head;
	sq->ktail = ptr + p->sq_off.tail;
	sq->kring_mask = ptr + p->sq_off.ring_mask;
	sq->kring_entries = ptr + p->sq_off.ring_entries;
	sq->kflags = ptr + p->sq_off.flags;
	sq->kdropped = ptr + p->sq_off.dropped;
	sq->array = ptr + p->sq_off.array;

	size = p->sq_entries * sizeof(struct io_uring_iocb);
	sq->iocbs = mmap(0, size, PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_POPULATE, fd,
				IORING_OFF_IOCB);
	if (sq->iocbs == MAP_FAILED) {
		ret = -errno;
err:
		munmap(sq->khead, sq->ring_sz);
		return ret;
	}

	cq->ring_sz = p->cq_off.events + p->cq_entries * sizeof(struct io_uring_event);
	ptr = mmap(0, cq->ring_sz, PROT_READ | PROT_WRITE,
			MAP_SHARED | MAP_POPULATE, fd, IORING_OFF_CQ_RING);
	if (ptr == MAP_FAILED) {
		ret = -errno;
		munmap(sq->iocbs, p->sq_entries * sizeof(struct io_uring_iocb));
		goto err;
	}
	cq->khead = ptr + p->cq_off.head;
	cq->ktail = ptr + p->cq_off.tail;
	cq->kring_mask = ptr + p->cq_off.ring_mask;
	cq->kring_entries = ptr + p->cq_off.ring_entries;
	cq->koverflow = ptr + p->cq_off.overflow;
	cq->events = ptr + p->cq_off.events;
	return fd;
}

/*
 * Returns -1 on error, or an 'fd' on success. On success, 'sq' and 'cq'
 * contain the necessary information to read/write to the rings.
 */
int io_uring_queue_init(unsigned entries, struct io_uring_params *p,
			struct iovec *iovecs, struct io_uring_sq *sq,
			struct io_uring_cq *cq)
{
	int fd;

	fd = io_uring_setup(entries, iovecs, p);
	if (fd < 0)
		return fd;

	memset(sq, 0, sizeof(*sq));
	memset(cq, 0, sizeof(*cq));
	return io_uring_mmap(fd, p, sq, cq);
}

void io_uring_queue_exit(int fd, struct io_uring_sq *sq, struct io_uring_cq *cq)
{
	munmap(sq->iocbs, *sq->kring_entries * sizeof(struct io_uring_iocb));
	munmap(sq->khead, sq->ring_sz);
	munmap(cq->khead, cq->ring_sz);
	close(fd);
}
