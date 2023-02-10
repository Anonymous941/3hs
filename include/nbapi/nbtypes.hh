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

#ifndef inc_nbtypes_hh
#define inc_nbtypes_hh

#include <string>
#include <map>

#include "nb.hh"

namespace nb /* nbType */
{
	// title

	template <typename TString>
	struct NbTitle
	{
		u8 seed[16];
		u64 size;
		u64 tid;
		u64 added; /* timestamp */
		u64 updated; /* timestamp */
		u64 dlCount;
		u64 flags;
		u32 id;
		TString name;
		TString alt;
		TString region;
		TString filename;
		TString desc;
		TString prod;
		u16 version;
		u8 contentType; /* piratelegit/legit/stnadard */
		u8 cat;
		u8 subcat;
		bool listed;
	};

	class Title : nb::INbDeserializable, public NbTitle<std::string>
	{
	public:
		static constexpr const char *magic = "TITL";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbTitle<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbTitle<nb::BlobPtr> *thdr = (NbTitle<nb::BlobPtr> *)header;

			this->tid = thdr->tid;
			this->added = thdr->added;
			this->updated = thdr->updated;
			this->dlCount = thdr->dlCount;
			this->flags = thdr->flags;
			this->id = thdr->id;
			this->version = thdr->version;
			this->contentType = thdr->contentType;
			this->listed = thdr->listed;

			memcpy(this->seed, thdr->seed, 16);

			char *strdata = (char *)blob;

			this->name = std::string(&strdata[thdr->name]);
			this->region = std::string(&strdata[thdr->region]);
			this->filename = std::string(&strdata[thdr->filename]);
			this->prod = std::string(&strdata[thdr->prod]);
			if (thdr->desc) this->desc = std::string(&strdata[thdr->desc]);
			if (thdr->alt) this->alt = std::string(&strdata[thdr->alt]);

