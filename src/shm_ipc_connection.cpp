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
#include "session_base.hpp"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>


zmq::shm_ipc_connection_t::shm_ipc_connection_t (fd_t fd, std::string *addr_) :
	remote_addr (addr_),
	local_sockfd (fd),
	conn_type (SHM_IPC_CONNECTER)
{
	std::cout<<"Constructing the connection for connecter\n";
	create_connection ();
	connect_syn ()
}

zmq::shm_ipc_connection_t::shm_ipc_connection_t (fd_t fd) :
	local_sockfd (fd),
	conn_type = SHM_IPC_LISTENER
{
	std::cout<<"Constructing the connection for listener\n";
}

zmq::shm_ipc_connection_t::~shm_ipc_connection_t ()
{
}

void zmq::shm_ipc_connection_t::in_event ()
{
	std::cout << "In in_event of connection\n";
	zmq_assert(false);
}

void zmq::shm_ipc_connection_t::out_event ()
{
	zmq_assert(false);
}

int zmq::shm_ipc_connection_t::create_connection ()
{
	alloc_conn();
	init_conn();

	/*
	 * At this point, we should have an in-memory queue which we can use to
	 * send stuff.
	 */
	return 0;
}

void zmq::shm_ipc_connection_t::set_control_data(struct msghdr *msg, int fd)
{
	struct cmsghdr *cmsg;
	int *fdptr;

	cmsg = CMSG_FIRSTHDR(msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(fd));

	/* Initialize the payload */
	fdptr = (int *)CMSG_DATA(cmsg);
	*fdptr = fd;

	/* Sum of the length of all control messages in the buffer */
	msg->msg_controllen = cmsg->cmsg_len;
}

int zmq::shm_ipc_connection_t::get_control_data(struct msghdr *msgh)
{
	struct cmsghdr *cmsg;
	int *fdptr;

	cmsg = CMSG_FIRSTHDR(msgh);
	if (!cmsg || cmsg->cmsg_level != SOL_SOCKET
			|| cmsg->cmsg_type != SCM_RIGHTS) {
		return -EINVAL;
	}

	fdptr = (int *)CMSG_DATA(cmsg);
	return *fdptr;
}

/**
 * Free all fields of a datagram message, initialized, half-initialized or not.
 */
void zmq::shm_ipc_connection_t::free_dgram_msg(struct msghdr *msg)
{
	int i;

	if (!msg) {
		return; /* FIXME: is this an error? */
	}

	for (i = 0; i < msg->msg_iovlen; i++) {
		free(msg->msg_iov[i].iov_base);
	}

	free(msg->msg_iov);
	free(msg->msg_control);
	free(msg);
}

/**
 * Allocate a datagram message and set all values to zero. Moreover, allocate
 * in advance an iovec that will be used to store the rptl message.  Also,
 * optionally create a buffer to store the control message.
 */
struct msghdr * zmq::shm_ipc_connection_t::alloc_dgram_msg(int flag)
{
	struct msghdr *msg;
	struct iovec *iov;
	char *buf;
	int buf_size;

	msg = malloc(sizeof(struct msghdr));
	if (!msg) {
		return NULL;
	}
	memset(msg, 0, sizeof(struct msghdr));

	iov = malloc(sizeof(struct iovec));
	if (!iov) {
		goto out;
	}
	memset(iov, 0, sizeof(struct iovec));

	msg->msg_iov = iov;
	msg->msg_iovlen = 1;

	if (flag & RPTL_INCLUDE_CONTROL_DATA) {
		buf_size = CMSG_SPACE(sizeof(int));
		buf = malloc(buf_size);
		if (!buf) {
			goto out;
		}

		msg->msg_control = buf;
		msg->msg_controllen = buf_size;
	}

	return msg;

out:
	free_dgram_msg(msg);
	return NULL;
}

int zmq::shm_ipc_connection_t::receive_dgram_msg(int fd, struct msghdr *msg)
{
	if (recvmsg(fd, msg, 0) < 0) {
		return -errno;
	}

	return 0;
}

