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

#ifndef __ZMQ_SHM_UTILS_HPP_INCLUDED__
#define __ZMQ_SHM_UTILS_HPP_INCLUDED__

#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#define SHM_PATH "/dev/shm/zeromq/"
#define SHM_PATH_LEN 64

#include <string>
#include <stdint.h>

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

    unsigned int get_ypipe_size ();
    void prepare_shm_ring (void *mem, unsigned int size);
    void prepare_shm_cpipe (void *mem, unsigned int size);

    // Shm* high-level functions
    void shm_mkdir (const std::string &name);
    void *shm_map (std::string &name, unsigned int size);
    int shm_allocate (std::string &name, unsigned int size);

    std::string shm_generate_random_name (const std::string &prefix);
    void __prepare_shm_pipe (void *mem);
    void prepare_shm_ring (void *mem);
    void prepare_shm_cpipe (void *mem);
}

#endif
#endif
