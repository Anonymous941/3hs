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
#include "settings.hh"
#include "install.hh"
#include "thread.hh"
#include "update.hh" /* includes net constants */
#include "error.hh"
#include "proxy.hh"
#include "panic.hh"
#include "ctr.hh"
#include "log.hh"

#include <3ds.h>

namespace ui
{
	void scan_keys();
	u32 kDown();
	u32 kHeld();
}

using expandable_binary_data_type = std::basic_string<u8>;

namespace install { Handle active_cia_handle = CIA_HANDLE_INVALID; }
using install::active_cia_handle;

/* defined in file_fwd.cc */
Result install_forwarder(u8 *data, size_t len);

enum class ThreadState {
	Installing,
	Timeout,
	Abort,
	Finished,
};

struct thread_data {
	http::ResumableDownload *downloader;
	get_url_func *get_url;
	LightEvent thread_to_ui_event;
	LightEvent ui_to_thread_event;
	Result last_result;
	ThreadState state;
};

static void install_generic_thread(thread_data *data)
{
	std::string url;
	Result res;

	data->downloader->set_notify_event(&data->thread_to_ui_event);

	if(!ISET_RESUME_DOWNLOADS)
	{
		res = (*data->get_url)(url);
		if(R_FAILED(res))
		{
			elog("failed to fetch URL: %08lX", res);
			goto out;
		}
		data->downloader->set_target(url, HTTPC_METHOD_GET);
		res = data->downloader->execute_once();
	}
	else
	{
		while(data->state == ThreadState::Installing)
		{
			res = (*data->get_url)(url);
			if(R_SUCCEEDED(res))
			{
				data->downloader->set_target(url, HTTPC_METHOD_GET);
				res = data->downloader->execute_once();
				if(R_FAILED(res))
					elog("Failed to execute_once: %08lX", res);
			}
			else elog("Failed to fetch URL: %08lX", res);

			if(R_MODULE(res) == RM_HTTP)
			{
				/* there has probably been a timeout, we have to
				 * show this to the user and continue in due time */
				data->last_result = res;
				data->state = ThreadState::Timeout;
				LightEvent_Signal(&data->thread_to_ui_event);
				LightEvent_Wait(&data->ui_to_thread_event);
				/* the ui thread will set data->state to ThreadState::Insalling
				 * or ThreadState::Abort depending on what the user wants */
				continue;
			}

			/* Two cases:
			 *  - install succeeded: result is 0 and the above if is not reached; we want to break
			 *  - install failed (non-http error): result is not 0 and the above is not reached; we want to break */
			break;
		}
		if(data->state == ThreadState::Abort)
			res = APPERR_CANCELLED;
	}

out:
	data->last_result = res;
	data->state = ThreadState::Finished;
	LightEvent_Signal(&data->thread_to_ui_event);
	return;
}

static Result install_generic(http::ResumableDownload *downloader, get_url_func *get_url, prog_func *on_progress)
{
	thread_data data;
	LightEvent_Init(&data.thread_to_ui_event, RESET_ONESHOT);
	LightEvent_Init(&data.ui_to_thread_event, RESET_ONESHOT);
	data.state = ThreadState::Installing;
	data.downloader = downloader;
	data.get_url = get_url;
	data.last_result = 0;

	ctr::thread<thread_data *> thread(install_generic_thread, 1, std::move(&data));
	for(;;)
	{
		LightEvent_Wait(&data.thread_to_ui_event);
		/* te installation process is finished; we can exit this loop */
		if(data.state == ThreadState::Finished)
			break;
		/* the install thread wants us to display a timeout to the user */
		else if(data.state == ThreadState::Timeout)
		{
			if(ui::timeoutscreen(data.last_result, 10)) data.state = ThreadState::Abort;
			else                                        data.state = ThreadState::Installing;
			(*on_progress)(downloader->downloaded(), downloader->maybe_total_size());
			LightEvent_Signal(&data.ui_to_thread_event);
		}
		/* the install thread wants us to simply update the progress */
		else if(data.state == ThreadState::Installing)
			(*on_progress)(downloader->downloaded(), downloader->maybe_total_size());
		/* now we check if the user wants to abort */
		/* TODO: Ideally we'd subscribe to an event that tells us when
		 *       a button is pressed... */
		ui::scan_keys();
		/* The abort() will cause the ResumableDownload to return APPERR_CANCELLED, which in
		 * turn is futher thrown down more to cause an abort when ready to cancel */
		if(!aptMainLoop() || ((ui::kDown() | ui::kHeld()) & (KEY_B | KEY_START)))
			downloader->abort();
	}

	thread.join();
	return data.last_result;
}

static bool have_enough_space(ctr::Destination dest, u32 min_space)
{
	u64 freeSpace;
	Result res;
	if(R_FAILED(res = ctr::get_free_space(dest, &freeSpace)))
		return false;
	return min_space <= freeSpace;
}

void install::global_abort()
{
	if(active_cia_handle != CIA_HANDLE_INVALID)
	{
		AM_CancelCIAInstall(active_cia_handle);
		svcCloseHandle(active_cia_handle);
		active_cia_handle = CIA_HANDLE_INVALID;
	}
}

struct TitleInformation {
	u64 tid;
	bool isKTR;
	u16 version;
};

