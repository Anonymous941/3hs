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

#include <ui/progress_bar.hh>
#include <ui/loading.hh>

#include "thread.hh"
#include "util.hh"
#include "i18n.hh"

#include <unistd.h>
#include <3ds.h>

enum class SpinState {
	ToSetup,
	Spinning,
	Idle,
};

void ui::loading(std::function<void()> callback)
{
	std::string desc = ::set_desc(STRING(loading));
	bool focus = ::set_focus(true);

	bool spin_flag = true;

	static LightLock is_spinning_lock;
	static SpinState is_spinning = SpinState::ToSetup;
	if(is_spinning == SpinState::ToSetup)
	{
		LightLock_Init(&is_spinning_lock);
	}
	LightLock_Lock(&is_spinning_lock);
	SpinState spincopy = is_spinning;

	ctr::thread<> th([&spin_flag, spincopy]() -> void {
		if(spincopy == SpinState::Spinning)
			return;

		ui::RenderQueue queue;
		ui::builder<ui::Spinner>(ui::Screen::top)
			.x(ui::layout::center_x)
			.y(ui::layout::center_y)
			.add_to(queue);

		ui::Keys keys;
		while(spin_flag && queue.render_frame((keys = ui::RenderQueue::get_keys())))
			/* no-op */ ;

		LightLock_Lock(&is_spinning_lock);
		is_spinning = SpinState::Idle;
		LightLock_Unlock(&is_spinning_lock);
	}, -1);

	is_spinning = SpinState::Spinning;
	LightLock_Unlock(&is_spinning_lock);
	/* */ callback();
	spin_flag = false;
	th.join();
	aptSetHomeAllowed(true);

	::set_focus(focus);
	::set_desc(desc);
}

static std::string loadingbar_serialize(u64 cur, u64 total)
{
	(void) total;
	return std::to_string(cur);
}

static std::string loadingbar_postfix(u64 cur)
{
	(void) cur;
	return "";
}

ui::LoadingBar::LoadingBar(size_t max)
	: cur(0), max(max)
{
	ui::builder<ui::ProgressBar>(ui::Screen::top, max)
		.y(ui::layout::center_y)
		.add_to(&this->bar, this->rq);

	this->bar->set_serialize(loadingbar_serialize);
	this->bar->set_postfix(loadingbar_postfix);
	this->bar->activate();
	this->rq.render_frame();
}

void ui::LoadingBar::reset_to(size_t nmax)
{
	this->max = nmax;
	this->cur = 0;
	this->rq.render_frame();
}

void ui::LoadingBar::update(size_t n)
{
	this->cur += n;
	if(this->cur > this->max)
		this->cur = this->max;
	this->bar->update(this->cur, this->max);
	this->rq.render_frame();
}

/* class Spinner */

void ui::Spinner::setup()
{
	this->sprite.setup(this->screen, ui::Sprite::theme, ui::theme::spinner_image);
	this->sprite.ptr()->set_center(0.5f, 0.5f);
}

bool ui::Spinner::render(ui::Keys& keys)
{
	this->sprite.ptr()->rotate(1.0f);
	return this->sprite->render(keys);
}

float ui::Spinner::width()  { return this->sprite.ptr()->width(); }
float ui::Spinner::height() { return this->sprite.ptr()->height(); }

void ui::Spinner::set_x(float x)
{
	if((int) x == ui::layout::center_x)
	{
		// We need to do some extra maths here because we have a (0.5, 0.5) center
		x = ui::screen_width(this->screen) / 2;
	} else x = ui::transform(this, x);

	this->sprite.ptr()->set_x(x);
	this->x = x;
}

void ui::Spinner::set_y(float y)
{
	if((int) y == ui::layout::center_y)
	{
		// We need to do some extra maths here because we have a (0.5, 0.5) center
		y = ui::screen_height() / 2;
	} else y = ui::transform(this, y);

	this->sprite.ptr()->set_y(y);
	this->y = y;
}

void ui::Spinner::set_z(float z)
{ this->sprite.ptr()->set_z(z); }

void ui::detail::TimeoutScreenHelper::setup(Result res, size_t nsecs, bool *shouldStop)
{
	this->startTime = this->lastCheck = time(NULL);
	this->shouldStop = shouldStop;
	this->nsecs = nsecs;
	this->res = "0x" + pad8code(res);

	this->text.setup(ui::Screen::top);
	this->text->set_x(ui::layout::center_x);
	this->text->set_y(80.0f);
	this->text->autowrap();

	this->update_text(this->startTime);
}

void ui::detail::TimeoutScreenHelper::update_text(time_t now)
{
	this->text->set_text(PSTRING(netcon_lost, this->res, this->nsecs - (now - this->startTime)));
}

bool ui::detail::TimeoutScreenHelper::perform_frame_setup(ui::Keys *keys)
{
	if(keys && (keys->kDown & (KEY_START | KEY_B)) && this->shouldStop)
	{
		*this->shouldStop = true;
		return false;
	}
	time_t now = time(NULL);

	if(now - this->startTime >= this->nsecs)
		return false;

	if(this->lastCheck != now)
	{
		this->update_text(now);
		this->lastCheck = now;
	}

	return true;
}

bool ui::detail::TimeoutScreenHelper::render(ui::Keys& keys) { return this->perform_frame_setup(&keys) && this->text->render(keys); }
bool ui::detail::TimeoutScreenHelper::process_in_sleep() { return this->perform_frame_setup(nullptr); }

// timeoutscreen()

bool ui::timeoutscreen(Result res, size_t nsecs, bool allowCancel)
{
	bool ret = false;

	ui::RenderQueue queue;
	ui::builder<ui::detail::TimeoutScreenHelper>(ui::Screen::top, res, nsecs, allowCancel ? &ret : nullptr)
		.add_to(queue);
	queue.render_finite();

	return ret;
}

