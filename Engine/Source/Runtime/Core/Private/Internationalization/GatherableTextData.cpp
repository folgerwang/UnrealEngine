// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/GatherableTextData.h"
#include "Serialization/StructuredArchiveFromArchive.h"

FArchive& operator<<(FArchive& Archive, FTextSourceSiteContext& This)
{
	FStructuredArchiveFromArchive(Archive).GetSlot() << This;
	return Archive;
}

void operator<<(FStructuredArchive::FSlot Slot, FTextSourceSiteContext& This)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	Record << NAMED_ITEM("KeyName", This.KeyName);
	Record << NAMED_ITEM("SiteDescription", This.SiteDescription);
	Record << NAMED_ITEM("IsEditorOnly", This.IsEditorOnly);
	Record << NAMED_ITEM("IsOptional", This.IsOptional);
	Record << NAMED_ITEM("InfoMetaData", This.InfoMetaData);
	Record << NAMED_ITEM("KeyMetaData", This.KeyMetaData);
}

FArchive& operator<<(FArchive& Archive, FTextSourceData& This)
{
	FStructuredArchiveFromArchive(Archive).GetSlot() << This;
	return Archive;
}

void operator<<(FStructuredArchive::FSlot Slot, FTextSourceData& This)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	Record << NAMED_ITEM("SourceString", This.SourceString);
	Record << NAMED_ITEM("SourceStringMetaData", This.SourceStringMetaData);
}

FArchive& operator<<(FArchive& Archive, FGatherableTextData& This)
{
	FStructuredArchiveFromArchive(Archive).GetSlot() << This;
	return Archive;
}

void operator<<(FStructuredArchive::FSlot Slot, FGatherableTextData& This)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	Record << NAMED_ITEM("NamespaceName", This.NamespaceName);
	Record << NAMED_ITEM("SourceData", This.SourceData);
	Record << NAMED_ITEM("SourceSiteContexts", This.SourceSiteContexts);
}