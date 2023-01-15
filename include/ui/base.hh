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

#ifndef inc_ui_base_hh
#define inc_ui_base_hh

// Defines constants, we copy them over in
// our enum later
#include "build/next.h"

#include <ui/theme.hh>
#include <panic.hh>

#include <citro3d.h>
#include <citro2d.h>
#include <3ds.h>

#include <functional>
#include <sstream>
#include <vector>
#include <string>
#include <list>

#include "i18n.hh"

#define UI_WIDGET(name) \
	public: \
		using ui::BaseWidget::BaseWidget; \
		static constexpr const char *id = name; \
		const char *get_id() override { return name; } \
	private:

#define UI_COLOR(r,g,b,a) \
	0x##a##b##g##r

#define UI_THIS_SWAP_SLOTS(slots) \
	this->slots = ui::ThemeManager::global()->get_slots(this, (slot).id, (slot).count, (slot).getters);

// Button glyphs
#define UI_GLYPH_A               "\uE000"
#define UI_GLYPH_B               "\uE001"
#define UI_GLYPH_X               "\uE002"
#define UI_GLYPH_Y               "\uE003"
#define UI_GLYPH_L               "\uE004"
#define UI_GLYPH_R               "\uE005"
#define UI_GLYPH_DPAD_CLEAR      "\uE006"
#define UI_GLYPH_CPAD            "\uE077"
#define UI_GLYPH_DPAD_UP         "\uE079"
#define UI_GLYPH_DPAD_DOWN       "\uE07A"
#define UI_GLYPH_DPAD_LEFT       "\uE07B"
#define UI_GLYPH_DPAD_RIGHT      "\uE07C"
#define UI_GLYPH_DPAD_HORIZONTAL "\uE07D"
#define UI_GLYPH_DPAD_VERTICAL   "\uE07E"

#define UI_LED_MAKE_ANIMATION(delay, smoothing, loop_delay) \
	((((delay) & 0xFF) << 24) | (((smoothing) & 0xFF) << 16) | (((loop_delay) & 0xFF) << 8))

/* from supplement_sysfont_merger.c */
extern "C" C2D_Font ui__sysFontWithSupplement;

namespace ui
{
	namespace LED
	{
		typedef struct Pattern {
			u32 animation;
			u8 red_pattern[32];
			u8 green_pattern[32];
			u8 blue_pattern[32];
		} Pattern;

		void Solid(Pattern *info, u32 animation, u8 r, u8 g, u8 b);
		inline void Solid(Pattern *info, u32 animation, u32 abgr)
		{ Solid(info, animation, (abgr) & 0xFF, (abgr >> 8) & 0xFF, (abgr >> 16) & 0xFF); }
		Result SetSleepPattern(Pattern *info);
		Result SetPattern(Pattern *info);
		void SetTimeout(time_t newTime);
		Result ResetPattern(void);
		void ClearResetFlags(void);
	}

	class BaseWidget; /* forward declaration */

	enum class Screen
	{
		top, bottom,
		/* to be used by utility widgets */
		none
	};

	namespace dimensions
	{
		constexpr float height       = 240.0f; /* height of both screens */
		constexpr float width_bottom = 320.0f; /* width of the bottom screen */
		constexpr float width_top    = 400.0f; /* width of the top screen */
	}

	namespace layout
	{
		constexpr float center_x = -1.0f; /* center x position */
		constexpr float center_y = -2.0f; /* center y position */
		constexpr float left     =  3.0f; /* left of the screen */
		constexpr float right    = -4.0f; /* right of the screen */
		constexpr float top      =  3.0f; /* top of the screen */
		constexpr float bottom   = -6.0f; /* bottom of the screen */
		constexpr float base     = 35.0f; /* base heigth */
	}

	namespace layer
	{
		constexpr float middle = 0.5f;
		constexpr float top    = 0.9f;
		constexpr float bottom = 0.1f;
	}

