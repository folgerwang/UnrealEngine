// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PropertyValueColor.h"

#include "CoreMinimal.h"
#include "Components/LightComponent.h"
#include "Atmosphere/AtmosphericFogComponent.h"
#include "HAL/UnrealMemory.h"
#include "VariantObjectBinding.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "PropertyValueColor"

UPropertyValueColor::UPropertyValueColor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UPropertyValueColor::RecordDataFromResolvedObject()
{
	if (!Resolve())
	{
		return;
	}

	// Used by ULightComponent
	if (PropertySetterName == TEXT("SetLightColor"))
	{
		ULightComponent* ContainerObject = (ULightComponent*) ParentContainerAddress;
		if (!ContainerObject || !ContainerObject->IsValidLowLevel())
		{
			UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueColor '%s' does not have a ULightComponent as parent address!"), *GetFullDisplayString());
			return;
		}

		FLinearColor Col = ContainerObject->GetLightColor();

		int32 PropertySizeBytes = GetValueSizeInBytes();
		SetRecordedData((uint8*)&Col, PropertySizeBytes);
	}
	// Used by UAtmosphericFogComponent
	else if (PropertySetterName == TEXT("SetDefaultLightColor"))
	{
		UAtmosphericFogComponent* ContainerObject = (UAtmosphericFogComponent*) ParentContainerAddress;
		if (!ContainerObject || !ContainerObject->IsValidLowLevel())
		{
			UE_LOG(LogVariantContent, Error, TEXT("UPropertyValueColor '%s' does not have a UAtmosphericFogComponent as parent address!"), *GetFullDisplayString());
			return;
		}

		FLinearColor Col = FLinearColor(ContainerObject->DefaultLightColor);

		int32 PropertySizeBytes = GetValueSizeInBytes();
		SetRecordedData((uint8*)&Col, PropertySizeBytes);
	}

	OnPropertyRecorded.Broadcast();
}

UScriptStruct* UPropertyValueColor::GetStructPropertyStruct() const
{
	static UPackage* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	static UScriptStruct* LinearColorScriptStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("LinearColor"));
	return LinearColorScriptStruct;
}

int32 UPropertyValueColor::GetValueSizeInBytes() const
{
	return sizeof(FLinearColor);
}

#undef LOCTEXT_NAMESPACE
