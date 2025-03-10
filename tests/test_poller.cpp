/*
 * libstored, distributed debuggable data stores.
 * Copyright (C) 2020-2022  Jochem Rutgers
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#define STORED_NO_DEPRECATED

#include "libstored/poller.h"
#include "TestStore.h"
#include "gtest/gtest.h"

#include "LoggingLayer.h"

#include <poll.h>
#include <unistd.h>

#ifdef STORED_HAVE_ZMQ
#	include <zmq.h>
#endif

namespace {
TEST(Poller, Banner)
{
	puts(stored::banner());
}

#ifdef STORED_POLL_OLD
TEST(Poller, Pipe)
{
	int fd[2];
	ASSERT_EQ(pipe(fd), 0);

	char buf = '0';

	// Check if the pipe works at all.
	EXPECT_EQ(write(fd[1], "1", 1), 1);
	EXPECT_EQ(read(fd[0], &buf, 1), 1);
	EXPECT_EQ(buf, '1');

	stored::Poller poller;
	EXPECT_EQ(poller.add(fd[0], (void*)1, stored::Poller::PollIn), 0);

	auto const* res = &poller.poll(0);
	EXPECT_NE(errno, 0);
	EXPECT_TRUE(res->empty());

	// Put something in the pipe.
	EXPECT_EQ(write(fd[1], "2", 1), 1);
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).events, (stored::Poller::events_t)stored::Poller::PollIn);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	// Drain pipe.
	EXPECT_EQ(read(fd[0], &buf, 1), 1);
	EXPECT_EQ(buf, '2');
	res = &poller.poll(0);
	EXPECT_NE(errno, 0);
	EXPECT_TRUE(res->empty());

	// Add a second fd.
	EXPECT_EQ(poller.add(fd[1], (void*)2, stored::Poller::PollOut), 0);
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).events, (stored::Poller::events_t)stored::Poller::PollOut);
	EXPECT_EQ(res->at(0).user_data, (void*)2);

	EXPECT_EQ(write(fd[1], "3", 1), 1);
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 2);
	// The order is undefined.
	if(res->at(0).user_data == (void*)1) {
		EXPECT_EQ(res->at(1).events, (stored::Poller::events_t)stored::Poller::PollOut);
		EXPECT_EQ(res->at(1).user_data, (void*)2);
		EXPECT_EQ(res->at(0).events, (stored::Poller::events_t)stored::Poller::PollIn);
		EXPECT_EQ(res->at(0).user_data, (void*)1);
	} else {
		EXPECT_EQ(res->at(0).events, (stored::Poller::events_t)stored::Poller::PollOut);
		EXPECT_EQ(res->at(0).user_data, (void*)2);
		EXPECT_EQ(res->at(1).events, (stored::Poller::events_t)stored::Poller::PollIn);
		EXPECT_EQ(res->at(1).user_data, (void*)1);
	}

	// Drain pipe again.
	EXPECT_EQ(read(fd[0], &buf, 1), 1);
	EXPECT_EQ(buf, '3');
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).events, (stored::Poller::events_t)stored::Poller::PollOut);
	EXPECT_EQ(res->at(0).user_data, (void*)2);

	// Close read end.
	EXPECT_EQ(poller.remove(fd[0]), 0);
	close(fd[0]);
	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_NE(res->at(0).events, 0);
	EXPECT_EQ(res->at(0).user_data, (void*)2);
}

#	if defined(STORED_HAVE_ZMQ) && !defined(STORED_POLL_POLL) && !defined(STORED_POLL_LOOP) \
		&& !defined(STORED_POLL_ZTH_LOOP)
TEST(Poller, Zmq)
{
	void* context = zmq_ctx_new();
	ASSERT_NE(context, nullptr);
	void* rep = zmq_socket(context, ZMQ_REP);
	ASSERT_EQ(zmq_bind(rep, "inproc://poller"), 0);
	void* req = zmq_socket(context, ZMQ_REQ);
	ASSERT_EQ(zmq_connect(req, "inproc://poller"), 0);

	stored::Poller poller;
	EXPECT_EQ(poller.add(rep, (void*)1, stored::Poller::PollOut | stored::Poller::PollIn), 0);
	EXPECT_EQ(poller.add(req, (void*)2, stored::Poller::PollOut | stored::Poller::PollIn), 0);

	auto const* res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)2);
	EXPECT_EQ(res->at(0).revents, (stored::Poller::events_t)stored::Poller::PollOut);

	zmq_send(req, "Hi", 2, 0);

	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).user_data, (void*)1);

	char buffer[16];
	zmq_recv(rep, buffer, sizeof(buffer), 0);
	zmq_send(rep, buffer, 2, 0);

	res = &poller.poll(0);
	EXPECT_EQ(res->size(), 1);

	zmq_close(req);
	zmq_close(rep);
	zmq_ctx_destroy(context);
}
#	endif // STORED_HAVE_ZMQ
#endif	       // STORED_POLL_OLD

TEST(Poller, PollableCallback)
{
	int count = 0;
	auto p1 = stored::pollable(
		[&](stored::Pollable const& p) {
			count++;
			return p.events;
		},
		stored::Pollable::PollIn);

	stored::CustomPoller<stored::LoopPoller> poller = {p1};

	auto res = poller.poll(0);
	EXPECT_EQ(res.size(), 1U);
}

#if defined(STORED_HAVE_ZMQ)
TEST(Poller, PollableZmqSocket)
{
	void* context = zmq_ctx_new();
	ASSERT_NE(context, nullptr);
	void* rep = zmq_socket(context, ZMQ_REP);
	ASSERT_EQ(zmq_bind(rep, "inproc://poller"), 0);
	void* req = zmq_socket(context, ZMQ_REQ);
	ASSERT_EQ(zmq_connect(req, "inproc://poller"), 0);

	stored::Poller poller;
	stored::PollableZmqSocket preq(rep, stored::Pollable::PollIn);
	EXPECT_EQ(poller.add(preq), 0);

	auto const* res = &poller.poll(0);
	EXPECT_EQ(res->size(), 0);
	EXPECT_EQ(errno, EAGAIN);

	zmq_send(req, "Hi", 2, 0);

	res = &poller.poll(0);
	ASSERT_EQ(res->size(), 1);
	EXPECT_EQ(res->at(0).revents, stored::Pollable::PollIn + 0);

	zmq_close(req);
	zmq_close(rep);
	zmq_ctx_destroy(context);
}
#endif

} // namespace
