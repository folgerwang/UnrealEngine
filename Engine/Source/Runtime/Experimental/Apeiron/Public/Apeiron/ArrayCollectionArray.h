// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/ArrayCollectionArrayBase.h"

#include <algorithm>

namespace Apeiron
{
template<class T>
class TArrayCollectionArray : public TArrayCollectionArrayBase, public TArray<T>
{
	using TArray<T>::SetNum;

  public:
	TArrayCollectionArray()
	    : TArray<T>() {}
	TArrayCollectionArray(const TArrayCollectionArray<T>& Other) = delete;
	TArrayCollectionArray(TArrayCollectionArray<T>&& Other)
	    : TArray<T>(MoveTemp(Other)) {}
	TArrayCollectionArray& operator=(TArrayCollectionArray<T>&& Other)
	{
		TArray<T>::operator=(MoveTemp(Other));
		return *this;
	}
	virtual ~TArrayCollectionArray() {}

	TArrayCollectionArray<T> Clone()
	{
		TArrayCollectionArray<T> NewArray;
		static_cast<TArray<T>>(NewArray) = static_cast<TArray<T>>(*this);
		return NewArray;
	}

	void Resize(const int Num) override
	{
		SetNum(Num);
	}
};
}
