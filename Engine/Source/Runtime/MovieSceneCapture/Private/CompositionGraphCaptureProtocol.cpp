// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Protocols/CompositionGraphCaptureProtocol.h"
#include "Misc/CommandLine.h"
#include "HAL/IConsoleManager.h"
#include "EngineGlobals.h"
#include "Engine/Scene.h"
#include "Materials/MaterialInterface.h"
#include "SceneView.h"
#include "Engine/Engine.h"
#include "SceneViewExtension.h"
#include "Materials/Material.h"
#include "BufferVisualizationData.h"
#include "MovieSceneCaptureSettings.h"

struct FFrameCaptureViewExtension : public FSceneViewExtensionBase
{
	FFrameCaptureViewExtension( const FAutoRegister& AutoRegister, const TArray<FString>& InRenderPasses, bool bInCaptureFramesInHDR, int32 InHDRCompressionQuality, int32 InCaptureGamut, UMaterialInterface* InPostProcessingMaterial, bool bInDisableScreenPercentage)
		: FSceneViewExtensionBase(AutoRegister)
		, RenderPasses(InRenderPasses)
		, bNeedsCapture(false)
		, bCaptureFramesInHDR(bInCaptureFramesInHDR)
		, HDRCompressionQuality(InHDRCompressionQuality)
		, CaptureGamut(InCaptureGamut)
		, PostProcessingMaterial(InPostProcessingMaterial)
		, bDisableScreenPercentage(bInDisableScreenPercentage)
	{
		CVarDumpFrames = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFrames"));
		CVarDumpFramesAsHDR = IConsoleManager::Get().FindConsoleVariable(TEXT("r.BufferVisualizationDumpFramesAsHDR"));
		CVarHDRCompressionQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SaveEXR.CompressionQuality"));
		CVarDumpGamut = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.ColorGamut"));
		CVarDumpDevice = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HDR.Display.OutputDevice"));

		if (CaptureGamut == HCGM_Linear)
		{
			CVarDumpGamut->Set(1);
			CVarDumpDevice->Set(7);
		}
		else
		{
			CVarDumpGamut->Set(CaptureGamut);
		}

		RestoreDumpHDR = CVarDumpFramesAsHDR->GetInt();
		RestoreHDRCompressionQuality = CVarHDRCompressionQuality->GetInt();
		RestoreDumpGamut = CVarDumpGamut->GetInt();
		RestoreDumpDevice = CVarDumpDevice->GetInt();

		Disable();
	}

	virtual ~FFrameCaptureViewExtension()
	{
		Disable();

		CVarDumpGamut->Set(RestoreDumpGamut);
		CVarDumpDevice->Set(RestoreDumpDevice);
	}

	bool IsEnabled() const
	{
		return bNeedsCapture;
	}

	void Enable(FString&& InFilename)
	{
		OutputFilename = MoveTemp(InFilename);

		bNeedsCapture = true;

		CVarDumpFrames->Set(1);
		CVarDumpFramesAsHDR->Set(bCaptureFramesInHDR);
		CVarHDRCompressionQuality->Set(HDRCompressionQuality);
	}

