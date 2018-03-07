// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if WITH_OPENCV
#include "OpenCVHelper.h"
OPENCV_INCLUDES_START
#undef check // the check macro causes problems with opencv headers
#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
OPENCV_INCLUDES_END
#endif
#include "MixedRealityLensDistortion.generated.h"

USTRUCT(BlueprintType)
struct MIXEDREALITYFRAMEWORK_API FMRLensDistortion
{
	GENERATED_USTRUCT_BODY()

public:

	FMRLensDistortion()
		: K1(0.f)
		, K2(0.f)
		, P1(0.f)
		, P2(0.f)
		, K3(0.f)
		, K4(0.f)
		, K5(0.f)
		, F(FVector2D(1.f, 1.f))
		, C(FVector2D(0.5f, 0.5f))
	{}

#if WITH_OPENCV
	FMRLensDistortion(const cv::Mat& DistCoeffs, const cv::Mat& CameraMatrix);

	cv::Mat GetDistCoeffs();
	cv::Mat GetCameraMatrix();
#endif

	/** Compare two lens distortion models and return whether they are equal. */
	bool operator == (const FMRLensDistortion& Other) const
	{
		return (
			K1 == Other.K1 &&
			K2 == Other.K2 &&
			P1 == Other.P1 &&
			P2 == Other.P2 &&
			K3 == Other.K3 &&
			K4 == Other.K4 &&
			K5 == Other.K5 &&
			F == Other.F &&
			C == Other.C);
	}

	/** Compare two lens distortion models and return whether they are different. */
	bool operator != (const FMRLensDistortion& Other) const
	{
		return !(*this == Other);
	}

	/** Returns true if the the object contains intitialized distortion parameters */
	bool IsSet()
	{
		return *this != FMRLensDistortion();
	}

	/** Radial parameter #1. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float K1;

	/** Radial parameter #2. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float K2;

	/** Tangential parameter #1. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float P1;

	/** Tangential parameter #2. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float P2;

	/** Radial parameter #3. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float K3;

	/** Radial parameter #4. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float K4;

	/** Radial parameter #5. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float K5;
	
	/** Radial parameter #6. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	float K6;


	/** Camera matrix's Fx and Fy. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	FVector2D F;

	/** Camera matrix's Cx and Cy. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "MixedReality|LensCalibration")
	FVector2D C;

	/**
	 * Creates a texture containing a UVMap in the Red and the Green channel for undistorting a camera image.
	 * @param ImageSize The size of the camera image to be undistorted in pixels.
	 * @param Alpha How much to scale the undistorted image to compensate for uneven edges. 0.0 means the image will be scaled to hide invalid pixels on the edges, 1.0 will retain all source image pixels.
	 *        Use an intermediate value for a scaling result between the two edge cases.
	 * @param UndistortedHFOV the horizontal field of view of the undistorted image.
	 * @param UndistortedVFOV the vertical field of view of the undistorted image.
	 * @param UndistortedAspectRatio the aspect ratio of the undistorted image.
	 */
	class UTexture2D* CreateUndistortUVMap(FIntPoint ImageSize, float Alpha, float& UndistortedHFOV, float& UndistortedVFOV, float& UndistortedAspectRatio);

};