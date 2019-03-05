// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Executable.h"
#include "LC_Logging.h"
#include <algorithm>


// this is the default DLL entry point, taken from the CRT. we don't need it, but can extract its signature.
extern "C" BOOL WINAPI _DllMainCRTStartup(
	HINSTANCE const instance,
	DWORD     const reason,
	LPVOID    const reserved
);


namespace detail
{
	static inline bool SortSectionByAscendingRVA(const executable::ImageSection& lhs, const executable::ImageSection& rhs)
	{
		return lhs.rva < rhs.rva;
	}


	static const IMAGE_NT_HEADERS* GetNtHeader(const executable::Image* image)
	{
		void* base = image->base;

		// PE image start with a DOS header
		IMAGE_DOS_HEADER* dosHeader = static_cast<IMAGE_DOS_HEADER*>(base);
		if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		{
			LC_ERROR_USER("Image has unknown file format");
			return nullptr;
		}

		IMAGE_NT_HEADERS* ntHeader = pointer::Offset<IMAGE_NT_HEADERS*>(dosHeader, dosHeader->e_lfanew);
		if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
		{
			LC_ERROR_USER("Invalid .exe file");
			return nullptr;
		}

		return ntHeader;
	}


	static IMAGE_NT_HEADERS* GetNtHeader(executable::Image* image)
	{
		return const_cast<IMAGE_NT_HEADERS*>(GetNtHeader(const_cast<const executable::Image*>(image)));
	}


	static const IMAGE_SECTION_HEADER* GetSectionHeader(const IMAGE_NT_HEADERS* ntHeader)
	{
		return IMAGE_FIRST_SECTION(ntHeader);
	}
}


uint32_t executable::GetEntryPointRva(const Image* image)
{
	const IMAGE_NT_HEADERS* ntHeader = detail::GetNtHeader(image);
	if (!ntHeader)
	{
		return 0u;
	}

	return ntHeader->OptionalHeader.AddressOfEntryPoint;
}


executable::PreferredBase executable::GetPreferredBase(const Image* image)
{
	const IMAGE_NT_HEADERS* ntHeader = detail::GetNtHeader(image);
	if (!ntHeader)
	{
		return 0ull;
	}

	return ntHeader->OptionalHeader.ImageBase;
}


executable::Header executable::GetHeader(const Image* image)
{
	const IMAGE_NT_HEADERS* ntHeader = detail::GetNtHeader(image);
	if (!ntHeader)
	{
		return executable::Header {};
	}

	return executable::Header { ntHeader->FileHeader, ntHeader->OptionalHeader.SizeOfImage };
}


bool executable::IsValidHeader(const Header& header)
{
	return header.imageHeader.NumberOfSections != 0;
}


uint32_t executable::GetSize(const Image* image)
{
	const IMAGE_NT_HEADERS* ntHeader = detail::GetNtHeader(image);
	if (!ntHeader)
	{
		return 0ull;
	}

	return ntHeader->OptionalHeader.SizeOfImage;
}


uint32_t executable::RvaToFileOffset(const ImageSectionDB* database, uint32_t rva)
{
	LC_ASSERT(rva != 0u, "RVA cannot be mapped to image.");

	const size_t count = database->sections.size();
	for (size_t i = 0u; i < count; ++i)
	{
		const ImageSection& section = database->sections[i];
		if ((rva >= section.rva) && (rva < section.rva + section.size))
		{
			const uint32_t sectionOffset = rva - section.rva;
			if (sectionOffset >= section.rawDataSize)
			{
				// the offset relative to the section lies outside the section data stored in the image.
				// this can happen for sections like .bss/.data which don't store uninitialized data for
				// the symbols.
				return 0u;
			}

			return section.rawDataRva + sectionOffset;
		}
	}

	LC_ERROR_DEV("Cannot map RVA 0x%X to executable image file offset", rva);
	return 0u;
}


void executable::ReadFromFileOffset(const Image* image, uint32_t offset, void* destination, size_t byteCount)
{
	const void* address = pointer::Offset<const void*>(image->base, offset);
	memcpy(destination, address, byteCount);
}


void executable::WriteToFileOffset(Image* image, uint32_t offset, const void* source, size_t byteCount)
{
	void* address = pointer::Offset<void*>(image->base, offset);
	memcpy(address, source, byteCount);
}


executable::Image* executable::OpenImage(const wchar_t* filename, file::OpenMode::Enum openMode)
{
	return file::Open(filename, openMode);
}


void executable::CloseImage(Image*& image)
{
	file::Close(image);
}


