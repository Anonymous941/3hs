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

#include "httpclient.hh"
#include "update.hh" /* includes net constants like USER_AGENT */
#include "proxy.hh"
#include "error.hh"
#include "panic.hh"
#include "log.hh"
#include "ctr.hh"
#include <string.h>

#include "build/hscert_der.h"

#ifdef RELEASE
	#include <ui/base.hh>
#endif

#define TRYJ( expr ) if(R_FAILED(res = ( expr ))) goto fail
#define HTTP_MAX_REDIRECT 10

extern "C" void        hsapi_password(char *); /* hsapi_auth.c */
extern "C" const int   hsapi_password_length;  /* hsapi_auth.c */
extern "C" const char *hsapi_user;             /* hsapi_auth.c */

static http::ResumableDownload *current_download = nullptr;
static LightLock current_download_lock;

void http::ResumableDownload::global_abort()
{
	for(http::ResumableDownload *dl = current_download; dl; dl = dl->next)
	{
		dl->abort();
		dl->close_handle();
	}
}

http::ResumableDownload::ResumableDownload()
{
	this->buffer = malloc(http::ResumableDownload::ChunkMaxSize);
	panic_assert(this->buffer, "failed to allocate for download buffer");

	static bool lock_is_init = false;
	if(!lock_is_init)
	{
		LightLock_Init(&current_download_lock);
		lock_is_init = true;
	}
	ctr::LockedInScope slock { &current_download_lock };

	if(current_download)
	{
		this->next = current_download->next;
		this->prev = current_download;
		current_download->next = this;
	}
	else
	{
		this->next = this->prev = nullptr;
		current_download = this;
	}
}

http::ResumableDownload::~ResumableDownload()
{
	free(this->buffer);

	ctr::LockedInScope slock { &current_download_lock };

	if(this->next) this->next->prev = this->prev;
	if(this->prev) this->prev->next = this->next;
	if(current_download == this)
	{
		panic_assert(!this->prev, "invalid state");
		current_download = this->next;
	}

	this->abort();
	this->close_handle();
}

void http::ResumableDownload::close_handle()
{
	if(this->flags & http::ResumableDownload::flag_active)
	{
		if(!(this->flags & http::ResumableDownload::flag_exit))
			httpcCancelConnection(&this->hctx);
		httpcCloseContext(&this->hctx);
	}
}

Result http::ResumableDownload::perform_execute_once(const char *url, int redirection_depth)
{
	u32 pos, prev_pos = 0;
	size_t chunk_size;
	Result res = 0, nres;
	u32 status;

	if(R_FAILED(res = this->setup_handle(url)))
		return res;

	TRYJ(httpcBeginRequest(&this->hctx));
	TRYJ(httpcGetResponseStatusCodeTimeout(&this->hctx, &status, this->timeout));

#ifdef FULL_LOG
	{
		const char *mstr = nullptr;
		switch(this->method)
		{
		case HTTPC_METHOD_GET: mstr = "GET"; break;
		case HTTPC_METHOD_POST: mstr = "POST"; break;
		case HTTPC_METHOD_HEAD: mstr = "HEAD"; break;
		case HTTPC_METHOD_PUT: mstr = "PUT"; break;
		case HTTPC_METHOD_DELETE: mstr = "DELETE"; break;
		}
		vlog("Status code on URL %s '%s' is %ld.", mstr, url, status);
	}
#endif

	/* we may need to redirect (status code 3xx) */
	if(status / 100 == 3)
	{
		if(redirection_depth == HTTP_MAX_REDIRECT)
		{
			elog("Reached maximum amount of redirects (%d)", HTTP_MAX_REDIRECT);
			return APPERR_MAX_REDIRECTS;
		}

		char newurl[1024];
		TRYJ(httpcGetResponseHeader(&this->hctx, "location", newurl, sizeof(newurl)));
		newurl[sizeof(newurl) - 1] = '\0';

		this->close_handle();
		vlog("Following redirect to %s", newurl);
		return this->perform_execute_once(newurl, redirection_depth + 1);
	}

	if((this->downloadedSize == 0 && status != 200) || (this->downloadedSize != 0 && status != 206))
	{
#ifdef RELEASE
		// We _may_ require a different 3hs version
		if(status == 400)
		{
			char buffer[1024];
			/* we can assume it doesn't have the header if this fails */
			if(R_SUCCEEDED(httpcGetResponseHeader(&this->hctx, "x-minimum", buffer, sizeof(buffer))))
			{
				this->close_handle();
				ui::RenderQueue::terminate_render();
				ui::notice(PSTRING(min_constraint, VVERSION, buffer));
				exit(1);
			}
		}
#endif
		elog("Status code indicating failure, expected %d, got %ld", this->downloadedSize == 0 ? 200 : 206, status);
		res = this->downloadedSize == 0 ? (status == 413 ? APPERR_TOO_LARGE : APPERR_NON200) : APPERR_NORANGE;
		goto fail;
	}

	if(this->downloadedSize == 0)
	{
		TRYJ(httpcGetDownloadSizeState(&this->hctx, nullptr, &this->totalSize));
		if(R_FAILED(res = this->on_total_size_try_get_()))
			goto fail;
		if(this->notify_event)
			LightEvent_Signal(this->notify_event);
	}

	panic_assert(this->buffer, "buffer should be allocated");

	do {
		if(this->flags & http::ResumableDownload::flag_exit) goto cancel;
		res = httpcReceiveDataTimeout(&this->hctx, (u8 *) this->buffer, http::ResumableDownload::ChunkMaxSize, this->timeout);
		if(this->flags & http::ResumableDownload::flag_exit) goto cancel;

		nres = httpcGetDownloadSizeState(&this->hctx, &pos, nullptr);
		if(R_FAILED(nres)) res = nres;
		if((R_FAILED(res) && res != (Result) HTTPC_RESULTCODE_DOWNLOADPENDING))
		{
			elog("aborted http connection due to error: %08lX.", res);
			goto fail;
		}
		if(res == (Result) HTTPC_RESULTCODE_DOWNLOADPENDING)
			panic_assert(pos == prev_pos + http::ResumableDownload::ChunkMaxSize, "partial chunk when full expected");
		chunk_size = pos - prev_pos;
		nres = this->on_chunk_(chunk_size);
		/* we can't just assign res = nres becaues res may be either
		 * HTTPC_RESULTCODE_DOWNLOADPENDING or 0, which we can't discard */
		if(R_FAILED(nres)) res = nres;
		if(this->notify_event)
			LightEvent_Signal(this->notify_event);
		this->downloadedSize += chunk_size;
		prev_pos = pos;
	} while(res == (Result) HTTPC_RESULTCODE_DOWNLOADPENDING);

	if(res == 0 && this->notify_event)
		LightEvent_Signal(this->notify_event);

fail:
	this->close_handle();
	this->flags &= ~(http::ResumableDownload::flag_active | http::ResumableDownload::flag_exit);
	return res;
cancel:
	res = APPERR_CANCELLED;
	goto fail;
}

