// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalRHIPrivate.h: Private Metal RHI definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "PixelFormat.h"

// Dependencies
#include "MetalRHI.h"
#include "RHI.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

// Metal C++ wrapper
THIRD_PARTY_INCLUDES_START
#include "mtlpp.hpp"
THIRD_PARTY_INCLUDES_END

// Whether the Metal RHI is initialized sufficiently to handle resources
extern bool GIsMetalInitialized;

// Requirement for vertex buffer offset field
#if PLATFORM_MAC
const uint32 BufferOffsetAlignment = 256;
#else
const uint32 BufferOffsetAlignment = 16;
#endif

// The buffer page size that can be uploaded in a set*Bytes call
const uint32 MetalBufferPageSize = 4096;

#define BUFFER_CACHE_MODE mtlpp::ResourceOptions::CpuCacheModeDefaultCache

#if PLATFORM_MAC
#define BUFFER_MANAGED_MEM mtlpp::ResourceOptions::StorageModeManaged
#define BUFFER_STORAGE_MODE mtlpp::StorageMode::Managed
#define BUFFER_RESOURCE_STORAGE_MANAGED mtlpp::ResourceOptions::StorageModeManaged
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 31;
#else
#define BUFFER_MANAGED_MEM 0
#define BUFFER_STORAGE_MODE mtlpp::StorageMode::Shared
#define BUFFER_RESOURCE_STORAGE_MANAGED mtlpp::ResourceOptions::StorageModeShared
#define BUFFER_DYNAMIC_REALLOC BUF_AnyDynamic
// How many possible vertex streams are allowed
const uint32 MaxMetalStreams = 30;
#endif

#ifndef METAL_STATISTICS
#define METAL_STATISTICS 0
#endif

// Unavailable on iOS, but dealing with this clutters the code.
enum EMTLTextureType
{
	EMTLTextureTypeCubeArray = 6
};

// This is the right VERSION check, see Availability.h in the SDK
#define METAL_SUPPORTS_INDIRECT_ARGUMENT_BUFFERS ((PLATFORM_MAC && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300) || (!PLATFORM_MAC && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000)) && (__clang_major__ >= 9)
#define METAL_SUPPORTS_CAPTURE_MANAGER (PLATFORM_MAC && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300) || (!PLATFORM_MAC && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000) && (__clang_major__ >= 9)
#define METAL_SUPPORTS_TILE_SHADERS (!PLATFORM_MAC && !PLATFORM_TVOS && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000) && (__clang_major__ >= 9)
// In addition to compile-time SDK checks we also need a way to check if these are available on runtime
extern bool GMetalSupportsIndirectArgumentBuffers;
extern bool GMetalSupportsCaptureManager;
extern bool GMetalSupportsTileShaders;
extern bool GMetalSupportsStoreActionOptions;
extern bool GMetalSupportsDepthClipMode;
extern bool GMetalCommandBufferHasStartEndTimeAPI;

struct FMetalBufferFormat
{
	// Valid linear texture pixel formats - potentially different than the actual texture formats
	mtlpp::PixelFormat LinearTextureFormat;
	// Metal buffer data types for manual ALU format conversions
	uint8 DataFormat;
};

extern FMetalBufferFormat GMetalBufferFormats[PF_MAX];

#define METAL_DEBUG_OPTIONS !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
#if METAL_DEBUG_OPTIONS
#define METAL_DEBUG_OPTION(Code) Code
#else
#define METAL_DEBUG_OPTION(Code)
#endif

#if MTLPP_CONFIG_VALIDATE && METAL_DEBUG_OPTIONS
#define METAL_DEBUG_ONLY(Code) Code
#define METAL_DEBUG_LAYER(Level, Code) if (SafeGetRuntimeDebuggingLevel() >= Level) Code
#else
#define METAL_DEBUG_ONLY(Code)
#define METAL_DEBUG_LAYER(Level, Code)
#endif

extern bool GMetalSupportsTileShaders;

/** Set to 1 to enable GPU events in Xcode frame debugger */
#ifndef ENABLE_METAL_GPUEVENTS_IN_TEST
	#define ENABLE_METAL_GPUEVENTS_IN_TEST 0
#endif
#define ENABLE_METAL_GPUEVENTS	(UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT || (UE_BUILD_TEST && ENABLE_METAL_GPUEVENTS_IN_TEST) || METAL_STATISTICS)
#define ENABLE_METAL_GPUPROFILE	(ENABLE_METAL_GPUEVENTS && 1)

