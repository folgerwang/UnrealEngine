// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_Hook.h"
#include "LC_PointerUtil.h"
#include "LC_Symbols.h"


uint32_t hook::FindFirstInSection(const symbols::ImageSectionDB* imageSectionDb, const ImmutableString& sectionName)
{
	const symbols::ImageSection* imageSection = symbols::FindImageSectionByName(imageSectionDb, sectionName);
	if (imageSection)
	{
		return imageSection->rva;
	}

	return 0u;
}


uint32_t hook::FindLastInSection(const symbols::ImageSectionDB* imageSectionDb, const ImmutableString& sectionName)
{
	const symbols::ImageSection* imageSection = symbols::FindImageSectionByName(imageSectionDb, sectionName);
	if (imageSection)
	{
		return imageSection->rva + imageSection->size;
	}

	return 0u;
}


const hook::Function* hook::MakeFunction(const void* moduleBase, uint32_t rva)
{
	return pointer::Offset<const hook::Function*>(moduleBase, rva);
}
