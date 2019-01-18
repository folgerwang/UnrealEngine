// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Internationalization/TextGeneratorBase.h"
#include "UObject/NameTypes.h"

FName FTextGeneratorBase::GetTypeID() const
{
	return NAME_None;
}

void FTextGeneratorBase::Serialize(FStructuredArchive::FRecord Record)
{
	checkf(false, TEXT("Serialization is not supported for this text generator type"));
}