static Result install_generic_cia(get_url_func *get_url, prog_func *on_progress, bool reinstallable, const TitleInformation& info)
{
	panic_assert(active_cia_handle == CIA_HANDLE_INVALID, "May only install one CIA at a time");

	/* firstly we perform the prerequisite checks */
	FS_MediaType dest = ctr::mediatype_of(info.tid);
	Result res;

	/* check if the title is already installed and perhaps reinstall */
	bool has_ticket = ctr::ticket_exists(info.tid);
	bool has_title  = ctr::title_exists(info.tid, dest);
	if(has_ticket && !has_title)
	{
		ctr::delete_ticket(info.tid);
		/* reload dbs */
		AM_QueryAvailableExternalTitleDatabase(NULL);
	}
	AM_TitleEntry entry;
	/* only reinstall if we want to update */
	/* TODO: We should probably attempt to get this info from the CIA when the first chunk downloads */
	if(has_title && !(R_FAILED(ctr::get_title_entry(info.tid, entry)) || entry.version > info.version))
	{
		if(reinstallable || ISET_DEFAULT_REINSTALL)
		{
			FS_MediaType mydest;
			u64 mytid;
			if(R_FAILED(res = APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &mytid, (u8 *) &mydest, nullptr, nullptr, nullptr)))
				return res;
			/* we can only delete titles that are not ourselves */
			if(envIsHomebrew() || mytid != info.tid || mydest != dest)
			{
				if(R_FAILED(res = ctr::delete_title(info.tid, dest, ctr::DeleteTitleFlag::DeleteTicket | ctr::DeleteTitleFlag::CheckExistance)))
					return res;
				/* reload dbs */
				AM_QueryAvailableExternalTitleDatabase(NULL);
			}
		}
		else return APPERR_NOREINSTALL;
	}

	/* check if the title is meant exclusively for the "new" series */
	if(info.isKTR && !ctr::running_on_new_series())
		return APPERR_NOSUPPORT;

	/* we can only start the CIA installation a bit later due to
	 * some checks requiring file size, which is gotten through
	 * downloader.on_total_size_try_get() */

	http::ResumableDownload downloader;
	Handle ciaHandle = CIA_HANDLE_INVALID;
	u32 written;

	downloader.on_total_size_try_get([&]() -> Result {
		if(!downloader.maybe_total_size())
			return APPERR_NOSIZE;
		/* we do a little "do we have the required size" check before we do the actual work */
		ctr::Destination dest = ctr::detect_dest(info.tid);
		if(!have_enough_space(dest, downloader.maybe_total_size()))
			return APPERR_NOSPACE;

		/* just here can we actually start installing */
		res = AM_StartCiaInstall(ctr::to_mediatype(dest), &ciaHandle);
		if(R_FAILED(res)) ciaHandle = CIA_HANDLE_INVALID;
		active_cia_handle = ciaHandle;
		return res;
	});

	downloader.on_chunk([&](size_t chunk_size) -> Result {
		/* we don't need to add the FS_WRITE_FLUSH flag because AM just ignores write flags... */
		return FSFILE_Write(ciaHandle, &written, downloader.downloaded(), downloader.data_buffer(), chunk_size, 0);
	});

	res = install_generic(&downloader, get_url, on_progress);
	ilog("install_generic returned %08lX", res);

	/* finalize install */
	if(ciaHandle != CIA_HANDLE_INVALID)
	{
		if(R_FAILED(res)) AM_CancelCIAInstall(ciaHandle);
		else              res = AM_FinishCiaInstall(ciaHandle);
		svcCloseHandle(ciaHandle);
	}
	active_cia_handle = CIA_HANDLE_INVALID;

	ilog("final return of install_generic_cia is %08lX", res);
	return res;
}

Result install::net_cia(get_url_func get_url, u64 tid, prog_func prog, bool reinstallable)
{
	return install_generic_cia(&get_url, &prog, reinstallable, { tid, false, 0 });
}

Result install::hs_cia(const hsapi::Title& meta, prog_func prog, bool reinstallable)
{
	Result res;
	get_url_func get_url = [meta](std::string& ret) -> Result {
		return hsapi::get_download_link(ret, meta);
	};

	if(meta.flags & hsapi::TitleFlag::installer)
	{
		ilog("installing installer content");

		expandable_binary_data_type content;
		http::ResumableDownload downloader;

		downloader.on_total_size_try_get([&]() -> Result {
			if(!downloader.maybe_total_size())
				return APPERR_NOSIZE;
			/* file forwarders will always install to the SD card */
			if(!have_enough_space(ctr::DEST_Sdmc, downloader.maybe_total_size()))
				return APPERR_NOSPACE;
			content.reserve(downloader.maybe_total_size());
			return 0;
		});

		downloader.on_chunk([&](size_t chunk_size) -> Result {
			content.append(downloader.data_buffer(), chunk_size);
			return 0;
		});

		res = install_generic(&downloader, &get_url, &prog);
		if(R_FAILED(res)) return res;
		return install_forwarder((u8 *) content.c_str(), content.size());
	}
	else
	{
		res = install_generic_cia(&get_url, &prog, reinstallable,
			{ meta.tid,
			  (meta.flags & hsapi::TitleFlag::is_ktr) || strncmp(meta.prod.c_str(), "KTR-", 4) == 0,
			  meta.version });
	}
	return res;
}

