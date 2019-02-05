// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "HAL/CriticalSection.h"
#include "MediaOutput.h"
#include "Misc/Timecode.h"
#include "PixelFormat.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Templates/Atomic.h"

#include "MediaCapture.generated.h"

class FSceneViewport;
class UTextureRenderTarget2D;

/**
 * Possible states of media capture.
 */
UENUM()
enum class EMediaCaptureState
{
	/** Unrecoverable error occurred during capture. */
	Error,

	/** Media is currently capturing. */
	Capturing,

	/** Media is being prepared for capturing. */
	Preparing,

	/** Capture has been stopped but some frames may need to be process. */
	StopRequested,

	/** Capture has been stopped. */
	Stopped,
};

/**
 * Base class of additional data that can be stored for each requested capture.
 */
class FMediaCaptureUserData
{};

/**
 * Type of cropping 
 */
UENUM()
enum class EMediaCaptureCroppingType : uint8
{
	/** Do not crop the captured image. */
	None,
	/** Keep the center of the captured image. */
	Center,
	/** Keep the top left corner of the captured image. */
	TopLeft,
	/** Use the StartCapturePoint and the size of the MediaOutput to keep of the captured image. */
	Custom,
};

/**
 * Base class of additional data that can be stored for each requested capture.
 */
USTRUCT(BlueprintType)
struct MEDIAIOCORE_API FMediaCaptureOptions
{
	GENERATED_BODY()

	FMediaCaptureOptions();

public:
	/** Crop the captured SceneViewport or TextureRenderTarget2D to the desired size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="MediaCapture")
	EMediaCaptureCroppingType Crop;

	/**
	 * Crop the captured SceneViewport or TextureRenderTarget2D to the desired size.
	 * @note Only valid when Crop is set to Custom.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MediaCapture")
	FIntPoint CustomCapturePoint;
};

/**
 * Abstract base class for media capture.
 *
 * MediaCapture capture the texture of the Render target or the SceneViewport and sends it to an external media device.
 * MediaCapture should be created by a MediaOutput.
 */
UCLASS(Abstract, editinlinenew, BlueprintType, hidecategories = (Object))
class MEDIAIOCORE_API UMediaCapture : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	/**
	 * Stop the actual capture if there is one.
	 * Then start the capture of a SceneViewport.
	 * If the SceneViewport is destroyed, the capture will stop.
	 * The SceneViewport needs to be of the same size and have the same pixel format as requested by the media output.
	 * @note make sure the size of the SceneViewport doesn't change during capture.
	 * @return True if the capture was successfully started
	 */
	bool CaptureSceneViewport(TSharedPtr<FSceneViewport>& SceneViewport, FMediaCaptureOptions CaptureOptions);

	/**
	 * Stop the current capture if there is one.
	 * Then find and capture every frame from active SceneViewport.
	 * It can only find a SceneViewport when you play in Standalone or in "New Editor Window PIE".
	 * If the active SceneViewport is destroyed, the capture will stop.
	 * The SceneViewport needs to be of the same size and have the same pixel format as requested by the media output.
	 * @return True if the capture was successfully started
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	bool CaptureActiveSceneViewport(FMediaCaptureOptions CaptureOptions);

	/**
	 * Stop the actual capture if there is one.
	 * Then capture every frame for a TextureRenderTarget2D.
	 * The TextureRenderTarget2D needs to be of the same size and have the same pixel format as requested by the media output.
	 * @return True if the capture was successfully started
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	bool CaptureTextureRenderTarget2D(UTextureRenderTarget2D* RenderTarget, FMediaCaptureOptions CaptureOptions);

	/**
	 * Update the current capture with a SceneViewport.
	 * If the SceneViewport is destroyed, the capture will stop.
	 * The SceneViewport needs to be of the same size and have the same pixel format as requested by the media output.
	 * @note make sure the size of the SceneViewport doesn't change during capture.
	 * @return Return true if the capture was successfully updated. If false is returned, the capture was stopped.
	 */
	bool UpdateSceneViewport(TSharedPtr<FSceneViewport>& SceneViewport);


