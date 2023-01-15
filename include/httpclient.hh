/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef inc_httpclient_hh
#define inc_httpclient_hh

#include <functional>
#include <string>
#include <3ds.h>


namespace http
{

	class ResumableDownload
	{
	public:
		static constexpr u64 DefaultTimeout = 10000000000L; /* 10 seconds */
		static constexpr size_t ChunkMaxSize = 0x20000;

		static void global_abort();

		ResumableDownload();
		~ResumableDownload();

		/* Tries to execute the download, once, when called
		 * again it continues the download if there was still
		 * data left to download
		 */
		Result execute_once();

		void abort();

		void on_total_size_try_get(std::function<Result()> func) { this->on_total_size_try_get_ = func; }
		void requires_authentication() { this->flags |= http::ResumableDownload::flag_auth; }
		void on_chunk(std::function<Result(size_t)> func) { this->on_chunk_ = func; }

		/* Event that is signaled when the state changes, it is the equivalent of
		 * signalling the event in both on_chunk() and on_total_size_try_get(), additionally
		 * it also signals when the download is completed
		 * Note that this event is signaled *after* the callbacks are called */
		void set_notify_event(LightEvent *event) { this->notify_event = event; }
		/* 0 if total size is not known (yet), after on_total_size_try_get() it is known
		 * if the total size is known */
		u32 maybe_total_size() { return this->totalSize; }
		/* returns the amount of bytes downloaded since the creation of this object
		 * note that this count is only increment *after* on_chunk() is called and the
		 * notification event is signaled */
		u32 downloaded() { return this->downloadedSize; }
		/* returns the temporary buffer where data is downloaded in, should be used
		 * in on_chunk(size_t buffer_size), the parameter is the usable byte count in
		 * the buffer returned by data_buffer() */
		template <typename T = u8>
		const T *data_buffer() { return (T *) this->buffer; }

		/* the data pointer must remain valid as long as the ResumableDownload is */
		void set_postdata(const void *data, u32 len)
		{
			this->postdataPtr = data;
			this->postdataLen = len;
		}

		void set_target(const std::string& url, HTTPC_RequestMethod method)
		{
			this->url = url;
			this->method = method;
		}

		void set_timeout(u64 ntm)
		{
			this->timeout = ntm;
		}

	private:
		std::function<Result()> on_total_size_try_get_ = []() -> Result { return 0; };
		std::function<Result(size_t)> on_chunk_ = [](size_t) -> Result { return 0; };

		u64 timeout = DefaultTimeout;

		void *buffer;

		u32 totalSize = 0, downloadedSize = 0;

		HTTPC_RequestMethod method;
		std::string url;

		LightEvent *notify_event = nullptr;

		Result perform_execute_once(const char *url, int redirection_depth);
		Result setup_handle(const char *url);
		void close_handle();

		const void *postdataPtr = nullptr;
		u32 postdataLen = 0;

		httpcContext hctx;

		enum {
			flag_active = 1,
			flag_exit   = 2,
			flag_auth   = 4,
		}; int flags = 0;

		/* linked list for the global current downloads list */
		http::ResumableDownload *next, *prev;
	};
}

#endif

