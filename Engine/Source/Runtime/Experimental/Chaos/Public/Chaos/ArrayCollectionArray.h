// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/ArrayCollectionArrayBase.h"

#include <algorithm>

namespace Chaos
{
template<class T>
class TArrayCollectionArray : public TArrayCollectionArrayBase, public TArray<T>
{
	using TArray<T>::SetNum;
	using TArray<T>::RemoveAt;

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

	void RemoveAt(const int Num, const int Count) override
	{
		TArray<T>::RemoveAt(Num, Count);
	}
};

template<class T>
class TArrayCollectionArrayView : public TArrayCollectionArrayBase, public TArrayView<T>
{
  public:
	TArrayCollectionArrayView(T* BasePtr, const int32 Num)
	    : TArrayView<T>(BasePtr, Num) {}
	TArrayCollectionArrayView(const TArrayCollectionArrayView<T>& Other) = delete;
	TArrayCollectionArrayView(TArrayCollectionArrayView<T>&& Other)
	    : TArrayView<T>(MoveTemp(Other)) {}
	TArrayCollectionArrayView& operator=(TArrayCollectionArrayView<T>&& Other)
	{
		TArrayView<T>::operator=(MoveTemp(Other));
		return *this;
	}
	virtual ~TArrayCollectionArrayView() {}

	void Resize(const int Num) override
	{
		check(Num == TArrayView<T>::Num());
	}

	void RemoveAt(const int Num, const int Count) override
	{
		check(Count == 0);
	}
};
}
