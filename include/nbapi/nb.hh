/* This file is part of 3hs
 * Copyright (C) 2023 hShop developer team
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

#ifndef inc_nb_hh
#define inc_nb_hh

#include <3ds/types.h>
#include <string.h>
#include <vector>

namespace nb
{
	typedef u32 BlobPtr;

	static const char ArrayMagic[4] = { 'N', 'B', 'A', 'R' };

	struct ObjectHeader
	{
		char magic[4];
		u32  object_header_size;
		u32  header_size;
		u32  blob_size;
	};

	struct ArrayHeader
	{
		char magic[4];
		u32  array_header_size;
		u32  object_count;
		u32  object_headers_size;
		u32  shared_blob_size;
	};

	enum class StatusCode : int
	{
		SUCCESS              = 0,
		NO_INPUT_DATA        = 1,
		MAGIC_MISMATCH       = 2,
		INPUT_DATA_TOO_SHORT = 3,
	};

	/* this class is actually unused, thus it is empty */
	class INbDeserializable
	{
		/* virtual nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) = 0; */
	};

	template <typename T>
	static nb::StatusCode parse_full(T& out, u8 *data, size_t size, u32 *out_size = nullptr)
	{
		if (!data || !size)
			return nb::StatusCode::NO_INPUT_DATA;

		if (memcmp(T::magic, data, 4) != 0)
			return nb::StatusCode::MAGIC_MISMATCH;

		if (size < sizeof(nb::ObjectHeader))
			return nb::StatusCode::INPUT_DATA_TOO_SHORT;

		nb::ObjectHeader *hdr = (nb::ObjectHeader *)data;

		if (hdr->object_header_size + hdr->header_size + hdr->blob_size > size)
			return nb::StatusCode::INPUT_DATA_TOO_SHORT;

		u8 *obj_hdr = &data[hdr->object_header_size];
		u8 *blob = &data[hdr->object_header_size + hdr->header_size];

		if (out_size) *out_size = hdr->object_header_size + hdr->header_size + hdr->blob_size;

		return out.deserialize(obj_hdr, hdr->header_size, blob, hdr->blob_size);
	}

	template <typename T>
	static nb::StatusCode parse_array(std::vector<T>& out, u8 *data, size_t size, u32 *out_size = nullptr)
	{
		if (!data || !size)
			return nb::StatusCode::NO_INPUT_DATA;

		if (memcmp(nb::ArrayMagic, data, 4) != 0)
			return nb::StatusCode::MAGIC_MISMATCH;

		if (size < sizeof(nb::ArrayHeader))
			return nb::StatusCode::INPUT_DATA_TOO_SHORT;

		nb::ArrayHeader *hdr = (nb::ArrayHeader *)data;
		size_t total_objects_size = hdr->object_count * hdr->object_headers_size;

		if (hdr->array_header_size + total_objects_size + hdr->shared_blob_size > size)
			return nb::StatusCode::INPUT_DATA_TOO_SHORT;

		out.reserve(hdr->object_count);

		u8 *blob = &data[hdr->array_header_size + total_objects_size];

		nb::StatusCode c = nb::StatusCode::SUCCESS;

		for (u32 i = 0; i < hdr->object_count; i++)
		{
			u8 *obj_hdr = &data[hdr->array_header_size + i * hdr->object_headers_size];

			T obj;

			if ((c = obj.deserialize(obj_hdr, hdr->object_headers_size, blob, hdr->shared_blob_size)) != nb::StatusCode::SUCCESS)
				return c;

			out.push_back(obj);
		}

		if (out_size) *out_size = hdr->array_header_size + total_objects_size + hdr->shared_blob_size;

		return nb::StatusCode::SUCCESS;
	}
}

#endif
