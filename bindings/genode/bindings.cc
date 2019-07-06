/*
 * Copyright (c) 2018-2019 Contributors as noted in the AUTHORS file
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

/* Solo5 includes */
extern "C" {
#include "../bindings.h"
extern struct mft_note __solo5_manifest_note;
}

// Compile the MFT utilities as C++
#define memset Genode::memset
#define strncmp Genode::strcmp
#include "../../tenders/common/mft.c"

namespace Solo5
{
	using namespace Genode;
	struct Device;
	struct Net_device;
	struct Block_device;
	struct Platform;

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
	 * it to a Unix epoch (9FRONT method).
	 */
	static Genode::uint64_t rtc_epoch(Genode::Env &env)
	{
		enum {
			SEC_PER_MIN  = 60,
			SEC_PER_HOUR = SEC_PER_MIN * 60,
			SEC_PER_DAY  = SEC_PER_HOUR * 24
		};

		// a brief connection to a RTC service
		auto ts = Rtc::Connection(env).current_time();

		Genode::uint64_t epoch = ts.second;
		epoch += ts.minute * SEC_PER_MIN;
		epoch += ts.hour   * SEC_PER_HOUR;

		// add seconds from the start of the month
		epoch += (ts.day-1) * SEC_PER_DAY;

		// add seconds from the start of the year
		int const *month_lengths = year_info(ts.year);
		for (unsigned m = 1; m < ts.month; ++m)
			epoch += month_lengths[m] * SEC_PER_DAY;

		// add seconds from 1970 to the start of this year
		for (unsigned y = 1970; y < ts.year; ++y)
			epoch += year_info(y)[0] * SEC_PER_DAY;

		return epoch;
	}
}


struct Solo5::Device
{
	virtual
	solo5_result_t
	net_info(solo5_net_info &info) {
		return SOLO5_R_EINVAL; }

	virtual
	solo5_result_t
	net_write(const uint8_t *buf, size_t size) {
		return SOLO5_R_EINVAL; }

	virtual
	solo5_result_t
	net_read(uint8_t *buf, size_t size, size_t &read_size) {
		return SOLO5_R_EINVAL; }

	virtual
	solo5_result_t
	block_info(solo5_block_info &info) {
		return SOLO5_R_EINVAL; }

	virtual
	solo5_result_t
	block_write(solo5_off_t offset, const uint8_t *buf, size_t size) {
		return SOLO5_R_EINVAL; }

	virtual
	solo5_result_t
	block_read(solo5_off_t offset, uint8_t *buf, size_t size) {
		return SOLO5_R_EINVAL; }
};


struct Solo5::Net_device final : Device
{
	enum { NIC_BUFFER_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE * 128 };

	Nic::Connection _nic;

	/**
	 * Invokes the `_handle_signal` method when
	 * signals for incoming packets are dispatched.
	 */
	Io_signal_handler<Net_device> _signal_handler;

	solo5_handle_set_t &_ready_set;
	solo5_handle_set_t  _handle;

	void _handle_signal()
	{
		_ready_set |= 1<<_handle;
	}

	Net_device(struct mft_entry &me,
	           Genode::Env &env,
	           Range_allocator &alloc,
	           solo5_handle_set_t ready_set,
	           solo5_handle_t handle)
	: _nic(env, &alloc, NIC_BUFFER_SIZE, NIC_BUFFER_SIZE, me.name)
	, _signal_handler(env.ep(), *this, &Net_device::_handle_signal)
	, _ready_set(ready_set), _handle(handle)
	{
		/*
		 * Register the signal context capability for our
		 * packet handler at the Nic service.
		 */
		_nic.rx_channel()->sigh_packet_avail(_signal_handler);
	}

	solo5_result_t
	net_info(solo5_net_info &info) override
	{
		Net::Mac_address nic_mac = _nic.mac_address();

		Genode::memcpy(
			info.mac_address, nic_mac.addr,
			min(sizeof(info.mac_address), sizeof(nic_mac.addr)));

		// MTU is unknown
		info.mtu = 1500;

		return SOLO5_R_OK;
	}

