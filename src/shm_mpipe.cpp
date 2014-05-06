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

#include "shm_utils.hpp"
#include "shm_mpipe.hpp"
#include "err.hpp"
#include <iostream>

zmq::shm_mpipe_t::shm_mpipe_t ()
{
    //  Get the pipe into passive state. That way, if the users starts by
    //  polling on the associated file descriptor it will get woken up when
    //  new command is posted.
    bool ok = shm_cpipe.read (NULL);
    zmq_assert (!ok);
    active = false;
}

zmq::shm_mpipe_t::~shm_mpipe_t ()
{
    //  TODO: Retrieve and deallocate commands inside the shm_cpipe.

    // Work around problem that other threads might still be in our
    // send() method, by waiting on the mutex before disappearing.
    //sync.lock ();
    //sync.unlock ();
}

zmq::fd_t zmq::shm_mpipe_t::get_fd ()
{
    return signaler.get_fd ();
}

// Translate sync to something with shm semantics
void zmq::shm_mpipe_t::send (const command_t &cmd_)
{
	std::cout << "Send a command\n";
    shm_cpipe.write (cmd_, false);
    bool ok = shm_cpipe.flush ();
    //sync.unlock ();
    if (!ok) {
		std::cout << "Must signal\n";
        signaler.send ();
	} else {
		std::cout << "No need to signal\n";
	}
}

int zmq::shm_mpipe_t::recv (command_t *cmd_, int timeout_)
{
    zmq_assert (false);
}

shm_cpipe_t *zmq::shm_alloc_cpipe (std::string name)
{
    return new (std::nothrow) shm_cpipe_t (name);
}

shm_cpipe_t *zmq::shm_create_cpipe (std::string pipe_name)
{
    int r;

    shm_mkdir ("zeromq");
    unsigned int size = get_cpipe_size ();

    // If allocation fails due to a duplicate name, retry.
    // Note that this is uncommon, but we must handle it anyway.
    if (!pipe_name) {
        do {
            pipe_name = shm_generate_random_name ("cpipe");
            r = shm_allocate (pipe_name, size);
        } while (r < 0);
    }

    return shm_alloc_cpipe (pipe_name);
}


