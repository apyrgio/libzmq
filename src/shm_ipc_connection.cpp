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

#include "shm_ipc_connecter.hpp"

#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#include <new>
#include <string>

#include "stream_engine.hpp"
#include "io_thread.hpp"
#include "platform.hpp"
#include "random.hpp"
#include "err.hpp"
#include "ip.hpp"
#include "address.hpp"
#include "shm_ipc_address.hpp"
#include "shm_ipc_connection.hpp"
#include "shm_ipc_ring.hpp"
#include "session_base.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>


zmq::shm_ipc_connection_t::shm_ipc_connection_t (class socket_base_t *socket) :
	shm_ipc_ring_t(socket)
{
	std::cout<<"Constructing the connection_t\n";
}

zmq::shm_ipc_connection_t::~shm_ipc_connection_t ()
{
}

int zmq::shm_ipc_connecter_t::connect_syn ()
{
	int r = ::write(s, "lalalala", 9);
	return r;
}

int zmq::shm_ipc_connection_t::alloc_conn ()
{
	std::cout << "Alloc_conn: placeholder\n";
	return 0;
}

int zmq::shm_ipc_connection_t::init_conn ()
{
	std::cout << "Init_conn: placeholder\n";
	return 0;
}

int zmq::shm_ipc_connection_t::map_conn ()
{
	std::cout << "Map_conn: placeholder\n";
	return 0;
}

int zmq::shm_ipc_connection_t::create_connection ()
{
	alloc_conn();
	init_conn();
	map_conn();

	/*
	 * At this point, we should have an in-memory queue which we can use to
	 * send stuff.
	 */
	return 0;
}

#endif
