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
/* vim: foldmethod=marker */

#include "httpclient.hh"
#include "hsapi.hh"
#include "error.hh"
#include "proxy.hh"
#include "ctr.hh"
#include "log.hh"

#include <nbapi/nbtypes.hh>
#include <nbapi/nb.hh>

#include <algorithm>
#include <malloc.h>

#define SOC_ALIGN       0x100000
#define SOC_BUFFERSIZE  0x20000

#if defined(HS_DEBUG_SERVER)
	#define HS_NB_LOC   HS_DEBUG_SERVER ":5000/nbapi"
	#define HS_CDN_BASE HS_DEBUG_SERVER ":5001"
	#define HS_SITE_LOC HS_DEBUG_SERVER ":5002"
#endif
#if !defined(HS_CDN_BASE) || !defined(HS_SITE_LOC) || !defined(HS_UPDATE_BASE) || !defined(HS_NB_LOC)
	#error "You must define HS_CDN_BASE, HS_SITE_LOC, HS_NB_LOC and HS_UPDATE_BASE"
#endif

#define OK 0

/* {{{1 de/initialization */

static u32 *g_socbuf = nullptr;

bool hsapi::global_init()
{
	Result res;
	if(!(g_socbuf = (u32 *) memalign(SOC_ALIGN, SOC_BUFFERSIZE)))
	{
		elog("failed to allocate buffer of %X (aligned %X) for SOC", SOC_BUFFERSIZE, SOC_ALIGN);
		return false;
	}
	if(R_FAILED((res = socInit(g_socbuf, SOC_BUFFERSIZE))))
	{
		free(g_socbuf);
		g_socbuf = nullptr;
		elog("failed to initialize SOC: %08lX", res);
		return false;
	}
	ilog("%susing proxy for API", get_nsettings()->proxy_port ? "" : "not ");
	return true;
}

void hsapi::global_deinit()
{
	if(g_socbuf != nullptr)
	{
		socExit();
		free(g_socbuf);
		g_socbuf = nullptr;
	}
}

/* 1}}} */

/* {{{1 Network primitives */

static Result basereq(const std::string& url, std::string& data, HTTPC_RequestMethod reqmeth = HTTPC_METHOD_GET, const char *postdata = nullptr, u32 postdata_len = 0)
{
	http::ResumableDownload downloader;
	downloader.set_postdata(postdata, postdata_len);
	downloader.set_target(url, reqmeth);
	downloader.requires_authentication();

	downloader.on_total_size_try_get([&]() -> Result {
		if(downloader.maybe_total_size())
			data.reserve(downloader.maybe_total_size());
		return 0;
	});

	downloader.on_chunk([&](size_t chunk_size) -> Result {
		data.append(downloader.data_buffer<char>(), chunk_size);
		return 0;
	});

	Result res = downloader.execute_once();
	ilog("Got API data header: %.4s", data.c_str());
	return res;
}

static Result handle_nb_result(nb::Result& nres)
{
	elog("Failed with Result of: %s (%u)", nres.message, nres.code);
	return APPERR_API_FAIL;
}

template <typename T, nb::StatusCode (*parse_func)(T&, u8 *, size_t, u32 *) = nb::parse_full<T>>
static Result nbreq(const std::string& url, T& obj, HTTPC_RequestMethod reqm = HTTPC_METHOD_GET, const char *postdata = nullptr, u32 size = 0)
{
	std::string data;
	Result res = basereq(url, data, reqm, postdata, size);
	if(R_FAILED(res)) return res;

	nb::StatusCode sc = parse_func(obj, (u8 *) data.c_str(), data.size(), nullptr);

	switch(sc)
	{
	case nb::StatusCode::SUCCESS:
		dlog("nb parse: success");
		break;
	case nb::StatusCode::NO_INPUT_DATA:
		dlog("nb parse: no input data");
		break;
	case nb::StatusCode::MAGIC_MISMATCH:
		dlog("nb parse: magic mismatch");
		break;
	case nb::StatusCode::INPUT_DATA_TOO_SHORT:
		dlog("nb parse: input data too short");
		break;
	}

	if(sc == nb::StatusCode::MAGIC_MISMATCH)
	{
		/* We may have an nb::Result */
		nb::Result nres;
		sc = nb::parse_full<nb::Result>(nres, (u8 *) data.c_str(), data.size());
		if(sc == nb::StatusCode::SUCCESS)
			res = handle_nb_result(nres);
	}
	if(sc != nb::StatusCode::SUCCESS)
		res = APPERR_INVALID_NB;

	return res;
}

template <typename T>
static Result nbreqa(const std::string& url, std::vector<T>& obj, HTTPC_RequestMethod reqm = HTTPC_METHOD_GET)
{
	return nbreq<std::vector<T>, nb::parse_array<T>>(url, obj, reqm);
}

