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

namespace nbt /* nbType */
{
	// title

	template <typename TString>
	struct NbTitle
	{
		u8 Seed[16];
		u64 TitleID;
		u64 AddedDate;
		u64 UpdatedDate;
		u64 DownloadCount;
		u64 Flags;
		u32 ID;
		TString Name;
		TString AlternativeName;
		TString Region;
		TString FileName;
		TString ShortDescription;
		TString ProductCode;
		u16 Version;
		u8 ContentType;
		u8 CategoryID;
		u8 SubcategoryID;
		bool IsListed;
	};

	class Title : nb::INbDeserializable, public NbTitle<std::string>
	{
	public:
		static constexpr const char *magic = "TITL";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbTitle<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbTitle<nb::BlobPtr> *thdr = (NbTitle<nb::BlobPtr> *)header;

			this->TitleID = thdr->TitleID;
			this->AddedDate = thdr->AddedDate;
			this->UpdatedDate = thdr->UpdatedDate;
			this->DownloadCount = thdr->DownloadCount;
			this->Flags = thdr->DownloadCount;
			this->ID = thdr->ID;
			this->Version = thdr->Version;
			this->ContentType = thdr->ContentType;
			this->IsListed = thdr->IsListed;

			memcpy(this->Seed, thdr->Seed, 16);

			char *strdata = (char *)blob;

			this->Name = std::string(&strdata[thdr->Name]);
			this->Region = std::string(&strdata[thdr->Region]);
			this->FileName = std::string(&strdata[thdr->FileName]);
			this->ProductCode = std::string(&strdata[thdr->ProductCode]);
			if (thdr->ShortDescription) this->ShortDescription = std::string(&strdata[thdr->ShortDescription]);
			if (thdr->AlternativeName) this->AlternativeName = std::string(&strdata[thdr->AlternativeName]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// category

	template <typename TString>
	struct NbCategory
	{
		u32 ID;
		TString Name;
		TString DisplayName;
		TString Description;
		TString SubcategoryDescription;
		u8 Priority;
	};

	class Category : nb::INbDeserializable, public NbCategory<std::string>
	{
	public:
		static constexpr const char *magic = "CATE";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbCategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbCategory<nb::BlobPtr> *chdr = (NbCategory<nb::BlobPtr> *)header;

			this->ID = chdr->ID;

			char *strdata = (char *)blob;

			this->Name = std::string(&strdata[chdr->Name]);
			this->DisplayName = std::string(&strdata[chdr->DisplayName]);
			this->Description = std::string(&strdata[chdr->Description]);
			if (chdr->SubcategoryDescription) this->SubcategoryDescription = std::string(&strdata[chdr->SubcategoryDescription]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// subcategory

	template <typename TString>
	struct NbSubcategory
	{
		u32 ID;
		TString Name;
		TString DisplayName;
		TString Description;
		bool Standalone;
	};

	class Subcategory : nb::INbDeserializable, public NbSubcategory<std::string>
	{
	public:
		static constexpr const char *magic = "SCAT";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbSubcategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbSubcategory<nb::BlobPtr> *shdr = (NbSubcategory<nb::BlobPtr> *)header;

			this->ID = shdr->ID;
			this->Standalone = shdr->Standalone;

			char *strdata = (char *)blob;

			this->Name = std::string(&strdata[shdr->Name]);
			this->DisplayName = std::string(&strdata[shdr->DisplayName]);

			if (this->Standalone && shdr->Description)
				this->Description = std::string(&strdata[shdr->Description]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// 3hs release

	template <typename TString>
	struct NbThsRelease
	{
		u64 AddedDate;
		TString Version;
		TString VersionDescription;
		TString Changelog;
		TString DownloadURL;
		TString SourceURL;
	};

	class ThsRelease : nb::INbDeserializable, public NbThsRelease<std::string>
	{
	public:
		static constexpr const char *magic = "3HSR";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbThsRelease<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbThsRelease<nb::BlobPtr> *rhdr = (NbThsRelease<nb::BlobPtr> *)header;

			this->AddedDate = rhdr->AddedDate;

			char *strdata = (char *)blob;

			this->Version = std::string(&strdata[rhdr->Version]);
			this->VersionDescription = std::string(&strdata[rhdr->VersionDescription]);
			this->Changelog = std::string(&strdata[rhdr->Changelog]);
			this->DownloadURL = std::string(&strdata[rhdr->DownloadURL]);
			this->SourceURL = std::string(&strdata[rhdr->SourceURL]);
				
			return nb::StatusCode::SUCCESS;
		}
	};

	// ID pairs

	struct NbIDPair
	{
		u64 TitleID;
		u32 ID;
	};

	class IDPair : NbIDPair, public nb::INbDeserializable
	{
	public:
		static constexpr const char *magic = "PAIR";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbIDPair))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIDPair *phdr = (NbIDPair *)header;

			this->TitleID = phdr->TitleID;
			this->ID = phdr->ID;

			return nb::StatusCode::SUCCESS;
		}
	};

	// results

	template<typename TString>
	struct NbResult
	{
		u32 ResultCode;
		TString ErrorMessage;
	};

	class Result : nb::INbDeserializable, public NbResult<std::string>
	{
	public:
		static constexpr const char *magic = "RSLT";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbResult<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbResult<nb::BlobPtr> *rhdr = (NbResult<nb::BlobPtr> *)header;

			this->ResultCode = rhdr->ResultCode;
			if (rhdr->ErrorMessage)
				this->ErrorMessage = std::string((char *)&blob[rhdr->ErrorMessage]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// log results

	struct NbThsLogResult	
	{
		u32 ID;
		u64 ValidityDuration;
	};		

	class ThsLogResult : nb::INbDeserializable, public NbThsLogResult
	{
	public:
		static constexpr const char *magic = "HLOG";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbThsLogResult))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbThsLogResult *rhdr = (NbThsLogResult *)header;

			this->ID = rhdr->ID;
			this->ValidityDuration = rhdr->ValidityDuration;

			return nb::StatusCode::SUCCESS;
		};
	};

