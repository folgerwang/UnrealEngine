// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_PointerUtil.h"
#include "LC_MemoryFile.h"
#include "xxhash.h"
#include <vector>
#include "Windows/WindowsHWrapper.h"


namespace executable
{
#if LC_64_BIT
	typedef uint64_t PreferredBase;
#else
	typedef uint32_t PreferredBase;
#endif

	struct Header
	{
		IMAGE_FILE_HEADER imageHeader;
		uint64_t size;
	};

	typedef file::MemoryFile Image;

	struct ImageSection
	{
		uint32_t rva;
		uint32_t size;
		uint32_t rawDataRva;
		uint32_t rawDataSize;
	};

	struct ImageSectionDB
	{
		std::vector<ImageSection> sections;
	};


	Image* OpenImage(const wchar_t* filename, file::OpenMode::Enum openMode);
	void CloseImage(Image*& image);

	void RebaseImage(Image* image, PreferredBase preferredBase);


	ImageSectionDB* GatherSections(const Image* image);
	void DestroyImageSectionDB(ImageSectionDB* database);



	// returns the RVA of the entry point of the image when loaded into memory
	uint32_t GetEntryPointRva(const Image* image);

	// returns the preferred address at which the image is to be loaded into memory
	PreferredBase GetPreferredBase(const Image* image);

	// returns the image's header
	Header GetHeader(const Image* image);

	// checks whether a header is valid
	bool IsValidHeader(const Header& header);

	// returns the size of the image when loaded into memory
	uint32_t GetSize(const Image* image);

	// helper functions
	uint32_t RvaToFileOffset(const ImageSectionDB* database, uint32_t rva);

	void ReadFromFileOffset(const Image* file, uint32_t offset, void* destination, size_t byteCount);
	void WriteToFileOffset(Image* file, uint32_t offset, const void* source, size_t byteCount);


	// helper function to directly read from an RVA in the given image
	template <typename T>
	inline T ReadFromImage(const Image* image, const ImageSectionDB* database, uint32_t rva)
	{
		const uint32_t fileOffset = RvaToFileOffset(database, rva);
		if (fileOffset == 0u)
		{
			// don't try to read from sections without data in the image, e.g. .bss
			return T(0);
		}

		const T* address = pointer::Offset<const T*>(image->base, fileOffset);
		return *address;
	}

	void CallDllEntryPoint(void* moduleBase, uint32_t entryPointRva);
}


// specializations to allow executable::Header to be used as key in maps and sets
namespace std
{
	template <>
	struct hash<executable::Header>
	{
		inline std::size_t operator()(const executable::Header& header) const
		{
			return static_cast<uint32_t>(XXH32(&header.imageHeader, sizeof(executable::Header::imageHeader), 0u));
		}
	};

	template <>
	struct equal_to<executable::Header>
	{
		inline bool operator()(const executable::Header& lhs, const executable::Header& rhs) const
		{
			if (lhs.size != rhs.size)
				return false;

			return (::memcmp(&lhs.imageHeader, &rhs.imageHeader, sizeof(executable::Header::imageHeader)) == 0);
		}
	};
}
