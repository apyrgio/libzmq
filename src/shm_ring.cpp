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


#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#include "shm_ring.hpp"
#include "shm_utils.cpp"

unsigned int zmq::get_ring_size ()
{
    return 2 * get_ypipe_size ();
}


pipe_t *zmq::shm_create_ring (options_t *options, std::string ring_name,
        shm_pipe_t pipe_type)
{
    int r;

    shm_mkdir ("zeromq");
    unsigned int size = get_ring_size ();

    if (ring_name = "") {
        // If allocation fails due to a duplicate name, retry.
        // Note that this is uncommon, but we must handle it anyway.
        do {
            ring_name = shm_generate_random_name ("ring");
            r = shm_allocate (ring_name, size);
        } while (r < 0);
    }

    return shm_alloc_pipe (options, ring_name, pipe_type);
}

zmq::pipe_t *zmq::shm_alloc_pipe (options_t *options, std::string path,
        shm_pipe_t pipe_type)
{
    bool conflate = options->conflate &&
        (options->type == ZMQ_DEALER ||
         options->type == ZMQ_PULL ||
         options->type == ZMQ_PUSH ||
         options->type == ZMQ_PUB ||
         options->type == ZMQ_SUB);

    pipe_t *pipe;
    int r;

    int hwms[2] = {conflate? -1 : options->rcvhwm,
        conflate? -1 : options->sndhwm};

    r = zmq::shm_pipe (socket, &pipe, hwms, conflate, path, pipe_type);
    zmq_assert (r >= 0);

    return pipe;
}
