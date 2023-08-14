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

#include <ui/swkbd.hh>
#include <panic.hh>


void ui::AppletSwkbd::setup(std::string *ret, int maxLen, SwkbdType type,
	int numBtns)
{
	swkbdInit(&this->state, type, numBtns, maxLen);
	/* Let's always enable this... on JPN, this is required for Kanji input */
	swkbdSetFeatures(&this->state, SWKBD_PREDICTIVE_INPUT);
	this->len = maxLen;
	this->ret = ret;
}

float ui::AppletSwkbd::width()
{ return 0.0f; } /* fullscreen */

float ui::AppletSwkbd::height()
{ return 0.0f; } /* fullscreen */

void ui::AppletSwkbd::hint(const std::string& h)
{ swkbdSetHintText(&this->state, h.c_str()); }

void ui::AppletSwkbd::passmode(SwkbdPasswordMode mode)
{ swkbdSetPasswordMode(&this->state, mode); }

void ui::AppletSwkbd::init_text(const std::string& t)
{ swkbdSetInitialText(&this->state, t.c_str()); }

void ui::AppletSwkbd::valid(SwkbdValidInput mode, u32 filterFlags, u32 maxDigits)
{ swkbdSetValidation(&this->state, mode, filterFlags, maxDigits); }

bool ui::AppletSwkbd::render(ui::Keys& keys)
{
	((void) keys);

	/* why is this cast necessairy? */
	ui::RenderQueue::global()->render_and_then((std::function<bool()>) [this]() -> bool {
		char *buf = new char[this->len + 1];
		SwkbdButton btn = swkbdInputText(&this->state, buf, this->len + 1);
		*this->ret = buf;
		delete [] buf;

		if(this->resPtr != nullptr) *this->resPtr = swkbdGetResult(&this->state);
		if(this->buttonPtr != nullptr) *this->buttonPtr = btn;

		return false;
	});

	return true;
}

/* KBDEnabledButton */

void ui::KBDEnabledButton::setup(const std::string& default_label, const std::string& empty_label, const std::string& hint, size_t min_len)
{
	this->btn.setup(this->screen, default_label.size() ? default_label : empty_label);
	this->min_len = min_len;
	this->empty = empty_label;
	this->hint = hint;
	this->hasValue = !!default_label.size();
	ui::builder<ui::Button>(this->btn.ptr()).when_clicked([this]() -> bool {
		ui::RenderQueue::global()->render_and_then([this]() -> void {
			SwkbdResult res;
			SwkbdButton btn;

			std::string query = ui::keyboard([this](ui::AppletSwkbd *swkbd) -> void {
				swkbd->init_text(this->value());
				swkbd->hint(this->hint);
			}, &btn, &res);

			if(btn != SWKBD_BUTTON_CONFIRM || res == SWKBD_INVALID_INPUT || res == SWKBD_OUTOFMEM || res == SWKBD_BANNED_INPUT)
				return;

			this->hasValue = query.size() && query.size() >= this->min_len;
			if(this->hasValue)
				this->btn->set_label(query);
			else
				this->btn->set_label(this->empty);

			if(this->update_cb)
				this->update_cb(this);
		});
		return true;
	});
	this->btn->resize(ui::screen_width(this->screen), 24.0f);
}

void ui::KBDEnabledButton::set(const std::string& val)
{
	this->btn->set_label(val.size() ? val : this->empty);
}

const std::string& ui::KBDEnabledButton::value()
{
	static const std::string empty = "";
	return this->hasValue ? this->btn->get_label() : empty;
}

bool ui::KBDEnabledButton::render(ui::Keys& keys)
{
	return this->btn->render(keys);
}

/* keyboard */

std::string ui::keyboard(std::function<void(ui::AppletSwkbd *)> configure,
	SwkbdButton *btn, SwkbdResult *res, size_t length)
{
	ui::RenderQueue queue;
	std::string ret;

	ui::AppletSwkbd *swkbd;
	ui::builder<ui::AppletSwkbd>(ui::Screen::top, &ret, length)
		.button(btn)
		.result(res)
		.add_to(&swkbd, queue);
	configure(swkbd);

	queue.render_finite();
	return ret;
}

/* numpad */

uint64_t ui::numpad(std::function<void(ui::AppletSwkbd *)> configure,
	SwkbdButton *btn, SwkbdResult *res, size_t length)
{
	ui::RenderQueue queue;
	std::string ret;

	ui::AppletSwkbd *swkbd;
	ui::builder<ui::AppletSwkbd>(ui::Screen::top, &ret, length, SWKBD_TYPE_NUMPAD)
		.button(btn)
		.result(res)
		.add_to(&swkbd, queue);
	configure(swkbd);

	queue.render_finite();
	return strtoull(ret.c_str(), nullptr, 10);
}

