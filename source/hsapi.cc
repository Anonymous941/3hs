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

#include "update.hh" /* includes net constants */
#include "hsapi.hh"
#include "error.hh"
#include "proxy.hh"
#include "ctr.hh"
#include "log.hh"

#include <3rd/json.hh>
#include <algorithm>
#include <malloc.h>

#define SOC_ALIGN       0x100000
#define SOC_BUFFERSIZE  0x20000

#if defined(HS_DEBUG_SERVER)
	#define HS_BASE_LOC HS_DEBUG_SERVER ":5000/api"
	#define HS_CDN_BASE HS_DEBUG_SERVER ":5001"
	#define HS_SITE_LOC HS_DEBUG_SERVER ":5002"
#endif
#if !defined(HS_BASE_LOC) || !defined(HS_CDN_BASE) || !defined(HS_SITE_LOC) || !defined(HS_UPDATE_BASE)
	#error "You must define HS_BASE_LOC, HS_CDN_BASE, HS_SITE_LOC and HS_UPDATE_BASE"
#endif

#define REGION_JAPAN "japan"
#define REGION_USA "north-america"

#define TRYGET_T(dst,jsn,prp,T,chkr) do { if(!jsn[prp].chkr()) { elog("Prop: " prp); return APPERR_JSON_FAIL; } dst = jsn[prp].get<T>(); } while(0)
#define TRYGET_S(jsn,dst,prp) TRYGET_T(dst,jsn,prp,std::string,is_string)
#define TRYGET_SZ(jsn,dst,prp) TRYGET_T(dst,jsn,prp,hsapi::hsize,is_number_unsigned)
#define TRYGET_PRIO(jsn,dst,prp) TRYGET_T(dst,jsn,prp,hsapi::hprio,is_number)
#define TRYGET_FLAGS(jsn,dst,prp) TRYGET_T(dst,jsn,prp,hsapi::hflags,is_number_unsigned)
#define TRYGET_ID(jsn,dst,prp) TRYGET_T(dst,jsn,prp,hsapi::hid,is_number_unsigned)
#define TRYGET_VER(jsn,dst,prp) TRYGET_T(dst,jsn,prp,hsapi::hiver,is_number_unsigned)
#define TRYGETOPT_T(jsn,dst,prp,T,chkr) do { if(jsn.contains(prp)) { if(!jsn[prp].chkr()) { elog("Prop: " prp); return APPERR_JSON_FAIL; } dst = jsn[prp].get<T>(); } } while(0)
#define TRYGETOPT_S(dst,jsn,prp) TRYGETOPT_T(dst,jsn,prp,std::string,is_string)
#define TRYCHECK_T(jsn,prp,chkr) do { if(!jsn[prp].chkr()) { elog("Prop: " prp); return APPERR_JSON_FAIL; } } while(0)
#define TRYCHECK_S(jsn,prp) TRYCHECK_T(jsn,prp,is_string)
#define TRYCHECK_A(jsn,prp) TRYCHECK_T(jsn,prp,is_array)
#define TRYCHECK_OBJ(jsn,prp) TRYCHECK_T(jsn,prp,is_object)
#define TRYCHECK_RES(jsn,prp) TRYCHECK_T(jsn,prp,is_number)
#define CHECK_BOOL(jsn,prp) (jsn[prp].is_boolean())
#define CHECKAPI(type) if((res = api_res_to_rc(j)) != OK) return res; TRYCHECK_##type(j, "value")
#define OK 0

extern "C" unsigned int hscert_der_len; /* hscert.c */
extern "C" unsigned char hscert_der[];  /* hscert.c */

extern "C" void        hsapi_password(char *); /* hsapi_auth.c */
extern "C" const int   hsapi_password_length;  /* hsapi_auth.c */
extern "C" const char *hsapi_user;             /* hsapi_auth.c */

using json = nlohmann::json;

static u32 *g_socbuf = nullptr;
static hsapi::Index g_index;
#ifndef RELEASE
static bool g_indexLoaded = false;
#endif
hsapi::Index *hsapi::get_index()
{
#ifndef RELEASE
	/* since sometimes you just gotta shortcut your way to code that you want */
	if(!g_indexLoaded) hsapi::fetch_index();
#endif
	return &g_index;
}


