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

#include <algorithm>

#include <widgets/meta.hh>
#include <ui/base.hh>

#include "image_ldr.hh"
#include "extmeta.hh"
#include "thread.hh"
#include "hsapi.hh"
#include "queue.hh"
#include "panic.hh"
#include "i18n.hh"
#include "util.hh"
#include "ctr.hh"
#include "log.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <3rd/stb_image.h>


enum class extmeta_return { yes, no, none };

template <typename TTitle = hsapi::PartialTitle>
static void show_preview(const TTitle& title)
{
	std::string png_data;
	if(R_FAILED(hsapi::call(hsapi::get_theme_preview_png, png_data, (hsapi::hid) title.id)))
		return;
	int x, y;
	u8 *bitmap = stbi_load_from_memory((const u8 *) png_data.data(), png_data.size(), &x, &y, NULL, 4);
	/* invalid file */
	if(!bitmap || x < ui::dimensions::width_top || y != ui::dimensions::height * 2)
	{
		ui::notice(STRING(invalid_theme_preview));
		return;
	}
	u32 bottom_offset = 4 * x * (y / 2);

	rgba_to_abgr((u32 *) bitmap, x, y);

	C2D_Image bottom, top;
	load_abgr8(&top, (u32 *) bitmap, x, ui::dimensions::height);
	load_abgr8(&bottom, (u32 *) (bitmap + bottom_offset), x, ui::dimensions::height);

	ui::RenderQueue queue;
	ui::builder<ui::Sprite>(ui::Screen::top, ui::Sprite::image, (u32) &top)
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.add_to(queue);
	ui::builder<ui::Sprite>(ui::Screen::bottom, ui::Sprite::image, (u32) &bottom)
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.add_to(queue);

	ui::Keys keys = ui::RenderQueue::get_keys();
	while(queue.render_exclusive_frame(keys) && !keys.kDown)
		keys = ui::RenderQueue::get_keys();

	delete_image(bottom);
	delete_image(top);
	free(bitmap);
}

/* don't call with exmeta_return::none */
static bool to_bool(extmeta_return r)
{
	return r == extmeta_return::yes;
}

