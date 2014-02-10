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

			/* The file descriptor of the remote end */
			fd_t remote_evfd;

			/* Our local file descriptor */
			fd_t local_evfd;

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

		private:

			void set_control_data(struct msghdr *msg, int fd);
			int get_control_data(struct msghdr *msgh);
			void free_dgram_msg(struct msghdr *msg);
			struct msghdr *alloc_dgram_msg(int flag);
			int receive_dgram_msg(int fd, struct msghdr *msg);
			int send_dgram_msg(int fd, struct msghdr *msg);
			int add_empty_rptl_msg(struct msghdr *msg);
			struct rptl_message *__get_rptl_msg(struct msghdr *msg);
			void __set_rptl_msg(struct msghdr *msg, struct rptl_message *rptl_msg);
			struct rptl_message *extract_rptl_msg(struct msghdr *msg);
			struct rptl_message *receive_rptl_msg(struct rptl_connection *conn,
				int flag);
			void free_rptl_msg(struct rptl_message *rptl_msg);
			struct msghdr *__create_msg(struct rptl_connection *conn, int flag);
			struct msghdr *create_syn_msg(struct rptl_connection *conn);
			struct msghdr *create_synack_msg(struct rptl_connection *conn);
			struct msghdr *create_ack_msg(struct rptl_connection *conn);
			int send_syn_msg(struct rptl_connection *conn);
			int send_synack_msg(struct rptl_connection *conn);
			int send_ack_msg(struct rptl_connection *conn);
			int connect_syn(struct rptl_connection *conn, char *dest);
			int connect_synack(struct rptl_connection *conn);
			int connect_ack(struct rptl_connection *conn);
			int handle_syn_msg(struct rptl_connection *conn);
			int handle_synack_msg(struct rptl_connection *conn);
			int handle_ack_msg(struct rptl_connection *conn);
	};
}

#endif

#endif