	void Disable(bool bFinalize = false)
	{
		if (bNeedsCapture || bFinalize)
		{
			bNeedsCapture = false;
			if (bFinalize)
			{
				RestoreDumpHDR = 0;
				RestoreHDRCompressionQuality = 0;
			}
			CVarDumpFramesAsHDR->Set(RestoreDumpHDR);
			CVarHDRCompressionQuality->Set(RestoreHDRCompressionQuality);
			CVarDumpFrames->Set(0);
		}
	}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
	{
		if (!bNeedsCapture)
		{
			return;
		}

		InView.FinalPostProcessSettings.bBufferVisualizationDumpRequired = true;
		InView.FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Empty();
		InView.FinalPostProcessSettings.BufferVisualizationDumpBaseFilename = MoveTemp(OutputFilename);

		struct FIterator
		{
			FFinalPostProcessSettings& FinalPostProcessSettings;
			const TArray<FString>& RenderPasses;

			FIterator(FFinalPostProcessSettings& InFinalPostProcessSettings, const TArray<FString>& InRenderPasses)
				: FinalPostProcessSettings(InFinalPostProcessSettings), RenderPasses(InRenderPasses)
			{}

			void ProcessValue(const FString& InName, UMaterial* Material, const FText& InText)
			{
				if (!RenderPasses.Num() || RenderPasses.Contains(InName) || RenderPasses.Contains(InText.ToString()))
				{
					FinalPostProcessSettings.BufferVisualizationOverviewMaterials.Add(Material);
				}
			}
		} Iterator(InView.FinalPostProcessSettings, RenderPasses);
		GetBufferVisualizationData().IterateOverAvailableMaterials(Iterator);

		if (PostProcessingMaterial)
		{
			FWeightedBlendable Blendable(1.f, PostProcessingMaterial);
			PostProcessingMaterial->OverrideBlendableSettings(InView, 1.f);
		}

		bNeedsCapture = false;
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{
		if (bDisableScreenPercentage)
		{
			// Ensure we're rendering at full size.
			InViewFamily.EngineShowFlags.ScreenPercentage = false;
		}
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {}

	virtual bool IsActiveThisFrame(class FViewport* InViewport) const override { return IsEnabled(); }

private:
	const TArray<FString>& RenderPasses;

	bool bNeedsCapture;
	FString OutputFilename;

	bool bCaptureFramesInHDR;
	int32 HDRCompressionQuality;
	int32 CaptureGamut;

	UMaterialInterface* PostProcessingMaterial;

	bool bDisableScreenPercentage;

	IConsoleVariable* CVarDumpFrames;
	IConsoleVariable* CVarDumpFramesAsHDR;
	IConsoleVariable* CVarHDRCompressionQuality;
	IConsoleVariable* CVarDumpGamut;
	IConsoleVariable* CVarDumpDevice;

	int32 RestoreDumpHDR;
	int32 RestoreHDRCompressionQuality;
	int32 RestoreDumpGamut;
	int32 RestoreDumpDevice;
};

bool UCompositionGraphCaptureProtocol::SetupImpl()
{
	SceneViewport = InitSettings->SceneViewport;

	FString OverrideRenderPasses;
	if (FParse::Value(FCommandLine::Get(), TEXT("-CustomRenderPasses="), OverrideRenderPasses, /*bShouldStopOnSeparator*/ false))
	{
		OverrideRenderPasses.ParseIntoArray(IncludeRenderPasses.Value, TEXT(","), true);
	}

	int32 OverrideCaptureGamut = (int32)CaptureGamut;
	FParse::Value(FCommandLine::Get(), TEXT("-CaptureGamut="), OverrideCaptureGamut);
	FParse::Value(FCommandLine::Get(), TEXT( "-HDRCompressionQuality=" ), HDRCompressionQuality);
	FParse::Bool(FCommandLine::Get(), TEXT("-CaptureFramesInHDR="), bCaptureFramesInHDR);
	FParse::Bool(FCommandLine::Get(), TEXT("-DisableScreenPercentage="), bDisableScreenPercentage);

	PostProcessingMaterialPtr = Cast<UMaterialInterface>(PostProcessingMaterial.TryLoad());
	ViewExtension = FSceneViewExtensions::NewExtension<FFrameCaptureViewExtension>(IncludeRenderPasses.Value, bCaptureFramesInHDR, HDRCompressionQuality, OverrideCaptureGamut, PostProcessingMaterialPtr, bDisableScreenPercentage);

	return true;
}

void UCompositionGraphCaptureProtocol::OnReleaseConfigImpl(FMovieSceneCaptureSettings& InSettings)
{
	// Remove {material} if it exists
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT("{material}"), TEXT(""));

	// Remove .{frame} if it exists
	InSettings.OutputFormat = InSettings.OutputFormat.Replace(TEXT(".{frame}"), TEXT(""));
}

void UCompositionGraphCaptureProtocol::OnLoadConfigImpl(FMovieSceneCaptureSettings& InSettings)
{
	// Add .{frame} if it doesn't already exist
	FString OutputFormat = InSettings.OutputFormat;

	// Ensure the format string tries to always export a uniquely named frame so the file doesn't overwrite itself if the user doesn't add it.
	bool bHasFrameFormat = OutputFormat.Contains(TEXT("{frame}")) || OutputFormat.Contains(TEXT("{shot_frame}"));
	if (!bHasFrameFormat)
	{
		OutputFormat.Append(TEXT(".{frame}"));

		InSettings.OutputFormat = OutputFormat;
		UE_LOG(LogTemp, Warning, TEXT("Automatically appended .{frame} to the format string as specified format string did not provide a way to differentiate between frames via {frame} or {shot_frame}!"));
	}

	// Add {material} if it doesn't already exist
	if (!OutputFormat.Contains(TEXT("{material}")))
	{
		int32 FramePosition = OutputFormat.Find(TEXT(".{frame}"));
		if (FramePosition != INDEX_NONE)
		{
			OutputFormat.InsertAt(FramePosition, TEXT("{material}"));
		}
		else
		{
			OutputFormat.Append(TEXT("{material}"));
		}

		InSettings.OutputFormat = OutputFormat;
	}
}

void UCompositionGraphCaptureProtocol::FinalizeImpl()
{
	ViewExtension->Disable(true);
}

void UCompositionGraphCaptureProtocol::CaptureFrameImpl(const FFrameMetrics& FrameMetrics)
{
	ViewExtension->Enable(GenerateFilenameImpl(FrameMetrics, TEXT("")));
}

bool UCompositionGraphCaptureProtocol::HasFinishedProcessingImpl() const
{
	return !ViewExtension->IsEnabled();
}

void UCompositionGraphCaptureProtocol::TickImpl()
{
	// If the extension is not enabled, ensure all the CVars have been reset on tick
	if (ViewExtension.IsValid() && !ViewExtension->IsEnabled())
	{
		ViewExtension->Disable();
	}
}
