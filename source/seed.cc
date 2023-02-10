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

#include "seed.hh"

#include <unordered_map>
#include <string.h>
#include <stdio.h>
#include <3ds.h>

#include "panic.hh"
#include "i18n.hh"
#include "log.hh"

// from FBI:
// https://github.com/Steveice10/FBI/blob/6e3a28e4b674e0d7a6f234b0419c530b358957db/source/core/http.c#L440-L453
static Result FSUSER_AddSeed(u64 tid, const void *seed)
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = 0x087A0180;
	cmdbuf[1] = (u32) (tid & 0xFFFFFFFF);
	cmdbuf[2] = (u32) (tid >> 32);
	memcpy(&cmdbuf[3], seed, 16);

	Result ret;
	if(R_FAILED(ret = svcSendSyncRequest(*fsGetSessionHandle())))
		return ret;

	ret = cmdbuf[1];
	return ret;
}

static bool seed_is_empty(const u8 *seed)
{
	for(int i = 0; i < 16; ++i)
		if(seed[i] != 0x00)
			return false;
	return true;
}

Result add_seed(const hsapi::Title& title)
{
	if(seed_is_empty(title.seed)) // title does not have a known seed, or doesn't use one
	{
		ilog("Not adding seed for %016llX", title.tid);
		return 0x0;
	}
	ilog("Adding seed for %016llX", title.tid);
	return FSUSER_AddSeed(title.tid, title.seed);
}