void hsapi::global_deinit()
{
	socExit();
	if(g_socbuf != NULL)
		free(g_socbuf);
}

bool hsapi::global_init()
{
	if(!(g_socbuf = (u32 *) memalign(SOC_ALIGN, SOC_BUFFERSIZE)))
	{
		elog("failed to allocate buffer of %X (aligned %X) for SOC", SOC_BUFFERSIZE, SOC_ALIGN);
		return false;
	}
	if(R_FAILED(socInit(g_socbuf, SOC_BUFFERSIZE)))
	{
		elog("failed to initialize SOC");
		return false;
	}
	return true;
}

static Result basereq(const std::string& url, std::string& data, HTTPC_RequestMethod reqmeth = HTTPC_METHOD_GET, const char *postdata = nullptr, u32 postdata_len = 0)
{
	u32 dled = 0, status = 0, totalSize = 0;
	std::string redir;
	char buffer[4096];
	httpcContext ctx;
	Result res = OK;
	char *password;
	ui::Keys k;

#define TRY(expr) if(R_FAILED(res = ( expr ) )) goto out
	if(R_FAILED(res = httpcOpenContext(&ctx, reqmeth, url.c_str(), 0)))
		return res;
	TRY(httpcSetSSLOpt(&ctx, SSLCOPT_DisableVerify));
	TRY(httpcSetKeepAlive(&ctx, HTTPC_KEEPALIVE_ENABLED));
	TRY(httpcAddRequestHeaderField(&ctx, "Connection", "Keep-Alive"));
	TRY(httpcAddRequestHeaderField(&ctx, "User-Agent", USER_AGENT));
	TRY(httpcAddRequestHeaderField(&ctx, "X-Auth-User", hsapi_user));
/*TRY(httpcAddRequestHeaderField(&ctx, "X-Auth-Password", password));*/password=(char*)malloc(hsapi_password_length+1);hsapi_password(password);password[hsapi_password_length]=0;TRY(httpcAddRequestHeaderField(&ctx,"X-Auth-Password",password));memset(password,0,hsapi_password_length);free(password);
	if(hscert_der_len && url.find("https") == 0) // only use certs on https
		TRY(httpcAddTrustedRootCA(&ctx, hscert_der, hscert_der_len));
	if(postdata && postdata_len != 0)
		/* for some reason postdata is a u32 instead of u8.... */
		TRY(httpcAddPostDataRaw(&ctx, (const u32 *) postdata, postdata_len));
	TRY(proxy::apply(&ctx));

	TRY(httpcBeginRequest(&ctx));

	TRY(httpcGetResponseStatusCode(&ctx, &status));
	vlog("API status code on %s: %lu", url.c_str(), status);

	// Do we want to redirect?
	if(status / 100 == 3)
	{
		TRY(httpcGetResponseHeader(&ctx, "location", buffer, sizeof(buffer)));
		redir = buffer;

		vlog("Redirected to %s", redir.c_str());
		httpcCancelConnection(&ctx);
		httpcCloseContext(&ctx);
		return basereq(redir, data, reqmeth);
	}

	if(status != 200)
	{
		elog("HTTP status was NOT 200 but instead %lu", status);
#ifdef RELEASE
		// We _may_ require a different 3hs version
		if(status == 400)
		{
			/* we can assume it doesn't have the header if this fails */
			if(R_SUCCEEDED(httpcGetResponseHeader(&ctx, "x-minimum", buffer, sizeof(buffer))))
			{
				httpcCancelConnection(&ctx);
				httpcCloseContext(&ctx);
				ui::RenderQueue::terminate_render();
				ui::notice(PSTRING(min_constraint, VVERSION, buffer));
				exit(1);
			}
		}
#endif
		res = status == 413 ? APPERR_TOO_LARGE : APPERR_NON200;
		goto out;
	}

	TRY(httpcGetDownloadSizeState(&ctx, nullptr, &totalSize));
	if(totalSize != 0) data.reserve(totalSize);

	do {
		res = httpcDownloadData(&ctx, (unsigned char *) buffer, sizeof(buffer), &dled);
		k = ui::RenderQueue::get_keys();
		if(R_SUCCEEDED(res) && ((k.kDown | k.kHeld) & (KEY_B | KEY_START)))
			res = APPERR_CANCELLED;
		// Other type of fail
		if(R_FAILED(res) && res != (Result) HTTPC_RESULTCODE_DOWNLOADPENDING)
			goto out;
		data += std::string(buffer, dled);
	} while(res == (Result) HTTPC_RESULTCODE_DOWNLOADPENDING);

	vlog("API data gotten:\n%s", data.c_str());

out:
	httpcCancelConnection(&ctx);
	httpcCloseContext(&ctx);
	return res;
#undef TRY
}

