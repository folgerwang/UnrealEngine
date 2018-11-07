// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "HighResScreenshot.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"
#include "Materials/Material.h"
#include "Slate/SceneViewport.h"
#include "ImageWriteQueue.h"

static TAutoConsoleVariable<int32> CVarSaveEXRCompressionQuality(
	TEXT("r.SaveEXR.CompressionQuality"),
	1,
	TEXT("Defines how we save HDR screenshots in the EXR format.\n")
	TEXT(" 0: no compression\n")
	TEXT(" 1: default compression which can be slow (default)"),
	ECVF_RenderThreadSafe);

DEFINE_LOG_CATEGORY(LogHighResScreenshot);

FHighResScreenshotConfig& GetHighResScreenshotConfig()
{
	static FHighResScreenshotConfig Instance;
	return Instance;
}

const float FHighResScreenshotConfig::MinResolutionMultipler = 1.0f;
const float FHighResScreenshotConfig::MaxResolutionMultipler = 10.0f;

FHighResScreenshotConfig::FHighResScreenshotConfig()
	: ResolutionMultiplier(FHighResScreenshotConfig::MinResolutionMultipler)
	, ResolutionMultiplierScale(0.0f)
	, bMaskEnabled(false)
	, bDumpBufferVisualizationTargets(false)
{
	ChangeViewport(TWeakPtr<FSceneViewport>());
	SetHDRCapture(false);
	SetForce128BitRendering(false);
}

void FHighResScreenshotConfig::Init()
{
	ImageWriteQueue = &FModuleManager::LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();

#if WITH_EDITOR
	HighResScreenshotMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/HighResScreenshot.HighResScreenshot"));
	HighResScreenshotMaskMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/HighResScreenshotMask.HighResScreenshotMask"));
	HighResScreenshotCaptureRegionMaterial = LoadObject<UMaterial>(NULL, TEXT("/Engine/EngineMaterials/HighResScreenshotCaptureRegion.HighResScreenshotCaptureRegion"));

	if (HighResScreenshotMaterial)
	{
		HighResScreenshotMaterial->AddToRoot();
	}
	if (HighResScreenshotMaskMaterial)
	{
		HighResScreenshotMaskMaterial->AddToRoot();
	}
	if (HighResScreenshotCaptureRegionMaterial)
	{
		HighResScreenshotCaptureRegionMaterial->AddToRoot();
	}
#endif
}

void FHighResScreenshotConfig::PopulateImageTaskParams(FImageWriteTask& InOutTask)
{
	static const TConsoleVariableData<int32>* CVarDumpFramesAsHDR = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.BufferVisualizationDumpFramesAsHDR"));

	const bool bCaptureHDREnabledInUI = bCaptureHDR && bDumpBufferVisualizationTargets;

	const bool bLocalCaptureHDR = bCaptureHDREnabledInUI || CVarDumpFramesAsHDR->GetValueOnAnyThread();

	InOutTask.Format = bLocalCaptureHDR ? EImageFormat::EXR : EImageFormat::PNG;

	InOutTask.CompressionQuality = (int32)EImageCompressionQuality::Default;
	if (bLocalCaptureHDR && CVarSaveEXRCompressionQuality.GetValueOnAnyThread() == 0)
	{
		InOutTask.CompressionQuality = (int32)EImageCompressionQuality::Uncompressed;
	}
}

void FHighResScreenshotConfig::ChangeViewport(TWeakPtr<FSceneViewport> InViewport)
{
	if (FSceneViewport* Viewport = TargetViewport.Pin().Get())
	{
		// Force an invalidate on the old viewport to make sure we clear away the capture region effect
		Viewport->Invalidate();
	}

	UnscaledCaptureRegion = FIntRect(0, 0, 0, 0);
	CaptureRegion = UnscaledCaptureRegion;
	bMaskEnabled = false;
	bDumpBufferVisualizationTargets = false;
	ResolutionMultiplier = FHighResScreenshotConfig::MinResolutionMultipler;
	ResolutionMultiplierScale = 0.0f;
	TargetViewport = InViewport;
}

bool FHighResScreenshotConfig::ParseConsoleCommand(const FString& InCmd, FOutputDevice& Ar)
{
	GScreenshotResolutionX = 0;
	GScreenshotResolutionY = 0;
	ResolutionMultiplier = FHighResScreenshotConfig::MinResolutionMultipler;
	ResolutionMultiplierScale = 0.0f;

	if( GetHighResScreenShotInput(*InCmd, Ar, GScreenshotResolutionX, GScreenshotResolutionY, ResolutionMultiplier, CaptureRegion, bMaskEnabled, bDumpBufferVisualizationTargets, bCaptureHDR, FilenameOverride) )
	{
		GScreenshotResolutionX *= ResolutionMultiplier;
		GScreenshotResolutionY *= ResolutionMultiplier;

		uint32 MaxTextureDimension = GetMax2DTextureDimension();

		// Check that we can actually create a destination texture of this size
		if ( GScreenshotResolutionX > MaxTextureDimension || GScreenshotResolutionY > MaxTextureDimension )
		{
			Ar.Logf(TEXT("Error: Screenshot size exceeds the maximum allowed texture size (%d x %d)"), GetMax2DTextureDimension(), GetMax2DTextureDimension());
			return false;
		}

		GIsHighResScreenshot = true;

		return true;
	}

	return false;
}

bool FHighResScreenshotConfig::MergeMaskIntoAlpha(TArray<FColor>& InBitmap)
{
	bool bWritten = false;

	if (bMaskEnabled)
	{
		// If this is a high resolution screenshot and we are using the masking feature,
		// Get the results of the mask rendering pass and insert into the alpha channel of the screenshot.
		TArray<FColor>* MaskArray = FScreenshotRequest::GetHighresScreenshotMaskColorArray();
		check(MaskArray->Num() == InBitmap.Num());
		for (int32 i = 0; i < MaskArray->Num(); ++i)
		{
			InBitmap[i].A = (*MaskArray)[i].R;
		}

		bWritten = true;
	}
	else
	{
		// Ensure that all pixels' alpha is set to 255
		for (auto& Color : InBitmap)
		{
			Color.A = 255;
		}
	}

	return bWritten;
}

void FHighResScreenshotConfig::SetHDRCapture(bool bCaptureHDRIN)
{
	bCaptureHDR = bCaptureHDRIN;
}

void FHighResScreenshotConfig::SetForce128BitRendering(bool bForce)
{
	bForce128BitRendering = bForce;
}

bool FHighResScreenshotConfig::SetResolution(uint32 ResolutionX, uint32 ResolutionY, float ResolutionScale)
{
	if ( ResolutionX > GetMax2DTextureDimension() || ResolutionY > GetMax2DTextureDimension() )
	{
		// TODO LOG
		//Ar.Logf(TEXT("Error: Screenshot size exceeds the maximum allowed texture size (%d x %d)"), GetMax2DTextureDimension(), GetMax2DTextureDimension());
		return false;
	}

	UnscaledCaptureRegion = FIntRect(0, 0, 0, 0);
	CaptureRegion = UnscaledCaptureRegion;
	bMaskEnabled = false;

	GScreenshotResolutionX = ResolutionX;
	GScreenshotResolutionY = ResolutionY;
	GIsHighResScreenshot = true;

	return true;
}