#if ENABLE_METAL_GPUPROFILE
#define METAL_GPUPROFILE(Code) Code
#else
#define METAL_GPUPROFILE(Code) 
#endif

#if METAL_STATISTICS
#define METAL_STATISTIC(Code) Code
#else
#define METAL_STATISTIC(Code)
#endif

#define UNREAL_TO_METAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)
#define METAL_TO_UNREAL_BUFFER_INDEX(Index) ((MaxMetalStreams - 1) - Index)

#define METAL_NEW_NONNULL_DECL (__clang_major__ >= 9)

// Access the internal context for the device-owning DynamicRHI object
FMetalDeviceContext& GetMetalDeviceContext();

// Safely release a metal object, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalObject(id Object);

// Safely release a metal texture, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalTexture(FMetalTexture& Object);

// Safely release a metal buffer, correctly handling the case where the RHI has been destructed first
void SafeReleaseMetalBuffer(FMetalBuffer& Buffer);

// Safely release a fence, correctly handling cases where fences aren't supported or the debug implementation is used.
void SafeReleaseMetalFence(id Object);

// Access the underlying surface object from any kind of texture
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture);

#define NOT_SUPPORTED(Func) UE_LOG(LogMetal, Fatal, TEXT("'%s' is not supported"), L##Func);

FORCEINLINE mtlpp::IndexType GetMetalIndexType(EMetalIndexType IndexType)
{
	switch (IndexType)
	{
		case EMetalIndexType_UInt16: return mtlpp::IndexType::UInt16;
		case EMetalIndexType_UInt32: return mtlpp::IndexType::UInt32;
		case EMetalIndexType_None:
		default:
		{
			UE_LOG(LogMetal, Fatal, TEXT("There is not equivalent mtlpp::IndexType for EMetalIndexType_None"));
			return mtlpp::IndexType::UInt16;
		}
	}
}

FORCEINLINE EMetalIndexType GetRHIMetalIndexType(mtlpp::IndexType IndexType)
{
	switch (IndexType)
	{
		case mtlpp::IndexType::UInt16: return EMetalIndexType_UInt16;
		case mtlpp::IndexType::UInt32: return EMetalIndexType_UInt32;
		default: return EMetalIndexType_None;
	}
}

FORCEINLINE int32 GetMetalCubeFace(ECubeFace Face)
{
	// According to Metal docs these should match now: https://developer.apple.com/library/prerelease/ios/documentation/Metal/Reference/MTLTexture_Ref/index.html#//apple_ref/c/tdef/MTLTextureType
	switch (Face)
	{
		case CubeFace_PosX:;
		default:			return 0;
		case CubeFace_NegX:	return 1;
		case CubeFace_PosY:	return 2;
		case CubeFace_NegY:	return 3;
		case CubeFace_PosZ:	return 4;
		case CubeFace_NegZ:	return 5;
	}
}

FORCEINLINE mtlpp::LoadAction GetMetalRTLoadAction(ERenderTargetLoadAction LoadAction)
{
	switch(LoadAction)
	{
		case ERenderTargetLoadAction::ENoAction: return mtlpp::LoadAction::DontCare;
		case ERenderTargetLoadAction::ELoad: return mtlpp::LoadAction::Load;
		case ERenderTargetLoadAction::EClear: return mtlpp::LoadAction::Clear;
		default: return mtlpp::LoadAction::DontCare;
	}
}

uint32 TranslateElementTypeToSize(EVertexElementType Type);

mtlpp::PrimitiveType TranslatePrimitiveType(uint32 PrimitiveType);

#if PLATFORM_MAC
mtlpp::PrimitiveTopologyClass TranslatePrimitiveTopology(uint32 PrimitiveType);
#endif

mtlpp::PixelFormat ToSRGBFormat(mtlpp::PixelFormat LinMTLFormat);

uint8 GetMetalPixelFormatKey(mtlpp::PixelFormat Format);

template<typename TRHIType>
static FORCEINLINE typename TMetalResourceTraits<TRHIType>::TConcreteType* ResourceCast(TRHIType* Resource)
{
	return static_cast<typename TMetalResourceTraits<TRHIType>::TConcreteType*>(Resource);
}

uint32 SafeGetRuntimeDebuggingLevel();

#include "MetalStateCache.h"
#include "MetalContext.h"