	namespace tag
	{
		constexpr int action         = -1;  /* action header */
		constexpr int more           = -2;  /* more button */
		constexpr int settings       = -3;  /* settings button */
		constexpr int search         = -4;  /* search button */
		constexpr int queue          = -5;  /* queue button */
		constexpr int konami         = -6;  /* konami listner */
		constexpr int free_indicator = -7;  /* free space indicator */
		constexpr int random         = -8;  /* random button */
		constexpr int status         = -9;  /* status line */
		constexpr int net_indicator  = -10; /* network indicator */
	};

	/* holds sprite ids used for ui::SpriteStore::get_by_id() */
	enum class sprite : u32
	{
		bun = next_bun_idx,
#undef next_bun_idx
		logo = next_logo_idx,
#undef next_logo_idx
		net_discon = next_net_discon_idx,
#undef next_net_discon_idx
		net_0 = next_net_0_idx,
#undef next_net_0_idx
		net_1 = next_net_1_idx,
#undef next_net_1_idx
		net_2 = next_net_2_idx,
#undef next_net_2_idx
		net_3 = next_net_3_idx,
#undef next_net_3_idx
		gplv3 = next_gplv3_idx,
#undef next_gplv3_idx
	};

	struct Keys
	{
		u32 kDown, kHeld, kUp;
		touchPosition touch;
	};

	inline const char *parse_text(C2D_Text *text, C2D_TextBuf buf, const char *str)
	{
		return C2D_TextFontParse(text, ui__sysFontWithSupplement, buf, str);
	}

	typedef void (*select_command_handler)(u32 kHeld);
	void set_select_command_handler(select_command_handler handler);

	void set_touch_lock(ui::Keys& keys);
	void set_touch_lock();
	void scan_keys();
	u32 kDown();
	u32 kHeld();

	/* do not use */
	void maybe_end_frame();

	/* is there a way we can avoid doing this entirely? */
	void background_rect(ui::Screen scr, float x, float y, float z, float w, float h);

	/* gets the width of a screen */
	constexpr inline float screen_width(ui::Screen scr)
	{ return scr == ui::Screen::top ? ui::dimensions::width_top : ui::dimensions::width_bottom; }
	/* gets the height of the screen (exists for consistency) */
	constexpr inline float screen_height()
	{ return ui::dimensions::height; }

	/* used internally by set_x, set_y,
	 * returns the same coord unless it fits into
	 * ui::layer or ui::layout */
	float transform(BaseWidget *wid, float v);

