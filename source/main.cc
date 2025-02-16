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

#include <3ds.h>

#include <ui/list.hh>
#include <ui/base.hh>

#include <widgets/indicators.hh>
#include <widgets/konami.hh>
#include <widgets/meta.hh>

#include "audio/configuration.h"
#include "audio/cwav_reader.h"
#include "audio/player.h"
#include "lumalocale.hh"
#include "installgui.hh"
#include "settings.hh"
#include "log_view.hh"
#include "extmeta.hh"
#include "update.hh"
#include "search.hh"
#include "queue.hh"
#include "panic.hh"
#include "about.hh"
#include "hsapi.hh"
#include "more.hh"
#include "next.hh"
#include "i18n.hh"
#include "seed.hh"
#include "util.hh"
#include "log.hh"
#include "ctr.hh"

#define ENVINFO (* (u8 *) 0x1FF80014)
#define VERSION_CHECK 1
#define TIP_GIVER 0

#ifndef RELEASE
class FrameCounter : public ui::BaseWidget
{ UI_WIDGET("FrameCounter")
public:
	float width() override { return this->t->width(); }
	float height() override { return this->t->height(); }
	void set_x(float x) override { this->x = x; this->t->set_x(x); }
	void set_y(float y) override { this->y = y; this->t->set_y(y); }
	void resize(float x, float y) { this->t->resize(x, y); }

	void setup()
	{
		this->t.setup(this->screen, "0 fps");
	}

	bool render(ui::Keys& k) override
	{
		time_t now = time(NULL);
		if(now != this->frames[this->i].time)
			this->switch_frame(now);
		++this->frames[this->i].frames;
		this->t->render(k);
		return true;
	}

	int fps()
	{
		return this->frames[1 - this->i].frames;
	}

private:
	struct {
		time_t time;
		int frames;
	} frames[2] = {
		{ 0, 60 },
		{ 0, 60 },
	};

	ui::ScopedWidget<ui::Text> t;
	size_t i = 0;

	void set_label(int fps)
	{
		this->t->set_text(std::to_string(fps) + " fps");
		this->t->set_x(this->x);
	}

	void switch_frame(time_t d)
	{
		this->set_label(this->frames[this->i].frames);
		this->i = 1 - this->i;
		this->frames[this->i].time = d;
		this->frames[this->i].frames = 0;
	}

};
#endif

#if TIP_GIVER
class TipGiver : public ui::BaseWidget
{ UI_WIDGET("TipGiver")
public:
	void setup()
	{
		this->frames_until_tip = this->initial_frames_until_tip();
	}

	float height() override { return 0.0f; }
	float width() override { return 0.0f; }

	bool render(ui::Keys&) override
	{
		/* don't advance if we're already using the status or installing a game */
		if(install::is_in_progress() || status_running() || !this->frames_until_tip)
			return true;

		--this->frames_until_tip;
		if(!this->frames_until_tip)
		{
			this->frames_until_tip = this->next_frames_until_tip();
			set_ticker(this->select_string());
		}
		return true;
	}

private:
	/* frames_to_seconds = frames => frames * 60 */
	/* seconds_to_frames = secs => secs / 60 */
	unsigned initial_frames_until_tip()
	{
		return 30;
	}

	unsigned next_frames_until_tip()
	{
		return ~0;
	}

	const char *select_string()
	{
		return STRING(do_donate);
	}

	unsigned frames_until_tip;

};
#endif

static void brick_negro()
{
}

void make_render_queue(ui::RenderQueue& queue, ui::ProgressBar **bar, const std::string& label);