	solo5_result_t
	net_write(const uint8_t *buf, size_t size) override
	{
		auto &tx = *_nic.tx();

		/*
		 * release buffer packet space that
		 * has been processed by the server
		 */
		while (tx.ack_avail())
			tx.release_packet(tx.get_acked_packet());

		// do not block if packets congest at the server
		if (!tx.ready_to_submit())
			return SOLO5_R_AGAIN;

		// allocate a packet in the shared packet buffer
		try {
			auto pkt = tx.alloc_packet(size);
			// copy-in payload
			Genode::memcpy(tx.packet_content(pkt), buf, size);

			// signal the server
			tx.submit_packet(pkt);
			return SOLO5_R_OK;
		}

		catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
			// the packet buffer is full, try again later
			return SOLO5_R_AGAIN;
		}
	}

	solo5_result_t
	net_read(uint8_t *buf, size_t size, size_t &read_size) override
	{
		auto &rx = *_nic.rx();

		// check for queued packets from the server
		if (!rx.packet_avail() || !rx.ready_to_ack())
			return SOLO5_R_AGAIN;

		// copy-out payload
		auto pkt = rx.get_packet();
		size_t n = min(size, pkt.size());
		Genode::memcpy(buf, rx.packet_content(pkt), n);
		read_size = n;

		// inform the server that the packet is processed
		rx.acknowledge_packet(pkt);

		// TODO: flag if more packets are pending
		return SOLO5_R_OK;
	}
};


struct Solo5::Block_device final : Device
{
	Block::Connection<> _block;

	Block::Session::Info const _info { _block.info() };

	Block_device(struct mft_entry &me,
	             Genode::Env &env,
	             Range_allocator &alloc)
	: _block(env, &alloc, 128*1024, me.name)
	{ }

	solo5_result_t
	block_info(struct solo5_block_info &info) override
	{
		info.capacity   = _info.block_count * _info.block_size;
		info.block_size = _info.block_size;
		return SOLO5_R_OK;
	}

	solo5_result_t
	block_write(solo5_off_t offset, const uint8_t *buf, size_t size) override
	{
		if ((offset|size) % _info.block_size)
			return SOLO5_R_EINVAL;

		auto &source = *_block.tx();

		// allocate a region in the packet buffer
		Block::Packet_descriptor pkt(
			_block.alloc_packet(size),
			Block::Packet_descriptor::WRITE,
			offset / _info.block_size, size / _info.block_size);

		// copy-in write
		Genode::memcpy(source.packet_content(pkt), buf, pkt.size());

		// submit, block for response, release
		source.submit_packet(pkt);
		pkt = source.get_acked_packet();
		source.release_packet(pkt);

		return pkt.succeeded() ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
	}

	virtual
	solo5_result_t
	block_read(solo5_off_t offset, uint8_t *buf, size_t size) override
	{
		if ((offset|size) % _info.block_size)
			return SOLO5_R_EINVAL;

		auto &source = *_block.tx();

		// allocate a region in the packet buffer
		Block::Packet_descriptor pkt(
			_block.alloc_packet(size),
			Block::Packet_descriptor::READ,
			offset / _info.block_size, size / _info.block_size);

		// submit, block for response
		source.submit_packet(pkt);
		pkt = source.get_acked_packet();

		// copy-out read
		Genode::memcpy(buf, source.packet_content(pkt), pkt.size());

		// release packet region
		source.release_packet(pkt);

		return pkt.succeeded() ? SOLO5_R_OK : SOLO5_R_EUNSPEC;
	}
};


/**
 * Class for containing and initializing platform services
 */
struct Solo5::Platform
{
	static Platform *instance;
	static Device   *devices[MFT_MAX_ENTRIES];

	struct mft &mft;

