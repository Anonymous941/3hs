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

#ifndef inc_hsapi_hh
#define inc_hsapi_hh

#include <nbapi/nbtypes.hh>

#include <unordered_map>
#include <string>
#include <vector>

#include <3ds.h>

#include <ui/confirm.hh>
#include <ui/loading.hh>
#include <ui/base.hh>

#include "settings.hh"
#include "panic.hh"
#include "error.hh"
#include "util.hh"
#include "i18n.hh"


namespace hsapi
{
	/* base types */
	using hsize      = uint64_t; /* size type */
	using hiver      = uint16_t; /* integer version type */
	using htid       = uint64_t; /* title id type */
	using hid        = uint32_t; /* landing id type */
	using hflags     = uint64_t; /* flag */
	using hprio      = uint8_t;  /* priority */
	using hcid       = uint8_t;  /* (sub)category id */

	namespace TitleFlag
	{
		enum TitleFlag {
			is_ktr           = 0x1,
			locale_emulation = 0x2,
			installer        = 0x4,
		};
	}

	namespace VCType
	{
		enum VCFlag {
			none     = 0,
			gb       = 1,
			gbc      = 2,
			gba      = 3,
			nes      = 4,
			snes     = 5,
			gamegear = 6,
			pcengine = 7
		};
		constexpr int shift = 59;
		constexpr int mask  = 7;
	}

	/* copied NB types */
	using IndexSubcategory = nb::IndexSubcategory;
	using IndexCategory = nb::IndexCategory;
	using PartialTitle = nb::SimpleTitle;
	using IndexMeta = nb::NbIndexMeta;
	using Title = nb::Title;

	using Subcategory = IndexSubcategory;
	using Category = IndexCategory;

	using SubcategoryMap = std::map<hcid, Subcategory>;
	using CategoryMap = std::map<hcid, Category>;

	/* index ops */
	void sorted_categories(std::vector<hsapi::Category>& categories);
	IndexSubcategory& subcategory(hcid cid, hcid sid);
	IndexCategory& category(hcid cid);
	CategoryMap& categories();
	Result fetch_index();
	IndexMeta& imeta();

	/* title ops */
	Result search(std::vector<PartialTitle>& ret, const std::unordered_map<std::string, std::string>& params);
	Result titles_in(std::vector<PartialTitle>& ret, const IndexCategory& cat, const IndexSubcategory& scat);
	Result get_by_title_id(std::vector<Title>& ret, const std::string& title_id);
	Result batch_related(std::vector<Title>& ret, const std::vector<htid>& tids);
	Result title_meta(Title& ret, hid id);
	Result random(Title& ret);

	/* misc. api */
	Result upload_log(const char *contents, u32 size, std::string& logid);
	Result get_theme_preview_png(std::string& ret, hsapi::hid id);
	Result get_latest_version_string(std::string& ret);

	/* dlapi ops */
	Result get_download_link(std::string& ret, const Title& meta);

	/* de/initialization */
	void global_deinit();
	bool global_init();

	/* offline string construction */
	std::string format_category_and_subcategory(hcid cid, hcid sid);
	std::string update_location(const std::string& ver);
	std::string parse_vstring(hiver version);

	inline const std::string& title_name(const hsapi::PartialTitle& title) { return ISET_SHOW_ALT ? (title.alt.size() ? title.alt : title.name) : title.name; }
	inline const std::string& title_name(const hsapi::Title& title)        { return ISET_SHOW_ALT ? (title.alt.size() ? title.alt : title.name) : title.name; }

	// Silent call. ui::loading() is not called and it will stop after 3 tries
	template <typename ... Ts>
	Result scall(Result (*func)(Ts...), Ts&& ... args)
	{
		Result res = 0;
		int tries = 0;

		do {
			res = (*func)(args...);
			++tries;
		} while(R_FAILED(res) && res != APPERR_CANCELLED && tries < 3);
		return res;
	}

	// NOTE: You have to std::move() primitives (hid, hiver, htid, ...)
	template <typename ... Ts>
	Result call(Result (*func)(Ts...), Ts&& ... args)
	{
		std::string desc = set_desc(STRING(loading));
		bool focus = set_focus(false);
		Result res;
		do {
			ui::loading([&res, func, &args...]() -> void {
				res = (*func)(args...);
			});

			if(res == APPERR_CANCELLED)
				break; /* done already */
			if(R_FAILED(res)) // Ask if we want to retry
			{
				error_container err = get_error(res);
				report_error(err);
				handle_error(err);

				ui::RenderQueue queue; bool cont = true;
				ui::builder<ui::Confirm>(ui::Screen::bottom, STRING(retry_req), cont)
					.y(ui::layout::center_y)
					.add_to(queue);
				queue.render_finite();

				if(!cont) break;
			}
		} while(R_FAILED(res));
		set_focus(focus);
		set_desc(desc);
		return res;
	}
}

#endif

