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


#if !defined ZMQ_HAVE_WINDOWS && !defined ZMQ_HAVE_OPENVMS

#define SHM_PATH "/dev/shm/zeromq/"
#define SHM_PATH_LEN 64

#include "shm_ypipe.hpp"
#include "shm_ipc_connection.hpp"

namespace zmq
{
    typedef shm_ypipe_t <command_t, command_pipe_granularity> shm_cpipe_t;
    typedef char[SHM_PATH_LEN] shm_path_t;

    unsigned int get_ypipe_size ();
    shm_cpipe_t shm_create_cpipe ();
    void *shm_map (std::string path, unsigned int size);
    void shm_allocate(std::string path, unsigned int size);

    enum shm_pipe_t {SHM_PIPE_CONNECTER, SHM_PIPE_LISTENER};
}

#endif
