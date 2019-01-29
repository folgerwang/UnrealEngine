// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

// AppleARKit
#include "AppleARKitAvailability.h"
#include "AppleARKitCamera.h"
#include "AppleARKitLightEstimate.h"

#include "AppleARKitFrame.generated.h"

/**
 * An object representing a frame processed by FAppleARKitSystem.
 * @discussion Each frame contains information about the current state of the scene.
 */
USTRUCT( BlueprintType, Category="AppleARKit" )
struct APPLEARKIT_API FAppleARKitFrame
{
	GENERATED_BODY()
	
	// Default constructor
	FAppleARKitFrame();

#if SUPPORTS_ARKIT_1_0

	/** 
	 * This is a conversion copy-constructor that takes a raw ARFrame and fills this structs members
	 * with the UE4-ified versions of ARFrames properties.
	 *
	 * @param InARFrame 		- The frame to convert / copy
	 * @param MetalTextureCache - A CVMetalTextureCacheRef to use in the conversion of InARFrame's CVPixelBufferRef to CVMetalTextureRef for our CapturedImage property.
	 */ 
	FAppleARKitFrame( ARFrame* InARFrame, CVMetalTextureCacheRef MetalTextureCache );

	/** 
	 * Copy contructor. CapturedImage is skipped as we don't need / want to retain access to the 
	 * image buffer.
	 */
	FAppleARKitFrame( const FAppleARKitFrame& Other );

	/** Destructor. Calls CFRelease on CapturedImage */
	virtual ~FAppleARKitFrame();

	/** 
	 * Assignment operator. CapturedImage is skipped as we don't need / want to retain access to the 
	 * image buffer. */
	FAppleARKitFrame& operator=( const FAppleARKitFrame& Other );

#endif

	/** A timestamp identifying the frame. */
	double Timestamp;

#if SUPPORTS_ARKIT_1_0

	/** The frame's captured images */
    CVMetalTextureRef CapturedYImage;
    CVMetalTextureRef CapturedCbCrImage;

	/** The raw camera buffer from ARKit */
	CVPixelBufferRef CameraImage;
	/** The raw camera depth info from ARKit (needs iPhone X) */
	AVDepthData* CameraDepth;
	void* NativeFrame;
#endif

	/** The width and height in pixels of the frame's captured images. */
	uint32 CapturedYImageWidth;
	uint32 CapturedYImageHeight;
    
    uint32 CapturedCbCrImageWidth;
    uint32 CapturedCbCrImageHeight;
    
	/** The camera used to capture the frame's image. */
	FAppleARKitCamera Camera;

	/**
	 * A light estimate representing the estimated light in the scene.
	 */
	FAppleARKitLightEstimate LightEstimate;
	
	/** The current world mapping state is reported on the frame */
	EARWorldMappingState WorldMappingState;

	/* 
	 * When adding new member variables, don't forget to handle them in the copy constructor and
	 * assignment operator above.
	 */
};