// https://en.wikipedia.org/wiki/Percent-encoding
static std::string percent_encode(const std::string& str)
{
	std::string ret;
	ret.reserve(str.size() * 3);
	char hex[4];

	for(size_t i = 0; i < str.size(); ++i)
	{
		if((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= '0' && str[i] <= '9')
			|| str[i] == '.' || str[i] == '-' || str[i] == '_' || str[i] == '~')
			ret += str[i];
		else
		{
			snprintf(hex, 4, "%%%02X", str[i]);
			ret += hex;
		}
	}

	return ret;
}

static std::string make_query_string_url(const std::string& base, const std::unordered_map<std::string, std::string>& params)
{
	std::string ret = base;
	bool first_set = false;

	for(std::unordered_map<std::string, std::string>::const_iterator i = params.begin(); i != params.end(); ++i)
	{
		if(first_set)
			ret += "&" + i->first + "=" + percent_encode(i->second);
		else
		{
			ret += "?" + i->first + "=" + percent_encode(i->second);
			first_set = true;
		}
	}

	return ret;
}

/* 1}}} */

/* {{{1 Index ops */

namespace hsapi { using Index = nb::Index; }

static hsapi::Index g_index;
#if !RELEASE
static bool g_indexloaded = false;
#endif

static hsapi::Index& hindex()
{
#if !RELEASE
	if(!g_indexloaded)
		panic_assert(R_SUCCEEDED(hsapi::fetch_index()), "failed to fetch unloaded index (debug)");
#endif
	return g_index;
}

Result hsapi::fetch_index()
{
	Result res = nbreq<hsapi::Index>(HS_NB_LOC "/title-index", g_index);
#if !RELEASE
	if(R_SUCCEEDED(res))
		g_indexloaded = true;
#endif
	return res;
}

hsapi::IndexSubcategory& hsapi::subcategory(hcid cid, hcid sid)
{
	hsapi::IndexCategory& cat = category(cid);
	auto it = cat.subcategories.find(sid);
	if(it == cat.subcategories.end())
		panic("invalid subcategory id");
	return it->second;
}

hsapi::IndexCategory& hsapi::category(hcid cid)
{
	hsapi::Index& dx = hindex();
	auto it = dx.categories.find(cid);
	if(it == dx.categories.end())
		panic("invalid category id");
	return it->second;
}

void hsapi::sorted_categories(std::vector<hsapi::Category>& categories)
{
	hsapi::Index& dx = hindex();
	categories.reserve(dx.categories.size());
	for(auto it = dx.categories.begin(); it != dx.categories.end(); ++it)
		categories.push_back(it->second);
	std::sort(categories.begin(), categories.end(), [](const hsapi::Category& lhs, const hsapi::Category& rhs) -> bool {
		return lhs.prio < rhs.prio;
	});
}

hsapi::CategoryMap& hsapi::categories() { return hindex().categories; }
hsapi::IndexMeta& hsapi::imeta() { return hindex().meta; }

/* 1}}} */

/* {{{1 Title API */

Result hsapi::titles_in(std::vector<hsapi::PartialTitle>& ret, const hsapi::IndexCategory& cat, const hsapi::IndexSubcategory& scat)
{
	ilog("Listing titles in subcategory");
	return nbreqa<hsapi::PartialTitle>(HS_NB_LOC "/title/category/" + cat.name + "/" + scat.name, ret);
}

Result hsapi::title_meta(hsapi::Title& ret, hsapi::hid id)
{
	ilog("Transforming PartialTitle into Title");
	return nbreq<hsapi::Title>(HS_NB_LOC "/title/" + std::to_string(id), ret);
}

Result hsapi::search(std::vector<hsapi::PartialTitle>& ret, const std::unordered_map<std::string, std::string>& params)
{
	ilog("Performing search action");
	return nbreqa<hsapi::PartialTitle>(make_query_string_url(HS_NB_LOC "/title/search", params), ret);
}

Result hsapi::random(hsapi::Title& ret)
{
	ilog("Getting random title");
	return nbreq<hsapi::Title>(HS_NB_LOC "/title/random", ret);
}

Result hsapi::batch_related(std::vector<hsapi::Title>& ret, const std::vector<hsapi::htid>& tids)
{
	ilog("Doing batch related query");
	if(tids.size() == 0)
		return OK;

	std::string url = HS_NB_LOC "/title/related/batch?title_ids=" + ctr::tid_to_str(tids[0]);
	url.reserve((tids.size() - 1) * (16 + 1));
	for(size_t i = 1; i < tids.size(); ++i)
		url += "," + ctr::tid_to_str(tids[i]);

	return nbreqa<hsapi::Title>(url, ret);
}

