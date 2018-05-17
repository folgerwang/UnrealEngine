// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LocalLightComponentDetails.h"
#include "Components/LocalLightComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

#define LOCTEXT_NAMESPACE "LocalLightComponentDetails"

TSharedRef<IDetailCustomization> FLocalLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FLocalLightComponentDetails );
}

void FLocalLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TSharedPtr<IPropertyHandle> LightIntensityProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULightComponentBase, Intensity), ULightComponentBase::StaticClass());
	TSharedPtr<IPropertyHandle> IntensityUnitsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULocalLightComponent, IntensityUnits), ULocalLightComponent::StaticClass());

	float ConversionFactor = 1.f;
	uint8 Value = 0; // Unitless
	if (IntensityUnitsProperty->GetValue(Value) == FPropertyAccess::Success)
	{
		ConversionFactor = ULocalLightComponent::GetUnitsConversionFactor((ELightUnits)0, (ELightUnits)Value);
	}
	IntensityUnitsProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FLocalLightComponentDetails::OnIntensityUnitsChanged));
	
	// Inverse squared falloff point lights (the default) are in units of lumens, instead of just being a brightness scale
	LightIntensityProperty->SetInstanceMetaData("UIMin",TEXT("0.0f"));
	LightIntensityProperty->SetInstanceMetaData("UIMax",  *FString::SanitizeFloat(100000.0f * ConversionFactor));
	LightIntensityProperty->SetInstanceMetaData("SliderExponent", TEXT("2.0f"));

	// Make these come first
	IDetailCategoryBuilder& LightCategory = DetailBuilder.EditCategory( "Light", FText::GetEmpty(), ECategoryPriority::TypeSpecific );
	LightCategory.AddProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(ULocalLightComponent, IntensityUnits), ULocalLightComponent::StaticClass() ) );
	LightCategory.AddProperty( DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED(ULocalLightComponent, AttenuationRadius), ULocalLightComponent::StaticClass() ) );
}

void FLocalLightComponentDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
	CachedDetailBuilder = DetailBuilder;
	CustomizeDetails(*DetailBuilder);
}

void FLocalLightComponentDetails::OnIntensityUnitsChanged()
{
	// Here we can only take the ptr as ForceRefreshDetails() checks that the reference is unique.
	IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get();
	if (DetailBuilder)
	{
		DetailBuilder->ForceRefreshDetails();
	}
}

#undef LOCTEXT_NAMESPACE