int main(int argc, char* argv[])
{
	((void) argc);
	((void) argv);
	Result res;

	/* If the settings were reset by ensure_settings() the language was detected */
	bool languageDetected = ensure_settings(); /* log_init() uses settings ... */
	atexit(settings_sync);
	log_init();
	atexit(log_exit);
#ifdef RELEASE
	#define EV
#else
	#define EV "-debug"
#endif
	ilog("current 3hs version is " VVERSION EV "%s" " \"" VERSION_DESC "\"", envIsHomebrew() ? "-3dsx" : "");
#undef EV
	log_settings();

	bool isLuma = false;
	res = init_services(isLuma);
	panic_assert(R_SUCCEEDED(res),
		"init_services() failed, this should **never** happen (0x" + pad8code(res) + ")");
	atexit(exit_services);
	load_current_theme();
	atexit(cleanup_themes);
	panic_assert(themes().size() > 0, "failed to load any themes");
	panic_assert(ui::init(), "ui::init() failed, this should **never** happen");
	atexit(ui::exit);
	gfx_was_init();

	hidScanInput();
	if((hidKeysDown() | hidKeysHeld()) & KEY_R)
	{
		reset_settings();
		languageDetected = false;
	}

	if(get_nsettings()->lang == lang::spanish)
		brick_negro();

	/* Checking if the user actually speaks the target language should be done before any other string is display by the user */
	if(languageDetected && get_nsettings()->lang != lang::english)
	{
		/* These strings must be in English */
		std::string lang = i18n::langname(get_nsettings()->lang);
		std::string string = PSTRING(automatically_detected, lang);
		string += "\n3hs has automatically detected the system language is ";
		string += lang;
		string += ". Press " UI_GLYPH_B " to reset to English.";

		/* TODO: The confirmation menu perhaps needs some more work */
		if(!ui::Confirm::exec("Is this correct?", string))
			get_nsettings()->lang = lang::english;
	}

#if VERSION_CHECK
	// Check if we are under system version 9.6 (9.6 added seed support, which is essential for most new titles)
	OS_VersionBin version;
	if(R_SUCCEEDED(osGetSystemVersionData(nullptr, &version)))
		if(SYSTEM_VERSION(version.mainver, version.minor, version.build) < SYSTEM_VERSION(9, 6, 0))
		{
			flog("User is on an unsupported system version: %d.%d.%d", version.mainver, version.minor, version.build);
			ui::notice(STRING(outdated_system));
			exit(0);
		}
#endif

#ifdef RELEASE
	// Check if luma is installed
	// 1. Citra is used; not compatible
	// 2. Other cfw used; not supported
	if(!isLuma)
	{
		flog("Luma3DS is not installed, user is using an unsupported CFW or running in Citra");
		ui::RenderQueue queue;

		ui::builder<ui::Text>(ui::Screen::top, STRING(luma_not_installed))
			.x(ui::layout::center_x).y(45.0f)
			.wrap()
			.add_to(queue);
		ui::builder<ui::Text>(ui::Screen::top, STRING(install_luma))
			.x(ui::layout::center_x).under(queue.back())
			.wrap()
			.add_to(queue);

		queue.render_finite_button(KEY_START | KEY_B);
		exit(0);
	}
#endif

	if(!(ENVINFO & 1))
	{
		flog("Detected dev ENVINFO, aborting startup");

		ui::RenderQueue queue;

		ui::builder<ui::Text>(ui::Screen::top, STRING(dev_unitinfo))
			.x(ui::layout::center_x).y(45.0f)
			.wrap()
			.add_to(queue);

		queue.render_finite_button(KEY_START | KEY_B);
		exit(0);
	}

	osSetSpeedupEnable(true); // speedup for n3dses

	/* new ui setup */
	ui::builder<ui::Text>(ui::Screen::top) /* text is not immediately set */
		.x(ui::layout::center_x)
		.y(4.0f)
		.tag(ui::tag::action)
		.wrap()
		.add_to(ui::RenderQueue::global());

	/* buttons */
	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::settings_image)
		.when_clicked([]() -> bool {
			ui::RenderQueue::global()->render_and_then(show_settings);
			return true;
		})
		.disable_background()
		.wrap()
		.x(5.0f)
		.y(210.0f)
		.tag(ui::tag::settings)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::more_image)
		.when_clicked([]() -> bool {
			ui::RenderQueue::global()->render_and_then(show_more);
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::more)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::search_image)
		.when_clicked([]() -> bool {
			ui::RenderQueue::global()->render_and_then(show_search);
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::search)
		.add_to(ui::RenderQueue::global());

	static bool isInRand = false;
	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::random_image)
		.when_clicked([]() -> bool {
			ui::RenderQueue::global()->render_and_then([]() -> void {
				if(isInRand) return;
				isInRand = true;
				hsapi::Title t;
				if(R_SUCCEEDED(hsapi::call(hsapi::random, t)) && show_extmeta(t))
					install::gui::hs_cia(t);
				isInRand = false;
			});
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::random)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, STRING(queue))
		.when_clicked([]() -> bool {
			ui::RenderQueue::global()->render_and_then(show_queue);
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::queue)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::KonamiListner>(ui::Screen::top)
		.tag(ui::tag::konami)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::FreeSpaceIndicator>(ui::Screen::top)
		.tag(ui::tag::free_indicator)
		.add_to(ui::RenderQueue::global());

	ui::builder<StatusLine>(ui::Screen::top)
		.tag(ui::tag::status)
		.add_to(ui::RenderQueue::global());

#if TIP_GIVER
	ui::builder<TipGiver>(ui::Screen::top)
		.add_to(ui::RenderQueue::global());
#endif

	ui::builder<ui::TimeIndicator>(ui::Screen::top)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::BatteryIndicator>(ui::Screen::top)
		.add_to(ui::RenderQueue::global());

#ifndef RELEASE
	ui::builder<FrameCounter>(ui::Screen::top)
		.size(0.4f)
		.x(ui::layout::right).y(20.0f)
		.add_to(ui::RenderQueue::global());
#endif

	ui::builder<ui::NetIndicator>(ui::Screen::top)
		.tag(ui::tag::net_indicator)
		.add_to(ui::RenderQueue::global());

	// DRM Check
#ifdef DEVICE_ID
	u32 devid = 0;
	panic_assert(R_SUCCEEDED(psInit()), "failed to initialize PS");
	PS_GetDeviceId(&devid);
	psExit();
	// DRM Check failed
	if(devid != DEVICE_ID)
	{
		flog("Piracyception");
		(* (int *) nullptr) = 0xdeadbeef;
	}
#endif
	// end DRM Check
	//

	/* initialize audio subsystem */
	panic_assert(R_SUCCEEDED(player_init()), "failed to initialize audio system");
	atexit(player_exit);
	panic_assert(acfg_load() == ACE_NONE, "failed to load audio configuration");
	atexit(acfg_free);

	player_set_switch_callback([](const struct cwav *cwav) -> void {
		if(cwav->artist) set_status(PSTRING(playing_x_by_y, cwav->title, cwav->artist));
		else             set_status(PSTRING(playing_x, cwav->title));
	});

	panic_assert(acfg_realise() == ACE_NONE, "failed to set audio configuration");

	ui::set_select_command_handler([](u32 kDown) -> void {
		/* process audio command */
		if(kDown & KEY_L) player_previous();
		if(kDown & KEY_R) player_next();
		if(kDown & KEY_A) player_unpause();
		if(kDown & KEY_B) player_pause();
		if(kDown & KEY_X) { player_halt(); reset_status(); }
	});

#ifdef RELEASE
	// If we updated ...
	ilog("Checking for updates");
	if(update_app())
	{
		ilog("Updated from " VERSION);
		exit(0);
	}
#endif

	while(R_FAILED(hsapi::call(hsapi::fetch_index)))
		show_more();

	vlog("Done fetching index.");

	size_t catptr = 0, subptr = 0;
	hsapi::hcid associatedcat = -1, associatedsub = -1;
	std::vector<hsapi::PartialTitle> titles;
	bool visited_sub = false, visited_gam = false;
	next::gam_reenter_data grdata;

	// Old logic was cursed, made it a bit better :blobaww:
	while(aptMainLoop())
	{
cat:
		hsapi::hcid cat = next::sel_cat(&catptr);
		// User wants to exit app
		if(cat == next_cat_exit) break;
		ilog("NEXT(c): %s", hsapi::category(cat).name.c_str());
		/* we need to reset this since we've changed categories, meaning
		 * the subcategory data is invalid */
		if(cat != associatedcat)
		{
			associatedsub = -1;
			subptr = 0;
		}
		associatedcat = cat;

sub:
		hsapi::hcid sub = next::sel_sub(cat, &subptr);
		if(sub == next_sub_back) goto cat;
		if(sub == next_sub_exit) break;
		ilog("NEXT(s): %s", hsapi::subcategory(cat, sub).name.c_str());

		if(associatedsub != sub)
		{
			titles.clear();
			const hsapi::Category& cato = hsapi::category(cat);
			const hsapi::Subcategory& subo = hsapi::subcategory(cat, sub);
			visited_gam = false;
			if(R_FAILED(hsapi::call(hsapi::titles_in, titles, cato, subo)))
				goto sub;
		}
		else visited_gam = true;
		associatedsub = sub;

gam:
//		hsapi::hid id = next::sel_icon_gam(titles, hsapi::category(cat), hsapi::subcategory(cat, sub)); //&grdata, visited_gam);
		hsapi::hid id = next::sel_gam(titles, &grdata, visited_gam);
		if(id == next_gam_back) goto sub;
		if(id == next_gam_exit) break;
		/* this means we've been in the category before,
		 * this will get set to false if we re-enter */
		visited_gam = true;

		ilog("NEXT(g): %lli", id);

		hsapi::Title meta;
		if(show_extmeta_lazy(titles, id, &meta))
			install::gui::hs_cia(meta);

		goto gam;
	}

	ilog("Goodbye, app deinit");
	exit(0);
}

