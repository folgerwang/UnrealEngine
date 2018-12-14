// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/ArrayCollectionArrayBase.h"

namespace Chaos
{
class TArrayCollection
{
  public:
	TArrayCollection()
	    : MSize(0) {}
	TArrayCollection(const TArrayCollection& Other) = delete;
	TArrayCollection(TArrayCollection&& Other) = delete;
	~TArrayCollection() {}

	int32 AddArray(TArrayCollectionArrayBase* Array)
	{
		int32 Index = MArrays.Num();
		MArrays.Add(Array);
		MArrays[Index]->Resize(MSize);
		return Index;
	}

	void AddElements(const int Num)
	{
		if(Num == 0)
		{
			return;
		}

		Resize(MSize + Num);
	}

	void Resize(const int Num)
	{
		MSize = Num;
		for (TArrayCollectionArrayBase* Array : MArrays)
		{
			Array->Resize(Num);
		}
	}

	void RemoveAt(const int Num, const int Count)
	{
		for(TArrayCollectionArrayBase* Array : MArrays)
		{
			Array->RemoveAt(Num, Count);
		}
	}

	uint32 Size() const { return MSize; }

  private:
	TArray<TArrayCollectionArrayBase*> MArrays;

  protected:
	uint32 MSize;
};
}
