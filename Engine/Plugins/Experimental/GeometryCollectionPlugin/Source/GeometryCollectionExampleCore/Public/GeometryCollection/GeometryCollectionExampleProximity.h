// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollection/GeometryCollectionExample.h"

namespace GeometryCollectionExample
{	
	
	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API BuildProximity(ExampleResponse&& R);
	
	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteFromStart(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteFromEnd(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteFromMiddle(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteMultipleFromMiddle(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteRandom(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteRandom2(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API GeometryDeleteAll(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API TestFracturedGeometry(ExampleResponse&& R);
	
}