			return nb::StatusCode::SUCCESS;
		}
	};
	static_assert(sizeof(Title) == sizeof(NbTitle<std::string>));

	// category

	template <typename TString>
	struct NbCategory
	{
		u32 id;
		TString disp;
		TString name;
		TString desc;
		TString subcatDesc;
		u8 prio;
	};

	class Category : nb::INbDeserializable, public NbCategory<std::string>
	{
	public:
		static constexpr const char *magic = "CATE";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbCategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbCategory<nb::BlobPtr> *chdr = (NbCategory<nb::BlobPtr> *)header;

			this->id = chdr->id;

			char *strdata = (char *)blob;

			this->name = std::string(&strdata[chdr->name]);
			this->disp = std::string(&strdata[chdr->disp]);
			this->desc = std::string(&strdata[chdr->desc]);
			if (chdr->subcatDesc) this->subcatDesc = std::string(&strdata[chdr->subcatDesc]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// subcategory

	template <typename TString>
	struct NbSubcategory
	{
		u32 id;
		TString disp;
		TString name;
		TString desc;
		bool standalone;
	};

	class Subcategory : nb::INbDeserializable, public NbSubcategory<std::string>
	{
	public:
		static constexpr const char *magic = "SCAT";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbSubcategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbSubcategory<nb::BlobPtr> *shdr = (NbSubcategory<nb::BlobPtr> *)header;

			this->id = shdr->id;
			this->standalone = shdr->standalone;

			char *strdata = (char *)blob;

			this->name = std::string(&strdata[shdr->name]);
			this->disp = std::string(&strdata[shdr->disp]);

			if (this->standalone && shdr->desc)
				this->desc = std::string(&strdata[shdr->desc]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// 3hs release

	template <typename TString>
	struct NbThsRelease
	{
		u64 added;
		TString version;
		TString versiondesc;
		TString changelog;
		TString dlUrl;
		TString sourceUrl;
	};

	class ThsRelease : nb::INbDeserializable, public NbThsRelease<std::string>
	{
	public:
		static constexpr const char *magic = "3HSR";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbThsRelease<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbThsRelease<nb::BlobPtr> *rhdr = (NbThsRelease<nb::BlobPtr> *)header;

			this->added = rhdr->added;

			char *strdata = (char *)blob;

			this->version = std::string(&strdata[rhdr->version]);
			this->versiondesc = std::string(&strdata[rhdr->versiondesc]);
			this->changelog = std::string(&strdata[rhdr->changelog]);
			this->dlUrl = std::string(&strdata[rhdr->dlUrl]);
			this->sourceUrl = std::string(&strdata[rhdr->sourceUrl]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// id pairs

	struct NbIdPair { u64 tid; u32 id; };

	class idPair : NbIdPair, public nb::INbDeserializable
	{
	public:
		static constexpr const char *magic = "PAIR";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbIdPair))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIdPair *phdr = (NbIdPair *)header;

			this->tid = phdr->tid;
			this->id = phdr->id;

			return nb::StatusCode::SUCCESS;
		}
	};

	// results

	template<typename TString>
	struct NbResult
	{
		u32 code;
		TString message;
	};

	class Result : nb::INbDeserializable, public NbResult<std::string>
	{
	public:
		static constexpr const char *magic = "RSLT";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbResult<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbResult<nb::BlobPtr> *rhdr = (NbResult<nb::BlobPtr> *)header;

			this->code = rhdr->code;
			if (rhdr->message) this->message = std::string((char *)&blob[rhdr->message]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// log results

	struct NbThsLogResult
	{
		u32 id;
		u64 duration;
	};

	class ThsLogResult : nb::INbDeserializable, public NbThsLogResult
	{
	public:
		static constexpr const char *magic = "HLOG";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbThsLogResult))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbThsLogResult *rhdr = (NbThsLogResult *)header;

			this->id = rhdr->id;
			this->duration = rhdr->duration;

			return nb::StatusCode::SUCCESS;
		};
	};

	// tokens

	template <typename TString>
	struct NbDLToken
	{
		u32 id;
		u64 expiry;
		TString token;
	};

	class DLToken : nb::INbDeserializable, public NbDLToken<std::string>
	{
	public:
		static constexpr const char *magic = "TOKN";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbDLToken<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbDLToken<nb::BlobPtr> *thdr = (NbDLToken<nb::BlobPtr> *)header;

			this->id = thdr->id;
			this->expiry = thdr->expiry;
			this->token = std::string((char *)&blob[thdr->token]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// the real fuckery, aka title index

	struct NbIndexMeta
	{
		u32 titles;
		u32 officialTitles;
		u32 legitTitles;
		u64 size;
		u64 downloads;
	};

	template <typename TString>
	struct NbIndexCategoryBase
	{
		NbIndexMeta meta;
		u32 id;
		TString disp;
		TString name;
		TString desc;
	};


	template <typename TString>
	struct NbIndexCategory : NbIndexCategoryBase<TString>
	{
		u8 prio;
	};

	class IndexCategory;
	class IndexSubcategory : nb::INbDeserializable, public NbIndexCategoryBase<std::string>
	{
	public:
		static constexpr const char *magic = "IXCB";

		IndexCategory *parent;

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbIndexCategoryBase<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndexCategoryBase<nb::BlobPtr> *chdr = (NbIndexCategoryBase<nb::BlobPtr> *)header;

			memcpy(&this->meta, &chdr->meta, sizeof(chdr->meta));

			char *strdata = (char *)blob;

			this->id = chdr->id;
			this->name = std::string(&strdata[chdr->name]);
			this->disp = std::string(&strdata[chdr->disp]);
			this->desc = std::string(&strdata[chdr->desc]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// begin runtime classes

	class IndexCategory : nb::INbDeserializable, public NbIndexCategoryBase<std::string>
	{
	public:
		static constexpr const char *magic = "IXCT"; // even though this won't be used ever since this obj is in an array

		u8 prio = 0;

		std::map<u8, IndexSubcategory> subcategories;

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size < sizeof(NbIndexCategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndexCategory<nb::BlobPtr> *chdr = (NbIndexCategory<nb::BlobPtr> *)header;

			memcpy(&this->meta, &chdr->meta, sizeof(this->meta));

			char *strdata = (char *)blob;

			this->id = chdr->id;
			this->name = std::string(&strdata[chdr->name]);
			this->disp = std::string(&strdata[chdr->disp]);
			this->desc = std::string(&strdata[chdr->desc]);
			this->prio = chdr->prio;

			return nb::StatusCode::SUCCESS;
		};
	};

	struct NbIndex
	{
		NbIndexMeta meta;
		u64 Date;
	};

	class Index : nb::INbDeserializable, public NbIndex
	{
	public:
		static constexpr const char *magic = "TIDX";

		std::map<u8, IndexCategory> categories;

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size <= sizeof(NbIndex))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndex *ihdr = (NbIndex *)header;

			memcpy(dynamic_cast<NbIndex *>(this), ihdr, sizeof(NbIndex));

			header += sizeof(NbIndex);
			header_size -= sizeof(NbIndex);

			nb::StatusCode err = nb::StatusCode::SUCCESS;
			u32 tmp = 0;

			std::vector<IndexCategory> cat_array;

			if ((err = nb::parse_array<IndexCategory>(cat_array, header, header_size, &tmp)) != nb::StatusCode::SUCCESS)
				return err;

			header += tmp;
			header_size -= tmp;

			for (const IndexCategory& c : cat_array)
			{
				this->categories[c.id] = c;

				std::vector<IndexSubcategory> scat_array;

				if ((err = nb::parse_array<IndexSubcategory>(scat_array, header, header_size, &tmp)) != nb::StatusCode::SUCCESS)
					return err;

				header += tmp;
				header_size -= tmp;

				for (const IndexSubcategory& s : scat_array)
				{
					IndexCategory& cat = this->categories[c.id];
					cat.subcategories[s.id] = s;
					cat.subcategories[s.id].parent = &cat;
				}
			}

			return nb::StatusCode::SUCCESS;
		}
	};

	template <typename TString>
	struct NbTopTitle
	{
		u32 id;
		u64 dlCount;
		TString name;
		TString alt;
	};

	class TopTitle : nb::INbDeserializable, public NbTopTitle<std::string>
	{
	public:
		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size <= sizeof(NbTopTitle<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbTopTitle<nb::BlobPtr> *thdr = (NbTopTitle<nb::BlobPtr> *)header;

			this->id = thdr->id;
			this->dlCount = thdr->dlCount;

			char *strdata = (char *)blob;

			this->name = std::string(&strdata[thdr->name]);
			if (thdr->alt) this->alt = std::string(&strdata[thdr->alt]);

			return nb::StatusCode::SUCCESS;
		}
	};

	template <typename TString>
	struct NbSimpleTitle
	{
		u64 tid;
		u64 size;
		u64 flags;
		u64 dlCount;
		u32 id;
		TString name;
		TString alt;
		TString prod;
		u16 version;
		u8 contentType;
		u8 cat;
		u8 subcat;
	};

	class SimpleTitle : nb::INbDeserializable, public NbSimpleTitle<std::string>
	{
	public:
		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size)
		{
			if (header_size <= sizeof(NbSimpleTitle<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbSimpleTitle<nb::BlobPtr> *sthdr = (NbSimpleTitle<nb::BlobPtr> *)header;

			this->tid = sthdr->tid;
			this->size = sthdr->size;
			this->flags = sthdr->flags;
			this->id = sthdr->id;
			this->version = sthdr->version;
			this->contentType = sthdr->contentType;
			this->cat = sthdr->cat;
			this->subcat = sthdr->subcat;

			char *strdata = (char *)blob;

			this->name = std::string(&strdata[sthdr->name]);
			this->prod = std::string(&strdata[sthdr->prod]);
			if (sthdr->alt) this->alt = std::string(&strdata[sthdr->alt]);

			return nb::StatusCode::SUCCESS;
		}
	};
}

#endif