static Result api_res_to_rc(json& j)
{
	TRYCHECK_OBJ(j, "status");
	TRYCHECK_RES(j["status"], "code");
	Result code = j["status"]["code"].get<Result>();
	switch(code)
	{
	case 0:
		return OK;
	/* perhaps expand this switch for common API errors
	 * although they shouldn't happen */
	default:
		TRYCHECK_S(j, "error_message");
		elog("API Error: %s (%08lX)", j["error_message"].get<std::string>().c_str(), code);
		return APPERR_API_FAIL;
	}
}

template <typename J>
static Result basereq(const std::string& url, J& j, HTTPC_RequestMethod reqmeth = HTTPC_METHOD_GET, const char *postdata = nullptr, u32 postdata_len = 0)
{
	std::string data;
	Result res = basereq(url, data, reqmeth, postdata, postdata_len);
	if(R_FAILED(res)) return res;

	j = J::parse(data, nullptr, false);
	if(j == J::value_t::discarded)
		return APPERR_JSON_FAIL;
	return OK;
}

static Result serialize_subcategories(std::vector<hsapi::Subcategory>& rscats, const std::string& cat, json& scats)
{
	for(json::iterator scat = scats.begin(); scat != scats.end(); ++scat)
	{
		json& jscat = scat.value();

		rscats.emplace_back();
		hsapi::Subcategory& s = rscats.back();

		TRYGET_S(jscat, s.disp, "display_name");
		TRYGET_S(jscat, s.desc, "description");
		s.name = scat.key();
		s.cat = cat;

		TRYGET_SZ(jscat, s.titles, "total_content_count");
		TRYGET_SZ(jscat, s.size, "size");
	}
	return OK;
}

static Result serialize_categories(std::vector<hsapi::Category>& rcats, json& cats)
{
	for(json::iterator cat = cats.begin(); cat != cats.end(); ++cat)
	{
		json& jcat = cat.value();

		rcats.emplace_back();
		hsapi::Category& c = rcats.back();

		TRYGET_SZ(jcat, c.titles, "total_content_count");
		TRYGET_S(jcat, c.disp, "display_name");
		TRYGET_S(jcat, c.desc, "description");
		TRYGET_PRIO(jcat, c.prio, "priority");
		TRYGET_SZ(jcat, c.size, "size");
		c.name = cat.key();

		TRYCHECK_OBJ(jcat, "subcategories");
		if(serialize_subcategories(c.subcategories, c.name, jcat["subcategories"]) != OK)
			return APPERR_JSON_FAIL;
	}
	return OK;
}