	/**
	 * Update the current capture with every frame for a TextureRenderTarget2D.
	 * The TextureRenderTarget2D needs to be of the same size and have the same pixel format as requested by the media output.
	 * @return Return true if the capture was successfully updated. If false is returned, the capture was stopped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	bool UpdateTextureRenderTarget2D(UTextureRenderTarget2D* RenderTarget);


	/**
	 * Stop the previous requested capture.
	 * @param bAllowPendingFrameToBeProcess	Keep copying the pending frames asynchronously or stop immediately without copying the pending frames.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	void StopCapture(bool bAllowPendingFrameToBeProcess);

	/** Get the current state of the capture. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	virtual EMediaCaptureState GetState() const { return MediaState; }

	/** Set the media output. Can only be set when the capture is stopped. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	void SetMediaOutput(UMediaOutput* InMediaOutput);

	/** Get the desired size of the current capture. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	FIntPoint GetDesiredSize() const { return DesiredSize; }

	/** Get the desired pixel format of the current capture. */
	UFUNCTION(BlueprintCallable, Category = "Media|Output")
	EPixelFormat GetDesiredPixelFormat() const { return DesiredPixelFormat; }

	/** Check whether this capture has any processing left to do. */
	virtual bool HasFinishedProcessing() const;

public:
	//~ UObject interface
	virtual void BeginDestroy() override;
	virtual FString GetDesc() override;

protected:
	//~ UMediaCapture interface
	virtual bool ValidateMediaOutput() const;
	virtual bool CaptureSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) { return true; }
	virtual bool CaptureRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) { return true; }
	virtual bool UpdateSceneViewportImpl(TSharedPtr<FSceneViewport>& InSceneViewport) { return true; }
	virtual bool UpdateRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) { return true; }
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) { }


	struct FCaptureBaseData
	{
		FCaptureBaseData();

		FTimecode SourceFrameTimecode;
		uint32 SourceFrameNumberRenderThread;
	};
	virtual TSharedPtr<FMediaCaptureUserData> GetCaptureFrameUserData_GameThread() { return TSharedPtr<FMediaCaptureUserData>(); }
	virtual void OnFrameCaptured_RenderingThread(const FCaptureBaseData& InBaseData, TSharedPtr<FMediaCaptureUserData> InUserData, void* InBuffer, int32 Width, int32 Height) { }

protected:
	UTextureRenderTarget2D* GetTextureRenderTarget() { return CapturingRenderTarget; }
	TSharedPtr<FSceneViewport> GetCapturingSceneViewport() { return CapturingSceneViewport.Pin(); }
	EMediaCaptureConversionOperation GetConversionOperation() const { return ConversionOperation; }

protected:
	UPROPERTY(Transient)
	UMediaOutput* MediaOutput;

	EMediaCaptureState MediaState;

private:
	void InitializeResolveTarget(int32 InNumberOfBuffers);
	void OnEndFrame_GameThread();
	void CacheMediaOutput();
	FIntPoint GetOutputSize(const FIntPoint & InSize, const EMediaCaptureConversionOperation & InConversionOperation) const;
	EPixelFormat GetOutputPixelFormat(const EPixelFormat & InPixelFormat, const EMediaCaptureConversionOperation & InConversionOperation) const;

private:
	struct FCaptureFrame
	{
		FCaptureFrame();

		FTexture2DRHIRef ReadbackTexture;
		FCaptureBaseData CaptureBaseData;
		bool bResolvedTargetRequested;
		TSharedPtr<FMediaCaptureUserData> UserData;
	};

	TArray<FCaptureFrame> CaptureFrames;
	int32 CurrentResolvedTargetIndex;
	int32 NumberOfCaptureFrame;

	UPROPERTY(Transient)
	UTextureRenderTarget2D* CapturingRenderTarget;
	TWeakPtr<FSceneViewport> CapturingSceneViewport;
	FCriticalSection AccessingCapturingSource;

	FIntPoint DesiredSize;
	EPixelFormat DesiredPixelFormat;
	FIntPoint DesiredOutputSize;
	EPixelFormat DesiredOutputPixelFormat;
	FMediaCaptureOptions DesiredCaptureOptions;
	EMediaCaptureConversionOperation ConversionOperation;
	FString MediaOutputName;

	bool bResolvedTargetInitialized;
	TAtomic<int32> WaitingForResolveCommandExecutionCounter;
};
