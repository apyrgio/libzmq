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

#include <new>
#include <stddef.h>

#include "pipe.hpp"
#include "err.hpp"

#include "ypipe.hpp"
#include "ypipe_conflate.hpp"
#include "shm_ypipe.hpp"
#include "shm_ipc_connection.hpp"
#include "shm_ring.hpp"

int zmq::pipepair (class object_t *parents_ [2], class pipe_t* pipes_ [2],
    int hwms_ [2], bool conflate_ [2])
{
    //   Creates two pipe objects. These objects are connected by two ypipes,
    //   each to pass messages in one direction.

    typedef ypipe_t <msg_t, message_pipe_granularity> upipe_normal_t;
    typedef ypipe_conflate_t <msg_t> upipe_conflate_t;

    pipe_t::upipe_t *upipe1;
    if(conflate_ [0])
        upipe1 = new (std::nothrow) upipe_conflate_t ();
    else
        upipe1 = new (std::nothrow) upipe_normal_t ();
    alloc_assert (upipe1);

    pipe_t::upipe_t *upipe2;
    if(conflate_ [1])
        upipe2 = new (std::nothrow) upipe_conflate_t ();
    else
        upipe2 = new (std::nothrow) upipe_normal_t ();
    alloc_assert (upipe2);

    pipes_ [0] = new (std::nothrow) pipe_t (parents_ [0], upipe1, upipe2,
        hwms_ [1], hwms_ [0], conflate_ [0]);
    alloc_assert (pipes_ [0]);
    pipes_ [1] = new (std::nothrow) pipe_t (parents_ [1], upipe2, upipe1,
        hwms_ [0], hwms_ [1], conflate_ [1]);
    alloc_assert (pipes_ [1]);

    pipes_ [0]->set_peer (pipes_ [1]);
    pipes_ [1]->set_peer (pipes_ [0]);

    return 0;
}

//   Creates two pipe objects. These objects are connected by two ypipes,
//   each to pass messages in one direction.
//   Since we need a way to distinguish between calls to this function in
//   order to create pipes and their mirror counterparts, we will introduce the
//   concept of CONNECTER and LISTENER.
int zmq::shm_pipe (class object_t *parent_, class pipe_t **shm_pipe_,
    int hwms_ [2], bool conflate_, std::string path, shm_pipe_t pipe_type)
{
    typedef shm_ypipe_t <msg_t, message_pipe_granularity> shm_upipe_normal_t;

    unsigned int offset_in = 0;
    unsigned int offset_out = 0;
    int temp;

    if (pipe_type == SHM_PIPE_CONNECTER) {
        offset_out = get_ypipe_size ();
    } else if (pipe_type == SHM_PIPE_LISTENER) {
        offset_in = get_ypipe_size ();
        temp = hwms_[0];
        hwms_[0] = hwms_[1];
        hwms_[1] = temp;
    } else {
        zmq_assert(false);
    }

    pipe_t::upipe_t *shm_upipe_in;
    shm_upipe_in = new (std::nothrow) shm_upipe_normal_t (path, offset_in);
    alloc_assert (shm_upipe_in);
    std::cout << "Pipe 1: " << shm_upipe_in << "\n";

    pipe_t::upipe_t *shm_upipe_out;
    shm_upipe_out = new (std::nothrow) shm_upipe_normal_t (path, offset_out);
    alloc_assert (shm_upipe_out);
    std::cout << "Pipe 2: " << shm_upipe_out << "\n";

    *shm_pipe_ = new (std::nothrow) pipe_t (parent_, shm_upipe_in,
            shm_upipe_out, hwms_ [1], hwms_ [0], conflate_);
    alloc_assert (shm_pipe_);

    (*shm_pipe_)->set_peer (NULL);

    return 0;
}

zmq::pipe_t *zmq::shm_alloc_pipe (object_t *parent_, options_t *options,
        std::string path, shm_pipe_t pipe_type)
{
    bool conflate = options->conflate &&
        (options->type == ZMQ_DEALER ||
         options->type == ZMQ_PULL ||
         options->type == ZMQ_PUSH ||
         options->type == ZMQ_PUB ||
         options->type == ZMQ_SUB);

    pipe_t *pipe;
    int r;

    int hwms[2] = {conflate? -1 : options->rcvhwm,
        conflate? -1 : options->sndhwm};

    r = zmq::shm_pipe (parent_, &pipe, hwms, conflate, path, pipe_type);
    zmq_assert (r >= 0);

    return pipe;
}

