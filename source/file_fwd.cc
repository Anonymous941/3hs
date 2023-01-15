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

#define PRIX64 "%lld" /* for some reason this isn't provided by inttypes.h ? */
#include <nncpp/stream.hh>
#include <nncpp/romfs.hh>
#include <nncpp/ncch.hh>
#include <nncpp/cia.hh>

#include <sys/stat.h>
#include <string.h>
#include <3ds.h>

#include "error.hh"
#include "log.hh"

/**
 * This code basically just takes a theme installer CIA file (internally known as a "file forwarder") and
 * installs the file in RomFS to the correct directory on the SD card
 */

/** format:
 *   - cia ncch0 romfs contains 2 files
 *     - config.ini is a configuration file containing 2 lines
 *       - destination=... # File destination on SD
 *       - location=...    # The location in RomFS of the file being copied
 *     - the other file has a variable name found in config.ini
 */

#define TRY(expr) if((res = ( expr )) != nnc::result::ok) goto fail

template <typename T> /* TODO: Rethink the c_read_stream abstraction */
static bool fully_buffer_stream(nnc::c_read_stream<T>& stream, nnc::dynamic_array<char>& contents)
{
	size_t len = stream.size();
	u32 readSize;
	if(!contents.allocate(len + 1))
		return false;
	contents.data()[len] = '\0';
	return stream.read(contents.data(), len, readSize) == nnc::result::ok && readSize == len;
}

static const char deststr[] = "destination=";
static const size_t deststrlen = sizeof(deststr) - 1;
static const char locstr[] = "location=";
static const size_t locstrlen = sizeof(locstr) - 1;

Result install_forwarder(u8 *data, size_t len)
{
	/* we don't have to initialize seeddb; 1) the 3ds platform has no standard seeddb location and 2) theme forwarders will never be seeded */
	nnc::memory mem { data, len };

	nnc::dynamic_array<char> file;
	nnc::cia_content ncch0s;
	nnc::cia_reader reader;
	nnc::subview romfsfile;
	nnc::romfs romfs;
	nnc::result res;
	nnc::ncch ncch0;
	nnc::cia cia;

	/* TODO: Test dtors if obj is empty */
	/* TODO: Move read funcs to read_like_stream instead of c_stream */
	/* TODO: Wrap the writer api */
	/* TODO: Helper for nnc::cia_reader to nnc::ncch */

	nnc_wfile wf = { nullptr, nullptr }; /* TODO: Move this to the C++ wrappers once possible */

	char *dest, *src, *end, *slash;
	std::string rdest;

	TRY(cia.read(mem));
	TRY(reader.initialize(cia));
	TRY(reader.open(0, ncch0s));
	TRY(ncch0.read(ncch0s));
	TRY(ncch0.romfs_section(romfs));

	TRY(romfs.open("/config.ini", romfsfile));
	if(!fully_buffer_stream(romfsfile, file))
		goto fail;

	dest = strstr(file.data(), deststr);
	if(!dest) goto fail;
	dest += deststrlen;
	end = strchr(dest, '\n');

	src = strstr(file.data(), locstr);
	if(!src) goto fail;
	src += locstrlen;
	/* we can only now terminate the dest end because else the previous strstr may fail */
	if(end) *end = '\0';
	end = strchr(src, '\n');
	/* this one terminates the src end */
	if(end) *end = '\0';

	/* now we just have to copy the files around */
	slash = *dest == '/' ? dest + 1 : dest;
	rdest = strstr(dest, "sdmc:/") == dest ? dest : "sdmc:/" + std::string(slash);
	/*  mkdirp(/Themes/file.txt)
	 *   1. /Themes
	 * */
	while((slash = strchr(slash, '/')))
	{
		*slash = '\0';
		mkdir(dest, 0777);
		*slash = '/';
		++slash;
	}

	TRY(romfs.open(src, romfsfile));
	if(nnc_wfile_open(&wf, rdest.c_str()) != NNC_R_OK)
		goto fail;
	res = (nnc::result) nnc_copy(romfsfile.as_rstream(), (nnc_wstream *) &wf);

out:
	if(wf.funcs) wf.funcs->close((nnc_wstream *) &wf);
	return res == nnc::result::ok ? 0 : APPERR_FILEFWD_FAIL;
fail:
	res = nnc::result::nomem; /* doesn't matter, not used anymore after this */
	goto out;
}