void executable::RebaseImage(Image* image, PreferredBase preferredBase)
{
	void* base = image->base;

	IMAGE_NT_HEADERS* ntHeader = detail::GetNtHeader(image);
	if (!ntHeader)
	{
		return;
	}

	const IMAGE_SECTION_HEADER* sectionHeader = detail::GetSectionHeader(ntHeader);
	if (!sectionHeader)
	{
		return;
	}

	ImageSectionDB* database = GatherSections(image);

	// the image has been linked against a certain base address, namely ntHeader->OptionalHeader.ImageBase.
	// work out by how much all relocations need to be shifted if basing the image against the new
	// preferred base.
	const int64_t baseDelta = static_cast<int64_t>(preferredBase - ntHeader->OptionalHeader.ImageBase);

	// this is the easy part: simply set the new preferred base address in the image
	ntHeader->OptionalHeader.ImageBase = preferredBase;

	// now comes the hard part: patch all relocation entries in the image
	const DWORD relocSectionSize = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
	if (relocSectionSize != 0u)
	{
		// .reloc section exists, patch it
		const DWORD baseRelocationOffset = RvaToFileOffset(database, ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		const IMAGE_BASE_RELOCATION* baseRelocations = pointer::Offset<const IMAGE_BASE_RELOCATION*>(base, baseRelocationOffset);

		DWORD blockSizeLeft = relocSectionSize;
		while (blockSizeLeft > 0u)
		{
			const DWORD pageRVA = baseRelocations->VirtualAddress;
			const DWORD blockSize = baseRelocations->SizeOfBlock;
			const DWORD blockOffset = RvaToFileOffset(database, pageRVA);

			// PE spec: Block size: The total number of bytes in the base relocation block, *including* the Page RVA and
			// Block Size fields and the Type/Offset fields that follow.
			DWORD numberOfEntriesInThisBlock = (blockSize - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
			const WORD* entries = pointer::Offset<const WORD*>(baseRelocations, sizeof(IMAGE_BASE_RELOCATION));
			for (DWORD i = 0u; i < numberOfEntriesInThisBlock; ++i)
			{
				// PE spec: Type: Stored in the high 4 bits of the WORD
				//			Offset: Stored in the remaining 12 bits of the WORD
				const WORD low12BitMask = 0x0FFF;
				const WORD type = static_cast<WORD>(entries[i] >> 12u);
				const WORD offset = static_cast<WORD>(entries[i] & low12BitMask);

				// PE spec:
				// IMAGE_REL_BASED_ABSOLUTE: The base relocation is skipped. This type can be used to pad a block.
				// IMAGE_REL_BASED_HIGH: The base relocation adds the high 16 bits of the difference to the 16-bit field
				// at offset. The 16-bit field represents the high value of a 32-bit word.
				// IMAGE_REL_BASED_LOW: The base relocation adds the low 16 bits of the difference to the 16-bit field
				// at offset. The 16-bit field represents the low half of a 32-bit word.
				// IMAGE_REL_BASED_HIGHLOW: The base relocation applies all 32 bits of the difference to the 32-bit field at
				// offset.
				// IMAGE_REL_BASED_HIGHADJ: The base relocation adds the high 16 bits of the difference to the 16-bit field
				// at offset. The 16-bit field represents the high value of a 32-bit word. The low 16 bits of the 32-bit value
				// are stored in the 16-bit word that follows this base relocation. This means that this base relocation
				// occupies two slots.
				// IMAGE_REL_BASED_DIR64: The base relocation applies the difference to the 64-bit field at offset.
				if (type == IMAGE_REL_BASED_ABSOLUTE)
				{
					continue;
				}
				else if (type == IMAGE_REL_BASED_HIGHLOW)
				{
					uint32_t* relocation = pointer::Offset<uint32_t*>(base, blockOffset + offset);
					*relocation += static_cast<uint32_t>(baseDelta);
				}
				else if (type == IMAGE_REL_BASED_DIR64)
				{
					uint64_t* relocation = pointer::Offset<uint64_t*>(base, blockOffset + offset);
					*relocation += static_cast<uint64_t>(baseDelta);
				}
			}

			baseRelocations = pointer::Offset<const IMAGE_BASE_RELOCATION*>(baseRelocations, baseRelocations->SizeOfBlock);
			LC_ASSERT(blockSizeLeft >= blockSize, "Underflow while reading image relocations");
			blockSizeLeft -= blockSize;
		}
	}

	DestroyImageSectionDB(database);
}


executable::ImageSectionDB* executable::GatherSections(const Image* image)
{
	const IMAGE_NT_HEADERS* ntHeader = detail::GetNtHeader(image);
	if (!ntHeader)
	{
		return nullptr;
	}

	const IMAGE_SECTION_HEADER* sectionHeader = detail::GetSectionHeader(ntHeader);
	if (!sectionHeader)
	{
		return nullptr;
	}

	ImageSectionDB* database = new ImageSectionDB;
	const size_t sectionCount = ntHeader->FileHeader.NumberOfSections;
	database->sections.reserve(sectionCount);

	for (size_t i = 0u; i < sectionCount; ++i)
	{
		database->sections.emplace_back(ImageSection { sectionHeader->VirtualAddress, sectionHeader->Misc.VirtualSize, sectionHeader->PointerToRawData, sectionHeader->SizeOfRawData });
		++sectionHeader;
	}

	std::sort(database->sections.begin(), database->sections.end(), &detail::SortSectionByAscendingRVA);

	return database;
}


void executable::DestroyImageSectionDB(ImageSectionDB* database)
{
	delete database;
}


void executable::CallDllEntryPoint(void* moduleBase, uint32_t entryPointRva)
{
	typedef decltype(_DllMainCRTStartup) DllEntryPoint;

	DllEntryPoint* entryPoint = pointer::Offset<DllEntryPoint*>(moduleBase, entryPointRva);
	entryPoint(static_cast<HINSTANCE>(moduleBase), DLL_PROCESS_ATTACH, NULL);
}
