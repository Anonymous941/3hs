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

#include "find_missing.hh"
#include "install.hh"
#include "hsapi.hh"
#include "queue.hh"
#include "panic.hh"
#include "ctr.hh"
#include "log.hh"

#include <ui/loading.hh>

#include <unordered_set>
#include <algorithm>


template <typename T>
static void vecappend(std::vector<T>& a, const std::vector<T>& b)
{ a.insert(a.end(), b.begin(), b.end()); }

bool tid_can_have_missing(hsapi::htid tid)
{
	u16 category = ctr::get_tid_cat(tid);
	return category == 0x0000 /* normal */ || category == 0x0010 /* system title */ || category == 0x8000 /* DSiWare/TWL */;
}

Result show_find_missing(hsapi::htid tid, size_t& amount_found)
{
	Result ret = 0;
	amount_found = 0;
	ui::loading([tid, &ret, &amount_found]() -> void {
		std::vector<hsapi::htid> installed;
		panic_if_err_3ds(ctr::list_titles_on(MEDIATYPE_SD, installed));
		panic_if_err_3ds(ctr::list_titles_on(MEDIATYPE_NAND, installed)); // mostly for streetpass dlc
		ctr::list_titles_on(MEDIATYPE_GAME_CARD, installed); // it might error if there is no cart inserted so we don't want to panic if it fails

		std::vector<hsapi::htid> doCheckOn;
		if(tid == 0) doCheckOn = installed;
		else doCheckOn.push_back(tid);

		std::vector<hsapi::htid> installedGames;
		std::copy_if(doCheckOn.begin(), doCheckOn.end(), std::back_inserter(installedGames), tid_can_have_missing);
		/* deduplicate installedGames based on unique id */
		std::unordered_set<u32> dedupe;
		for(size_t i = 0; i < installedGames.size(); ++i)
		{
			u32 unique = ctr::get_tid_unique(installedGames[i]);
			if(dedupe.find(unique) == dedupe.end())
				dedupe.insert(unique);
			else /* duplicate detected */
			{
				installedGames.erase(installedGames.begin() + i);
				/* reset i to account for erasure */
				--i;
			}
		}

		std::vector<hsapi::Title> potentialInstalls;
		ret = hsapi::scall<std::vector<hsapi::Title>&, const std::vector<hsapi::htid>&>(hsapi::batch_related, potentialInstalls, installedGames);
		if(R_FAILED(ret)) return;

		std::vector<hsapi::Title> newInstalls;
		std::copy_if(potentialInstalls.begin(), potentialInstalls.end(), std::back_inserter(newInstalls), [installed](const hsapi::Title& title) -> bool {
			/* don't import demo's */
			if(ctr::get_tid_cat(title.tid) == 0x2)
				return false;
			/* already in queue */
			if(std::find_if(queue_get().begin(), queue_get().end(), [&title](const hsapi::Title& it) -> bool { return title.id == it.id; }) != queue_get().end())
				return false;
			/* not installed */
			if(std::find(installed.begin(), installed.end(), title.tid) == installed.end())
				return true;
			AM_TitleEntry te;
			if(R_FAILED(ctr::get_title_entry(title.tid, te)))
				return false;
			/* installed version is lower than version on server */
			return title.version > te.version;
		});

		for(const hsapi::Title& title : newInstalls)
			queue_add(title);
		amount_found = newInstalls.size();
	});
	return ret;
}

void show_find_missing_all()
{
	size_t done;
	Result res = show_find_missing(0x0, done);
	if(R_FAILED(res))
	{
		error_container err = get_error(res);
		report_error(err, "Find missing");
		handle_error(err);
	}
	else if(done == 0) ui::notice(STRING(found_0_missing));
	else ui::notice(PSTRING(found_missing, done));
}