zmq::pipe_t *zmq::shm_create_ring (object_t *parent_, options_t *options,
        std::string ring_name, shm_pipe_t pipe_type)
{
    int r;

    shm_mkdir ("zeromq");
    unsigned int size = get_ring_size ();

    if (ring_name == "") {
        // If allocation fails due to a duplicate name, retry.
        // Note that this is uncommon, but we must handle it anyway.
        do {
            ring_name = shm_generate_random_name ("ring");
            r = shm_allocate (ring_name, size);
        } while (r < 0);
    }

    return shm_alloc_pipe (parent_, options, ring_name, pipe_type);
}

zmq::pipe_t::pipe_t (object_t *parent_, upipe_t *inpipe_, upipe_t *outpipe_,
      int inhwm_, int outhwm_, bool conflate_) :
    object_t (parent_),
    assoc_fd (retired_fd),
    inpipe (inpipe_),
    outpipe (outpipe_),
    in_active (true),
    out_active (true),
    hwm (outhwm_),
    lwm (compute_lwm (inhwm_)),
    msgs_read (0),
    msgs_written (0),
    peers_msgs_read (0),
    peer (NULL),
    sink (NULL),
    state (active),
    delay (true),
    conflate (conflate_)
{
}

zmq::pipe_t::~pipe_t ()
{
}

void zmq::pipe_t::set_peer (pipe_t *peer_)
{
    //  Peer can be set once only.
    zmq_assert (!peer);
    peer = peer_;
}

void zmq::pipe_t::set_event_sink (i_pipe_events *sink_)
{
    // Sink can be set once only.
    zmq_assert (!sink);
    sink = sink_; }

void zmq::pipe_t::set_identity (const blob_t &identity_)
{
    identity = identity_;
}

zmq::blob_t zmq::pipe_t::get_identity ()
{
    return identity;
}

zmq::blob_t zmq::pipe_t::get_credential () const
{
    return credential;
}

bool zmq::pipe_t::check_read ()
{
    if (unlikely (!in_active))
        return false;
    if (unlikely (state != active && state != waiting_for_delimiter))
        return false;

    //  Check if there's an item in the pipe.
    if (!inpipe->check_read ()) {
        in_active = false;
        return false;
    }

    //  If the next item in the pipe is message delimiter,
    //  initiate termination process.
    if (inpipe->probe (is_delimiter)) {
        msg_t msg;
        bool ok = inpipe->read (&msg);
        zmq_assert (ok);
        process_delimiter ();
        return false;
    }

    return true;
}

bool zmq::pipe_t::read (msg_t *msg_)
{
    if (unlikely (!in_active))
        return false;
    if (unlikely (state != active && state != waiting_for_delimiter))
        return false;

read_message:
    if (!inpipe->read (msg_)) {
        in_active = false;
        return false;
    }

    //  If this is a credential, save a copy and receive next message.
    if (unlikely (msg_->is_credential ())) {
        const unsigned char *data = static_cast <const unsigned char *> (msg_->data ());
        credential = blob_t (data, msg_->size ());
        const int rc = msg_->close ();
        zmq_assert (rc == 0);
        goto read_message;
    }

    //  If delimiter was read, start termination process of the pipe.
    if (msg_->is_delimiter ()) {
        process_delimiter ();
        return false;
    }

    if (!(msg_->flags () & msg_t::more) && !msg_->is_identity ())
        msgs_read++;

    if (lwm > 0 && msgs_read % lwm == 0)
        send_activate_write (peer, msgs_read);

    return true;
}

bool zmq::pipe_t::check_write ()
{
    if (unlikely (!out_active || state != active))
        return false;

    bool full = hwm > 0 && msgs_written - peers_msgs_read == uint64_t (hwm);

    if (unlikely (full)) {
        out_active = false;
        return false;
    }

    return true;
}

bool zmq::pipe_t::write (msg_t *msg_)
{
    if (unlikely (!check_write ()))
        return false;

    bool more = msg_->flags () & msg_t::more ? true : false;
    const bool is_identity = msg_->is_identity ();
    outpipe->write (*msg_, more);
    if (!more && !is_identity)
        msgs_written++;

    return true;
}

