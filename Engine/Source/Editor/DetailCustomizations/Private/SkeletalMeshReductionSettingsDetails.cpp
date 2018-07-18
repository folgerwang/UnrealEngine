// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSkeletalMeshReductionSettingsDetails::GetVisibiltyIfCurrentReductionMethodIsNot, SMOT_MaxDeviation)))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("PercentTriangles", "Triangle Percentage"))
			.ToolTipText(LOCTEXT("PercentTriangles_ToolTip", "The simplification uses this percentage of source mesh's triangle count as a target."))
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

	StructBuilder.AddCustomRow(LOCTEXT("Accuracy_Row", "Accuracy Percentage"))
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FSkeletalMeshReductionSettingsDetails::GetVisibiltyIfCurrentReductionMethodIsNot, SMOT_NumOfTriangles)))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("PercentAccuracy", "Accuracy Percentage"))
			.ToolTipText(LOCTEXT("PercentAccuracy_ToolTip", "The simplification uses this as how much deviate from source mesh. Better works with hard surface meshes."))
		]
		.ValueContent()
		[
			SNew(SSpinBox<float>)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.MinValue(0.0f)
			// if you set 100% accuracy, which will set 0.f as max deviation, simplygon ignores the value. Considered invalid.
			.MaxValue(100.f) 
			.Value(this, &FSkeletalMeshReductionSettingsDetails::GetAccuracyPercentage)
			.OnValueChanged(this, &FSkeletalMeshReductionSettingsDetails::SetAccuracyPercentage)
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


float FSkeletalMeshReductionSettingsDetails::GetAccuracyPercentage() const
{
	float CurrentValue;
	if (MaxDeviationPercentagePropertyHandle->GetValue(CurrentValue) != FPropertyAccess::Fail)
	{
		return ConvertToPercentage(1.f-CurrentValue);
	}

	return 0.f;
}
void FSkeletalMeshReductionSettingsDetails::SetAccuracyPercentage(float Value)
{
	float PropertyValue = 1.f - ConvertToDecimal(Value);
	ensure(MaxDeviationPercentagePropertyHandle->SetValue(PropertyValue) != FPropertyAccess::Fail);
}

EVisibility FSkeletalMeshReductionSettingsDetails::GetVisibiltyIfCurrentReductionMethodIsNot(enum SkeletalMeshOptimizationType ReductionType) const
{
	uint8 CurrentEnum;
	if (ReductionMethodPropertyHandle->GetValue(CurrentEnum) != FPropertyAccess::Fail)
	{
		enum SkeletalMeshOptimizationType CurrentReductionType = (SkeletalMeshOptimizationType)CurrentEnum;
		if (CurrentReductionType != ReductionType)
		{
			return EVisibility::Visible;
		}
	}

	return EVisibility::Hidden;
}
#undef LOCTEXT_NAMESPACE
