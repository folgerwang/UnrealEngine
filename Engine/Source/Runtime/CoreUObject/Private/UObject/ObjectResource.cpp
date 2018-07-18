// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UObject/ObjectResource.h"
#include "UObject/Class.h"

/*-----------------------------------------------------------------------------
	Helper functions.
-----------------------------------------------------------------------------*/
namespace
{
	FORCEINLINE bool IsCorePackage(const FName& PackageName)
	{
		return PackageName == NAME_Core || PackageName == GLongCorePackageName;
	}
}

/*-----------------------------------------------------------------------------
	FObjectResource
-----------------------------------------------------------------------------*/

FObjectResource::FObjectResource()
{}

FObjectResource::FObjectResource( UObject* InObject )
:	ObjectName		( InObject ? InObject->GetFName() : FName(NAME_None)		)
{
}

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

FObjectExport::FObjectExport()
: FObjectResource()
, ObjectFlags(RF_NoFlags)
, SerialSize(0)
, SerialOffset(0)
, ScriptSerializationStartOffset(0)
, ScriptSerializationEndOffset(0)
, Object(NULL)
, HashNext(INDEX_NONE)
, bForcedExport(false)
, bNotForClient(false)
, bNotForServer(false)
, bNotAlwaysLoadedForEditorGame(true)
, bIsAsset(false)
, bExportLoadFailed(false)
, DynamicType(EDynamicType::NotDynamicExport)
, bWasFiltered(false)
, PackageGuid(FGuid(0, 0, 0, 0))
, PackageFlags(0)
, FirstExportDependency(-1)
, SerializationBeforeSerializationDependencies(0)
, CreateBeforeSerializationDependencies(0)
, SerializationBeforeCreateDependencies(0)
, CreateBeforeCreateDependencies(0)

{}

FObjectExport::FObjectExport( UObject* InObject )
: FObjectResource(InObject)
, ObjectFlags(InObject ? InObject->GetMaskedFlags() : RF_NoFlags)
, SerialSize(0)
, SerialOffset(0)
, ScriptSerializationStartOffset(0)
, ScriptSerializationEndOffset(0)
, Object(InObject)
, HashNext(INDEX_NONE)
, bForcedExport(false)
, bNotForClient(false)
, bNotForServer(false)
, bNotAlwaysLoadedForEditorGame(true)
, bIsAsset(false)
, bExportLoadFailed(false)
, DynamicType(EDynamicType::NotDynamicExport)
, bWasFiltered(false)
, PackageGuid(FGuid(0, 0, 0, 0))
, PackageFlags(0)
, FirstExportDependency(-1)
, SerializationBeforeSerializationDependencies(0)
, CreateBeforeSerializationDependencies(0)
, SerializationBeforeCreateDependencies(0)
, CreateBeforeCreateDependencies(0)
{
	if(Object)		
	{
		bNotForClient = Object->HasAnyMarks(OBJECTMARK_NotForClient);
		bNotForServer = Object->HasAnyMarks(OBJECTMARK_NotForServer);
		bNotAlwaysLoadedForEditorGame = Object->HasAnyMarks(OBJECTMARK_NotAlwaysLoadedForEditorGame);
		bIsAsset = Object->IsAsset();
	}
}

FArchive& operator<<(FArchive& Ar, FObjectExport& E)
{
	FStructuredArchiveFromArchive(Ar).GetSlot() << E;
	return Ar;
}

