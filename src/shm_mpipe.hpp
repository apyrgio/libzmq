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

#ifndef __ZMQ_SHM_MPIPE_HPP_INCLUDED__
#define __ZMQ_SHM_MPIPE_HPP_INCLUDED__

#include <stddef.h>

#include "platform.hpp"
#include "signaler.hpp"
#include "fd.hpp"
#include "config.hpp"
#include "command.hpp"
#include "shm_ypipe.hpp"
#include "mutex.hpp"
#include "shm_utils.hpp"

namespace zmq
{
    // The definition for shared memory command pipes
    typedef shm_ypipe_t <command_t, command_pipe_granularity> shm_cpipe_t;

    class shm_mpipe_t
    {
    public:

        shm_mpipe_t ();
        ~shm_mpipe_t ();

        fd_t get_fd ();
        void send (const command_t &cmd_);
        int recv (command_t *cmd_, int timeout_);

        typedef shm_ypipe_t <command_t, command_pipe_granularity> shm_cpipe_t;
        shm_cpipe_t *get_shm_cpipe ();
        void set_shm_cpipe (shm_cpipe_t *shm_cpipe);

#ifdef HAVE_FORK
        // close the file descriptors in the signaller. This is used in a forked
        // child process to close the file descriptors so that they do not interfere
        // with the context in the parent process.
        void forked() { signaler.forked(); }
#endif

    private:

        //  The pipe to store actual commands.
        shm_cpipe_t *shm_cpipe;

        //  Signaler to pass signals from writer thread to reader thread.
        signaler_t signaler;

        //  There's only one thread receiving from the shm_mpipe, but there
        //  is arbitrary number of threads sending. Given that ypipe requires
        //  synchronised access on both of its endpoints, we have to synchronise
        //  the sending side.
        mutex_t sync;

        //  True if the underlying pipe is active, ie. when we are allowed to
        //  read commands from it.
        bool active;

        //  Disable copying of shm_mpipe_t object.
        shm_mpipe_t (const shm_mpipe_t&);
        const shm_mpipe_t &operator = (const shm_mpipe_t&);
    };

}

#endif
