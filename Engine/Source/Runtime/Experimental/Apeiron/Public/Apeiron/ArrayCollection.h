// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Apeiron/Array.h"
#include "Apeiron/ArrayCollectionArrayBase.h"

namespace Apeiron
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
		if (Num == 0)
			return;
		Resize(MSize + Num);
	}

	void Resize(const int Num)
	{
		MSize = Num;
		for (auto Array : MArrays)
		{
			Array->Resize(Num);
		}
	}

	uint32 Size() const { return MSize; }

  private:
	TArray<TArrayCollectionArrayBase*> MArrays;

  protected:
	uint32 MSize;
};
}
