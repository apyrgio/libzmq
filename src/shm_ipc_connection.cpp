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


zmq::shm_ipc_connection_t::shm_ipc_connection_t (fd_t fd_, std::string addr_) :
	local_sockfd (fd_),
	remote_addr (addr_),
	conn_type (SHM_IPC_CONNECTER)
	// FIXME socket
{
	std::cout<<"Constructing the connection for connecter\n";

	snprintf(ring_name, "zeromq/%s_%d", remote_addr.to_string, 1134);
	create_connection ();
	connect_syn ();
}

zmq::shm_ipc_connection_t::shm_ipc_connection_t (fd_t fd_) :
	local_sockfd (fd_),
	conn_type (SHM_IPC_LISTENER)
	// FIXME socket
{
	std::cout<<"Constructing the connection for listener\n";
	init_conn();
}

zmq::shm_ipc_connection_t::~shm_ipc_connection_t ()
{
}

void zmq::shm_ipc_connection_t::timer_event (int id_)
{
	// Only for compilation reasons
	zmq_assert(false);
}

#if 0
char *zmq::shm_ipc_connection_t::get_ring_name ()
{
	if (!ring_name[0])
		snprintf(ring_name, "zeromq/%s_%d", remote_addr.to_string, 1134);

	return ring_name;
}
#endif

unsigned int get_ring_size()
{
	unsigned int size;

	size = 0;
	size += sizeof(struct ctrl_block_t);
	size += message_pipe_granularity * sizeof(msg_t);

	return size;
}

unsigned int get_shm_size()
{
	unsigned int size;

	return 2 * get_ring_size ();
}

void zmq::shm_ipc_connection_t::shm_allocate (unsigned int size)
{
	int fd, r;

	fd = shm_open(ring_name, O_RDWR|O_CREAT|O_EXCL, 0600);
	zmq_assert (fd >= 0);

	r = ftruncate(fd, size - 1);
	zmq_assert (r >= 0);

	close(fd);
	return 0;
}

void zmq::shm_ipc_connection_t::in_event ()
{
	int r;

	switch (conn_state) {
	case SHM_IPC_STATE_EXPECT_SYN:
		r = handle_syn_msg();
		zmq_assert (r >= 0);

		std::cout << "SYN finished\n";
		r = connect_synack();
		break;
	case SHM_IPC_STATE_EXPECT_SYNACK:
		r = handle_synack_msg();
		zmq_assert (r >= 0);

		std::cout << "SYNACK finished\n";
		r = connect_ack();

#if 0
		r = register_connection_eventfd(&control, conn);
#endif

		break;
	case SHM_IPC_STATE_EXPECT_ACK:
		r = handle_ack_msg();
		zmq_assert (r >= 0);

		printf("ACK finished\n");
		conn_state = SHM_IPC_STATE_OPEN;
#if 0
		r = register_connection_eventfd(&control, conn);
#endif

		break;
	default:
		zmq_assert (false);
	}
}

void zmq::shm_ipc_connection_t::out_event ()
{
	zmq_assert(false);
}

/**
 * When we receive a SYN message, we need to do the following:
 * a) Receive the rptl message
 * b) Store the remote eventfd
 * c) Map the connection path
 * d) Free the received rptl message
 * e) Send a SYNACK message
 */
int zmq::shm_ipc_connection_t::handle_syn_msg()
{
	struct hs_message *hs_msg;

	hs_msg = receive_hs_msg(HS_INCLUDE_CONTROL_DATA);
	if (!hs_msg || hs_msg->phase != HS_MSG_SYN) {
		free_hs_msg(hs_msg);
		return -EBADMSG;
	}

#if 0
	/* Send these things to the principal socket */
	store_remote_eventfd(conn, hs_msg->fd);
	store_connection_path(conn, hs_msg->conn_path);
	r = map_conn(conn);
	if (r < 0) {
		free_hs_msg(hs_msg);
		return r;
	}
#endif

	//TODO cleanup
	free_hs_msg(hs_msg);

	return 0;
}

/**
 * When we receive a SYNACK message, we need to do the following:
 * a) Receive the rptl message
 * b) Store the remote eventfd
 * c) Free the received rptl message
 * d) Send a SYNACK message
 */
int zmq::shm_ipc_connection_t::handle_synack_msg()
{
	struct hs_message *hs_msg;

	hs_msg = receive_hs_msg(HS_INCLUDE_CONTROL_DATA);
	if (!hs_msg || hs_msg->phase != HS_MSG_SYNACK) {
		free_hs_msg(hs_msg);
		return -EBADMSG;
	}

#if 0
	/* Store somewhere the remote eventfd */
	store_remote_eventfd(conn, hs_msg->fd);
	/* TODO: map_connection_path */
#endif

	free_hs_msg(hs_msg);

	return 0;
}