	/* get the x position right from `from' for `newel' */
	float right(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* get the x position left from `from' for `newel' */
	float left(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* get the y position under `from' for `newel' */
	float under(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* get the y position above `from' for `newel' */
	float above(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* Gets the center y position for owid in wid */
	float ycenter_rel(ui::BaseWidget *wid, ui::BaseWidget *owid);

	/* centers both `first' and `second' */
	void set_double_center(BaseWidget *first, BaseWidget *second, float pad = 3.0f);
	/* returns the center y position of `newel' from `from'. calling with newel > from is UB */
	float center_align_y(BaseWidget *from, BaseWidget *newel);

	/* global initializiation */
	bool init();
	/* init with render targets */
	void init(C3D_RenderTarget *top, C3D_RenderTarget *bot);
	/* global deinitialization */
	void exit();

	/* [msg]\nPress [A] to continue */
	void notice(const std::string& msg, float ypos = 70.0f);

	Result shell_is_open(bool *is_open);

	constexpr inline bool is_touched(const ui::Keys& keys)
	{
		return (keys.kDown | keys.kHeld) & KEY_TOUCH;
	}

	/* base widget class */
	class BaseWidget
	{
	public:
		BaseWidget(ui::Screen scr)
			: screen(scr) { }

		/* not supposed to be overriden */
		virtual ~BaseWidget() = default;

		/* a simple basic constructor */
		virtual void setup() { }

		virtual void set_x(float x)
		{
			if(x == ui::layout::center_x) this->set_center_x();
			else this->x = ui::transform(this, x);
		}

		void set_raw_x(float x) { this->x = x; }
		void set_raw_y(float y) { this->y = y; }

		virtual void set_y(float y) { this->y = ui::transform(this, y); }
		virtual void set_z(float z) { this->z = z; }

		ui::Screen renders_on() { return this->screen; }

		virtual void set_center_x() { this->x = ui::transform(this, ui::layout::center_x); }

		virtual bool render(ui::Keys& keys) = 0;
		virtual void destroy() { }

		virtual float height() = 0;
		virtual float width() = 0;

		virtual const char *get_id() = 0;

		virtual float get_x() { return this->x; }
		virtual float get_y() { return this->y; }
		virtual float get_z() { return this->z; }

		virtual void finalize() { }

		void set_hidden(bool b) { this->hidden = b; }
		bool is_hidden() { return this->hidden; }

		bool matches_tag(int t) { return this->tag == t; }
		void set_tag(int t) { this->tag = t; }

		virtual bool supports_theme_hook() { return false; }
		virtual void update_theme_hook() { }

		virtual void swap_slots(const StaticSlot& slot)
		{
			(void) slot;
			panic("swap_slots() called on widget that does not implement it");
		}

		virtual bool processes_in_sleep() { return false; }
		virtual bool process_in_sleep() { return true; }


	protected:
		ui::Screen screen;
		float z = ui::layer::middle;
		bool hidden = false;
		float x = 0, y = 0;
		int tag = 0;


	};

	/* class for basic operations on an array of widgets */
	class WidgetGroup
	{
	public:
		void position_under(ui::BaseWidget *other, float initpad = 4.0f, float elpadding = 2.0f);
		void position_under_horizontal(ui::BaseWidget *other, float pad = 3.0f);
		void set_y_descending(float base, float elpadding = 2.0f);
		void translate(float x, float y);
		void add(ui::BaseWidget *w);
		void set_hidden(bool b);

		ui::BaseWidget *max_height();
		ui::BaseWidget *max_width();
		ui::BaseWidget *min_height();
		ui::BaseWidget *min_width();
		ui::BaseWidget *highest();
		ui::BaseWidget *lowest();
		ui::BaseWidget *rightmost();
		ui::BaseWidget *leftmost();

	private:
		std::vector<ui::BaseWidget *> ws;

	};

	/* Renders a list of derivatives of ui::BaseWidget */
	class RenderQueue
	{
	public:
		~RenderQueue();

		/* ensure all RenderQueue's will stop rendering on all threads, if this is called
		 * this function may deadlock, be careful */
		static void terminate_render();

		/* push a new widget into the queue */
		void push(ui::BaseWidget *wid);
		/* render forever */
		void render_forever();
		/* render until render_frame() returns false */
		void render_finite();
		/* Renders until keys.kDown & kDownMask > 0 */
		void render_until_button(u32 kDownMask);
		/* Renders until keys.kDown & kDownMask > 0 or if render_frame() returns false */
		void render_finite_button(u32 kDownMask);
		/* returns false if this should be the last frame,
		 * else returns true.
		 * Same as render_frame() except the global queue doesn't get rendered */
		bool render_exclusive_frame(ui::Keys&);
		/* returns false if this should be the last frame,
		 * else returns true */
		bool render_frame(ui::Keys&);
		/* returns false if this should be the last frame,
		 * else returns true */
		bool render_frame();
		/* renders only the bottom widgets */
		bool render_bottom(ui::Keys&);
		/* renders only the top widgets */
		bool render_top(ui::Keys&);
		/* renders a frame on `screen` */
		bool render_screen(ui::Keys&, ui::Screen screen);
		/* removes all widgets in the queue */
		void clear();
		/* runs the callback after the frame render is done. Runs only once
		 * NOTE: you can only have 1 callback every frame
		 * NOTE 2: only works on the global renderqueue */
		void render_and_then(std::function<bool()> cb);
		void render_and_then(std::function<void()> cb);
		/* Detaches a callback set by
		 * render_and_then */
		void detach_after();
		/* Signals the RenderQueue */
		void signal(u8 bits);
		/* Unsets a signal from the RenderQueue */
		void unsignal(u8 bits);
		/* find a widget by tag
		 * First searches top widgets, then bottom
		 * Returns nullptr if no matches were found */
		template <typename TWidget = ui::BaseWidget>
		TWidget *find_tag(int t)
		{
			for(ui::BaseWidget *w : this->top)
				if(w->matches_tag(t)) return (TWidget *) w;
			for(ui::BaseWidget *w : this->bot)
				if(w->matches_tag(t)) return (TWidget *) w;
			return nullptr;
		}
		/* gets the last pushed element
		 * returns nullptr if the queue is empty */
		template <typename TWidget = ui::BaseWidget>
		TWidget *back()
		{
			return (TWidget *) this->backPtr;
		}
		/* adds the last pushed element to a group */
		void group_last(ui::WidgetGroup& group)
		{
			group.add(this->backPtr);
		}

		/* Gets the global RenderQueue */
		static ui::RenderQueue *global();
		/* Gets the pressed keys */
		static ui::Keys get_keys();

		enum signal { signal_cancel = 1 };

		std::list<ui::BaseWidget *> sleepProcessors;
		std::list<ui::BaseWidget *> top;
		std::list<ui::BaseWidget *> bot;

	private:
		int enter_frame();
		bool exit_frame();

		std::function<bool()> *after_render_complete = nullptr;
		ui::BaseWidget *backPtr = nullptr;
		u8 signalBit = 0;


	};

	inline constexpr float diff(float a, float b)
	{
		return a > b ? a - b : b - a;
	}

	/* builder for a ui::BaseWidget derivative */
	template <typename TWidget>
	class builder
	{
	public:
		template<typename ... Ts>
		builder(ui::Screen scr, Ts&& ... args)
		{
			this->el = new TWidget(scr);
			this->el->setup(args...);
		}

		~builder()
		{
			if(this->el != nullptr)
				delete this->el;
		}

		/* Sets the size of a widget. Not supported by all widgets */
		ui::builder<TWidget>& size(float x, float y) { this->el->resize(x, y); return *this; }
		/* Sets the size of a widget. Not supported by all widgets */
		ui::builder<TWidget>& size(float xy) { this->el->resize(xy, xy); return *this; }
		/* Sets the size of any potential widget children. Not supported by all widgets */
		ui::builder<TWidget>& size_children(float x, float y) { this->el->resize_children(x, y); return *this; }
		/* Sets the size of any potential widget children. Not supported by all widgets */
		ui::builder<TWidget>& size_children(float xy) { this->el->resize_children(xy, xy); return *this; }
		/* Autowraps the widget. Not supported by all widgets */
		ui::builder<TWidget>& wrap() { this->el->autowrap(); return *this; }
		/* Sets a border around the widget. Not supported by all widgets */
		ui::builder<TWidget>& border() { this->el->set_border(true); return *this; }
		/* Makes the widget scroll. Not supported by all widgets */
		ui::builder<TWidget>& scroll() { this->el->scroll(); return *this; }
		/* Connects a type and argument, effect depends on the widget.
		 * Not supported by all widgets */
		template <typename T = TWidget /* weird template is so that builder works on types without T::connect_type */,
			typename ... Ts> ui::builder<T>& connect(typename T::connect_type type, Ts&& ... args)
				{ this->el->connect(type, args...); return *this; }

		/* Do a manual configuration with a callback */
		ui::builder<TWidget>& configure(std::function<void(TWidget*)> conf) { conf(this->el); return *this; }
		/* Hide/unhide the widget */
		ui::builder<TWidget>& hide(bool hidden = true) { this->el->set_hidden(hidden); return *this; }
		/* Set the tag of the widget */
		ui::builder<TWidget>& tag(int t) { this->el->set_tag(t); return *this; }
		/* Set x position. note: you most likely want to call this last */
		ui::builder<TWidget>& x(float v) { this->el->set_x(v); return *this; }
		/* Set y position. note: you most likely want to call this last */
		ui::builder<TWidget>& y(float v) { this->el->set_y(v); return *this; }
		/* Set z position. note: you most likely want to call this last */
		ui::builder<TWidget>& z(float v) { this->el->set_z(v); return *this; }
		/* positions the widget under another widget */
		ui::builder<TWidget>& under(ui::BaseWidget *w, float pad = 3.0f) { this->el->set_y(ui::under(w, this->el, pad)); return *this; }
		/* positions the widget above another widget */
		ui::builder<TWidget>& above(ui::BaseWidget *w, float pad = 3.0f) { this->el->set_y(ui::above(w, this->el, pad)); return *this; }
		/* positions the widget right from another widget */
		ui::builder<TWidget>& right(ui::BaseWidget *w, float pad = 3.0f) { this->el->set_x(ui::right(w, this->el, pad)); return *this; }
		/* positions the widget in the center next to another widget */
		ui::builder<TWidget>& next_center(ui::BaseWidget *w, float pad = 3.0f) { ui::set_double_center(w, this->el, pad); return *this; }
		/* positions the widget left from another widget */
		ui::builder<TWidget>& left(ui::BaseWidget *w, float pad = 3.0f) { this->el->set_x(ui::left(w, this->el, pad)); return *this; }
		/* sets the y position to the center of w relative to the y-axis */
		ui::builder<TWidget>& middle(ui::BaseWidget *w, float offset = -1.0f) { this->el->set_y(ui::ycenter_rel(w, this->el) - offset); return *this; }
		/* sets the y position of the OTHER widget to the center of this relative to the y-axis */
		ui::builder<TWidget>& omiddle(ui::BaseWidget *w, float offset = -1.0f) { w->set_y(ui::ycenter_rel(this->el, w) - offset); return *this; }
		/* sets the y of the building widget to that of another one offseted so it hits the baseline of the other one */
		ui::builder<TWidget>& align_y(ui::BaseWidget *w, float offset = 0.0f) { this->el->set_y(w->get_y() - offset + diff(w->height(), this->el->height())); return *this; }
		/* sets the x of the building widget to that of another one */
		ui::builder<TWidget>& align_x(ui::BaseWidget *w, float offset = 0.0f) { this->el->set_x(w->get_x() + offset); return *this; }
		/* sets the y of the building widget to the center of another one */
		ui::builder<TWidget>& align_y_center(ui::BaseWidget *w) { this->el->set_y(ui::center_align_y(w, this->el)); return *this; }
		/* swaps the slots for a widget */
		ui::builder<TWidget>& swap_slots(const StaticSlot& slot) { this->el->swap_slots(slot); return *this; }

		/* Add the built widget to a RenderQueue */
		void add_to(ui::RenderQueue& queue) { queue.push((ui::BaseWidget *) this->finalize()); }
		/* Add the built widget to a RenderQueue */
		void add_to(ui::RenderQueue *queue) { queue->push((ui::BaseWidget *) this->finalize()); }
		/* Add the built widget to a RenderQueue and your own pointer */
		void add_to(TWidget **ret, ui::RenderQueue& queue) { *ret = this->finalize(); queue.push((ui::BaseWidget *) *ret); }
		/* Add the built widget to a RenderQueue and your own pointer */
		void add_to(TWidget **ret, ui::RenderQueue *queue) { *ret = this->finalize(); queue->push((ui::BaseWidget *) *ret); }
		/* finalize the built widget and return a pointer to it */
		TWidget *finalize() { this->el->finalize(); TWidget *ret = this->el; this->el = nullptr; return ret; }


	private:
		TWidget *el = nullptr;


	};

	class SpriteStore
	{
	public:
		SpriteStore(const std::string& fname);
		SpriteStore() { }
		~SpriteStore();

		void open(const std::string& fname);
		size_t size();

		static C2D_Sprite get_by_id(ui::sprite id);


	private:
		C2D_SpriteSheet sheet = nullptr;


	};

	/* A wrapper for ui::BaseWidget with automatic:tm: management */
	template <typename TWidget>
	class ScopedWidget
	{
	public:
		~ScopedWidget()
		{
			this->destroy();
		}

		void destroy()
		{
			if(this->wid != nullptr)
			{
				this->wid->destroy();
				delete this->wid;
				this->wid = nullptr;
			}
		}

		template <typename ... Ts>
		void setup(ui::Screen scr, Ts&& ... args)
		{
			this->wid = new TWidget(scr);
			this->wid->setup(args...);
		}

		/* for some widgets it's of great importance to call this when
		 * you're finished configuring, you should always do it just in case */
		void finalize()
		{
			this->wid->finalize();
		}

		TWidget *ptr() { return this->wid; }

		TWidget *operator -> () { return this->wid; }
		TWidget& operator * () { return *this->wid; }


	private:
		TWidget *wid = nullptr;


	};

	UI_SLOTS_PROTO_EXTERN(Text_color)
	class Text : public ui::BaseWidget
	{ UI_WIDGET("Text")
	public:
		void setup(const std::string& label);
		void setup(); /* inits with an empty string */
		void destroy() override;

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void resize(float x, float y);
		void autowrap();
		void scroll(); /* only supported with !center(), doesn't look amazing with multiline text but works */

		void set_center_x() override;

		/* sets the text of the widget to a new string */
		void set_text(const std::string& label);
		/* gets the current text of the widget */
		const std::string& get_text();

		enum connect_type { max_width };
		void connect(connect_type, float);

		void swap_slots(const StaticSlot&) override;

	private:
		UI_SLOTS_PROTO(Text_color, 1)

		void push_str(const std::string& str);
		void prepare_arrays();
		void reset_scroll();

		float xsiz = 0.65f, ysiz = 0.65f;
		std::vector<C2D_Text> lines;
		C2D_TextBuf buf = nullptr;
		bool doAutowrap = false;
		float lineHeight = 0.0f;
		bool drawCenter = false;
		bool doScroll = false;
		float maxw = 0.0f;
		std::string text;

		struct ScrollCtx {
			size_t timing;
			size_t offset;
			float height;
			float width;
			float rx;
		} sctx;


	};

	class Sprite : public ui::BaseWidget
	{ UI_WIDGET("Sprite")
	public:
		void destroy() override { ui::ThemeManager::global()->unregister(this); }

		static void spritesheet(C2D_Sprite& sprite, u32 data)
		{
			C2D_DrawParams params = sprite.params;
			sprite = ui::SpriteStore::get_by_id((ui::sprite) data);
			params.pos.w = sprite.params.pos.w;
			params.pos.h = sprite.params.pos.h;
			sprite.params = params;
		}
		static void theme(C2D_Sprite& sprite, u32 data)
		{
			sprite.image = *ui::Theme::global()->get_image(data);
			if(sprite.image.subtex) /* this may be false for an optional image (and then has_image() would return false) */
			{
				sprite.params.pos.w = sprite.image.subtex->width;
				sprite.params.pos.h = sprite.image.subtex->height;
			}
		}
		static void image(C2D_Sprite& sprite, u32 data)
		{
			C2D_Image *ptr = (C2D_Image *) data;
			sprite.image = *ptr;
			sprite.params.pos.w = sprite.image.subtex->width;
			sprite.params.pos.h = sprite.image.subtex->height;
		}

		void setup(std::function<void(C2D_Sprite&, u32)> get_cb, u32 data = 0);

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_x(float x) override;
		void set_y(float y) override;
		void set_z(float z) override;

		void set_center(float x, float y);
		void set_data(u32 data);
		void rotate(float degs);

		inline C2D_Sprite& get_sprite() { return this->sprite; }

		bool supports_theme_hook() override { return true; }
		void update_theme_hook() override;

		inline bool has_image() { return this->sprite.image.tex != NULL; }
		inline C2D_Image *get_image() { return &this->sprite.image; }


	private:
		std::function<void(C2D_Sprite&, u32)> get_sprite_func;
		C2D_Sprite sprite;
		u32 unspecified_data;


	};

	UI_SLOTS_PROTO_EXTERN(Button_colors)
	class Button : public ui::BaseWidget
	{ UI_WIDGET("Button")
	public:
		using click_cb_t = std::function<bool()>;

		void setup(std::function<void(C2D_Sprite&, u32)> get_image_cb, u32 data);
		void setup(const std::string& label);
		void destroy() override;
		void setup();

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_border(bool b);

		void resize_children(float x, float y);
		void resize(float x, float y);
		void set_x(float x) override;
		void set_y(float x) override;
		void set_z(float z) override;

		bool press();

		/* autowrap for text size */
		void autowrap();
		/* update the label or set one */
		void set_label(const std::string& v);
		/* get the label, if the subwidget is not a ui::Text this probably crashes */
		const std::string& get_label();

		enum connect_type { click, nobg };
		/* click */
		void connect(connect_type type, click_cb_t callback);
		/* nobg */
		void connect(connect_type type);

		float textwidth();

		/* NOTE: This doesn't swap this->slots, but this->widget->slots...
		 * TODO: There should probably be a way to swap this->slots */
		void swap_slots(const StaticSlot&) override;

	private:
		UI_SLOTS_PROTO(Button_colors, 2)

		click_cb_t on_click = []() -> bool { return true; };
		bool showBg = true, showBorder = false;
		ui::BaseWidget *widget = nullptr;
		float ox = 0.0f, oy = 0.0f;
		float w = 0.0f, h = 0.0f;

		void readjust();


	};

	/* utility */

	class ButtonCallback : public ui::BaseWidget
	{ UI_WIDGET("ButtonCallback")
	public:
		void setup(u32 keys);

		bool render(ui::Keys&) override;
		float height() override { return 0.0f; }
		float width() override { return 0.0f; }

		enum connect_type {
			none,
			kdown,
			kheld,
			kup,
		};
		/* activate */
		void connect(connect_type type, std::function<bool(u32)> cb);


	private:
		std::function<bool(u32)> cb = [](u32) -> bool { return true; };
		connect_type type = none;
		u32 keys;


	};

	UI_SLOTS_PROTO_EXTERN(Toggle_color)
	class Toggle : public ui::BaseWidget
	{ UI_WIDGET("Toggle")
	public:
		void setup(bool state, std::function<void()> on_toggle_cb);
		bool render(ui::Keys& keys) override;
		float height() override { return 20.0f; }
		float width() override { return 40.0f; }
		void toggle(bool toggled);
		void set_toggled(bool toggled);

	private:
		UI_SLOTS_PROTO(Toggle_color, 3)
		std::function<void()> toggle_cb;
		bool toggled_state;
		u64 last_touch_toggle;

	};

	template <typename T>
	std::string floating_prec(T inte, int prec = 2)
	{
		std::ostringstream ss; ss.precision(prec);
		ss << std::fixed << inte; return ss.str();
	}

	template <typename TInt>
	std::string human_readable_size(TInt i)
	{
		// Sorry for this mess.....
		if(i < 1024) return std::to_string(i) + " B"; /* < 1 KiB */
		if(i < 1024 * 1024) /* < 1 MiB */
			return floating_prec<float>((float) i / 1024) + " KiB";
		if(i < 1024 * 1024 * 1024) /* < 1 GiB */
			return floating_prec<float>((float) i / 1024 / 1024) + " MiB";
		if(i < (long long) 1024 * 1024 * 1024 * 1024) /* < 1TiB */
			return floating_prec<float>((float) i / 1024 / 1024 / 1024) + " GiB";
		return floating_prec<float>((float) i / 1024 / 1024 / 1024 / 1024) + " TiB";
	}

	template <typename TInt>
	std::string human_readable_size_block(TInt i)
	{
		std::string ret = human_readable_size<TInt>(i);
		TInt blk = (double) i / 1024 / 128;
		blk = blk == 0 ? 1 : blk;

		return PSTRING(size_plus_block, ret, blk);
	}
}

#endif