Result http::ResumableDownload::execute_once()
{
	/* we require the sleep lock for downloads and such, lets just ensure it's available */
	Result res = ctr::increase_sleep_lock_ref();
	if(R_FAILED(res)) elog("failed to acquire sleep lock: %08lX", res);
	vlog("Trying to download URL '%s'...", this->url.c_str());
	res = this->perform_execute_once(this->url.c_str(), 0);
	ctr::decrease_sleep_lock_ref();
	return res;
}

Result http::ResumableDownload::setup_handle(const char *url)
{
	char *password;
	Result res;

	/* the last argument is use_default_proxy, we don't want that since we set it ourselves later */
	TRYJ(httpcOpenContext(&this->hctx, this->method, url, 0));
	if(hscert_der_bin_size && strncmp(url, "https:", 6) == 0)
		TRYJ(httpcAddTrustedRootCA(&this->hctx, hscert_der_bin, hscert_der_bin_size));
	TRYJ(httpcSetSSLOpt(&this->hctx, SSLCOPT_DisableVerify));
	TRYJ(httpcAddRequestHeaderField(&this->hctx, "User-Agent", USER_AGENT));
	if(this->flags & http::ResumableDownload::flag_auth)
	{
		TRYJ(httpcAddRequestHeaderField(&this->hctx, "X-Auth-User", hsapi_user));
		/*TRYJ(httpcAddRequestHeaderField(&this->hctx, "X-Auth-Password", password));*/password=(char*)malloc(hsapi_password_length+1);hsapi_password(password);password[hsapi_password_length]=0;TRYJ(httpcAddRequestHeaderField(&this->hctx,"X-Auth-Password",password));memset(password,0,hsapi_password_length);free(password);
	}
	if(this->postdataPtr && this->postdataLen)
		TRYJ(httpcAddPostDataRaw(&this->hctx, (const u32 *) this->postdataPtr, this->postdataLen));
	/* we start from downloadedSize; 0 initially and it increments after the second execute_once() */
	if(this->downloadedSize != 0)
	{
		std::string val = "bytes=" + std::to_string(this->downloadedSize) + "-";
		TRYJ(httpcAddRequestHeaderField(&this->hctx, "Range", val.c_str()));
	}
	TRYJ(proxy::apply(&this->hctx));

	this->flags |= http::ResumableDownload::flag_active;
	return 0;
fail:
	this->close_handle();
	return res;
}

void http::ResumableDownload::abort()
{
	this->flags |= http::ResumableDownload::flag_exit;
	if(this->flags & http::ResumableDownload::flag_active)
		httpcCancelConnection(&this->hctx);
}

