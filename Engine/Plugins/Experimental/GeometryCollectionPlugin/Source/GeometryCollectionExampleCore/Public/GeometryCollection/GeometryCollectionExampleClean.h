// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollection/GeometryCollectionExample.h"

namespace GeometryCollectionExample
{	
	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API TestDeleteCoincidentVertices(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API TestDeleteCoincidentVertices2(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API TestDeleteZeroAreaFaces(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API TestDeleteHiddenFaces(ExampleResponse&& R);
}
