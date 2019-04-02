// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MaterialInstanceConstant.cpp: MaterialInstanceConstant implementation.
=============================================================================*/

#include "Materials/MaterialInstanceConstant.h"
#if WITH_EDITOR
#include "MaterialEditor/DEditorScalarParameterValue.h"
#endif

UMaterialInstanceConstant::UMaterialInstanceConstant(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UMaterialInstanceConstant::PostLoad()
{
	LLM_SCOPE(ELLMTag::Materials);
	Super::PostLoad();
}

FLinearColor UMaterialInstanceConstant::K2_GetVectorParameterValue(FName ParameterName)
{
	FLinearColor Result(0,0,0);
	Super::GetVectorParameterValue(ParameterName, Result);
	return Result;
}

float UMaterialInstanceConstant::K2_GetScalarParameterValue(FName ParameterName)
{
	float Result = 0.f;
	Super::GetScalarParameterValue(ParameterName, Result);
	return Result;
}

UTexture* UMaterialInstanceConstant::K2_GetTextureParameterValue(FName ParameterName)
{
	UTexture* Result = NULL;
	Super::GetTextureParameterValue(ParameterName, Result);
	return Result;
}

#if WITH_EDITOR
void UMaterialInstanceConstant::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	ParameterStateId = FGuid::NewGuid();
}

void UMaterialInstanceConstant::SetParentEditorOnly(UMaterialInterface* NewParent)
{
	check(GIsEditor || IsRunningCommandlet());
	SetParentInternal(NewParent, true);
}

void UMaterialInstanceConstant::CopyMaterialUniformParametersEditorOnly(UMaterialInterface* Source, bool bIncludeStaticParams)
{
	CopyMaterialUniformParametersInternal(Source);

	if (bIncludeStaticParams && (Source != nullptr) && (Source != this))
	{
		if (UMaterialInstance* SourceMatInst = Cast<UMaterialInstance>(Source))
		{
			FStaticParameterSet SourceParamSet;
			SourceMatInst->GetStaticParameterValues(SourceParamSet);

			FStaticParameterSet MyParamSet;
			GetStaticParameterValues(MyParamSet);

			MyParamSet.StaticSwitchParameters = SourceParamSet.StaticSwitchParameters;

			UpdateStaticPermutation(MyParamSet);

			InitResources();
		}
	}
}

void UMaterialInstanceConstant::SetVectorParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, FLinearColor Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetVectorParameterValueInternal(ParameterInfo,Value);
}

void UMaterialInstanceConstant::SetScalarParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, float Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetScalarParameterValueInternal(ParameterInfo,Value);
}

void UMaterialInstanceConstant::SetScalarParameterAtlasEditorOnly(const FMaterialParameterInfo& ParameterInfo, FScalarParameterAtlasInstanceData AtlasData)
{
	check(GIsEditor || IsRunningCommandlet());
	SetScalarParameterAtlasInternal(ParameterInfo, AtlasData);
}

void UMaterialInstanceConstant::SetTextureParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo, UTexture* Value)
{
	check(GIsEditor || IsRunningCommandlet());
	SetTextureParameterValueInternal(ParameterInfo,Value);
}

void UMaterialInstanceConstant::SetFontParameterValueEditorOnly(const FMaterialParameterInfo& ParameterInfo,class UFont* FontValue,int32 FontPage)
{
	check(GIsEditor || IsRunningCommandlet());
	SetFontParameterValueInternal(ParameterInfo,FontValue,FontPage);
}

void UMaterialInstanceConstant::ClearParameterValuesEditorOnly()
{
	check(GIsEditor || IsRunningCommandlet());
	ClearParameterValuesInternal();
}
#endif // #if WITH_EDITOR
