// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshReductionSettingsDetails.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSpinBox.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "SkeletalMeshReductionSettings.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshReductionSettingsDetails"

TSharedRef<IPropertyTypeCustomization> FSkeletalMeshReductionSettingsDetails::MakeInstance()
{
	return MakeShareable(new FSkeletalMeshReductionSettingsDetails);
}

void FSkeletalMeshReductionSettingsDetails::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];
}
void FSkeletalMeshReductionSettingsDetails::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// here, we have to keep track of customizing properties, so that we don't display twice
	const TArray<FName> CustomizedProperties = { GET_MEMBER_NAME_CHECKED(FSkeletalMeshOptimizationSettings, ReductionMethod), GET_MEMBER_NAME_CHECKED(FSkeletalMeshOptimizationSettings, NumOfTrianglesPercentage), 
		GET_MEMBER_NAME_CHECKED(FSkeletalMeshOptimizationSettings, MaxDeviationPercentage)};

	ReductionMethodPropertyHandle = StructPropertyHandle->GetChildHandle(CustomizedProperties[0]);
	NumTrianglesPercentagePropertyHandle = StructPropertyHandle->GetChildHandle(CustomizedProperties[1]);
	MaxDeviationPercentagePropertyHandle = StructPropertyHandle->GetChildHandle(CustomizedProperties[2]);

	StructBuilder.AddProperty(ReductionMethodPropertyHandle.ToSharedRef());

	StructBuilder.AddCustomRow(LOCTEXT("PercentTriangles_Row", "Triangle Percentage"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSkeletalMeshReductionSettingsDetails::GetVisibiltyIfCurrentReductionMethod, SMOT_NumOfTriangles)))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("PercentTriangles", "Percentage of Triangles"))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.MinValue(0.0f)
			.MaxValue(100.0f)
			.Value(this, &FSkeletalMeshReductionSettingsDetails::GetNumTrianglesPercentage)
			.OnValueChanged(this, &FSkeletalMeshReductionSettingsDetails::SetNumTrianglesPercentage)
		];

	StructBuilder.AddCustomRow(LOCTEXT("MaxDeviation_Row", "MaxDeviation Percentage"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSkeletalMeshReductionSettingsDetails::GetVisibiltyIfCurrentReductionMethod, SMOT_MaxDeviation)))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("MaxDeviation", "Percentage of Max Deviation"))
			.ToolTipText(LOCTEXT("MaxDeviation_ToolTip", "100% means it will deviate as much as mesh's bound radius."))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			// if you're choosing reductionmethod to be max deviation, and if your MaxDeviation % is 0.f, that means you don't really reduce
			// so we set min to be 1.0f, that means 1%, and translate to 0.01 for MaxDeviationPercentage value
			.MinValue(1.0f)
			.MaxValue(100.0f)
			.Value(this, &FSkeletalMeshReductionSettingsDetails::GetMaxDeviationPercentage)
			.OnValueChanged(this, &FSkeletalMeshReductionSettingsDetails::SetMaxDeviationPercentage)
		];

	uint32 NumChildren = 0;
	
	if (StructPropertyHandle->GetNumChildren(NumChildren) != FPropertyAccess::Fail)
	{
		for (uint32 Index = 0; Index < NumChildren; ++Index)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(Index);
			// we don't want to add the things that we added first
			// maybe we make array later if we have a lot. 
			if (!CustomizedProperties.Contains(ChildHandle->GetProperty()->GetFName()))
			{
				StructBuilder.AddProperty(ChildHandle.ToSharedRef());
			}
		}
	}
}

float FSkeletalMeshReductionSettingsDetails::ConvertToPercentage(float Input) const
{
	return FMath::Clamp(Input*100.f, 0.f, 100.f);
}

float FSkeletalMeshReductionSettingsDetails::ConvertToDecimal(float Input) const
{
	return FMath::Clamp(Input / 100.f, 0.f, 1.f);
}

float FSkeletalMeshReductionSettingsDetails::GetNumTrianglesPercentage() const
{
	float CurrentValue;
	if (NumTrianglesPercentagePropertyHandle->GetValue(CurrentValue) != FPropertyAccess::Fail)
	{
		return ConvertToPercentage(CurrentValue);
	}

	return 0.f;
}

void FSkeletalMeshReductionSettingsDetails::SetNumTrianglesPercentage(float Value)
{
	float PropertyValue = ConvertToDecimal(Value);
	ensure(NumTrianglesPercentagePropertyHandle->SetValue(PropertyValue) != FPropertyAccess::Fail);
}

float FSkeletalMeshReductionSettingsDetails::GetMaxDeviationPercentage() const
{
	float CurrentValue;
	if (MaxDeviationPercentagePropertyHandle->GetValue(CurrentValue) != FPropertyAccess::Fail)
	{
		return ConvertToPercentage(CurrentValue);
	}

	return 0.f;
}
void FSkeletalMeshReductionSettingsDetails::SetMaxDeviationPercentage(float Value)
{
	float PropertyValue = ConvertToDecimal(Value);
	ensure(MaxDeviationPercentagePropertyHandle->SetValue(PropertyValue) != FPropertyAccess::Fail);
}

EVisibility FSkeletalMeshReductionSettingsDetails::GetVisibiltyIfCurrentReductionMethod(enum SkeletalMeshOptimizationType ReductionType) const
{
	uint8 CurrentEnum;
	if (ReductionMethodPropertyHandle->GetValue(CurrentEnum) != FPropertyAccess::Fail)
	{
		if ((SkeletalMeshOptimizationType)CurrentEnum == ReductionType)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}
#undef LOCTEXT_NAMESPACE
