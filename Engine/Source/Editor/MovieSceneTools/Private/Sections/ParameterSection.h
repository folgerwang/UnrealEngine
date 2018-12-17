// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerSection.h"

/**
 * A movie scene section for material parameters.
 */
class FParameterSection
	: public FSequencerSection
{
public:

	FParameterSection(UMovieSceneSection& InSectionObject)
		: FSequencerSection(InSectionObject)
	{ }

public:

	//~ ISequencerSection interface

	virtual bool RequestDeleteCategory(const TArray<FName>& CategoryNamePath) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;
};
