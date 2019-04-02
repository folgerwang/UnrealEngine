// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HTML5TargetSettings.h: Declares the UHTML5TargetSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "HTML5TargetSettings.generated.h"


/**
* Enumerates available Canvas Scaling Modes
*/
UENUM()
namespace ECanvasScalingMode
{
	enum Type
	{
		Stretch = 1,
		Aspect,
		Fixed,
	};
}

/**
 * Implements the settings for the HTML5 target platform.
 */
UCLASS(config=Engine, defaultconfig)
class HTML5PLATFORMEDITOR_API UHTML5TargetSettings
	: public UObject
{
public:

	GENERATED_UCLASS_BODY()

	// ------------------------------------------------------------

	/**
	 * Use IndexedDB storage
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "IndexedDB storage"))
	bool EnableIndexedDB;

	/**
	 * Use Fixed TimeStep
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Fixed TimeStep (i.e. use requestAnimationFrame)"))
	bool UseFixedTimeStep; // need to make a note of: answerhub 409629

	/**
	 * Enable Multithreading (Experimental)
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Multithreading support"))
	bool EnableMultithreading;

	/**
	 * Use OffscreenCanvas Support (else use Offscreen Framebuffer)
	 * Note: Multithreading rendering option (WORK IN PROGRESS)
	 */
//	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Offscreen Canvas [experimental]"))
//	bool OffscreenCanvas;

	/**
	 * Use LLVM WASM Backend (WORK IN PROGRESS)
	 */
//	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "LLVM Wasm backend [experimental]"))
//	bool LLVMWasmBackend;

	/**
	 * Enable Tracing (trace.h)
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Emscripten, Meta = (DisplayName = "Tracing support"))
	bool EnableTracing;

	// ------------------------------------------------------------

	/**
	 * Canvas scaling mode
	 * How the canvas size changes when the browser window is resized by dragging from the corner.
	 * STRETCH : dynamic dimensions (both canvas size and app resolution scales)
	 * ASPECT  : use the aspect ratio that the canvas will be constrained to (canvas will scale while app stays locked)
	 * FIXED   : fixed resolution that the app will render to (canvas and app dimensions will be locked)
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Canvas)
	TEnumAsByte<ECanvasScalingMode::Type> CanvasScalingMode;

	// ------------------------------------------------------------

	/**
	 * Compress Files
	 * NOTE 1: it is also recommended to NOT enable PAK file packaging - this is currently redundant
	 * NOTE 2: future emscripten version will allow separate (asset) files in a new FileSystem feature - which will make use of this (as well as PAK file) option again
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category=Packaging, Meta = (DisplayName = "Compress files during shipping packaging"))
	bool Compressed;

	// ------------------------------------------------------------

	/**
	 * Port to use when deploying game from the editor
	 */
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Launch, Meta = (DisplayName = "Port to use when deploying game from the editor", ClampMin="49152", ClampMax="65535"))
	int32 DeployServerPort;

	// ------------------------------------------------------------

	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Upload builds to Amazon S3 when packaging"))
	bool UploadToS3;

	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Region", EditCondition = "UploadToS3"))
	FString S3Region;
	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Key ID", EditCondition = "UploadToS3"))
	FString S3KeyID;
	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Secret Access Key", EditCondition = "UploadToS3"))
	FString S3SecretAccessKey;
	/**
	* Required
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Amazon S3 Bucket Name", EditCondition = "UploadToS3"))
	FString S3BucketName;
	/**
	* Provide another level of nesting beyond the bucket. Can be left empty, defaults to game name. DO NOT LEAVE A TRAILING SLASH!
	*/
	UPROPERTY(GlobalConfig, EditAnywhere, Category = Amazon_S3, Meta = (DisplayName = "Nested Folder Name", EditCondition = "UploadToS3"))
	FString S3FolderName;

	/** Which of the currently enabled spatialization plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString SpatializationPlugin;

	/** Which of the currently enabled reverb plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString ReverbPlugin;

	/** Which of the currently enabled occlusion plugins to use on Windows. */
	UPROPERTY(config, EditAnywhere, Category = "Audio")
	FString OcclusionPlugin;
};

