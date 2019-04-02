// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusMR_Settings.h"
#include "OculusMRPrivate.h"
#include "OculusHMD.h"
#include "Engine/Engine.h"

UOculusMR_Settings::UOculusMR_Settings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)	
	, ClippingReference(EOculusMR_ClippingReference::CR_Head)
	, bUseTrackedCameraResolution(true)
	, WidthPerView(960)
	, HeightPerView(540)
	, CastingLatency(0.0f)
	, HandPoseStateLatency(0.0f)
	, ChromaKeyColor(FColor::Green)
	, ChromaKeySimilarity(0.6f)
	, ChromaKeySmoothRange(0.03f)
	, ChromaKeySpillRange(0.04f)
	, VirtualGreenScreenType(EOculusMR_VirtualGreenScreenType::VGS_Off)
	, DynamicLightingDepthSmoothFactor(8.0f)
	, DynamicLightingDepthVariationClampingValue(0.001f)
	, ExternalCompositionPostProcessEffects(EOculusMR_PostProcessEffects::PPE_Off)
	, bIsCasting(false)
	, CompositionMethod(EOculusMR_CompositionMethod::ExternalComposition)
	, CapturingCamera(EOculusMR_CameraDeviceEnum::CD_WebCamera0)
	, bUseDynamicLighting(false)
	, DepthQuality(EOculusMR_DepthQuality::DQ_Medium)
	, BindToTrackedCameraIndex(-1)
{}

void UOculusMR_Settings::SetCompositionMethod(EOculusMR_CompositionMethod val)
{
	if (CompositionMethod == val)
	{
		return;
	}
	auto old = CompositionMethod;
	CompositionMethod = val;
	CompositionMethodChangeDelegate.Execute(old, val);
}

void UOculusMR_Settings::SetCapturingCamera(EOculusMR_CameraDeviceEnum val)
{
	if (CapturingCamera == val)
	{
		return;
	}
	auto old = CapturingCamera;
	CapturingCamera = val;
	CapturingCameraChangeDelegate.Execute(old, val);
}

void UOculusMR_Settings::SetIsCasting(bool val)
{
	if (bIsCasting == val)
	{
		return;
	}
	auto old = bIsCasting;
	bIsCasting = val;
	IsCastingChangeDelegate.Execute(old, val);
}

void UOculusMR_Settings::SetUseDynamicLighting(bool val)
{
	if (bUseDynamicLighting == val)
	{
		return;
	}
	auto old = bUseDynamicLighting;
	bUseDynamicLighting = val;
	UseDynamicLightingChangeDelegate.Execute(old, val);
}

void UOculusMR_Settings::SetDepthQuality(EOculusMR_DepthQuality val)
{
	if (DepthQuality == val)
	{
		return;
	}
	auto old = DepthQuality;
	DepthQuality = val;
	DepthQualityChangeDelegate.Execute(old, val);
}

void UOculusMR_Settings::BindToTrackedCameraIndexIfAvailable(int InTrackedCameraIndex)
{
	if (BindToTrackedCameraIndex == InTrackedCameraIndex)
	{
		return;
	}
	auto old = BindToTrackedCameraIndex;
	BindToTrackedCameraIndex = InTrackedCameraIndex;
	TrackedCameraIndexChangeDelegate.Execute(old, InTrackedCameraIndex);
}

