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

#ifndef __ZMQ_SHM_IPC_ADDRESS_HPP_INCLUDED__
#define __ZMQ_SHM_IPC_ADDRESS_HPP_INCLUDED__

#include <string>

#include "platform.hpp"

#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#include <sys/socket.h>
#include <sys/un.h>

namespace zmq
{
	class shm_ipc_ring_t
	{
		public:

			shm_ipc_ring_t (const char *ring_name_);
			~shm_ipc_ring_t ();

			/* Name of the ring */
			const char ring_name[100];

			/* Pointers to shared part */

			/* Shared part */
	}
}

#endif

#endif


