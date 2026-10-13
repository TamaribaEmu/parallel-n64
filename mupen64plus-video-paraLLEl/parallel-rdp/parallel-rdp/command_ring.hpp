/* Copyright (c) 2020 Themaister
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <stddef.h>
#include <stdint.h>

#ifdef PARALLEL_RDP_SHADER_DIR
#include "global_managers.hpp"
#endif

namespace RDP
{
class CommandProcessor;
class CommandRing
{
public:
	void init(
#ifdef PARALLEL_RDP_SHADER_DIR
			Granite::Global::GlobalManagersHandle global_handles,
#endif
			CommandProcessor *processor, unsigned count);
	~CommandRing();
	void drain();

	void enqueue_command(unsigned num_words, const uint32_t *words);
	// A run of commands already in the ring's layout, [num_words][words...] repeated,
	// queued with one lock per stretch of free space instead of one per command.
	void enqueue_packed_commands(const uint32_t *packed, size_t num_packed_words);

private:
	CommandProcessor *processor = nullptr;
	std::thread thr;
	std::mutex lock;
	// The consumer waits on data_cond for commands; the producer waits on space_cond for
	// ring space or for drain(). Each side notifies only when the other is waiting: a
	// notify is a futex syscall on Android whether or not anyone sleeps.
	std::condition_variable data_cond;
	std::condition_variable space_cond;
	bool consumer_waiting = false;
	unsigned producers_waiting = 0;

	std::vector<uint32_t> ring;
	uint64_t write_count = 0;
	uint64_t read_count = 0;
	uint64_t completed_count = 0;

	void wait_for_space(std::unique_lock<std::mutex> &holder, uint64_t num_words);
	void copy_in(const uint32_t *words, size_t num_words);

	void thread_loop();
	void teardown_thread();
#ifdef PARALLEL_RDP_SHADER_DIR
	Granite::Global::GlobalManagersHandle global_handles;
#endif
};
}
