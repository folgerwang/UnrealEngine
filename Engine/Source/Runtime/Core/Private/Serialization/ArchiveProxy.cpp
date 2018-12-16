// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Serialization/ArchiveProxy.h"

/*-----------------------------------------------------------------------------
	FArchiveProxy implementation.
-----------------------------------------------------------------------------*/

FArchiveProxy::FArchiveProxy(FArchive& InInnerArchive)
: FArchive    (InInnerArchive)
, InnerArchive(InInnerArchive)
{
}