void operator<<(FStructuredArchive::FSlot Slot, FObjectExport& E)
{
	FArchive& BaseArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	Record << NAMED_ITEM("ClassIndex", E.ClassIndex);
	Record << NAMED_ITEM("SuperIndex", E.SuperIndex);

	if (BaseArchive.UE4Ver() >= VER_UE4_TemplateIndex_IN_COOKED_EXPORTS)
	{
		Record << NAMED_ITEM("TemplateIndex", E.TemplateIndex);
	}

	Record << NAMED_ITEM("OuterIndex", E.OuterIndex);
	Record << NAMED_ITEM("ObjectName", E.ObjectName);

	uint32 Save = E.ObjectFlags & RF_Load;
	Record << NAMED_ITEM("ObjectFlags", Save);

	if (BaseArchive.IsLoading())
	{
		E.ObjectFlags = EObjectFlags(Save & RF_Load);
	}

	if (BaseArchive.UE4Ver() < VER_UE4_64BIT_EXPORTMAP_SERIALSIZES)
	{
		int32 SerialSize = E.SerialSize;
		Record << NAMED_FIELD(SerialSize);
		E.SerialSize = (int64)SerialSize;

		int32 SerialOffset = E.SerialOffset;
		Record << NAMED_FIELD(SerialOffset);
		E.SerialOffset = SerialOffset;
	}
	else
	{
		Record << NAMED_ITEM("SerialSize", E.SerialSize);
		Record << NAMED_ITEM("SerialOffset", E.SerialOffset);
	}

	Record << NAMED_ITEM("bForcedExport", E.bForcedExport);
	Record << NAMED_ITEM("bNotForClient", E.bNotForClient);
	Record << NAMED_ITEM("bNotForServer", E.bNotForServer);

	Record << NAMED_ITEM("PackageGuid", E.PackageGuid);
	Record << NAMED_ITEM("PackageFlags", E.PackageFlags);

	if (BaseArchive.UE4Ver() >= VER_UE4_LOAD_FOR_EDITOR_GAME)
	{
		Record << NAMED_ITEM("bNotAlwaysLoadedForEditorGame", E.bNotAlwaysLoadedForEditorGame);
	}

	if (BaseArchive.UE4Ver() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		Record << NAMED_ITEM("bIsAsset", E.bIsAsset);
	}

	if (BaseArchive.UE4Ver() >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
	{
		Record << NAMED_ITEM("FirstExportDependency", E.FirstExportDependency);
		Record << NAMED_ITEM("SerializationBeforeSerializationDependencies", E.SerializationBeforeSerializationDependencies);
		Record << NAMED_ITEM("CreateBeforeSerializationDependencies", E.CreateBeforeSerializationDependencies);
		Record << NAMED_ITEM("SerializationBeforeCreateDependencies", E.SerializationBeforeCreateDependencies);
		Record << NAMED_ITEM("CreateBeforeCreateDependencies", E.CreateBeforeCreateDependencies);
	}	
}

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

FObjectImport::FObjectImport()
	: FObjectResource()
	, bImportPackageHandled(false)
	, bImportSearchedFor(false)
	, bImportFailed(false)
{
}

FObjectImport::FObjectImport(UObject* InObject)
	: FObjectResource(InObject)
	, ClassPackage(InObject ? InObject->GetClass()->GetOuter()->GetFName() : NAME_None)
	, ClassName(InObject ? InObject->GetClass()->GetFName() : NAME_None)
	, XObject(InObject)
	, SourceLinker(NULL)
	, SourceIndex(INDEX_NONE)
	, bImportPackageHandled(false)
	, bImportSearchedFor(false)
	, bImportFailed(false)
{
}

FObjectImport::FObjectImport(UObject* InObject, UClass* InClass)
	: FObjectResource(InObject)
	, ClassPackage((InObject && InClass) ? InClass->GetOuter()->GetFName() : NAME_None)
	, ClassName((InObject && InClass) ? InClass->GetFName() : NAME_None)
	, XObject(InObject)
	, SourceLinker(NULL)
	, SourceIndex(INDEX_NONE)
	, bImportPackageHandled(false)
	, bImportSearchedFor(false)
	, bImportFailed(false)
{
}

FArchive& operator<<( FArchive& Ar, FObjectImport& I )
{
	FStructuredArchiveFromArchive(Ar).GetSlot() << I;
	return Ar;
}

void operator<<(FStructuredArchive::FSlot Slot, FObjectImport& I)
{
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	Record << NAMED_ITEM("ClassPackage", I.ClassPackage);
	Record << NAMED_ITEM("ClassName", I.ClassName);
	Record << NAMED_ITEM("OuterIndex", I.OuterIndex);
	Record << NAMED_ITEM("ObjectName", I.ObjectName);

	if (Slot.GetUnderlyingArchive().IsLoading())
	{
		I.SourceLinker = NULL;
		I.SourceIndex = INDEX_NONE;
		I.XObject = NULL;
	}
}