void zmq::pipe_t::rollback ()
{
    //  Remove incomplete message from the outbound pipe.
    msg_t msg;
    if (outpipe) {
        while (outpipe->unwrite (&msg)) {
            zmq_assert (msg.flags () & msg_t::more);
            int rc = msg.close ();
            errno_assert (rc == 0);
        }
    }
}

void zmq::pipe_t::flush ()
{
    //  The peer does not exist anymore at this point.
    if (state == term_ack_sent)
        return;

    if (outpipe && !outpipe->flush ())
        send_activate_read (peer);
    // We need a way to signal the remote mailbox here.
}

void zmq::pipe_t::process_activate_read ()
{
    if (!in_active && (state == active || state == waiting_for_delimiter)) {
        in_active = true;
        sink->read_activated (this);
    }
}

void zmq::pipe_t::process_activate_write (uint64_t msgs_read_)
{
    //  Remember the peers's message sequence number.
    peers_msgs_read = msgs_read_;

    if (!out_active && state == active) {
        out_active = true;
        sink->write_activated (this);
    }
}

void zmq::pipe_t::process_hiccup (void *pipe_)
{
    //  Destroy old outpipe. Note that the read end of the pipe was already
    //  migrated to this thread.
    zmq_assert (outpipe);
    outpipe->flush ();
    msg_t msg;
    while (outpipe->read (&msg)) {
       int rc = msg.close ();
       errno_assert (rc == 0);
    }
    delete outpipe;

    //  Plug in the new outpipe.
    zmq_assert (pipe_);
    outpipe = (upipe_t*) pipe_;
    out_active = true;

    //  If appropriate, notify the user about the hiccup.
    if (state == active)
        sink->hiccuped (this);
}

void zmq::pipe_t::process_pipe_term ()
{
    //  This is the simple case of peer-induced termination. If there are no
    //  more pending messages to read, or if the pipe was configured to drop
    //  pending messages, we can move directly to the term_ack_sent state.
    //  Otherwise we'll hang up in waiting_for_delimiter state till all
    //  pending messages are read.
    if (state == active) {
        if (!delay) {
            state = term_ack_sent;
            outpipe = NULL;
            send_pipe_term_ack (peer);
        }
        else
            state = waiting_for_delimiter;
        return;
    }

    //  Delimiter happened to arrive before the term command. Now we have the
    //  term command as well, so we can move straight to term_ack_sent state.
    if (state == delimiter_received) {
        state = term_ack_sent;
        outpipe = NULL;
        send_pipe_term_ack (peer);
        return;
    }

    //  This is the case where both ends of the pipe are closed in parallel.
    //  We simply reply to the request by ack and continue waiting for our
    //  own ack.
    if (state == term_req_sent1) {
        state = term_req_sent2;
        outpipe = NULL;
        send_pipe_term_ack (peer);
        return;
    }

    //  pipe_term is invalid in other states.
    zmq_assert (false);
}

void zmq::pipe_t::process_pipe_term_ack ()
{
    //  Notify the user that all the references to the pipe should be dropped.
    zmq_assert (sink);
    sink->pipe_terminated (this);

    //  In term_ack_sent and term_req_sent2 states there's nothing to do.
    //  Simply deallocate the pipe. In term_req_sent1 state we have to ack
    //  the peer before deallocating this side of the pipe.
    //  All the other states are invalid.
    if (state == term_req_sent1) {
        outpipe = NULL;
        send_pipe_term_ack (peer);
    }
    else
        zmq_assert (state == term_ack_sent || state == term_req_sent2);

    //  We'll deallocate the inbound pipe, the peer will deallocate the outbound
    //  pipe (which is an inbound pipe from its point of view).
    //  First, delete all the unread messages in the pipe. We have to do it by
    //  hand because msg_t doesn't have automatic destructor. Then deallocate
    //  the ypipe itself.

    if (!conflate) {
        msg_t msg;
        while (inpipe->read (&msg)) {
            int rc = msg.close ();
            errno_assert (rc == 0);
        }
    }

    delete inpipe;

    //  Deallocate the pipe object
    delete this;
}

void zmq::pipe_t::set_nodelay ()
{
    this->delay = false;
}