int zmq::shm_ipc_connection_t::send_dgram_msg(int fd, struct msghdr *msg)
{
	if (sendmsg(fd, msg, 0) < 0) {
		return -errno;
	}

	/* The message should be no longer necessary */
	free_dgram_msg(msg);

	return 0;
}

/**
 * Allocate an rptl message and add it to an existing datagram message
 */
int zmq::shm_ipc_connection_t::add_empty_rptl_msg(struct msghdr *msg)
{
	/* The rptl message is always stored in the first iovec */
	struct iovec *iov = &msg->msg_iov[0];
	struct rptl_message *rptl_msg;

	if (!iov || msg->msg_iovlen < 1) {
		return -EBADMSG;
	}

	rptl_msg = malloc(sizeof(struct rptl_message));
	if (!rptl_msg) {
		return -ENOMEM;
	}
	memset(rptl_msg, 0, sizeof(struct rptl_message));

	iov->iov_base = rptl_msg;
	iov->iov_len = sizeof(struct rptl_message);

	return 0;
}

struct rptl_message * zmq::shm_ipc_connection_t::__get_rptl_msg(struct msghdr *msg)
{
	return msg->msg_iov[0].iov_base;
}

void zmq::shm_ipc_connection_t::__set_rptl_msg(struct msghdr *msg, struct rptl_message *rptl_msg)
{
	msg->msg_iov[0].iov_base = rptl_msg;
}

/**
 * Return the address of the rptl message and then erase it from the datagram
 * message. This way, we can free the datagram message without losing the
 * rptl message.
 */
struct rptl_message * zmq::shm_ipc_connection_t::extract_rptl_msg(struct msghdr *msg)
{
	struct rptl_message *rptl_msg;

	rptl_msg = __get_rptl_msg(msg);
	__set_rptl_msg(msg, NULL);

	return rptl_msg;
}

struct rptl_message * zmq::shm_ipc_connection_t::receive_rptl_msg(struct rptl_connection *conn,
		int flag)
{
	struct rptl_message *rptl_msg;
	struct msghdr *msg;
	int sfd = conn->conn_sfd.fd;
	int r;

	msg = alloc_dgram_msg(flag);
	if (!msg) {
		return NULL;
	}

	r = add_empty_rptl_msg(msg);
	if (r < 0) {
		return NULL;
	}

	r = receive_dgram_msg(sfd, msg);
	if (r < 0) {
		return NULL;
	}

	rptl_msg = extract_rptl_msg(msg);

	if (flag & RPTL_INCLUDE_CONTROL_DATA) {
		r = get_control_data(msg);
		rptl_msg->fd = r; /* Store temporarily the received fd to the rptl
				     message*/
	}

	free_dgram_msg(msg);

	return rptl_msg;
}

void zmq::shm_ipc_connection_t::free_rptl_msg(struct rptl_message *rptl_msg)
{
	free(rptl_msg);
}

struct msghdr * zmq::shm_ipc_connection_t::__create_msg(struct rptl_connection *conn, int flag)
{
	struct msghdr *msg;
	int fd;
	int r;

	msg = alloc_dgram_msg(flag);
	if (!msg) {
		return NULL;
	}

	r = add_empty_rptl_msg(msg);
	if (r < 0) {
		return NULL;
	}

	if (flag & RPTL_INCLUDE_CONTROL_DATA) {
		fd = conn->local_evfd.fd;
		set_control_data(msg, fd);
	}

	return msg;
}

struct msghdr * zmq::shm_ipc_connection_t::create_syn_msg(struct rptl_connection *conn)
{
	struct rptl_message *rptl_msg;
	struct msghdr *msg;

	msg = __create_msg(conn, RPTL_INCLUDE_CONTROL_DATA);
	if (!msg) {
		return NULL;
	}

	rptl_msg = __get_rptl_msg(msg);
	rptl_msg->phase = RPTL_MSG_SYN;
	strncpy(rptl_msg->conn_path, conn->path, RPTL_MAX_CONN_PATH_LEN);

	return msg;
}

struct msghdr *zmq::shm_ipc_connection_t::create_synack_msg(struct rptl_connection *conn)
{
	struct rptl_message *rptl_msg;
	struct msghdr *msg;