Result hsapi::get_by_title_id(std::vector<hsapi::Title>& ret, const std::string& title_id)
{
	ilog("Getting hshop title by title id");
	return nbreqa<hsapi::Title>(HS_NB_LOC "/title/id/" + title_id, ret);
}

/* 1}}} */

/* {{{1 Misc. API */

Result hsapi::upload_log(const char *contents, u32 size, std::string& logid)
{
	ilog("Uploading log");
	nb::ThsLogResult logres;
	Result res = nbreq<nb::ThsLogResult>(HS_SITE_LOC "/nblog", logres, HTTPC_METHOD_POST, contents, size);
	if(R_FAILED(res)) return res;
	char hex[9];
	snprintf(hex, 9, "%08X", logres.id);
	logid = hex;
	return OK;
}

Result hsapi::get_latest_version_string(std::string& ret)
{
	ilog("Getting latest version");
	Result res = basereq(HS_UPDATE_BASE "/version", ret);
	if(R_FAILED(res)) return res;
	trim(ret, " \t\n");
	return OK;
}

Result hsapi::get_theme_preview_png(std::string& ret, hsapi::hid id)
{
	ilog("Getting theme preview");
	Result res = basereq(HS_NB_LOC "/title/" + std::to_string(id) + "/theme-preview", ret);
	if(R_FAILED(res)) return res;
	nb::Result nres;
	if(nb::parse_full<nb::Result>(nres, (u8 *) ret.c_str(), ret.size()) == nb::StatusCode::SUCCESS)
		return handle_nb_result(nres);
	return OK;
}

/* 1}}} */

/* {{{1 DLApi */

Result hsapi::get_download_link(std::string& ret, const Title& meta)
{
	nb::DLToken tok;
	Result res = nbreq<nb::DLToken>(HS_CDN_BASE "/nbcontent/" + std::to_string(meta.id) + "/request", tok);
	if(R_FAILED(res)) return res;

	ret = HS_CDN_BASE "/nbcontent/" + std::to_string(meta.id) + "?token=" + tok.token;
	return OK;
}

/* 1}}} */

/* {{{1 Misc. */

std::string hsapi::update_location(const std::string& ver)
{
#ifdef DEVICE_ID
#define STRING_(id) #id
#define STRINGIFY(id) STRING_(id)
	return HS_UPDATE_BASE "/3hs-" + ver + "-" STRINGIFY(DEVICE_ID) ".cia";
#undef STRING_
#undef STRINGIFY
#else
	return HS_UPDATE_BASE "/3hs-" + ver + ".cia";
#endif
}

std::string hsapi::parse_vstring(hsapi::hiver version)
{
	// based on:
	//  "{(ver >> 10) & 0x3F}.{(ver >> 4) & 0x3F}.{ver & 0xF}"
	return "v"
		+ std::to_string(version >> 10 & 0x3F) + "."
		+ std::to_string(version >> 4  & 0x3F) + "."
		+ std::to_string(version       & 0xF );
}

std::string hsapi::format_category_and_subcategory(hsapi::hcid cid, hsapi::hcid sid)
{
	hsapi::Category& cat = hsapi::category(cid);
	hsapi::Subcategory& scat = hsapi::subcategory(cid, sid);
	return cat.disp + " -> " + scat.disp;
}

template <typename T>
std::string real_title_name(const T& title)
{
	const std::string& base = ISET_SHOW_ALT ? (title.alt.size() ? title.alt : title.name) : title.name;

	const char *vc_type;
	switch((title.flags >> hsapi::VCType::shift) & hsapi::VCType::mask)
	{
	case hsapi::VCType::gb: vc_type = "[GB] "; break;
	case hsapi::VCType::gbc: vc_type = "[GBC] "; break;
	case hsapi::VCType::gba: vc_type = "[GBA] "; break;
	case hsapi::VCType::nes: vc_type = hsapi::subcategory(title.cat, title.subcat).name == REGION_JAPAN ? "[Famicom] " : "[NES] "; break;
	case hsapi::VCType::snes: vc_type = hsapi::subcategory(title.cat, title.subcat).name == REGION_JAPAN ? "[Super Famicom] " : "[SNES] "; break;
	case hsapi::VCType::gamegear: vc_type = "[GameGear] "; break;
	case hsapi::VCType::pcengine: vc_type = hsapi::subcategory(title.cat, title.subcat).name == REGION_USA ? "[TurboGrafx-16] " : "[PC Engine] "; break;
	case hsapi::VCType::none:
	default:
		vc_type = nullptr;
		break;
	}

	return vc_type ? vc_type + base : base;
}

std::string hsapi::title_name(const hsapi::PartialTitle& title) { return real_title_name(title); }
std::string hsapi::title_name(const hsapi::Title& title)        { return real_title_name(title); }

/* 1}}} */

