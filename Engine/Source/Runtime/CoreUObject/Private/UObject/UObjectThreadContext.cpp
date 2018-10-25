// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectThreadContext.cpp: Unreal object globals
=============================================================================*/

#include "UObject/UObjectThreadContext.h"

DEFINE_LOG_CATEGORY(LogUObjectThreadContext);

FUObjectThreadContext::FUObjectThreadContext()
: IsRoutingPostLoad(false)
, CurrentlyPostLoadedObjectByALT(nullptr)
, IsDeletingLinkers(false)
, IsInConstructor(0)
, ConstructedObject(nullptr)
, AsyncPackage(nullptr)
{}


FUObjectSerializeContext::FUObjectSerializeContext()
	: RefCount(0)
	, ImportCount(0)
	, ForcedExportCount(0)
	, ObjBeginLoadCount(0)
	, SerializedObject(nullptr)
	, SerializedPackageLinker(nullptr)
	, SerializedImportIndex(0)
	, SerializedImportLinker(nullptr)
	, SerializedExportIndex(0)
	, SerializedExportLinker(nullptr)
{}

FUObjectSerializeContext::~FUObjectSerializeContext()
{
	checkf(!HasLoadedObjects(), TEXT("FUObjectSerializeContext is being destroyed but it still has pending loaded objects in its ObjectsLoaded list."));
}

int32 FUObjectSerializeContext::IncrementBeginLoadCount()
{
	return ++ObjBeginLoadCount;
}
int32 FUObjectSerializeContext::DecrementBeginLoadCount()
{
	check(HasStartedLoading());
	return --ObjBeginLoadCount;
}