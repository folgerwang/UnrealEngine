// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollection/GeometryCollectionExample.h"

namespace GeometryCollectionExample
{	
	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Creation(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API ContiguousElementsTest(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API DeleteFromEnd(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API DeleteFromStart(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API DeleteFromMiddle(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API DeleteBranch(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API DeleteRootLeafMiddle(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API DeleteEverything(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API ParentTransformTest(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API ReindexMaterialsTest(ExampleResponse&& R);
}