	/**
	 * Reference to the Genode base enviroment
	 */
	Genode::Env &env;

	/**
	 * Default device
	 */
	Device invalid_device { };

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
	 * Allocator for Nic and Block packet metadata
	 */
	Heap heap { env.pd(), env.rm() };
	Allocator_avl pkt_alloc { &heap };

	/* No-op */
	void handle_timeout(Duration) { }

	solo5_handle_set_t nic_ready { 0 };

	/**
	 * Flag that an incoming packet is pending.
	 */
	void handle_nic() { nic_ready = true; }

	/**
	 * Initial wall time
	 *
	 * TODO: periodic RTC synchronization
	 */
	Genode::uint64_t _initial_epoch { rtc_epoch(env) };

	/**
	 * Commandline buffer
	 */
	typedef Genode::String<1024> Cmdline;
	Cmdline cmdline { };

	/**
	 * Constructor
	 */
	Platform(struct mft &mft, Genode::Env &env) : mft(mft), env(env)
	{
		/**
		 * Acquire and attach a ROM dataspace (shared
		 * read-only memory) containing our configuration
		 * as provided by our parent. This attachment is
		 * bound to the scope of this constructor.
		 */
		Attached_rom_dataspace config_rom(env, "config");
		Xml_node const config = config_rom.xml();

		// Copy-out the cmdline if configured.
		try { cmdline = config.sub_node("cmdline").decoded_content<Cmdline>(); }
		catch (...) { }

		for (solo5_handle_t i = 0U; i < MFT_MAX_ENTRIES; ++i) {
			devices[i] = &invalid_device;

			if (struct mft_entry *me = mft_get_by_index(&mft, i, MFT_BLOCK_BASIC)) {
				devices[i] = new (heap)
					Block_device(*me, env, pkt_alloc);
			}
			else
			if (struct mft_entry *me = mft_get_by_index(&mft, i, MFT_NET_BASIC)) {
				devices[i] = new (heap)
					Net_device(*me, env, pkt_alloc, nic_ready, i);
			}
		}
	}


	/********************
	 ** Solo5 bindings **
	 ********************/

	bool
	yield(solo5_time_t deadline_ns, solo5_handle_set_t *ready_set)
	{
		if (!nic_ready) {
			solo5_time_t deadline_us = deadline_ns / 1000;
			solo5_time_t now_us = timer.curr_time()
				.trunc_to_plain_us().value;

			// schedule a timeout signal
			yield_timeout.schedule(
				Genode::Microseconds{deadline_us - now_us});

			/*
			 * Block for Nic and Timer signals until a packet is
			 * pending or until the timeout expires.
			 * The handlers defined in the Net_device class will be
			 * invoked during "wait_and_dispatch_one_io_signal".
			 */
			while (!nic_ready && yield_timeout.scheduled())
				env.ep().wait_and_dispatch_one_io_signal();

			yield_timeout.discard();
		}

		if (nic_ready) {
			if (ready_set != nullptr) {
				*ready_set = nic_ready;
			}
			nic_ready = 0;
			return true;
		}

		return false;
	}

	solo5_result_t
	net_acquire(const char *name, solo5_handle_t &handle, solo5_net_info &info)
	{
		unsigned index = ~0;
		struct mft_entry *me = mft_get_by_name(&mft, name, MFT_NET_BASIC, &index); {
		if (me != nullptr && index < MFT_MAX_ENTRIES)
			handle = index;
			return devices[index]->net_info(info);
		}
		return SOLO5_R_EUNSPEC;
	}

	solo5_result_t
	block_acquire(const char *name, solo5_handle_t &handle, solo5_block_info &info)
	{
		unsigned index = ~0;
		struct mft_entry *me = mft_get_by_name(&mft, name, MFT_BLOCK_BASIC, &index);
		if (me != nullptr && index < MFT_MAX_ENTRIES) {
			handle = index;
			return devices[index]->block_info(info);
		}
		return SOLO5_R_EUNSPEC;
	}

