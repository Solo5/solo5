/*
 * Copyright (c) 2018 Contributors as noted in the AUTHORS file
 *
 * This file is part of Solo5, a sandboxed execution environment.
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Note: these bindings are not to be built by a standard
 * C++ compiler, they are instead built by the Genode
 * toolchain and build system provided elsewhere.
 */

/* Genode includes */
#include <block_session/connection.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/heap.h>
#include <base/component.h>
#include <base/sleep.h>

/* Jitterentropy includes */
#include <jitterentropy.h>

/* Solo5 includes */
extern "C" {
#include "../bindings.h"
}


namespace Solo5 {
	using namespace Genode;
	struct Platform;
};


/**
 * Naive SSP implementation
 */
extern "C" {
	Genode::addr_t __stack_chk_guard;

	extern "C" __attribute__((noreturn))
	void __stack_chk_fail (void)
	{
		Genode::error("stack smashing detected");
		solo5_abort();
	}
}

/**
 * Generate a random stack canary value from CPU jitter
 */
static Genode::addr_t generate_canary(Genode::Allocator &alloc)
{
	using namespace Genode;

	auto die = [] (char const *msg)
	{
		error(msg);
		throw Exception();
	};

	Genode::addr_t canary = 0;

	jitterentropy_init(alloc);
	if (jent_entropy_init())
		die("jitterentropy library could not be initialized!");

	struct rand_data *ec = jent_entropy_collector_alloc(0, 0);
	if (!ec)
		die("failed to allocate jitter entropy collector");

	ssize_t n = jent_read_entropy(ec, (char*)&canary, sizeof(canary));
	if (n != sizeof(canary) || !canary)
		die("failed to generate stack canary");

	jent_entropy_collector_free(ec);

	/* inject a NULL byte to terminate C strings */
	return canary & (~(Genode::addr_t)0xff00UL);
}


/**
 * Class for containing and initializing platform services
 */
struct Solo5::Platform
{
	/**
	 * Reference to the Genode base enviroment
	 */
	Genode::Env &env;

	/**
	 * Connection to Timer service, this provides
	 * our running time and timeout signals.
	 */
	Timer::Connection timer { env, "solo5" };

	/**
	 * Timeount handler, calls the `handle_timeout`
	 * method when timer signals are dispatched.
	 */
	Timer::One_shot_timeout<Platform> yield_timeout {
		timer, *this, &Platform::handle_timeout };

	/**
	 * Initial wall time
	 *
	 * TODO: periodic RTC synchronization
	 */
	Genode::uint64_t _initial_epoch = 0;

	/**
	 * Allocator for Nic and Block packet metadata
	 */
	Heap heap { env.pd(), env.rm() };
	Allocator_avl pkt_alloc { &heap };

	/**
	 * Optional connection to network service
	 */
	Constructible<Nic::Connection> nic { };

	/**
	 * Optional connection to block service
	 */
	Constructible<Block::Connection> block { };

	Block::sector_t blk_count = 0;
	Genode::size_t blk_size = 0;

	/**
	 * Incoming Nic packet handler, calls the `handle_nic`
	 * method when Nic signals are dispatched.
	 */
	Io_signal_handler<Platform> nic_handler {
		env.ep(), *this, &Platform::handle_nic };

	/* No-op */
	void handle_timeout(Duration) { }

	bool nic_ready = false;

	/**
	 * Flag that an incoming packet is pending.
	 */
	void handle_nic() { nic_ready = true; }

