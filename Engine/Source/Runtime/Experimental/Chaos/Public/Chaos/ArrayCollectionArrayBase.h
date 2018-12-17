// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

namespace Chaos
{
class TArrayCollectionArrayBase
{
  public:
	virtual void Resize(const int Num) = 0;
	virtual void RemoveAt(const int Num, const int Count) = 0;
};
}
