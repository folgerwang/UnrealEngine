// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GeometryCollection/GeometryCollectionExample.h"

namespace GeometryCollectionExample
{	
	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_RadialIntMask(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_RadialFalloff(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_UniformVector(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_RaidalVector(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumVectorFullMult(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumVectorFullDiv(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumVectorFullAdd(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumVectorFullSub(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumVectorLeftSide(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumVectorRightSide(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumScalar(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumScalarRightSide(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_SumScalarLeftSide(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_ContextOverrides(ExampleResponse&& R);

	template<class T>
	bool GEOMETRYCOLLECTIONEXAMPLECORE_API Fields_DefaultRadialFalloff(ExampleResponse&& R);
}
