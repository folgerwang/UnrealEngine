// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

class UObject;
class FMenuBuilder;

template<typename, typename> class TArray;

struct FLevelSequenceActionExtender
{
	virtual ~FLevelSequenceActionExtender() {}
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) = 0;
};