#if 0

/* prototyped in install.cc */
Result install_forwarder(u8 *data, size_t len)
{
	nnc_keyset kset;
	nnc_keyset_default(&kset, false);

	nnc_memory cia;
	nnc_mem_open(&cia, data, len);

	nnc_cia_content_reader reader;
	nnc_cia_header header;
	u8 *contents = NULL;
	char *contents_s, *dest, *src, *end, *slash;
	nnc_romfs_ctx romfs;
	nnc_ncch_section_stream romfsSection;
	nnc_cia_content_stream ncch0;
	nnc_keypair kpair;
	nnc_romfs_info info;
	nnc_subview sv;
	nnc_ncch_header ncchHeader;
	FILE *out = NULL;
	std::string rdest;

	const char *deststr = "destination=";
	const size_t deststrlen = strlen(deststr);
	const char *locstr = "location=";
	const size_t locstrlen = strlen(locstr);

	if(nnc_read_cia_header(NNC_RSP(&cia), &header) != NNC_R_OK) return APPERR_FILEFWD_FAIL;
	if(nnc_cia_make_reader(&header, NNC_RSP(&cia), &kset, &reader) != NNC_R_OK) return APPERR_FILEFWD_FAIL;
	if(nnc_cia_open_content(&reader, 0, &ncch0, NULL) != NNC_R_OK) goto fail;
	if(nnc_read_ncch_header(NNC_RSP(&ncch0), &ncchHeader) != NNC_R_OK) goto fail;
	/* we don't need a seeddb here since theme installers will never use a seed */
	if(nnc_fill_keypair(&kpair, &kset, NULL, &ncchHeader) != NNC_R_OK) goto fail;
	if(nnc_ncch_section_romfs(&ncchHeader, NNC_RSP(&ncch0), &kpair, &romfsSection)) goto fail;
	if(nnc_init_romfs(NNC_RSP(&romfsSection), &romfs) != NNC_R_OK) goto fail;

	/* finally the actual parsing starts */
	if(nnc_get_info(&romfs, &info, "/config.ini") != NNC_R_OK) goto fail2;
	if(nnc_romfs_open_subview(&romfs, &sv, &info) != NNC_R_OK) goto fail2;
	if(!(contents = read_rs(NNC_RSP(&sv), &len))) goto fail2;
	contents_s = (char *) contents;
	contents[len] = '\0';

	dest = strstr(contents_s, deststr);
	if(!dest) goto fail2;
	dest += deststrlen;
	end = strchr(dest, '\n');

	src = strstr(contents_s, locstr);
	if(!src) goto fail2;
	src += locstrlen;
	/* we can only now terminate the dest end because else the previous strstr may fail */
	if(end) *end = '\0';
	end = strchr(src, '\n');
	/* this one terminates the src end */
	if(end) *end = '\0';

	/* now we just have to copy the files around */
	slash = *dest == '/' ? dest + 1 : dest;
	rdest = strstr(dest, "sdmc:/") == dest ? dest : "sdmc:/" + std::string(slash);
	/*  /Themes/file.txt
	 *   1. /Themes
	 * */
	while((slash = strchr(slash, '/')))
	{
		*slash = '\0';
		mkdir(dest, 0777);
		*slash = '/';
		++slash;
	}

	if(nnc_get_info(&romfs, &info, src) != NNC_R_OK) goto fail2;
	if(nnc_romfs_open_subview(&romfs, &sv, &info)) goto fail2;
	free(contents);
	if(!(contents = read_rs(NNC_RSP(&sv), &len))) goto fail2;
	out = fopen(rdest.c_str(), "w");
	if(!out) goto fail2;
	if(fwrite(contents, len, 1, out) != 1) goto fail2;

	fclose(out);
	nnc_free_romfs(&romfs);
	free(contents);
	nnc_cia_free_reader(&reader);
	return 0;
fail2:
	if(out) fclose(out);
	nnc_free_romfs(&romfs);
fail:
	free(contents);
	nnc_cia_free_reader(&reader);
	return APPERR_FILEFWD_FAIL;
}
#endif