int zmq::shm_ipc_connection_t::handle_ack_msg()
{
	struct hs_message *hs_msg;

	hs_msg = receive_hs_msg();
	if (!hs_msg || hs_msg->phase != HS_MSG_ACK) {
		free_hs_msg(hs_msg);
		return -EBADMSG;
	}

	free_hs_msg(hs_msg);

	return 0;
}

void *zmq::shm_ipc_connection_t::map_conn ()
{
	int fd = shm_open(ring_name, O_RDWR, 0600);
	zmq_assert (fd >= 0);

	unsigned size = get_shm_size ();

	mem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	zmq_assert (mem != MAP_FAILED);

	close(fd);
	return mem;
}

void zmq::shm_ipc_connection_t::alloc_conn ()
{
	std::cout << "In alloc_conn of connection\n";

	unsigned int size = get_shm_size ();
	shm_allocate(size);
}

void zmq::shm_ipc_connection_t::init_conn ()
{
	pipe_t *pipe;

	std::cout << "In init_conn of connection\n";
	local_evfd = eventfd(0, 0);

	void *mem = map_conn ();
	unsigned int size = get_ring_size ();

	if (conn_t == SHM_IPC_CONNECTER) {
		shm_pipe (socket, pipe, hwms, false, [mem, mem + size]);
	} else {
		shm_pipe (socket, pipe, hwms, false, [mem + size, mem]);
	}
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
	unsigned int i;

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
 * in advance an iovec that will be used to store the hs message.  Also,
 * optionally create a buffer to store the control message.
 */
struct msghdr * zmq::shm_ipc_connection_t::alloc_dgram_msg(int flag)
{
	struct msghdr *msg;
	struct iovec *iov;
	char *buf;
	int buf_size;

	msg = (struct msghdr *) malloc(sizeof(struct msghdr));
	if (!msg) {
		return NULL;
	}
	memset(msg, 0, sizeof(struct msghdr));

	iov = (struct iovec *) malloc(sizeof(struct iovec));
	if (!iov) {
		goto out;
	}
	memset(iov, 0, sizeof(struct iovec));

	msg->msg_iov = iov;
	msg->msg_iovlen = 1;

	if (flag & HS_INCLUDE_CONTROL_DATA) {
		buf_size = CMSG_SPACE(sizeof(int));
		buf = (char *) malloc(buf_size);
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
	if (::recvmsg(fd, msg, 0) < 0) {
		return -errno;
	}

	return 0;
}

int zmq::shm_ipc_connection_t::send_dgram_msg(int fd, struct msghdr *msg)
{
	if (::sendmsg(fd, msg, 0) < 0) {
		std::cout << "Error during send (fd: " << fd << ", error: "
			<< strerror(errno) <<")\n";
		return -errno;
	}

	/* The message should be no longer necessary */
	free_dgram_msg(msg);

	return 0;
}

/**
 * Allocate an hs message and add it to an existing datagram message
 */
int zmq::shm_ipc_connection_t::add_empty_hs_msg(struct msghdr *msg)
{
	/* The hs message is always stored in the first iovec */
	struct iovec *iov = &msg->msg_iov[0];
	struct hs_message *hs_msg;

	if (!iov || msg->msg_iovlen < 1) {
		return -EBADMSG;
	}

	hs_msg = (struct hs_message *) malloc(sizeof(struct hs_message));
	if (!hs_msg) {
		return -ENOMEM;
	}
	memset(hs_msg, 0, sizeof(struct hs_message));

	iov->iov_base = hs_msg;
	iov->iov_len = sizeof(struct hs_message);

	return 0;
}

struct zmq::shm_ipc_connection_t::hs_message * zmq::shm_ipc_connection_t::__get_hs_msg(struct msghdr *msg)
{
	return (struct hs_message *) msg->msg_iov[0].iov_base;
}

void zmq::shm_ipc_connection_t::__set_hs_msg(struct msghdr *msg,
		struct hs_message *hs_msg)
{
	msg->msg_iov[0].iov_base = hs_msg;
}

/**
 * Return the address of the hs message and then erase it from the datagram
 * message. This way, we can free the datagram message without losing the
 * hs message.
 */
struct zmq::shm_ipc_connection_t::hs_message *
		zmq::shm_ipc_connection_t::extract_hs_msg(struct msghdr *msg)
{
	struct hs_message *hs_msg;

	hs_msg = __get_hs_msg(msg);
	__set_hs_msg(msg, NULL);

	return hs_msg;
}

struct zmq::shm_ipc_connection_t::hs_message *zmq::shm_ipc_connection_t::receive_hs_msg(int flag)
{
	struct hs_message *hs_msg;
	struct msghdr *msg;
	int sfd = local_sockfd;
	int r;

	msg = alloc_dgram_msg(flag);
	if (!msg) {
		return NULL;
	}

	r = add_empty_hs_msg(msg);
	if (r < 0) {
		return NULL;
	}

	r = receive_dgram_msg(sfd, msg);
	if (r < 0) {
		return NULL;
	}

	hs_msg = extract_hs_msg(msg);

	if (flag & HS_INCLUDE_CONTROL_DATA) {
		r = get_control_data(msg);
		hs_msg->fd = r; /* Store temporarily the received fd to the hs
				     message*/
	}

	free_dgram_msg(msg);

	return hs_msg;
}

void zmq::shm_ipc_connection_t::free_hs_msg(struct hs_message *hs_msg)
{
	free(hs_msg);
}

struct msghdr * zmq::shm_ipc_connection_t::__create_msg(int flag)
{
	struct msghdr *msg;
	int fd;
	int r;

	msg = alloc_dgram_msg(flag);
	if (!msg) {
		return NULL;
	}

	r = add_empty_hs_msg(msg);
	if (r < 0) {
		return NULL;
	}

	if (flag & HS_INCLUDE_CONTROL_DATA) {
		fd = local_evfd;
		set_control_data(msg, fd);
	}

	return msg;
}

struct msghdr * zmq::shm_ipc_connection_t::create_syn_msg()
{
	struct hs_message *hs_msg;
	struct msghdr *msg;

	msg = __create_msg(HS_INCLUDE_CONTROL_DATA);
	if (!msg) {
		return NULL;
	}

	hs_msg = __get_hs_msg(msg);
	hs_msg->phase = HS_MSG_SYN;
	strncpy(hs_msg->conn_path, "path", 5);

	return msg;
}

struct msghdr *zmq::shm_ipc_connection_t::create_synack_msg()
{
	struct hs_message *hs_msg;
	struct msghdr *msg;

	msg = __create_msg(HS_INCLUDE_CONTROL_DATA);
	if (!msg) {
		return NULL;
	}

	hs_msg = __get_hs_msg(msg);
	hs_msg->phase = HS_MSG_SYNACK;

	return msg;
}

struct msghdr *zmq::shm_ipc_connection_t::create_ack_msg()
{
	struct hs_message *hs_msg;
	struct msghdr *msg;

	msg = __create_msg();
	if (!msg) {
		return NULL;
	}

	hs_msg = __get_hs_msg(msg);
	hs_msg->phase = HS_MSG_ACK;

	return msg;
}

/*
 * FIXME: What I'd like to have here is the following:
 * create_hs_msg()
 * edit the message (essentially shape it into a syn/synack/ack msg)
 * send_hs_msg()
 * free_hs_msg()
 *
 * However, unlike receive_hs_msg, we cannot send an hs_message due to the
 * fact that there is no backwards reference from the hs_msg to the struct
 * msghdr (in receive_hs_msg's case, we can extract the hs_msg from the
 * struct msghdr, which is why it's possible).
 */
int zmq::shm_ipc_connection_t::send_syn_msg()
{
	struct msghdr *msg;
	int sfd = local_sockfd;
	int r;

	msg = create_syn_msg();
	if (!msg) {
		return -ENOMEM;
	}

	r = send_dgram_msg(sfd, msg);
	if (r < 0) {
		return -errno;
	}

	return 0;
}

int zmq::shm_ipc_connection_t::send_synack_msg()
{
	struct msghdr *msg;
	int sfd = local_sockfd;
	int r;

	msg = create_synack_msg();
	if (!msg) {
		return -ENOMEM;
	}

	r = send_dgram_msg(sfd, msg);
	if (r < 0) {
		return -errno;
	}

	return 0;
}

int zmq::shm_ipc_connection_t::send_ack_msg()
{
	struct msghdr *msg;
	int sfd = local_sockfd;
	int r;

	msg = create_ack_msg();
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

int zmq::shm_ipc_connection_t::connect_syn()
{
	int r;

	r = send_syn_msg();
	if (r < 0) {
		return r;
	}

	conn_state = SHM_IPC_STATE_EXPECT_SYNACK;

	/* TODO: Free all allocated structures */
	return 0;
}

int zmq::shm_ipc_connection_t::connect_synack()
{
	int r;

	r = send_synack_msg();
	if (r < 0) {
		return r;
	}

	conn_state = SHM_IPC_STATE_EXPECT_ACK;
	return 0;
}

int zmq::shm_ipc_connection_t::connect_ack()
{
	int r;

	r = send_ack_msg();
	if (r < 0) {
		return r;
	}

	conn_state = SHM_IPC_STATE_OPEN;
	return 0;
}

#endif
