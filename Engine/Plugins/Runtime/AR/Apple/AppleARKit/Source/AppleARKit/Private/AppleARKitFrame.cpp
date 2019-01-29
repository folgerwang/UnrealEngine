// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

// AppleARKit
#include "AppleARKitFrame.h"
#include "AppleARKitModule.h"
#include "Misc/ScopeLock.h"

// Default constructor
FAppleARKitFrame::FAppleARKitFrame()
	: Timestamp(0.0)
#if SUPPORTS_ARKIT_1_0
	, CapturedYImage(nullptr)
	, CapturedCbCrImage(nullptr)
	, CameraImage(nullptr)
	, CameraDepth(nullptr)
	, NativeFrame(nullptr)
#endif
	, CapturedYImageWidth(0)
	, CapturedYImageHeight(0)
	, CapturedCbCrImageWidth(0)
	, CapturedCbCrImageHeight(0)
	, WorldMappingState(EARWorldMappingState::NotAvailable)
{
};

#if SUPPORTS_ARKIT_2_0
EARWorldMappingState ToEARWorldMappingState(ARWorldMappingStatus MapStatus)
{
	switch (MapStatus)
	{
		// These both mean more data is needed
		case ARWorldMappingStatusLimited:
		case ARWorldMappingStatusExtending:
			return EARWorldMappingState::StillMappingNotRelocalizable;

		case ARWorldMappingStatusMapped:
			return EARWorldMappingState::Mapped;
	}
	return EARWorldMappingState::NotAvailable;
}
#endif

#if SUPPORTS_ARKIT_1_0

FAppleARKitFrame::FAppleARKitFrame( ARFrame* InARFrame, CVMetalTextureCacheRef MetalTextureCache )
	: Camera( InARFrame.camera )
	, LightEstimate( InARFrame.lightEstimate )
	, WorldMappingState(EARWorldMappingState::NotAvailable)
{
	// Sanity check
	check( InARFrame );
	check( MetalTextureCache );

	// Copy timestamp
	Timestamp = InARFrame.timestamp;

	CameraImage = nullptr;
	CameraDepth = nullptr;

	// Copy / convert pass-through camera image's CVPixelBuffer to an MTLTexture so we can pass it
	// directly to FTextureResource's.
	// @see AppleARKitCameraTextureResource.cpp
	CapturedYImage = nullptr;
	CapturedYImageWidth = InARFrame.camera.imageResolution.width;
	CapturedYImageHeight = InARFrame.camera.imageResolution.height;
	
	CapturedCbCrImage = nullptr;
	CapturedCbCrImageWidth = InARFrame.camera.imageResolution.width;
	CapturedCbCrImageHeight = InARFrame.camera.imageResolution.height;
	
	if ( InARFrame.capturedImage )
	{
		CameraImage = InARFrame.capturedImage;
		CFRetain(CameraImage);

		// Update SizeX & Y
		CapturedYImageWidth = CVPixelBufferGetWidthOfPlane( InARFrame.capturedImage, 0 );
		CapturedYImageHeight = CVPixelBufferGetHeightOfPlane( InARFrame.capturedImage, 0 );
		CapturedCbCrImageWidth = CVPixelBufferGetWidthOfPlane( InARFrame.capturedImage, 1 );
		CapturedCbCrImageHeight = CVPixelBufferGetHeightOfPlane( InARFrame.capturedImage, 1 );
		
		// Create a metal texture from the CVPixelBufferRef. The CVMetalTextureRef will
		// be released in the FAppleARKitFrame destructor.
		// NOTE: On success, CapturedImage will be a new CVMetalTextureRef with a ref count of 1
		// 		 so we don't need to CFRetain it. The corresponding CFRelease is handled in
		//
		CVReturn Result = CVMetalTextureCacheCreateTextureFromImage( nullptr, MetalTextureCache, InARFrame.capturedImage, nullptr, MTLPixelFormatR8Unorm, CapturedYImageWidth, CapturedYImageHeight, /*PlaneIndex*/0, &CapturedYImage );
		check( Result == kCVReturnSuccess );
		check( CapturedYImage );
		check( CFGetRetainCount(CapturedYImage) == 1);
		
		Result = CVMetalTextureCacheCreateTextureFromImage( nullptr, MetalTextureCache, InARFrame.capturedImage, nullptr, MTLPixelFormatRG8Unorm, CapturedCbCrImageWidth, CapturedCbCrImageHeight, /*PlaneIndex*/1, &CapturedCbCrImage );
		check( Result == kCVReturnSuccess );
		check( CapturedCbCrImage );
		check( CFGetRetainCount(CapturedCbCrImage) == 1);
	}

	if (InARFrame.capturedDepthData)
	{
		CameraDepth = InARFrame.capturedDepthData;
		CFRetain(CameraDepth);
	}

	NativeFrame = (void*)CFRetain(InARFrame);

#if SUPPORTS_ARKIT_2_0
	if (FAppleARKitAvailability::SupportsARKit20())
	{
		WorldMappingState = ToEARWorldMappingState(InARFrame.worldMappingStatus);
	}
#endif
}

