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

#ifndef __ZMQ_SHM_IPC_CONNECTION_HPP_INCLUDED__
#define __ZMQ_SHM_IPC_CONNECTION_HPP_INCLUDED__

#include <string>

#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#include <stddef.h>

#include "stdint.hpp"
#include "platform.hpp"
#include "poller.hpp"
#include "i_poll_events.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/eventfd.h>

#define HS_INCLUDE_CONTROL_DATA 1

namespace zmq
{
	class shm_ipc_connection_t : public i_poll_events
	{
		public:

			shm_ipc_connection_t (fd_t fd_, const std::string addr_);
			shm_ipc_connection_t (fd_t fd_);
			~shm_ipc_connection_t ();

		protected:
			/* The connection type, CONNECTER or LISTENER */
			enum shm_conn_t {SHM_IPC_CONNECTER, SHM_IPC_LISTENER};

			/* The connection current state */
			enum shm_conn_state_t {
				SHM_IPC_STATE_EXPECT_SYN,
				SHM_IPC_STATE_EXPECT_SYNACK,
				SHM_IPC_STATE_EXPECT_ACK,
				SHM_IPC_STATE_OPEN,
				SHM_IPC_STATE_FAILED
			};

			enum hs_msg_type {
				HS_MSG_SYN,
				HS_MSG_SYNACK,
				HS_MSG_ACK
			};

			struct hs_message {
				hs_msg_type phase;
				int fd;
				char conn_path[5];
			};

			/* The file descriptor of the remote end */
			fd_t remote_evfd;

			/* Our local file descriptor */
			fd_t local_evfd;

			/* Our local unix domain socket */
			fd_t local_sockfd;

			/* Reserved for the shared memory stuff */
			std::string remote_addr;

			/* The connection type */
			shm_conn_t conn_type;

			/* The state of the connection */
			shm_conn_state_t conn_state;

			/* Entry function for creating a connection */
			int create_connection();

			// Methods to handle handshake messages
			void in_event ();
			void out_event ();
			void timer_event (int id_);

		private:

			void alloc_conn();
			void init_conn();

			void set_control_data(struct msghdr *msg, int fd);
			int get_control_data(struct msghdr *msgh);
			void free_dgram_msg(struct msghdr *msg);
			struct msghdr *alloc_dgram_msg(int flag);
			int receive_dgram_msg(int fd, struct msghdr *msg);
			int send_dgram_msg(int fd, struct msghdr *msg);
			int add_empty_hs_msg(struct msghdr *msg);
			struct hs_message *__get_hs_msg(struct msghdr *msg);
			void __set_hs_msg(struct msghdr *msg, struct hs_message *hs_msg);
			struct hs_message *extract_hs_msg(struct msghdr *msg);
			struct hs_message *receive_hs_msg(int flag = 0);
			void free_hs_msg(struct hs_message *hs_msg);
			struct msghdr *__create_msg(int flag = 0);
			struct msghdr *create_syn_msg();
			struct msghdr *create_synack_msg();
			struct msghdr *create_ack_msg();
			int send_syn_msg();
			int send_synack_msg();
			int send_ack_msg();
			int connect_syn();
			int connect_synack();
			int connect_ack();
			int handle_syn_msg();
			int handle_synack_msg();
			int handle_ack_msg();

			const unsigned MAX_RING_NAME = 256;
			char ring_name[MAX_RING_NAME];
	};
}

#endif

#endif
