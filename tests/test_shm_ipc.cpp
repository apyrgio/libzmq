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

#include "testutil.hpp"
#include <iostream>

const char sname[100] = "shm_ipc:///tmp/test";

int main (void)
{
    setup_test_environment();

	std::cout<<"Creating new zmq context\n";
    void *ctx = zmq_ctx_new ();
    assert (ctx);

	std::cout<<"Creating server socket\n";
    void *sb = zmq_socket (ctx, ZMQ_DEALER);
    assert (sb);

	std::cout<<"Binding server socket\n";
    int rc = zmq_bind (sb, sname);
    assert (rc == 0);

    char endpoint[200];
    size_t size = sizeof(endpoint);
    rc = zmq_getsockopt (sb, ZMQ_LAST_ENDPOINT, endpoint, &size);
    assert (rc == 0);

	std::cout<<"Verifying registration of server socket\n";
	std::cout<< sname << " ?= "<< endpoint << "\n";
    rc = strncmp(endpoint, sname, size);

    assert (rc == 0);

	std::cout<<"Creating client socket\n";
    void *sc = zmq_socket (ctx, ZMQ_DEALER);
    assert (sc);

	std::cout<<"Connecting client socket to server socket\n";
    rc = zmq_connect (sc, sname);
    assert (rc == 0);

	sleep (2);
	return 0;

	std::cout<<"PAIN!\n";
    bounce (sb, sc);

    rc = zmq_close (sc);
    assert (rc == 0);

    rc = zmq_close (sb);
    assert (rc == 0);

    rc = zmq_ctx_term (ctx);
    assert (rc == 0);

    return 0 ;
}