	// tokens

	template <typename TString>
	struct NbDLToken
	{
		u32 ID;
		u64 ExpiryDate;
		TString Token;
	};

	class DLToken : nb::INbDeserializable, NbDLToken<std::string>
	{
	public:
		static constexpr const char *magic = "TOKN";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbDLToken<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbDLToken<nb::BlobPtr> *thdr = (NbDLToken<nb::BlobPtr> *)header;

			this->ID = thdr->ID;
			this->ExpiryDate = thdr->ExpiryDate;
			this->Token = std::string((char *)&blob[thdr->Token]);

			return nb::StatusCode::SUCCESS;
		}
	};

	// the real fuckery, aka title index

	struct NbIndexMeta
	{
		u32 TotalContentCount;
		u32 OfficialContentCount;
		u32 LegitContentCount;
		u64 Size;
		u64 TotalDownloads;
	};

	template <typename TString>
	struct NbIndexCategoryBase
	{
		NbIndexMeta Meta;
		u32 ID;
		TString Name;
		TString DisplayName;
		TString Description;
	};


	template <typename TString>
	struct NbIndexCategory : NbIndexCategoryBase<TString>
	{
		u8 Priority;
	};

	class IndexCategoryBase : nb::INbDeserializable, public NbIndexCategoryBase<std::string>
	{
	public:
		static constexpr const char *magic = "IXCB";

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbIndexCategoryBase<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndexCategoryBase<nb::BlobPtr> *chdr = (NbIndexCategoryBase<nb::BlobPtr> *)header;

			memcpy(&this->Meta, &chdr->Meta, sizeof(chdr->Meta));

			char *strdata = (char *)blob;

			this->ID = chdr->ID;
			this->Name = std::string(&strdata[chdr->Name]);
			this->DisplayName = std::string(&strdata[chdr->DisplayName]);
			this->Description = std::string(&strdata[chdr->Description]);

			return nb::StatusCode::SUCCESS;
		}
	};

	using IndexSubcategory = IndexCategoryBase;

	// begin runtime classes

	class IndexCategory : nb::INbDeserializable, public NbIndexCategoryBase<std::string>
	{
	public:
		static constexpr const char *magic = "IXCT"; // even though this won't be used ever since this obj is in an array

		u8 Priority = 0;

		std::map<u8, IndexSubcategory> Subcategories;

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
		{
			if (header_size < sizeof(NbIndexCategory<nb::BlobPtr>))
				return nb::StatusCode::INPUT_DATA_TOO_SHORT;

			NbIndexCategory<nb::BlobPtr> *chdr = (NbIndexCategory<nb::BlobPtr> *)header;

			memcpy(&this->Meta, &chdr->Meta, sizeof(this->Meta));

			char *strdata = (char *)blob;

			this->ID = chdr->ID;
			this->Name = std::string(&strdata[chdr->Name]);
			this->DisplayName = std::string(&strdata[chdr->DisplayName]);
			this->Description = std::string(&strdata[chdr->Description]);
			this->Priority = chdr->Priority;

			return nb::StatusCode::SUCCESS;
		};
	};

	struct NbIndex
	{
		NbIndexMeta Meta;
		u64 Date;
	};

	class Index : nb::INbDeserializable, public NbIndex
	{
	public:
		static constexpr const char *magic = "TIDX";

		std::map<u8, IndexCategory> Categories;

		nb::StatusCode deserialize(u8 *header, u32 header_size, u8 *blob, u32 blob_size) override
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
				this->Categories[c.ID] = c;

				std::vector<IndexSubcategory> scat_array;

				if ((err = nb::parse_array<IndexSubcategory>(scat_array, header, header_size, &tmp)) != nb::StatusCode::SUCCESS)
					return err;

				header += tmp;
				header_size -= tmp;

				for (const IndexSubcategory& s : scat_array)
					this->Categories[c.ID].Subcategories[s.ID] = s;
			}

			return nb::StatusCode::SUCCESS;
		}
	};
}

#endif
