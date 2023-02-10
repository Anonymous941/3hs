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

#include "widgets/meta.hh"
#include "i18n.hh"

#include "ctr.hh"

#define PAIR(val, title) do { \
	ui::builder<ui::Text>(this->screen, val) \
		.x(this->get_x()) \
		.under(this->queue.back(), 1.0f) \
		.scroll() \
		.add_to(this->queue); \
	ui::builder<ui::Text>(this->screen, title) \
		.size(0.45f) \
		.x(this->get_x()) \
		.under(this->queue.back(), -1.0f) \
		.add_to(this->queue); \
	} while(0)


/* CatMeta */

static void clip_q(ui::RenderQueue& q, float base, float size = 0.65f)
{
	float totalHeight = base;
	for(ui::BaseWidget *wid : q.bot)
		totalHeight += wid->height();
	if(totalHeight > 210.0f)
	{
		ui::Text *prev = nullptr;
		/* everything needs to be smaller...
		 * required on KOR where everything is a tad larger */
		for(std::list<ui::BaseWidget *>::iterator it = q.bot.begin(); it != q.bot.end(); ++it)
		{
			ui::Text *cur = (ui::Text *) *it;
			cur->resize(size - 0.10f, size - 0.10f);
			++it;
			ui::Text *label = (ui::Text *) *it;
			label->resize(size - 0.25f, size - 0.25f);
			if(prev)
				cur->set_y(ui::under(prev, cur, -2.5f));
			label->set_y(ui::under(cur, label, -3.5f));
			prev = label;
		}
	}
}

void ui::CatMeta::setup(const hsapi::Category& cat)
{ this->set_cat(cat); }

void ui::CatMeta::set_cat(const hsapi::Category& cat)
{
	this->queue.clear();

	ui::builder<ui::Text>(this->screen, cat.disp)
		.x(this->get_x())
		.y(this->get_y())
		.scroll()
		.add_to(this->queue);
	ui::builder<ui::Text>(this->screen, STRING(name))
		.size(0.45f)
		.x(this->get_x())
		.under(this->queue.back(), -1.0f)
		.add_to(this->queue);

	PAIR(ui::human_readable_size_block(cat.meta.size), STRING(size));
	PAIR(std::to_string(cat.meta.titles), STRING(total_titles));
	PAIR(cat.desc, STRING(description));
	clip_q(this->queue, this->get_y());
}

bool ui::CatMeta::render(ui::Keys& keys)
{
	return this->queue.render_screen(keys, this->screen);
}

float ui::CatMeta::width()
{ return 0.0f; } /* fullscreen */

float ui::CatMeta::height()
{ return 0.0f; } /* fullscreen */

float ui::CatMeta::get_y()
{ return 10.0f; }

float ui::CatMeta::get_x()
{ return 10.0f; }

/* SubMeta */

void ui::SubMeta::setup(const hsapi::Subcategory& sub)
{ this->set_sub(sub); }

void ui::SubMeta::set_sub(const hsapi::Subcategory& sub)
{
	this->queue.clear();

	ui::builder<ui::Text>(this->screen, sub.disp)
		.x(this->get_x())
		.y(this->get_y())
		.scroll()
		.add_to(this->queue);
	ui::builder<ui::Text>(this->screen, STRING(name))
		.size(0.45f)
		.x(this->get_x())
		.under(this->queue.back(), -1.0f)
		.add_to(this->queue);

	PAIR(ui::human_readable_size_block(sub.meta.size), STRING(size));
	PAIR(sub.parent->disp, STRING(category));
	PAIR(std::to_string(sub.meta.titles), STRING(total_titles));
	PAIR(sub.desc, STRING(description));
	clip_q(this->queue, this->get_y());
}

bool ui::SubMeta::render(ui::Keys& keys)
{
	return this->queue.render_screen(keys, this->screen);
}

float ui::SubMeta::width()
{ return 0.0f; } /* fullscreen */

float ui::SubMeta::height()
{ return 0.0f; } /* fullscreen */

float ui::SubMeta::get_y()
{ return 10.0f; }

float ui::SubMeta::get_x()
{ return 10.0f; }

/* TitleMeta */

template <typename T>
void ui::TitleMeta::setup_with_title(const T& meta)
{
	this->queue.clear();

	ui::builder<ui::Text>(this->screen, meta.name)
		.x(this->get_x())
		.y(this->get_y())
		.scroll()
		.add_to(this->queue);
	ui::builder<ui::Text>(this->screen, STRING(name))
		.size(0.45f)
		.x(this->get_x())
		.under(this->queue.back(), -1.0f)
		.add_to(this->queue);

	PAIR(hsapi::format_category_and_subcategory(meta.cat, meta.subcat), STRING(category));
	PAIR(ctr::tid_to_str(meta.tid), STRING(tid));
	PAIR(std::to_string(meta.id), STRING(landing_id));
	PAIR(ui::human_readable_size_block(meta.size), STRING(size));
	clip_q(this->queue, this->get_y());
}

void ui::TitleMeta::setup(const hsapi::PartialTitle& meta) { this->setup_with_title<hsapi::PartialTitle>(meta); }
void ui::TitleMeta::set_title(const hsapi::PartialTitle& meta) { this->setup_with_title<hsapi::PartialTitle>(meta); }
void ui::TitleMeta::setup(const hsapi::Title& meta) { this->setup_with_title<hsapi::Title>(meta); }
void ui::TitleMeta::set_title(const hsapi::Title& meta) { this->setup_with_title<hsapi::Title>(meta); }

bool ui::TitleMeta::render(ui::Keys& keys)
{
	return this->queue.render_screen(keys, this->screen);
}

float ui::TitleMeta::width()
{ return 0.0f; } /* fullscreen */

float ui::TitleMeta::height()
{ return 0.0f; } /* fullscreen */

float ui::TitleMeta::get_y()
{ return 10.0f; }

float ui::TitleMeta::get_x()
{ return 10.0f; }