	/**
	 * Month and leap year conversion
	 */
	static int const *year_info(int year)
	{
		static const int standard_year[] =
			{ 365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		static const int leap_year[] =
			{ 366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

		return ((year%4) == 0 && ((year%100) != 0 || (year%400) == 0))
			? leap_year : standard_year;
	}

	/**
	 * Collect a timestamp from the RTC service and convert
	 * it to a Unix epoch.
	 */
	static Genode::uint64_t rtc_epoch(Genode::Env &env)
	{
		enum {
			SEC_PER_MIN  = 60,
			SEC_PER_HOUR = SEC_PER_MIN * 60,
			SEC_PER_DAY  = SEC_PER_HOUR * 24
		};

		/* a brief connection to a RTC service */
		auto ts = Rtc::Connection(env).current_time();

		Genode::uint64_t epoch = ts.second;
		epoch += ts.minute * SEC_PER_MIN;
		epoch += ts.hour   * SEC_PER_HOUR;

		/* add seconds from the start of the month */
		epoch += (ts.day-1) * SEC_PER_DAY;

		/* add seconds from the start of the year */
		int const *month_lengths = year_info(ts.year);
		for (unsigned m = 1; m < ts.month; ++m)
			epoch += month_lengths[m] * SEC_PER_DAY;

		/* add seconds from 1970 to the start of this year */
		for (unsigned y = 1970; y < ts.year; ++y)
			epoch += year_info(y)[0] * SEC_PER_DAY;

		return epoch;
	}

	/**
	 * Commandline buffer
	 */
	typedef Genode::String<1024> Cmdline;
	Cmdline cmdline { };

	/**
	 * Constructor
	 */
	Platform(Genode::Env &e) : env(e)
	{
		/**
		 * Acquire and attach a ROM dataspace (shared
		 * read-only memory) containing our configuration
		 * as provided by our parent. This attachment is
		 * bound to the scope of this constructor.
		 */
		Attached_rom_dataspace config_rom(env, "config");
		Xml_node const config = config_rom.xml();

		bool has_rtc = config.has_sub_node("rtc");
		bool has_nic = config.has_sub_node("nic");
		bool has_blk = config.has_sub_node("blk");

		if (has_rtc) {
			_initial_epoch = rtc_epoch(env);
		}

		/*
		 * create sessions early to subtract session
		 * buffers from quota before the application
		 * heap is created
		 */

		if (has_nic) {
			enum { NIC_BUFFER_SIZE =
				Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

			nic.construct(
				env, &pkt_alloc,
				NIC_BUFFER_SIZE, NIC_BUFFER_SIZE,
				"guest");

			/*
			 * Register the signal context capability for our
			 * packet handler at the Nic service.
			 */
			nic->rx_channel()->sigh_packet_avail(nic_handler);
		}

		if (has_blk) {
			block.construct(env, &pkt_alloc, 128*1024, "guest");
		}

		/**
		 * Copy-out the cmdline if configured.
		 */
		try {
			config.sub_node("solo5")
				.attribute("cmdline")
					.value(&cmdline);
		}
		catch (...) { }
	}


	/********************
	 ** Solo5 bindings **
	 ********************/

	void net_info(struct solo5_net_info &info)
	{
		if (!nic.constructed()) {
			Genode::error("network device not available");
			return;
		}

		Net::Mac_address nic_mac = nic->mac_address();

		Genode::memcpy(
			info.mac_address, nic_mac.addr,
			min(sizeof(info.mac_address), sizeof(nic_mac.addr)));

		/* MTU is unknown */
		info.mtu = 1500;
	}

	solo5_result_t net_write(const uint8_t *buf, size_t size)
	{
		if (!nic.constructed())
			return SOLO5_R_EUNSPEC;

		auto &tx = *nic->tx();

		/*
		 * release buffer packet space that
		 * has been processed by the server
		 */
		while (tx.ack_avail())
			tx.release_packet(tx.get_acked_packet());

		/* do not block if packets congest at the server */
		if (!tx.ready_to_submit())
			return SOLO5_R_AGAIN;

		/* allocate a packet in the shared packet buffer */
		try {
			auto pkt = tx.alloc_packet(size);
			/* copy-in payload */
			Genode::memcpy(tx.packet_content(pkt), buf, size);

			/* signal the server */
			tx.submit_packet(pkt);
			return SOLO5_R_OK;
		}

		catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
			/* the packet buffer is full, try again later */
			return SOLO5_R_AGAIN;
		}
	}

	solo5_result_t net_read(uint8_t *buf, size_t size, size_t *read_size)
	{
		if (!nic.constructed())
			return SOLO5_R_EUNSPEC;

		auto &rx = *nic->rx();

		/* check for queued packets from the server */
		if (!rx.packet_avail() || !rx.ready_to_ack())
			return SOLO5_R_AGAIN;

		/* copy-out payload */
		auto pkt = rx.get_packet();
		size_t n = min(size, pkt.size());
		Genode::memcpy(buf, rx.packet_content(pkt), n);
		*read_size = n;

		/* inform the server that the packet is processed */
		rx.acknowledge_packet(pkt);

		/* flag if more packets are pending */
		nic_ready = rx.packet_avail();
		return SOLO5_R_OK;
	}

	void block_info(struct solo5_block_info &info)
	{
		if (!block.constructed()) {
			Genode::error("block device not available");
			return;
		}

		Block::Session::Operations ops;
		block->info(&blk_count, &blk_size, &ops);

		info.capacity = blk_count * blk_size;
		info.block_size = blk_size;
	}

	solo5_result_t block_write(solo5_off_t offset,
	                           const uint8_t *buf,
	                           size_t size)
	{
		if ((offset|size) % blk_size)
			return SOLO5_R_EINVAL;
		if (!block.constructed())
			return SOLO5_R_EUNSPEC;

		auto &source = *block->tx();

		/* allocate a region in the packet buffer */
		Block::Packet_descriptor pkt(
			source.alloc_packet(size),
			Block::Packet_descriptor::WRITE,
			offset / blk_size, size / blk_size);

		/* copy-in write */
		Genode::memcpy(source.packet_content(pkt), buf, pkt.size());

		/* submit, block for response, release */
		source.submit_packet(pkt);
		pkt = source.get_acked_packet();
		source.release_packet(pkt);

		return pkt.succeeded() ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
	}

	solo5_result_t block_read(solo5_off_t offset, uint8_t *buf, size_t size)
	{
		if ((offset|size) % blk_size)
			return SOLO5_R_EINVAL;
		if (!block.constructed())
			return SOLO5_R_EUNSPEC;

		auto &source = *block->tx();

		/* allocate a region in the packet buffer */
		Block::Packet_descriptor pkt(
			source.alloc_packet(size),
			Block::Packet_descriptor::READ,
			offset / blk_size, size / blk_size);

		/* submit, block for response */
		source.submit_packet(pkt);
		pkt = source.get_acked_packet();

		/* copy-out read */
		Genode::memcpy(buf, source.packet_content(pkt), pkt.size());

		/* release packet region */
		source.release_packet(pkt);

		return pkt.succeeded() ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
	}

	void exit(int status, void *cookie) __attribute__((noreturn))
	{
		if (cookie)
			log((Hex)addr_t(cookie));

		/* inform the parent we wish to exit */
		env.parent().exit(status);

		/* deadlock */
		Genode::sleep_forever();
	}
};


/**
 * Global pointer to platform object.
 */
static Solo5::Platform *_platform;


/**
 * Solo5 C API
 */
extern "C" {

void solo5_exit(int status)
{
	_platform->exit(status, nullptr);
}


void solo5_abort(void)
{
	_platform->exit(SOLO5_EXIT_ABORT, nullptr);
}


solo5_time_t solo5_clock_monotonic(void)
{
	return _platform->timer.curr_time()
		.trunc_to_plain_us().value*1000;
}


solo5_time_t solo5_clock_wall(void)
{
	return _platform->_initial_epoch * 1000000000ULL
	     + _platform->timer.curr_time().trunc_to_plain_us().value * 1000ULL;
}


bool solo5_yield(solo5_time_t deadline_ns)
{
	if (_platform->nic_ready) return true;

	solo5_time_t deadline_us = deadline_ns / 1000;
	solo5_time_t now_us = _platform->timer.curr_time()
		.trunc_to_plain_us().value;

	/* schedule a timeout signal */
	_platform->yield_timeout.schedule(
		Genode::Microseconds{deadline_us - now_us});

	/*
	 * Block for Nic and Timer signals until a packet is
	 * pending or until the timeout expires.
	 * The handlers defined in the Platform class will be
	 * invoked during "wait_and_dispatch_one_io_signal".
	 */
	while (!_platform->nic_ready && _platform->yield_timeout.scheduled())
		_platform->env.ep().wait_and_dispatch_one_io_signal();

	_platform->yield_timeout.discard();
	return _platform->nic_ready;
}


void solo5_console_write(const char *buf, size_t size)
{
	/* This isn't a buffered file-descriptor, strip the newline. */
	while (buf[size-1] == '\0' || buf[size-1] == '\n')
		--size;

	Genode::log(Genode::Cstring(buf, size));
}


void solo5_net_info(struct solo5_net_info *info)
{
	Genode::memset(info, 0, sizeof(info));
	_platform->net_info(*info);
}


solo5_result_t solo5_net_write(const uint8_t *buf, size_t size)
{
	return _platform->net_write(buf, size);
}


solo5_result_t solo5_net_read(uint8_t *buf, size_t size, size_t *read_size)
{
	return _platform->net_read(buf, size, read_size);
}


void solo5_block_info(struct solo5_block_info *info)
{
	Genode::memset(info, 0, sizeof(info));
	_platform->block_info(*info);
}


solo5_result_t solo5_block_write(solo5_off_t offset,
                                 const uint8_t *buf,
                                 size_t size)
{
	return _platform->block_write(offset, buf, size);
}


solo5_result_t solo5_block_read(solo5_off_t offset,
                                uint8_t *buf, size_t size)
{
	return _platform->block_read(offset, buf, size);
}

} // extern "C"


/**
 * Genode compontent entrypoint
 *
 * This is the entry symbol invoked by the dynamic loader.
 */
void Component::construct(Genode::Env &env)
{
	/* Construct a statically allocated platform object */
	static Solo5::Platform inst(env);
	_platform = &inst;

	static struct solo5_start_info si;

	si.cmdline = _platform->cmdline.string();

	/*
	 * Use the remaining memory quota for the
	 * application heap minus a reservation for
	 * allocation metadata.
	 */
	si.heap_size = env.pd().avail_ram().value;
	if (si.heap_size > 1<<20)
		si.heap_size -= 1<<19;

	/* allocate a contiguous memory region for the application */
	Genode::Dataspace_capability heap_ds =
		env.pd().alloc(si.heap_size);

	/* attach into our address-space */
	si.heap_start = env.rm().attach(heap_ds);

	/*
	 * set a new stack canary, this is possible
	 * because this procedure never returns
	 */
	__stack_chk_guard = generate_canary(inst.heap);

	/* block for application then exit */
	env.parent().exit(solo5_app_main(&si));

	/* deadlock */
	Genode::sleep_forever();
}


/**
 * Override stack size of initial entrypoint
 */
size_t Component::stack_size() {
	return 1<<19; }
