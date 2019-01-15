// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimCurveCompressionCodec.h"

UAnimCurveCompressionCodec::UAnimCurveCompressionCodec(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITORONLY_DATA
void UAnimCurveCompressionCodec::PostInitProperties()
{
	Super::PostInitProperties();

	if (!IsTemplate())
	{
		InstanceGuid = FGuid::NewGuid();
	}
}

void UAnimCurveCompressionCodec::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	InstanceGuid = FGuid::NewGuid();
}

void UAnimCurveCompressionCodec::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!Ar.IsCooking())
	{
		Ar << InstanceGuid;
	}
}

void UAnimCurveCompressionCodec::PopulateDDCKey(FArchive& Ar)
{
	Ar << InstanceGuid;
}
#endif
