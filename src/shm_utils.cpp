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

#include "random.hpp"
#include "shm_utils.hpp"

void zmq::shm_mkdir (const char &name)
{
    shm_path_t dir = SHM_PATH;

    zmq_assert (strlen(dir) + strlen(name) + 1 <= SHM_PATH_LEN)
    strncat(dir, name, SHM_PATH_LEN);

    int fd = mkdir(dir, 0600);
    close (fd);
}

// A uint32_t in hexademical is at most 8 characters. Prepend with
// zeroes where necessary.
shm_path_t &zmq::shm_generate_random_name (const char &prefix)
{
    shm_path_t name;
    uint32_t rand = zmq::generate_random ();

    snprintf (name, "%s_%.8x", prefix, rand);

    return name;
}

// FIXME: What if ZMQ_SHM_BUFER_SIZE changes in the meantime?
unsigned int zmq::get_ring_size ()
{
    return 2 * get_ypipe_size ();
}

unsigned int zmq::get_ypipe_size ()
{
    unsigned int opt_size = sizeof shm_buffer_size;
    unsigned int size;
    int r;

    r = socket->getsockopt (ZMQ_SHM_BUFFER_SIZE, &shm_buffer_size, &opt_size);
    zmq_assert (r >= 0);

    size = 0;
    size += sizeof(struct zmq::ctrl_block_t);
    size += message_pipe_granularity * sizeof(zmq::msg_t);
    size += shm_buffer_size;

    return size;
}

void zmq::__prepare_shm_pipe (shm_path_t &name, void *mem, unsigned int size)
{
    struct ctrl_block_t *ctrl = (struct ctrl_block_t *)mem;
    ctrl->initialized = 0;
    ctrl->must_signal = true;
    ctrl->name = name;
}

void zmq::prepare_shm_ring (void *mem, unsigned int size)
{
    unsigned int size = get_ypipe_size ();
    void *mem1 = mem;
    void *mem2 = (void *)((char *)mem + size);

    __prepare_shm_pipe (name, mem1, size);
    __prepare_shm_pipe (name, mem2, size);
}

void zmq::prepare_shm_cpipe (shm_path_t &name, void *mem)
{
    __prepare_shm_pipe (name, mem, 0);
}


shm_path_t &__shm_create_path_name(char *name)
{
    shm_path_t path_name = "/zeromq/";
    int len = 0;

    len += strlen (path_name);
    len += strlen (name);

    zmq_assert(len <= SHM_PATH_LEN);
    strncpy(path_name, name, SHM_PATH_LEN);

    return path_name;
}

// Create a file in shared memory using the provided name and size.
// If a file with the same name exists, return -1 else abort.
int zmq::shm_allocate (char *name, unsigned int size)
{
    int fd, r;
    shm_path_t path_name = __shm_create_path_name(name);

    fd = shm_open(path_name, O_RDWR|O_CREAT|O_EXCL, 0600);
    if (fd < 0) {
        zmq_assert (errno == EEXIST);
        return -1;
    }

    r = ftruncate(fd, size - 1);
    zmq_assert (r >= 0);

    close(fd);
    return 0;
}

void *zmq::shm_map (char *name, unsigned int size)
{
    shm_path_t path_name = __shm_create_path_name(name);

    int fd = shm_open(path_name, O_RDWR, 0600);
    zmq_assert (fd >= 0);

    void *mem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    zmq_assert (mem != MAP_FAILED);

    close(fd);
    return mem;
}

#endif

