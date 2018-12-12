// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionExampleUtility.h"
#include "UObject/UObjectGlobals.h"

namespace GeometryCollectionExample {

	TSharedPtr<FGeometryCollection> CopyGeometryCollection(FGeometryCollection* InputCollection)
	{
		TSharedPtr<FGeometryCollection> NewCollection(new FGeometryCollection());
		NewCollection->Initialize(*InputCollection);
		NewCollection->LocalizeAttribute("Transform", FGeometryCollection::TransformGroup);
		return NewCollection;
	}
}