static Result serialize_title(hsapi::Title& t, json& jt)
{
	TRYGET_SZ(jt, t.size, "size");
	TRYGET_SZ(jt, t.dlCount, "download_count");
	TRYGET_ID(jt, t.id, "id");
	TRYCHECK_S(jt, "title_id");
	t.tid = ctr::str_to_tid(jt["title_id"].get<std::string>());
	TRYGET_S(jt, t.cat, "category");
	TRYGET_S(jt, t.subcat, "subcategory");
	TRYGET_S(jt, t.name, "name");
	TRYGET_FLAGS(jt, t.flags, "flags");
	TRYGETOPT_S(jt, t.alt, "alternative_name");

	const char *vc_type;
	switch((t.flags >> hsapi::VCType::shift) & hsapi::VCType::mask)
	{
	case hsapi::VCType::gb: vc_type = "[GB] "; break;
	case hsapi::VCType::gbc: vc_type = "[GBC] "; break;
	case hsapi::VCType::gba: vc_type = "[GBA] "; break;
	case hsapi::VCType::nes: vc_type = t.subcat == REGION_JAPAN ? "[Famicom] " : "[NES] "; break;
	case hsapi::VCType::snes: vc_type = t.subcat == REGION_JAPAN ? "[Super Famicom] " : "[SNES] "; break;
	case hsapi::VCType::gamegear: vc_type = "[GameGear] "; break;
	case hsapi::VCType::pcengine: vc_type = t.subcat == REGION_USA ? "[TurboGrafx-16] " : "[PC Engine] "; break;
	case hsapi::VCType::none:
	default:
		vc_type = NULL;
	}
	if(vc_type)
	{
		if(t.alt.size()) t.alt = vc_type + t.alt;
		t.name = vc_type + t.name;
	}
	return OK;
}

static Result serialize_titles(std::vector<hsapi::Title>& rtitles, json& j)
{
	for(json::iterator it = j.begin(); it != j.end(); ++it)
		if(CHECK_BOOL(it.value(), "is_listed") && it.value()["is_listed"].get<bool>())
		{
			rtitles.emplace_back();
			if(serialize_title(rtitles.back(), it.value()) != OK)
				return APPERR_JSON_FAIL;
		}
	return OK;
}

static Result serialize_full_title(hsapi::FullTitle& ret, json& j)
{
	if(!j["is_listed"]) return APPERR_TITLE_UNLISTED;
	if(serialize_title(ret, j) != OK)
		return APPERR_JSON_FAIL;
	// now we serialize for the FullTitle exclusive fields
	TRYGET_S(j, ret.prod, "product_code");
	TRYGET_VER(j, ret.version, "version");
	TRYGETOPT_S(j, ret.seed, "seed"); // not all titles use a seed
	return OK;
}

static Result serialize_full_titles(std::vector<hsapi::FullTitle>& rtitles, json& j)
{
	for(json::iterator it = j.begin(); it != j.end(); ++it)
		if(CHECK_BOOL(it.value(), "is_listed") && it.value()["is_listed"].get<bool>())
		{
			rtitles.emplace_back();
			if(serialize_full_title(rtitles.back(), it.value()) != OK)
				return APPERR_JSON_FAIL;
		}
	return OK;
}

// https://en.wikipedia.org/wiki/Percent-encoding
static std::string url_encode(const std::string& str)
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
			// C++ can be retarded at times
			// Why is there no std::hex(uchar) function?
			snprintf(hex, 4, "%%%02X", str[i]);
			ret += hex;
		}
	}

	return ret;
}

static std::string gen_url(const std::string& base, const std::unordered_map<std::string, std::string>& params)
{
	std::string ret = base;
	bool first_set = false;

	for(std::unordered_map<std::string, std::string>::const_iterator i = params.begin(); i != params.end(); ++i)
	{
		if(first_set)
			ret += "&" + i->first + "=" + url_encode(i->second);
		else
		{
			ret += "?" + i->first + "=" + url_encode(i->second);
			first_set = true;
		}
	}

	return ret;
}

hsapi::Subcategory *hsapi::Category::find(const std::string& name)
{
	for(size_t i = 0; i < this->subcategories.size(); ++i)
	{
		if(this->subcategories[i].name == name || this->subcategories[i].disp == name)
			return &this->subcategories[i];
	}
	return nullptr;
}

hsapi::Category *hsapi::Index::find(const std::string& name)
{
	for(size_t i = 0; i < this->categories.size(); ++i)
	{
		if(this->categories[i].name == name || this->categories[i].disp == name)
			return &this->categories[i];
	}
	return nullptr;
}

Result hsapi::fetch_index()
{
#ifndef RELEASE
	if(g_indexLoaded) return OK;
#endif

	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(HS_BASE_LOC "/title-index", j)))
		return res;
	CHECKAPI(OBJ);
	j = j["value"];

	TRYGET_SZ(j, g_index.titles, "total_content_count");
	TRYGET_SZ(j, g_index.size, "size");

	TRYCHECK_OBJ(j, "entries");
	if(serialize_categories(g_index.categories, j["entries"]) != OK)
		return APPERR_JSON_FAIL;
	std::sort(g_index.categories.begin(), g_index.categories.end());

