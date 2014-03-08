/*
    Copyright (c) 2007-2014 Contributors as noted in the AUTHORS file

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __ZMQ_SHM_YQUEUE_HPP_INCLUDED__
#define __ZMQ_SHM_YQUEUE_HPP_INCLUDED__

#include <stdlib.h>
#include <stddef.h>

#include "err.hpp"
#include "atomic_ptr.hpp"

namespace zmq
{

	struct ctrl_block_t
	{
		volatile uint64_t initialized;
		volatile uint64_t head;
		volatile uint64_t tail;
		volatile uint64_t unflushed;
        volatile bool must_signal;
	};

    //  yqueue is an efficient queue implementation. The main goal is
    //  to minimise number of allocations/deallocations needed. Thus yqueue
    //  allocates/deallocates elements in batches of N.
    //
    //  yqueue allows one thread to use push/back function and another one 
    //  to use pop/front functions. However, user must ensure that there's no
    //  pop on the empty queue and that both threads don't access the same
    //  element in unsynchronised manner.
    //
    //  T is the type of the object in the queue.
    //  N is granularity of the queue (how many messages it can have inflight)

    template <typename T, int N> class shm_yqueue_t
    {
    public:
        //  Create the queue.
		//  The shared-memory part of the queue is initialized only one time
        inline shm_yqueue_t (void *ptr)
        {
			 ctrl = (struct ctrl_block_t *)ptr;
			 if (!ctrl->initialized) {
				 ctrl->msg_head = ctrl->msg_unflushed = ctrl->buf_head = 0;
				 ctrl->msg_tail = ctrl->buf->tail = 1;
				 ctrl->initialized = 1134;
			 }

			 msg = (T *)((char *)ptr + sizeof(struct ctrl_block_t));
			 buffer = (void *)((char *)msg + N * sizeof(T));

			 std::cout << "Ptr: " << ptr << "\n";
			 std::cout << "Ctrl: " << ctrl << "\n";
			 std::cout << "msg: " << msg << "\n";
			 std::cout << "buffer: " << buffer << "\n";
        }

        //  Destroy the queue.
        inline ~shm_yqueue_t ()
        {
        }

        //  Returns reference to the front element of the queue.
        //  If the queue is empty, behaviour is undefined.
        inline T &front ()
        {
             return msg[ctrl->msg_head];
        }

        //  Returns reference to the back element of the queue.
        //  If the queue is full, behaviour is undefined.
        inline T &back ()
        {
			std::cout << "Tail is " << ctrl->tail << "\n";
            return msg[ctrl->msg_tail];
        }

		// Check if we can push a new element.
		// We can push a new element as long as the new tail does not reach
		// the head of the queue.
        inline bool check_push ()
        {
			unsigned int new_tail = (ctrl->tail + 1) % N;

			return new_tail != ctrl->head;
		}

        inline void push ()
        {
			unsigned int new_tail = (ctrl->tail + 1) % N;

			if (likely(check_push ()))
				ctrl->tail = new_tail;
			else
				zmq_assert(false);
        }

        //  Removes element from the back end of the queue. In other words
        //  it rollbacks last push to the queue. Take care: Caller is
        //  responsible for destroying the object being unpushed.
        //  The caller must also guarantee that the queue isn't empty when
        //  unpush is called. It cannot be done automatically as the read
        //  side of the queue can be managed by different, completely
        //  unsynchronised thread.
        inline bool unpush ()
        {
			//FIXME: we must check if ctrl->tail == ctrl->unflushed
			// Else it might underflow
			if (ctrl->tail == ctrl->unflushed)
				return false;

			if (ctrl->tail == 0)
				ctrl->tail = N - 1;
			else
				ctrl->tail--;

			return true;
		}

		// Check if we can pop a new element.
		// We can pop a new element as long as the new head does not reach
		// the unflushed part of the queue.
        inline bool check_pop ()
        {
			return ctrl->head != ctrl->unflushed;
		}

        inline void pop ()
        {
			unsigned int new_head = (ctrl->head + 1) % N;

			if (likely(check_pop ()))
				ctrl->head = new_head;
			else
				zmq_assert(false);
        }

        inline T &peek ()
        {
			unsigned int new_head = (ctrl->head + 1) % N;

			if (likely(check_pop ()))
				zmq_assert(false);

			return msg[new_head];
        }

        //  Flushes the written msg.
        inline void flush ()
        {
			ctrl->unflushed = ctrl->tail;
		}

    private:

		// A control block where pointers to the head, tail and unflushed
		// counters are stored.
		struct ctrl_block_t *ctrl;

		// A templated array that points to the ring msg
		T *msg;

		// An extra buffer for non-VSM messages
		void *buffer;

        //  Disable copying of yqueue.
        shm_yqueue_t (const shm_yqueue_t&);
        const shm_yqueue_t &operator = (const shm_yqueue_t&);
    };
}

#endif
