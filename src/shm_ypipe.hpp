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

#ifndef __ZMQ_SHM_YPIPE_HPP_INCLUDED__
#define __ZMQ_SHM_YPIPE_HPP_INCLUDED__

#include "shm_utils.hpp"
#include "atomic_ptr.hpp"
#include "platform.hpp"
#include "ypipe_base.hpp"
#include "shm_yqueue.hpp"
#include <iostream>

namespace zmq
{
    //  Lock-free, shared-memory queue implementation.
    //  Only a single thread can read from the pipe at any specific moment.
    //  Only a single thread can write to the pipe at any specific moment.
    //  T is the type of the object in the queue.
    //  N is granularity of the pipe, i.e. how many items are needed to
    //  perform next memory allocation.
    template <typename T, int N> class shm_ypipe_t : public ypipe_base_t <T>
    {
    public:
        //  Creates the queue.
        inline shm_ypipe_t (std::string name_, unsigned int offset = 0)
        {
            unsigned int size = get_ypipe_size ();

            name = name_;
            void *mem = shm_map (name, size);
            mem = (void *)((char *)mem + offset);

            //prepare_shm_ring (mem, size);

			queue =	new (std::nothrow) shm_yqueue_t <T, N> (mem);
			alloc_assert (queue);
			ctrl = (struct ctrl_block_t *)mem;
        }

        //  The destructor doesn't have to be virtual. It is mad virtual
        //  just to keep ICC and code checking tools from complaining.
        inline virtual ~shm_ypipe_t ()
        {
            //  FIXME
            //shm_unmap((void *)ctrl);
        }

        //  Following function (write) deliberately copies uninitialised data
        //  when used with zmq_msg. Initialising the VSM body for
        //  non-VSM messages won't be good for performance.

#ifdef ZMQ_HAVE_OPENVMS
#pragma message save
#pragma message disable(UNINIT)
#endif
        inline std::string get_name ()
        {
            return name;
        }

        //  Write an item to the pipe.  Don't flush it yet. If incomplete is
        //  set to true the item is assumed to be continued by items
        //  subsequently written to the pipe. Incomplete items are never
        //  flushed down the stream.
        inline void write (const T &value_, bool incomplete_)
        {
            //  Place the value to the queue, add new terminator element.
            queue->back () = value_;
            queue->push ();

			if (!incomplete_)
				queue->flush ();
        }

#ifdef ZMQ_HAVE_OPENVMS
#pragma message restore
#endif

        //  Pop an incomplete item from the pipe. Returns true is such
        //  item exists, false otherwise.
        inline bool unwrite (T *value_)
        {
			// FIXME
			if (queue->unpush ())
				return false;

			*value_ =  queue->back ();
			return true;
        }

        //  Flush all the completed items into the pipe. Returns false if
        //  the reader thread is sleeping or otherwise needs to be signalled
        //  to take notice of the pipe. In that case, caller is obliged to
        //  wake the reader up before using the pipe again.
        inline bool flush ()
        {
			queue->flush ();

            if (ctrl->must_signal) {
                ctrl->must_signal = false;
                return false;
            }

			return true;
        }

        //  Check whether item is available for reading.
        inline bool check_read ()
        {
			return queue->check_pop ();

        }

        //  Reads an item from the pipe. Returns false if there is no value.
        //  available.
        inline bool read (T *value_)
        {
			if (!queue->check_pop ())
                ctrl->must_signal = true;
				return false;

            queue->pop ();
            *value_ = queue->front ();

            return true;
        }

        //  Applies the function fn to the first elemenent in the pipe
        //  and returns the value returned by the fn.
        //  The pipe mustn't be empty or the function crashes.
        inline bool probe (bool (*fn)(const T &))
        {
			bool rc = check_read ();
			zmq_assert (rc);

			return (*fn) (queue->peek ());
        }

		inline void mark_inactive ()
		{
			// Work your magic
            return;
		}

		inline void mark_active ()
		{
			// Work your magic
            return;
		}

    protected:
        // The shared memory name that this pipe corresponds to.
        std::string name;

        // A control block where the must_signal flag is stored
        struct ctrl_block_t *ctrl;

        //  Allocation-efficient queue to store pipe items.
        //  Front of the queue points to the first prefetched item, back of
        //  the pipe points to last un-flushed item. Front is used only by
        //  reader thread, while back is used only by writer thread.
        shm_yqueue_t <T, N> *queue;

        //  Disable copying of shm_ypipe object.
        shm_ypipe_t (const shm_ypipe_t&);
        const shm_ypipe_t &operator = (const shm_ypipe_t&);
    };

}

#endif
