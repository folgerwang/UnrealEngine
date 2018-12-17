// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FGeometryCollection;
class UGeometryCollectionCache;

namespace GeometryCollectionEngineUtility
{

	void GEOMETRYCOLLECTIONENGINE_API PrintDetailedStatistics(const FGeometryCollection* GeometryCollection, const UGeometryCollectionCache* InCache);

}