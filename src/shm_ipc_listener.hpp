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

#ifndef __ZMQ_SHM_IPC_LISTENER_HPP_INCLUDED__
#define __ZMQ_SHM_IPC_LISTENER_HPP_INCLUDED__

#include "platform.hpp"

#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#include <string>

#include "fd.hpp"
#include "own.hpp"
#include "stdint.hpp"
#include "io_object.hpp"

namespace zmq
{

    class io_thread_t;
    class socket_base_t;

    class shm_ipc_listener_t :  public own_t, public io_object_t
    {
    public:

        shm_ipc_listener_t (zmq::io_thread_t *io_thread_,
				zmq::socket_base_t *socket_, const options_t &options_);
        ~shm_ipc_listener_t ();

        //  Set address to listen on.
        int set_address (const char *addr_);

        // Get the bound address for use with wildcards
        int get_address (std::string &addr_);

    private:

        //  Handlers for incoming commands.
        void process_plug ();
        void process_term (int linger_);

        //  Handlers for I/O events.
        void in_event ();

        //  Close the listening socket.
        int close ();

        //  Filter new connections if the OS provides a mechanism to get
        //  the credentials of the peer process.  Called from accept().
#       if defined ZMQ_HAVE_SO_PEERCRED || defined ZMQ_HAVE_LOCAL_PEERCRED
        bool filter (fd_t sock);
#       endif

        //  Accept the new connection. Returns the file descriptor of the
        //  newly created connection. The function may return retired_fd
        //  if the connection was dropped while waiting in the listen backlog.
        fd_t accept ();

        //  True, if the undelying file for UNIX domain socket exists.
        bool has_file;

        //  Name of the file associated with the UNIX domain address.
        std::string filename;

        //  Underlying socket.
        fd_t s;

        //  Handle corresponding to the listening socket.
        handle_t handle;

        //  Socket the listerner belongs to.
        zmq::socket_base_t *socket;

		// Entry function for creating an shm connection
		int create_connection (fd_t fd_);

       // String representation of endpoint to bind to
        std::string endpoint;

        shm_ipc_listener_t (const shm_ipc_listener_t&);
        const shm_ipc_listener_t &operator = (const shm_ipc_listener_t&);
    };

}

#endif

#endif