	void exit(int status, void *cookie) __attribute__((noreturn))
	{
		if (cookie)
			log((Hex)addr_t(cookie));

		// inform the parent we wish to exit
		env.parent().exit(status);

		// deadlock
		Genode::sleep_forever();
	}
};


/**
 * Global pointer to platform object.
 */
using Solo5::Platform;
Platform *Platform::instance;
Solo5::Device *Platform::devices[MFT_MAX_ENTRIES];


/**
 * Solo5 C API
 */
extern "C" {

void solo5_exit(int status)
{
	Platform::instance->exit(status, nullptr);
}


void solo5_abort(void)
{
	Platform::instance->exit(SOLO5_EXIT_ABORT, nullptr);
}


solo5_time_t solo5_clock_monotonic(void)
{
	return Platform::instance->timer.curr_time()
		.trunc_to_plain_us().value*1000;
}


solo5_time_t solo5_clock_wall(void)
{
	return Platform::instance->_initial_epoch * 1000000000ULL
	     + Platform::instance->timer.curr_time()
	     	.trunc_to_plain_us().value * 1000ULL;
}


bool
solo5_yield(solo5_time_t deadline,
            solo5_handle_set_t *ready_set)
{
	return Platform::instance->yield(deadline, ready_set);
}


void
solo5_console_write(const char *buf, size_t size)
{
	// This isn't a buffered file-descriptor, strip the newline.
	while (buf[size-1] == '\0' || buf[size-1] == '\n')
		--size;

	Genode::log(Genode::Cstring(buf, size));
}


solo5_result_t
solo5_net_acquire(const char *name,
                  solo5_handle_t *handle,
                  struct solo5_net_info *info)
{
	return Platform::instance->net_acquire(name, *handle, *info);
}


solo5_result_t
solo5_net_write(solo5_handle_t handle, const uint8_t *buf, size_t size)
{
	return Platform::devices[handle]->net_write(buf, size);
}


solo5_result_t
solo5_net_read(solo5_handle_t handle, uint8_t *buf, size_t size, size_t *read_size)
{
	return Platform::devices[handle]->net_read(buf, size, *read_size);
}


solo5_result_t
solo5_block_acquire(const char *name,
                    solo5_handle_t *handle,
                    struct solo5_block_info *info)
{
	return Platform::instance->block_acquire(name, *handle, *info);
}


solo5_result_t
solo5_block_write(solo5_handle_t handle, solo5_off_t offset,
                  const uint8_t *buf, size_t size)
{
	return Platform::devices[handle]->block_write(offset, buf, size);
}


solo5_result_t
solo5_block_read(solo5_handle_t handle, solo5_off_t offset,
                 uint8_t *buf, size_t size)
{
	return Platform::devices[handle]->block_read(offset, buf, size);
}


solo5_result_t
solo5_set_tls_base(uintptr_t base)
{
    return SOLO5_R_EUNSPEC;
}

} // extern "C"


/**
 * Genode component entrypoint
 *
 * This is the entry symbol invoked by the dynamic loader.
 */
void Component::construct(Genode::Env &env)
{
	/* Validate the device manifest */
	struct mft &mft = __solo5_manifest_note.m;
	size_t mft_size = __solo5_manifest_note.h.descsz;
	if (mft_validate(&mft, mft_size) != 0) {
		Genode::error("Solo5: Built-in manifest validation failed. Aborting");
		env.parent().exit(~0);
		return;
	}

	/* Construct a statically allocated platform object */
	static Solo5::Platform inst(mft, env);
	Platform::instance = &inst;

	static struct solo5_start_info si {
		.cmdline = Platform::instance->cmdline.string(),
	};

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

	/* block for application then exit */
	env.parent().exit(solo5_app_main(&si));
}


/**
 * Override stack size of initial entrypoint
 */
size_t Component::stack_size() {
	return 1<<19; }
