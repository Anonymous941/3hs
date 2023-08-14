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

#include <widgets/konami.hh>

#include <ui/base.hh>

#include "panic.hh"
#include "util.hh"
#include "i18n.hh"

#define SLINE_MOD 0.5f


/*     StatusLine */
void StatusLine::reset()
{
	if(this->flags & StatusLine::flag_running)
	{
		ui::RenderQueue::global()->find_tag(ui::tag::net_indicator)->set_hidden(!!(this->flags & StatusLine::flag_net_is_hidden));
		ui::RenderQueue::global()->find_tag(ui::tag::free_indicator)->set_hidden(!!(this->flags & StatusLine::flag_free_is_hidden));
		this->text.destroy();
	}
	this->flags = 0;
}

void StatusLine::start(const std::string& str, StatusLine::StatusDisplayType type)
{
	this->text.setup(this->screen, str);
	this->text->resize(0.35f, 0.35f);
	this->text->set_raw_y(ui::screen_height() - 10.0f);
	this->fadeoutx = -this->text->width() - 10.0f;
	this->text.finalize();

	switch(type)
	{
	case StatusLine::StatusDisplayType::slide_in:
		this->xpos = this->fadeoutx;
		this->text->set_raw_x(this->xpos);
		break;
	case StatusLine::StatusDisplayType::ticker:
		this->xpos = ui::screen_width(this->screen) + 10.0f;
		this->text->set_raw_x(this->xpos);
		this->flags |= StatusLine::flag_is_ticker;
		break;
	}

	ui::BaseWidget *w = ui::RenderQueue::global()->find_tag(ui::tag::net_indicator);
	this->flags |= w->is_hidden() ? 0 : StatusLine::flag_net_is_hidden;
	w->set_hidden(true);

	w = ui::RenderQueue::global()->find_tag(ui::tag::free_indicator);
	this->flags |= w->is_hidden() ? 0 : StatusLine::flag_free_is_hidden;
	w->set_hidden(true);

	this->flags |= StatusLine::flag_running;
}

void StatusLine::ticker(const std::string& str)
{
	this->start(str, StatusLine::StatusDisplayType::ticker);
}

void StatusLine::run(const std::string& str)
{
	this->start(str, StatusLine::StatusDisplayType::slide_in);
}

bool StatusLine::render(ui::Keys& keys)
{
	if(!(this->flags & StatusLine::flag_running))
		return true;

	this->text->render(keys);

	/* we need to run in "ticker mode"; scroll text in screen once from right to left */
	if(this->flags & StatusLine::flag_is_ticker)
	{
		this->xpos -= SLINE_MOD;
		this->text->set_raw_x(this->xpos);
		if(this->xpos < this->fadeoutx)
			this->reset();
	}
	/* wait 3 seconds when we get in position */
	else if(this->flags & StatusLine::flag_is_in_position)
	{
		if(time(NULL) - this->in_pos_start > 3)
		{
			this->flags &= ~StatusLine::flag_is_in_position;
			this->flags |= StatusLine::flag_return_in_progress;
		}
	}
	/* return to -this->text->width() - 10.0f */
	else if(this->flags & StatusLine::flag_return_in_progress)
	{
		this->xpos -= SLINE_MOD;
		this->text->set_raw_x(this->xpos);
		if(this->xpos < this->fadeoutx)
			this->reset();
	}
	/* else we must progress to 5.0f */
	else
	{
		this->xpos += SLINE_MOD;
		this->text->set_raw_x(this->xpos);
		if(this->xpos > 5.0f)
		{
			this->in_pos_start = time(NULL);
			this->flags |= StatusLine::flag_is_in_position;
		}
	}

	return true;
}
/* end StatusLine */

bool set_focus(bool focus)
{
	/* panic() called before core global widgets are setup */
	if(!ui::RenderQueue::global()->find_tag(ui::tag::action))
		return false;
	bool ret = ui::RenderQueue::global()->find_tag(ui::tag::action)->is_hidden();
	ui::RenderQueue::global()->find_tag(ui::tag::settings)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::search)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::konami)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::action)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::random)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::queue)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::more)->set_hidden(focus);
	return ret;
}

u8 make_status_line_clear()
{
	u8 flags = 0;

	ui::BaseWidget *w = ui::RenderQueue::global()->find_tag(ui::tag::net_indicator);
	flags |= w->is_hidden() ? 0 : 1;
	w->set_hidden(true);

	w = ui::RenderQueue::global()->find_tag(ui::tag::free_indicator);
	flags |= w->is_hidden() ? 0 : 2;
	w->set_hidden(true);

	return flags;
}

void restore_status_line(u8 flags)
{
	ui::RenderQueue::global()->find_tag(ui::tag::net_indicator)->set_hidden(!!(flags & 1));
	ui::RenderQueue::global()->find_tag(ui::tag::free_indicator)->set_hidden(!!(flags & 2));
}

std::string set_desc(const std::string& nlabel)
{
	ui::Text *action = ui::RenderQueue::global()->find_tag<ui::Text>(ui::tag::action);
	std::string old = action->get_text();
	action->set_text(nlabel);
	return old;
}

void set_ticker(const std::string& text)
{
	StatusLine *sl = ui::RenderQueue::global()->find_tag<StatusLine>(ui::tag::status);
	panic_assert(sl, "status line not set up");
	sl->reset();
	sl->ticker(text);
}

void set_status(const std::string& text)
{
	StatusLine *sl = ui::RenderQueue::global()->find_tag<StatusLine>(ui::tag::status);
	panic_assert(sl, "status line not set up");
	sl->reset();
	sl->run(text);
}

bool status_running()
{
	StatusLine *sl = ui::RenderQueue::global()->find_tag<StatusLine>(ui::tag::status);
	return sl->is_running();
}

void reset_status()
{
	StatusLine *sl = ui::RenderQueue::global()->find_tag<StatusLine>(ui::tag::status);
	panic_assert(sl, "status line not set up");
	sl->reset();
}

void lower(std::string& s)
{
	for(size_t i = 0; i < s.size(); ++i)
		s[i] = tolower(s[i]);
}

// https://stackoverflow.com/questions/1798112/removing-leading-and-trailing-spaces-from-a-string#1798170
void trim(std::string& str, const std::string& whitespace)
{
	const size_t str_begin = str.find_first_not_of(whitespace);
	if(str_begin == std::string::npos) { str = ""; return; }

	const size_t str_end = str.find_last_not_of(whitespace);
	const size_t str_range = str_end - str_begin + 1;

	str = str.substr(str_begin, str_range);
}

void join(std::string& ret, const std::vector<std::string>& tokens, const std::string& sep)
{
	if(tokens.size() == 0) { ret = ""; return; }
	ret = tokens[0];
	for(size_t i = 1; i < tokens.size(); ++i)
		ret += sep + tokens[i];
}