void zmq::pipe_t::terminate (bool delay_)
{
    //  Overload the value specified at pipe creation.
    delay = delay_;

    //  If terminate was already called, we can ignore the duplicit invocation.
    if (state == term_req_sent1 || state == term_req_sent2)
        return;

    //  If the pipe is in the final phase of async termination, it's going to
    //  closed anyway. No need to do anything special here.
    else
    if (state == term_ack_sent)
        return;

    //  The simple sync termination case. Ask the peer to terminate and wait
    //  for the ack.
    else
    if (state == active) {
        send_pipe_term (peer);
        state = term_req_sent1;
    }

    //  There are still pending messages available, but the user calls
    //  'terminate'. We can act as if all the pending messages were read.
    else
    if (state == waiting_for_delimiter && !delay) {
        outpipe = NULL;
        send_pipe_term_ack (peer);
        state = term_ack_sent;
    }

    //  If there are pending messages still availabe, do nothing.
    else
    if (state == waiting_for_delimiter) {
    }

    //  We've already got delimiter, but not term command yet. We can ignore
    //  the delimiter and ack synchronously terminate as if we were in
    //  active state.
    else
    if (state == delimiter_received) {
        send_pipe_term (peer);
        state = term_req_sent1;
    }

    //  There are no other states.
    else
        zmq_assert (false);

    //  Stop outbound flow of messages.
    out_active = false;

    if (outpipe) {

        //  Drop any unfinished outbound messages.
        rollback ();

        //  Write the delimiter into the pipe. Note that watermarks are not
        //  checked; thus the delimiter can be written even when the pipe is full.
        msg_t msg;
        msg.init_delimiter ();
        outpipe->write (msg, false);
        flush ();
    }
}

bool zmq::pipe_t::is_delimiter (const msg_t &msg_)
{
    return msg_.is_delimiter ();
}

int zmq::pipe_t::compute_lwm (int hwm_)
{
    //  Compute the low water mark. Following point should be taken
    //  into consideration:
    //
    //  1. LWM has to be less than HWM.
    //  2. LWM cannot be set to very low value (such as zero) as after filling
    //     the queue it would start to refill only after all the messages are
    //     read from it and thus unnecessarily hold the progress back.
    //  3. LWM cannot be set to very high value (such as HWM-1) as it would
    //     result in lock-step filling of the queue - if a single message is
    //     read from a full queue, writer thread is resumed to write exactly one
    //     message to the queue and go back to sleep immediately. This would
    //     result in low performance.
    //
    //  Given the 3. it would be good to keep HWM and LWM as far apart as
    //  possible to reduce the thread switching overhead to almost zero,
    //  say HWM-LWM should be max_wm_delta.
    //
    //  That done, we still we have to account for the cases where
    //  HWM < max_wm_delta thus driving LWM to negative numbers.
    //  Let's make LWM 1/2 of HWM in such cases.
    int result = (hwm_ > max_wm_delta * 2) ?
        hwm_ - max_wm_delta : (hwm_ + 1) / 2;

    return result;
}

void zmq::pipe_t::process_delimiter ()
{
    zmq_assert (state == active
            ||  state == waiting_for_delimiter);

    if (state == active)
        state = delimiter_received;
    else {
        outpipe = NULL;
        send_pipe_term_ack (peer);
        state = term_ack_sent;
    }
}

void zmq::pipe_t::hiccup ()
{
    //  If termination is already under way do nothing.
    if (state != active)
        return;

    //  We'll drop the pointer to the inpipe. From now on, the peer is
    //  responsible for deallocating it.
    inpipe = NULL;

    //  Create new inpipe.
    if (conflate)
        inpipe = new (std::nothrow)
            ypipe_conflate_t <msg_t> ();
    else
        inpipe = new (std::nothrow)
            ypipe_t <msg_t, message_pipe_granularity> ();

    alloc_assert (inpipe);
    in_active = true;

    //  Notify the peer about the hiccup.
    send_hiccup (peer, (void*) inpipe);
}

void zmq::pipe_t::set_hwms (int inhwm_, int outhwm_)
{
    lwm = compute_lwm (inhwm_);
    hwm = outhwm_;
}

//void zmq::pipe_t::mark_inactive_write ()
//{
    //outpipe->mark_inactive ();
//}

//void zmq::pipe_t::mark_inactive_read ()
//{
    //inpipe->mark_inactive ();
//}