void UOculusMR_Settings::LoadFromIni()
{
	if (!GConfig)
	{
		UE_LOG(LogMR, Warning, TEXT("GConfig is NULL"));
		return;
	}

	// Flushing the GEngineIni is necessary to get the settings reloaded at the runtime, but the manual flushing
	// could cause an assert when loading audio settings if launching through editor at the 2nd time. Disabled temporarily.
	//GConfig->Flush(true, GEngineIni);

	const TCHAR* OculusMRSettings = TEXT("Oculus.Settings.MixedReality");
	bool v;
	float f;
	int32 i;
	FVector vec;
	FColor color;
	if (GConfig->GetInt(OculusMRSettings, TEXT("CompositionMethod"), i, GEngineIni))
	{
		SetCompositionMethod((EOculusMR_CompositionMethod)i);
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("ClippingReference"), i, GEngineIni))
	{
		ClippingReference = (EOculusMR_ClippingReference)i;
	}
	if (GConfig->GetBool(OculusMRSettings, TEXT("bUseTrackedCameraResolution"), v, GEngineIni))
	{
		bUseTrackedCameraResolution = v;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("WidthPerView"), i, GEngineIni))
	{
		WidthPerView = i;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("HeightPerView"), i, GEngineIni))
	{
		HeightPerView = i;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("CapturingCamera"), i, GEngineIni))
	{
		CapturingCamera = (EOculusMR_CameraDeviceEnum)i;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("CastingLatency"), f, GEngineIni))
	{
		CastingLatency = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("HandPoseStateLatency"), f, GEngineIni))
	{
		HandPoseStateLatency = f;
	}
	if (GConfig->GetColor(OculusMRSettings, TEXT("ChromaKeyColor"), color, GEngineIni))
	{
		ChromaKeyColor = color;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("ChromaKeySimilarity"), f, GEngineIni))
	{
		ChromaKeySimilarity = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("ChromaKeySmoothRange"), f, GEngineIni))
	{
		ChromaKeySmoothRange = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("ChromaKeySpillRange"), f, GEngineIni))
	{
		ChromaKeySpillRange = f;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("VirtualGreenScreenType"), i, GEngineIni))
	{
		VirtualGreenScreenType = (EOculusMR_VirtualGreenScreenType)i;
	}
	if (GConfig->GetBool(OculusMRSettings, TEXT("bUseDynamicLighting"), v, GEngineIni))
	{
		SetUseDynamicLighting(v);
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("DepthQuality"), i, GEngineIni))
	{
		SetDepthQuality((EOculusMR_DepthQuality)i);
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("DynamicLightingDepthSmoothFactor"), f, GEngineIni))
	{
		DynamicLightingDepthSmoothFactor = f;
	}
	if (GConfig->GetFloat(OculusMRSettings, TEXT("DynamicLightingDepthVariationClampingValue"), f, GEngineIni))
	{
		DynamicLightingDepthVariationClampingValue = f;
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("BindToTrackedCameraIndex"), i, GEngineIni))
	{
		BindToTrackedCameraIndexIfAvailable(i);
	}
	if (GConfig->GetInt(OculusMRSettings, TEXT("ExternalCompositionPostProcessEffects"), i, GEngineIni))
	{
		ExternalCompositionPostProcessEffects = (EOculusMR_PostProcessEffects)i;
	}

	UE_LOG(LogMR, Log, TEXT("MixedReality settings loaded from Engine.ini"));
}

void UOculusMR_Settings::SaveToIni() const
{
	if (!GConfig)
	{
		UE_LOG(LogMR, Warning, TEXT("GConfig is NULL"));
		return;
	}

	const TCHAR* OculusMRSettings = TEXT("Oculus.Settings.MixedReality");
	GConfig->SetInt(OculusMRSettings, TEXT("CompositionMethod"), (int32)CompositionMethod, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("ClippingReference"), (int32)ClippingReference, GEngineIni);
	GConfig->SetBool(OculusMRSettings, TEXT("bUseTrackedCameraResolution"), bUseTrackedCameraResolution, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("WidthPerView"), WidthPerView, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("HeightPerView"), HeightPerView, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("CapturingCamera"), (int32)CapturingCamera, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("CastingLatency"), CastingLatency, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("HandPoseStateLatency"), HandPoseStateLatency, GEngineIni);
	GConfig->SetColor(OculusMRSettings, TEXT("ChromaKeyColor"), ChromaKeyColor, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("ChromaKeySimilarity"), ChromaKeySimilarity, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("ChromaKeySmoothRange"), ChromaKeySmoothRange, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("ChromaKeySpillRange"), ChromaKeySpillRange, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("VirtualGreenScreenType"), (int32)VirtualGreenScreenType, GEngineIni);
	GConfig->SetBool(OculusMRSettings, TEXT("bUseDynamicLighting"), bUseDynamicLighting, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("DepthQuality"), (int32)DepthQuality, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("DynamicLightingDepthSmoothFactor"), DynamicLightingDepthSmoothFactor, GEngineIni);
	GConfig->SetFloat(OculusMRSettings, TEXT("DynamicLightingDepthVariationClampingValue"), DynamicLightingDepthVariationClampingValue, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("BindToTrackedCameraIndex"), (int32)BindToTrackedCameraIndex, GEngineIni);
	GConfig->SetInt(OculusMRSettings, TEXT("ExternalCompositionPostProcessEffects"), (int32)ExternalCompositionPostProcessEffects, GEngineIni);

	GConfig->Flush(false, GEngineIni);

	UE_LOG(LogMR, Log, TEXT("MixedReality settings saved to Engine.ini"));
}
