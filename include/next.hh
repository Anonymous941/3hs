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

#ifndef inc_next_hh
#define inc_next_hh

#include "installgui.hh"
#include "extmeta.hh"
#include "hsapi.hh"

#include <string>

#define next_cat_exit ((const hsapi::Category    *) 0)
#define next_sub_back ((const hsapi::Subcategory *) 0)
#define next_sub_exit ((const hsapi::Subcategory *) 1)

#define next_gam_back ((hsapi::hid) -1)
#define next_gam_exit ((hsapi::hid) -2)


namespace next
{
	const hsapi::Category *sel_cat(size_t *cursor = nullptr);
	const hsapi::Subcategory *sel_sub(const hsapi::Category& cat, size_t *cursor = nullptr, bool visited = false);
	hsapi::hid sel_gam(std::vector<hsapi::PartialTitle>& titles, size_t *cursor = nullptr);
	void maybe_install_gam(std::vector<hsapi::PartialTitle>& titles);
}

#endif

