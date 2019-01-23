// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyValue.h"

#include "PropertyValueMaterial.generated.h"

class UMaterialInterface;

UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueMaterial : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

public:

	UMaterialInterface* GetMaterial();
	void SetMaterial(UMaterialInterface* Mat);

	// Our leaf property will always be OverrideMaterials/OverrideMaterials[0] just for the
	// type/size/class information. It will normally fail to resolve if the StaticMeshComponent
	// is just using default materials though, so we have to intercept resolve calls and handle
	// them in a specific way. This will also let us zero out the value ptr and other things
	// that shouldn't be used by themselves
	virtual bool Resolve(UObject* OnObject = nullptr) override;

	virtual UStruct* GetPropertyParentContainerClass() const override;

	virtual void RecordDataFromResolvedObject() override;
	virtual void ApplyDataToResolvedObject() override;

	virtual UClass* GetPropertyClass() const override;
	virtual UClass* GetObjectPropertyObjectClass() const override;

	virtual int32 GetValueSizeInBytes() const override;
};