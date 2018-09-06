// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PIEPreviewDeviceSpecification.generated.h"

UENUM()
enum class EPIEPreviewDeviceType : uint8
{
	Unset,
	Android,
	IOS,
	TVOS,
	MAX,
};

UCLASS()
class PIEPREVIEWDEVICESPECIFICATION_API UPIEPreviewDeviceSpecification : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
		EPIEPreviewDeviceType PreviewDeviceType;

	UPROPERTY()
		FString GPUFamily;
	UPROPERTY()
		FString GLVersion;
	UPROPERTY()
		FString VulkanVersion;
	UPROPERTY()
		FString AndroidVersion;
	UPROPERTY()
		FString DeviceMake;
	UPROPERTY()
		FString DeviceModel;
	UPROPERTY()
		bool UsingHoudini;
};

USTRUCT()
struct FPIERHIOverrideState
{
public:
	GENERATED_USTRUCT_BODY()
	UPROPERTY()
	int32 MaxShadowDepthBufferSizeX;
	UPROPERTY()
	int32 MaxShadowDepthBufferSizeY;
	UPROPERTY()
	int32 MaxTextureDimensions;
	UPROPERTY()
	int32 MaxCubeTextureDimensions;
	UPROPERTY()
	bool SupportsRenderTargetFormat_PF_G8;
	UPROPERTY()
	bool SupportsRenderTargetFormat_PF_FloatRGBA;
	UPROPERTY()
	bool SupportsMultipleRenderTargets;
	UPROPERTY()
	bool SupportsInstancing;
};

USTRUCT()
struct FPIEAndroidDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString GPUFamily;
	UPROPERTY()
	FString GLVersion;
	UPROPERTY()
	FString VulkanVersion;
	UPROPERTY()
	FString AndroidVersion;
	UPROPERTY()
	FString DeviceMake;
	UPROPERTY()
	FString DeviceModel;
	UPROPERTY()
	bool VulkanAvailable;
	UPROPERTY()
	bool UsingHoudini;

	UPROPERTY()
	FPIERHIOverrideState GLES2RHIState;

	UPROPERTY()
	FPIERHIOverrideState GLES31RHIState;

// 	UPROPERTY()
// 	FPIERHIOverrideState VulkanRHIState;
};

USTRUCT()
struct FPIEIOSDeviceProperties
{
public:
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceModel;

	UPROPERTY()
	float NativeScaleFactor = 0.0f;

	UPROPERTY()
	FPIERHIOverrideState GLES2RHIState;

	UPROPERTY()
	FPIERHIOverrideState MetalRHIState;
};

USTRUCT()
struct FPIEPreviewDeviceBezelViewportRect
{
public:
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	int32 X = 0;
	UPROPERTY()
	int32 Y = 0;
	UPROPERTY()
	int32 Width = 0;
	UPROPERTY()
	int32 Height = 0;
}; 

USTRUCT()
struct FPIEBezelProperties
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString DeviceBezelFile;
	UPROPERTY()
	FPIEPreviewDeviceBezelViewportRect BezelViewportRect;
};

USTRUCT()
struct FPIEPreviewDeviceSpecifications
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY()
	EPIEPreviewDeviceType DevicePlatform;
	UPROPERTY()
	int32 ResolutionX;
	UPROPERTY()
	int32 ResolutionY;
	UPROPERTY()
	int32 ResolutionYImmersiveMode;

	UPROPERTY()
	int32 PPI;

	UPROPERTY()
	TArray<float> ScaleFactors;
	
	UPROPERTY()
	FPIEBezelProperties BezelProperties;

	UPROPERTY()
	FPIEAndroidDeviceProperties AndroidProperties;

	UPROPERTY()
	FPIEIOSDeviceProperties IOSProperties;
};
