// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "SkeletalMeshReductionSettings.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
struct EVisibility;

class FSkeletalMeshReductionSettingsDetails : public IPropertyTypeCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	// IPropertyTypeCustomization interface
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

private:
	float ConvertToPercentage(float Input) const;
	float ConvertToDecimal(float Input) const;

	TSharedPtr<IPropertyHandle> ReductionMethodPropertyHandle;
	TSharedPtr<IPropertyHandle> NumTrianglesPercentagePropertyHandle;
	TSharedPtr<IPropertyHandle> MaxDeviationPercentagePropertyHandle;

	float GetNumTrianglesPercentage() const;
	void SetNumTrianglesPercentage(float Value);

	float GetMaxDeviationPercentage() const;
	void SetMaxDeviationPercentage(float Value);

	EVisibility GetVisibiltyIfCurrentReductionMethod(SkeletalMeshOptimizationType ReductionType) const;
};
