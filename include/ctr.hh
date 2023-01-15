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

#ifndef inc_ctr_hh
#define inc_ctr_hh

#include <vector>
#include <string>

typedef struct AM_TicketEntry
{
	u64 title_id;
	u64 ticket_id;
	u16 version;
	u16 padding;
	u32 size;
} AM_TicketEntry;

#define CTR_REGION_ERROR 0xFF
#define CTR_REGION_UNSET 0xFE

#define MIIPLAZA_UNIQ_JPN 0x00208
#define MIIPLAZA_UNIQ_USA 0x00218
#define MIIPLAZA_UNIQ_EUR 0x00228
#define MIIPLAZA_UNIQ_CHN 0x00268
#define MIIPLAZA_UNIQ_KOR 0x00278
#define MIIPLAZA_UNIQ_TWN 0x00288


namespace ctr {
	enum Destination
	{
		DEST_CTRNand, DEST_TWLNand,
		DEST_Sdmc,
	};

	enum class RegionLockout : u32
	{
		JPN = 0x01, USA = 0x02, EUR = 0x04,
		AUS = 0x08, CHN = 0x10, KOR = 0x20,
		TWN = 0x40, WORLD = 0x7FFFFFFF,
	};

	enum class Region
	{
		JPN, USA, EUR, // EUR includes AUS
		CHN, KOR, TWN,
		WORLD,
	};

	enum class Ratings : u32
	{
		CERO       = 1 << 0,  // Japan
		ESRB       = 1 << 1,  // USA
		_reserved0 = 1 << 2,
		USK        = 1 << 3,  // Germany
		PEGI_GEN   = 1 << 4,  // Europe
		_reserved1 = 1 << 5,
		PEGI_PRT   = 1 << 6,  // Portugal
		PEGI_BBFC  = 1 << 7,  // England
		COB        = 1 << 8,  // Australia
		GRB        = 1 << 9,  // South Korea
		CGSRR      = 1 << 10, // Taiwan
		_reserved2 = 1 << 11,
		_reserved3 = 1 << 12,
		_reserved5 = 1 << 13,
		_reserved6 = 1 << 14,
		_reserved7 = 1 << 15,
	};


	typedef struct TitleSMDHTitle
	{
		u16 descShort[0x40]; // wchar
		u16 descLong[0x80]; // wchar
		u16 publisher[0x40]; // wchar
	} TitleSMDHTitle;

	// https://www.3dbrew.org/wiki/SMDH
	typedef struct TitleSMDH
	{
		// Header
		char magic[0x4]; // constant: 'SMDH'
		u16 version;

		u16 reserved_1;

		// Titles
		TitleSMDHTitle titles[0x10];

		// App settings
		u8 ratings[0x10]; // array, index with enum Ratings
		u32 region; // bitfield of enum RegionLockout
		u32 matchMaker;
		u64 matchMakerBit;
		u32 flags;
		u16 eulaVersion;
		u16 reserved_2;
		u32 optimalBnrFrame; // actually a float, but unused so it doesn't matter
		u32 CECID; // streetpass

		u64 reserved_3;

		// GFX
		u8 iconSmall[0x0480]; // 24x24
		u8 iconLarge[0x1200]; // 48x48
	} TitleSMDH;

	u64 str_to_tid(const std::string& str);
	std::string tid_to_str(u64 tid);

	Result list_titles_on(FS_MediaType media, std::vector<u64>& ret);
	Result list_tickets(std::vector<u64>& ret);

	Result get_free_space(Destination media, u64 *size);
	Result get_title_entry(u64 tid, AM_TitleEntry& entry);

	bool title_exists(u64 tid, FS_MediaType media = MEDIATYPE_SD);
	bool ticket_exists(u64 tid);

	namespace DeleteTitleFlag
	{
		enum {
			None           = 0,
			DeleteTicket   = 1,
			CheckExistance = 2,
		};
		using Type = u32;
	}

	Result delete_title(u64 tid, FS_MediaType media, DeleteTitleFlag::Type flags);
	Result delete_ticket(u64 tid);

	// tid stuff, can be inlined

	inline u32 get_tid_unique(u64 tid)
	{
		return (tid >> 8) & 0xFFFFFF;
	}

	inline u16 get_tid_cat(u64 tid)
	{
		return (tid >> 32) & 0xFFFF;
	}

	inline bool is_base_tid(u64 tid)
	{
		u16 cat = get_tid_cat(tid);
		/* 0x4 = AddOnContents,
		* which updates (0xE) and DLC (0x8C) also include
		* 0x8000 = DSiWare,
		* consider all DSiWare base. For some reason sets 0x4 for all DSiWare */
		return (cat & 0x8000) || (cat & 0x4) == 0;
	}

	inline u64 get_base_tid(u64 tid)
	{
		/* clear the bits of cat (2nd u16) */
		u64 ret = tid & 0xFFFF0000FFFFFFFF;
		u32 uniq = get_tid_unique(tid);

		if (uniq == MIIPLAZA_UNIQ_JPN || uniq == MIIPLAZA_UNIQ_USA || uniq == MIIPLAZA_UNIQ_EUR || uniq == MIIPLAZA_UNIQ_CHN || uniq == MIIPLAZA_UNIQ_KOR || uniq == MIIPLAZA_UNIQ_TWN)
			ret |= 0x1000000000; // add systitle bit

		return ret;
	}

	// https://www.3dbrew.org/wiki/Titles#Title_IDs
	static inline Destination detect_dest(u64 tid)
	{
		u16 cat = (tid >> 32) & 0xFFFF;
		// Install on nand on (DlpChild, System, TWL), else install on SD
		return (cat & (0x1 | 0x10 | 0x8000))
			? (cat & 0x8000 ? DEST_TWLNand : DEST_CTRNand)
			: DEST_Sdmc;
	}
	static inline FS_MediaType to_mediatype(Destination dest) { return dest == DEST_Sdmc ? MEDIATYPE_SD : MEDIATYPE_NAND; }
	static inline FS_MediaType mediatype_of(u64 tid) { return to_mediatype(detect_dest(tid)); }

	namespace smdh
	{
		TitleSMDHTitle *get_native_title(TitleSMDH *smdh);
		std::string u16conv(u16 *str, size_t size);
		TitleSMDH *get(u64 tid);
	}

	u8 get_system_region();

	Result increase_sleep_lock_ref();
	void decrease_sleep_lock_ref();
	void delete_sleep_lock();

	bool running_on_new_series();

	class LockedInScope
	{
	public:
		LockedInScope(LightLock *lock) { LightLock_Lock(this->lock = lock); }
		~LockedInScope() { LightLock_Unlock(this->lock); }
	private:
		LightLock *lock;
	};
}

#endif

