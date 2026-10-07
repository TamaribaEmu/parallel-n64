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

#include <chrono>
#include "command_ring.hpp"
#include "rdp_device.hpp"
#include "thread_id.hpp"
#include "thread_name.hpp"
#include <assert.h>
#ifdef __ANDROID__
#include <sched.h>
#endif

namespace RDP
{
void CommandRing::init(
#ifdef PARALLEL_RDP_SHADER_DIR
		Granite::Global::GlobalManagersHandle global_handles_,
#endif
		CommandProcessor *processor_, unsigned count)
{
	assert((count & (count - 1)) == 0);
	teardown_thread();
	processor = processor_;
	ring.resize(count);
	write_count = 0;
	read_count = 0;
#ifdef PARALLEL_RDP_SHADER_DIR
	global_handles = std::move(global_handles_);
#endif
	thr = std::thread(&CommandRing::thread_loop, this);
}

void CommandRing::teardown_thread()
{
	if (thr.joinable())
	{
		enqueue_command(0, nullptr);
		thr.join();
	}
}

CommandRing::~CommandRing()
{
	teardown_thread();
}

void CommandRing::drain()
{
	std::unique_lock<std::mutex> holder{lock};
	cond.wait(holder, [this]() {
		return write_count == completed_count;
	});
}

void CommandRing::enqueue_command(unsigned num_words, const uint32_t *words)
{
	bool was_empty;
	{
		std::unique_lock<std::mutex> holder{lock};
		cond.wait(holder, [this, num_words]() {
			return write_count + num_words + 1 <= read_count + ring.size();
		});

		// The consumer only sleeps when it observes an empty ring (its predicate
		// is write_count > read_count, evaluated under this same lock). So a
		// notify is only required on the empty -> non-empty transition; skipping
		// it otherwise cannot lose a wakeup, and the consumer's 500us wait_for
		// is a further backstop. This removes one futex wake per RDP command,
		// which on a 4x A57 was a large share of the emulation thread.
		was_empty = write_count == read_count;

		size_t mask = ring.size() - 1;
		ring[write_count++ & mask] = num_words;
		for (unsigned i = 0; i < num_words; i++)
			ring[write_count++ & mask] = words[i];
	}

	// Notified outside the lock so the woken consumer does not immediately
	// block on a mutex we still hold.
	if (was_empty)
		cond.notify_one();
}

void CommandRing::thread_loop()
{
	Util::set_current_thread_name("parallel-rdp");
#ifdef __ANDROID__
	cpu_set_t affinity = {};
	CPU_ZERO(&affinity);
	CPU_SET(2, &affinity);
	sched_setaffinity(0, sizeof(affinity), &affinity);
#endif
	Util::register_thread_index(0);

#ifdef PARALLEL_RDP_SHADER_DIR
	// Here to let the RDP play nice with full Granite.
	// When we move to standalone Granite, we won't need to interact with global subsystems like this.
	Granite::Global::set_thread_context(*global_handles);
	global_handles.reset();
#endif

	// Commands are drained in bulk rather than one per lock acquisition. The
	// batch is packed as [num_words][words...] repeated, and is bounded by the
	// ring size because read_count can never trail write_count by more than
	// that.
	std::vector<uint32_t> batch;
	batch.reserve(ring.size());
	size_t mask = ring.size() - 1;
	// The experimental core is a separate shared library. Keep its command
	// wait latency aligned with Accurate while preserving batch ordering.
#ifdef PARALLEL_N64_EXPERIMENTAL_CORE
	constexpr auto command_wait = std::chrono::microseconds(500);
#else
	constexpr auto command_wait = std::chrono::microseconds(500);
#endif

	for (;;)
	{
		bool is_idle = false;
		bool saw_exit = false;
		uint64_t drained_up_to = 0;
		batch.clear();

		{
			std::unique_lock<std::mutex> holder{lock};
			if (cond.wait_for(holder, command_wait, [this]() { return write_count > read_count; }))
			{
				// Drain in bulk, but bounded. Amortizing the lock over many
				// commands is the point; draining an unbounded run is not,
				// because the batch executes before the GPU sees any of it and
				// submission then arrives in large infrequent bursts, which
				// shows up as uneven frame delivery. A cap keeps submission
				// fine-grained while still removing almost all of the
				// per-command lock traffic. Whatever is left in the ring is
				// picked up on the next iteration without sleeping, since
				// wait_for's predicate is already satisfied.
				constexpr unsigned max_commands_per_batch = 32;
				unsigned batched_commands = 0;

				// The producer writes a command's length word and its payload in
				// one critical section, so write_count > read_count guarantees
				// the next command is complete.
				while (write_count > read_count && batched_commands < max_commands_per_batch)
				{
					uint32_t num_words = ring[read_count & mask];
					if (num_words == 0)
					{
						// Zero-length command is teardown_thread()'s exit
						// sentinel. Anything already batched still runs first.
						read_count++;
						saw_exit = true;
						break;
					}
					read_count++;
					batch.push_back(num_words);
					for (uint32_t i = 0; i < num_words; i++)
						batch.push_back(ring[read_count++ & mask]);
					batched_commands++;
				}
				// read_count is what the producer's free-space predicate tests,
				// so ring space is released here, before the batch executes.
				drained_up_to = read_count;
			}
			else
			{
				// If we don't receive commands at a steady pace,
				// notify rendering thread that we should probably kick some work.
				is_idle = true;
			}
		}

		if (is_idle)
		{
			const uint32_t meta_idle = uint32_t(Op::MetaIdle) << 24;
			processor->enqueue_command_direct(1, &meta_idle);
			continue;
		}

		// Wake a producer that was blocked on ring space.
		cond.notify_all();

		// enqueue_command_direct dispatches exactly one command, so walk the
		// batch. Done outside the lock: this is the actual RDP work.
		for (size_t i = 0; i < batch.size(); )
		{
			const uint32_t num_words = batch[i++];
			processor->enqueue_command_direct(num_words, &batch[i]);
			i += num_words;
		}

		{
			// completed_count means "executed", which is what drain() waits on,
			// so it is only published now that the batch has actually run.
			std::lock_guard<std::mutex> holder{lock};
			completed_count = drained_up_to;
		}
		cond.notify_all();

		if (saw_exit)
			break;
	}
}
}