#ifndef RELEASE
	g_indexLoaded = true;
#endif

	return OK;
}

Result hsapi::titles_in(std::vector<hsapi::Title>& ret, const std::string& cat, const std::string& scat)
{
	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(HS_BASE_LOC "/title/category/" + cat + "/" + scat, j)))
		return res;
	CHECKAPI(A);

	return serialize_titles(ret, j["value"]);
}

Result hsapi::title_meta(hsapi::FullTitle& ret, hsapi::hid id)
{
	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(HS_BASE_LOC "/title/" + std::to_string(id), j)))
		return res;
	CHECKAPI(OBJ);

	return serialize_full_title(ret, j["value"]);
}

Result hsapi::get_download_link(std::string& ret, const hsapi::Title& meta)
{
	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(HS_CDN_BASE "/content/" + std::to_string(meta.id) + "/request", j)))
		return res;
	CHECKAPI(OBJ);

	TRYCHECK_S(j["value"], "token");
	ret = HS_CDN_BASE "/content/" + std::to_string(meta.id) + "?token=" + j["value"]["token"].get<std::string>();
	return OK;
}

Result hsapi::search(std::vector<hsapi::Title>& ret, const std::unordered_map<std::string, std::string>& params)
{
	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(gen_url(HS_BASE_LOC "/title/search", params), j)))
		return res;
	CHECKAPI(A);

	return serialize_titles(ret, j["value"]);
}

Result hsapi::random(hsapi::FullTitle& ret)
{
	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(HS_BASE_LOC "/title/random", j)))
		return res;
	CHECKAPI(OBJ);

	return serialize_full_title(ret, j["value"]);
}

Result hsapi::upload_log(const char *contents, u32 size, std::string& logid)
{
	ilog("calling api");
	json j;
	Result res;
	if(R_FAILED(res = basereq<json>(HS_SITE_LOC "/log", j, HTTPC_METHOD_POST, contents, size)))
		return res;
	CHECKAPI(OBJ);
	TRYCHECK_S(j["value"], "id");
	logid = j["value"]["id"].get<std::string>();
	return OK;
}

Result hsapi::batch_related(hsapi::BatchRelated& ret, const std::vector<hsapi::htid>& tids)
{
	ilog("calling api");
	if(tids.size() == 0) return OK;

	std::string url = HS_BASE_LOC "/title/related/batch?title_ids=" + ctr::tid_to_str(tids[0]);
	for(size_t i = 1; i < tids.size(); ++i) url += "&title_ids=" + ctr::tid_to_str(tids[i]);

	json j;
	Result res = OK;
	if(R_FAILED(res = basereq<json>(url, j)))
		return res;
	CHECKAPI(OBJ);
	j = j["value"];

	for(json::iterator it = j.begin(); it != j.end(); ++it)
	{
		htid tid = ctr::str_to_tid(it.key());
		if(serialize_full_titles(ret[tid], it.value()) != OK)
			return APPERR_JSON_FAIL;
	}

	return OK;
}

Result hsapi::get_latest_version_string(std::string& ret)
{
	ilog("calling api");
	Result res = OK;
	if(R_FAILED(res = basereq(HS_UPDATE_BASE "/version", ret)))
		return res;
	trim(ret, " \t\n");
	return OK;
}

Result hsapi::get_by_title_id(std::vector<Title>& ret, const std::string& title_id)
{
	ilog("calling api");
	json j;
	Result res = OK;
	if(R_FAILED(res = basereq<json>(HS_BASE_LOC "/title/id/" + title_id, j)))
		return res;
	CHECKAPI(A);

	return serialize_titles(ret, j["value"]);
}

Result hsapi::get_theme_preview_png(std::string& ret, hsapi::hid id)
{
	ilog("calling api");
	Result res = OK;
	if(R_FAILED(res = basereq(HS_BASE_LOC "/title/" + std::to_string(id) + "/theme-preview", ret)))
		return res;
	return OK;
}

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

