// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"

namespace Apeiron
{
template<class T>
class VectorND : public TArray<T>
{
  public:
	using TArray<T>::SetNum;

	VectorND(const int32 Size)
	{
		SetNum(Size);
	}
};
}