template <typename TTitle = hsapi::PartialTitle>
static extmeta_return extmeta(ui::RenderQueue& queue, const TTitle& base, const std::string& version_s, const std::string& prodcode_s)
{
	extmeta_return ret = extmeta_return::none;
	ui::Text *press_to_install;
	ui::Text *prodcode;
	ui::Text *version;

	ui::builder<ui::Text>(ui::Screen::top, STRING(press_to_install))
		.size(0.42f, 0.42f)
		.x(ui::layout::center_x)
		.wrap()
		.add_to(&press_to_install, queue);
	press_to_install->set_y(ui::dimensions::height - press_to_install->height() - 10.0f);

	/***
	 * name (wrapped)
	 * alt_name (maybe) (wrapped)
	 * category -> subcategory
	 *
	 * "Press a to install, b to not"
	 * =======================
	 * version     prod
	 * tid
	 * size
	 * landing id
	 ***/

	/* name */
	ui::builder<ui::Text>(ui::Screen::top, base.name)
		.x(9.0f)
		.y(25.0f)
		// if this overflows to the point where it overlaps with
		// STRING(press_to_install) we're pretty fucked, but i
		// don't think we have such titles
		.wrap()
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, STRING(name))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* alternative name */
	if(base.alt.size())
	{
		ui::builder<ui::Text>(ui::Screen::top, base.alt)
			.x(9.0f)
			.under(queue.back(), 1.0f)
			.wrap()
			.add_to(queue);
		ui::builder<ui::Text>(ui::Screen::top, STRING(alt_name))
			.size(0.45f)
			.x(9.0f)
			.under(queue.back(), -1.0f)
			.add_to(queue);
	}

	/* category -> subcategory */
	// on the bottom screen there are cases where this overflows,
	// but i don't think that can happen on the top screen since it's a bit bigger
	ui::builder<ui::Text>(ui::Screen::top, hsapi::format_category_and_subcategory(base.cat, base.subcat))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, STRING(category))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* Button hint add to queue */
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(hint_add_queue))
		.x(9.0f).y(7.0f)
		.size(0.4f)
		.add_to(queue);

	/* only applies to themes */
	if(hsapi::category(base.cat).name == THEMES_CATEGORY)
	{
		/* Button hint preview theme */
		ui::builder<ui::Text>(ui::Screen::bottom, STRING(hint_preview_theme))
			.right(queue.back(), 5.0f).y(7.0f)
			.size(0.4f)
			.add_to(queue);

		ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_X)
			.connect(ui::ButtonCallback::kdown, [&base](u32) -> bool { ui::RenderQueue::global()->render_and_then([&base]() -> void {
					show_preview(base);
				}); return true; })
			.add_to(queue);
	}

	/* version */
	ui::builder<ui::Text>(ui::Screen::bottom, version_s)
		.x(9.0f)
		.y(20.0f)
		.add_to(&version, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(version))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* product code */
	ui::builder<ui::Text>(ui::Screen::bottom, prodcode_s)
		.x(150.0f)
		.align_y(version)
		.add_to(&prodcode, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(prodcode))
		.size(0.45f)
		.x(150.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* size */
	ui::builder<ui::Text>(ui::Screen::bottom, ui::human_readable_size_block<hsapi::hsize>(base.size))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(size))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* title id */
	ui::builder<ui::Text>(ui::Screen::bottom, ctr::tid_to_str(base.tid))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(tid))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	/* landing id */
	ui::builder<ui::Text>(ui::Screen::bottom, std::to_string(base.id))
		.x(9.0f)
		.under(queue.back(), 1.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, STRING(landing_id))
		.size(0.45f)
		.x(9.0f)
		.under(queue.back(), -1.0f)
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_B)
		.connect(ui::ButtonCallback::kdown, [&ret](u32) -> bool { ret = extmeta_return::no; return false; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_A)
		.connect(ui::ButtonCallback::kdown, [&ret](u32) -> bool { ret = extmeta_return::yes; return false; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_Y)
		.connect(ui::ButtonCallback::kdown, [&base](u32) -> bool { ui::RenderQueue::global()->render_and_then([&base]() -> void {
				queue_add(base.id, true);
			}); return true; })
		.add_to(queue);

	queue.render_finite();
	return ret;
}

static extmeta_return extmeta(const hsapi::Title& title)
{
	ui::RenderQueue queue;
	return extmeta(queue, title, hsapi::parse_vstring(title.version) + " (" + std::to_string(title.version) + ")", title.prod);
}

bool show_extmeta_lazy(const hsapi::PartialTitle& base, hsapi::Title *full)
{
	std::string desc = set_desc(STRING(more_about_content));
	ui::RenderQueue queue;
	bool ret = true;

	std::string version, prodcode, alt;

	ctr::thread<std::string&, std::string&, std::string&, ui::RenderQueue&, hsapi::Title *> th([&base]
			(std::string& version, std::string& prodcode, std::string& alt, ui::RenderQueue& queue, hsapi::Title *fullptr) -> void {
		hsapi::Title full;
		if(R_FAILED(hsapi::title_meta(full, base.id)))
			return;
		if(fullptr != nullptr)
			*fullptr = full;
		version = hsapi::parse_vstring(full.version) + " (" + std::to_string(full.version) + ")";
		prodcode = full.prod;
		alt = full.alt;
		queue.signal(ui::RenderQueue::signal_cancel);
	}, -1, version, prodcode, alt, queue, std::move(full));

	extmeta_return res = extmeta(queue, base, STRING(loading), STRING(loading));
	/* second thread returned more data */
	if(res == extmeta_return::none)
	{
		dlog("Lazy load finished before choice was made.");
		queue.clear();
		res = extmeta(queue, base, version, prodcode);
	}
	ret = to_bool(res);

	/* At this point we're done rendering and
	 * waiting for the *fetching* of the full data
	 * and *setting* of the renderqueue callback */
	th.join();

	set_desc(desc);
	return ret;
}

bool show_extmeta_lazy(std::vector<hsapi::PartialTitle>& titles, hsapi::hid id, hsapi::Title *full)
{
	std::vector<hsapi::PartialTitle>::iterator it =
		std::find_if(titles.begin(), titles.end(), [id](const hsapi::PartialTitle& t) -> bool {
			return t.id == id;
		});

	panic_assert(it != titles.end(), "Could not find id in vector");
	return show_extmeta_lazy(*it, full);
}

bool show_extmeta(const hsapi::Title& title)
{
	std::string desc = set_desc(STRING(more_about_content));
	bool ret = to_bool(extmeta(title));
	set_desc(desc);
	return ret;
}