FAppleARKitFrame::FAppleARKitFrame( const FAppleARKitFrame& Other )
	: Timestamp( Other.Timestamp )
	, CapturedYImage( nullptr )
	, CapturedCbCrImage( nullptr )
	, CameraImage( nullptr )
	, CameraDepth( nullptr )
	, CapturedYImageWidth( Other.CapturedYImageWidth )
	, CapturedYImageHeight( Other.CapturedYImageHeight )
	, CapturedCbCrImageWidth( Other.CapturedCbCrImageWidth )
	, CapturedCbCrImageHeight( Other.CapturedCbCrImageHeight )
	, Camera( Other.Camera )
	, LightEstimate( Other.LightEstimate )
	, WorldMappingState(Other.WorldMappingState)
{
	if(Other.NativeFrame != nullptr)
	{
		NativeFrame = (void*)CFRetain((CFTypeRef)Other.NativeFrame);
	}
}

FAppleARKitFrame::~FAppleARKitFrame()
{
	// Release captured image
	if ( CapturedYImage != nullptr )
	{
		CFRelease( CapturedYImage );
	}
	if ( CapturedCbCrImage != nullptr )
	{
		CFRelease( CapturedCbCrImage );
	}
	if (CameraImage != nullptr)
	{
		CFRelease(CameraImage);
	}
	if (CameraDepth != nullptr)
	{
		CFRelease(CameraDepth);
	}
	if(NativeFrame != nullptr)
	{
		CFRelease((CFTypeRef)NativeFrame);
	}
}

FAppleARKitFrame& FAppleARKitFrame::operator=( const FAppleARKitFrame& Other )
{
	if (&Other == this)
	{
		return *this;
	}

	// Release outgoing image
	if ( CapturedYImage != nullptr )
	{
		CFRelease( CapturedYImage );
	}
	if ( CapturedCbCrImage != nullptr )
	{
		CFRelease( CapturedCbCrImage );
	}
	if (CameraImage != nullptr)
	{
		CFRelease(CameraImage);
		CameraImage = nullptr;
	}
	if (CameraDepth != nullptr)
	{
		CFRelease(CameraDepth);
		CameraDepth = nullptr;
	}
	if(NativeFrame != nullptr)
	{
		CFRelease((CFTypeRef)NativeFrame);
		NativeFrame = nullptr;
	}

	if(Other.NativeFrame != nullptr)
	{
		NativeFrame = (void*)CFRetain((CFTypeRef)Other.NativeFrame);
	}
	
	// Member-wise copy
	Timestamp = Other.Timestamp;
	CapturedYImage = nullptr;
	CapturedYImageWidth = Other.CapturedYImageWidth;
	CapturedYImageHeight = Other.CapturedYImageHeight;
	CapturedCbCrImage = nullptr;
	CapturedCbCrImageWidth = Other.CapturedCbCrImageWidth;
	CapturedCbCrImageHeight = Other.CapturedCbCrImageHeight;
	Camera = Other.Camera;
	LightEstimate = Other.LightEstimate;
	WorldMappingState = Other.WorldMappingState;

	NativeFrame = Other.NativeFrame;

	return *this;
}

#endif