	msg = __create_msg(conn, RPTL_INCLUDE_CONTROL_DATA);
	if (!msg) {
		return NULL;
	}

	rptl_msg = __get_rptl_msg(msg);
	rptl_msg->phase = RPTL_MSG_SYNACK;

	return msg;
}

struct msghdr *zmq::shm_ipc_connection_t::create_ack_msg(struct rptl_connection *conn)
{
	struct rptl_message *rptl_msg;
	struct msghdr *msg;

	msg = __create_msg(conn, 0);
	if (!msg) {
		return NULL;
	}

	rptl_msg = __get_rptl_msg(msg);
	rptl_msg->phase = RPTL_MSG_ACK;

	return msg;
}

/*
 * FIXME: What I'd like to have here is the following:
 * create_rptl_msg()
 * edit the message (essentially shape it into a syn/synack/ack msg)
 * send_rptl_msg()
 * free_rptl_msg()
 *
 * However, unlike receive_rptl_msg, we cannot send an rptl_message due to the
 * fact that there is no backwards reference from the rptl_msg to the struct
 * msghdr (in receive_rptl_msg's case, we can extract the rptl_msg from the
 * struct msghdr, which is why it's possible).
 */
int zmq::shm_ipc_connection_t::send_syn_msg(struct rptl_connection *conn)
{
	struct msghdr *msg;
	int sfd = conn->conn_sfd.fd;
	int r;

	msg = create_syn_msg(conn);
	if (!msg) {
		return -ENOMEM;
	}

	r = send_dgram_msg(sfd, msg);
	if (r < 0) {
		return -errno;
	}

	return 0;
}

int zmq::shm_ipc_connection_t::send_synack_msg(struct rptl_connection *conn)
{
	struct msghdr *msg;
	int sfd = conn->conn_sfd.fd;
	int r;

	msg = create_synack_msg(conn);
	if (!msg) {
		return -ENOMEM;
	}

	r = send_dgram_msg(sfd, msg);
	if (r < 0) {
		return -errno;
	}

	return 0;
}

int zmq::shm_ipc_connection_t::send_ack_msg(struct rptl_connection *conn)
{
	struct msghdr *msg;
	int sfd = conn->conn_sfd.fd;
	int r;

	msg = create_ack_msg(conn);
	if (!msg) {
		return -ENOMEM;
	}

	r = send_dgram_msg(sfd, msg);
	if (r < 0) {
		return -errno;
	}

	return 0;
}

/*
 * Connect syn is called when we request a new connection. It is called in peer
 * process context.
 *
 * Connect_synack is called by the recipient of our connection. It is called in
 * control context.
 *
 * Connect ack is called by us when the connection is ready. It is called in
 * control context.
 */

int zmq::shm_ipc_connection_t::connect_syn(struct rptl_connection *conn, char *dest)
{
	int r, sfd;

	sfd = alloc_socket();
	if (sfd < 0) {
		return sfd;
	}

	conn->conn_sfd.fd = sfd; /* FIXME: Admittedly, this is uglier than
				    Ecce Homo after the restoration */

	r = connect_to_endpoint_socket(conn, dest);
	if (r < 0) {
		return r;
	}

	r = register_connection_socket(&control, conn);
	if (r < 0) {
		return r;
	}

	r = send_syn_msg(conn);
	if (r < 0) {
		return r;
	}

	conn->state = RPTL_CONN_STATE_EXPECT_SYNACK;

	/* TODO: Free all allocated structures */
	return 0;
}

int zmq::shm_ipc_connection_t::connect_synack(struct rptl_connection *conn)
{
	int r;

	r = send_synack_msg(conn);
	if (r < 0) {
		return r;
	}

	conn->state = RPTL_CONN_STATE_EXPECT_ACK;
	return 0;
}

int zmq::shm_ipc_connection_t::connect_ack(struct rptl_connection *conn)
{
	int r;

	r = send_ack_msg(conn);
	if (r < 0) {
		return r;
	}

	conn->state = RPTL_CONN_STATE_OPEN;
	return 0;
}




#endif
