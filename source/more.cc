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

#include "more.hh"

#include "find_missing.hh"
#include "audio_cfg.hh"
#include "sysmanage.hh"
#include "log_view.hh"
#include "settings.hh"
#include "hsapi.hh"
#include "about.hh"
#include "panic.hh"
#include "i18n.hh"
#include "util.hh"

#include <ui/menuselect.hh>
#include <ui/base.hh>

#include <stdlib.h>


void show_more()
{
	bool focus = set_focus(true);

	ui::RenderQueue queue;
	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.add_row(STRING(about_app), []() -> bool { show_about(); return true; })
		.add_row(STRING(find_missing_content), []() -> bool { show_find_missing_all(); return true; })
		.add_row(STRING(log), []() -> bool { show_logs_menu(); return true; })
		.add_row(STRING(themes), []() -> bool { show_theme_menu(); return true; })
		.add_row(STRING(delete_unused_tickets), []() -> bool { show_delete_unused_tickets(); return true; })
		.add_row(STRING(audio), []() -> bool { show_audio_config(); return true; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_START)
		.when_kdown([](u32) -> bool { exit(0); return false; })
		.add_to(queue);

	queue.render_finite_button(KEY_B);
	set_focus(focus);
}

