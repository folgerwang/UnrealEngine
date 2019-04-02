// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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

#include "OpenCVLensDistortionParameters.generated.h"


class UTexture2D;
class UTextureRenderTarget2D;


USTRUCT(BlueprintType)
struct OPENCVLENSDISTORTION_API FOpenCVCameraViewInfo
{
	GENERATED_USTRUCT_BODY()

	FOpenCVCameraViewInfo()
		: HorizontalFOV(0.0f)
		, VerticalFOV(0.0f)
		, FocalLengthRatio(0.0f)
	{ }

	/** Horizontal Field Of View in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Info")
	float HorizontalFOV;

	/** Vertical Field Of View in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Info")
	float VerticalFOV;

	/** Focal length aspect ratio -> Fy / Fx */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Info")
	float FocalLengthRatio;
};

/**
 * Mathematic camera model for lens distortion/undistortion.
 * Camera matrix =
 *  | F.X  0  C.x |
 *  |  0  F.Y C.Y |
 *  |  0   0   1  |
 * where F and C are normalized.
 */
USTRUCT(BlueprintType)
struct OPENCVLENSDISTORTION_API FOpenCVLensDistortionParameters
{
	GENERATED_USTRUCT_BODY()

public:
	FOpenCVLensDistortionParameters()
		: K1(0.f)
		, K2(0.f)
		, P1(0.f)
		, P2(0.f)
		, K3(0.f)
		, K4(0.f)
		, K5(0.f)
		, K6(0.f)
		, F(FVector2D(1.f, 1.f))
		, C(FVector2D(0.5f, 0.5f))
		, bUseFisheyeModel(false)
	{
	}

	/** Draws UV displacement map within the output render target.
	 * - Red & green channels hold the distort to undistort displacement;
	 * - Blue & alpha channels hold the undistort to distort displacement.
	 * @param InWorld Current world to get the rendering settings from (such as feature level).
	 * @param InOutputRenderTarget The render target to draw to. Don't necessarily need to have same resolution or aspect ratio as distorted render.
	 * @param InPreComputedUndistortDisplacementMap Distort to undistort displacement pre computed using OpenCV in engine or externally.
	 */
	static void DrawDisplacementMapToRenderTarget(UWorld* InWorld, UTextureRenderTarget2D* InOutputRenderTarget, UTexture2D* InPreComputedUndistortDisplacementMap);

	/**
	 * Creates a texture containing a DisplacementMap in the Red and the Green channel for undistorting a camera image.
	 * This call can take quite some time to process depending on the resolution.
	 * @param InImageSize The size of the camera image to be undistorted in pixels. Scaled down resolution will have an impact.
	 * @param InCroppingFactor One means OpenCV will attempt to crop out all empty pixels resulting from the process (essentially zooming the image). Zero will keep all pixels.
	 * @param OutCameraViewInfo Information computed by OpenCV about the undistorted space. Can be used with SceneCapture to adjust FOV.
	 */
	UTexture2D* CreateUndistortUVDisplacementMap(const FIntPoint& InImageSize, const float InCroppingFactor, FOpenCVCameraViewInfo& OutCameraViewInfo) const;

public:

	/** Compare two lens distortion models and return whether they are equal. */
	bool operator == (const FOpenCVLensDistortionParameters& Other) const
	{
		return (K1 == Other.K1 &&
				K2 == Other.K2 &&
				P1 == Other.P1 &&
				P2 == Other.P2 &&
				K3 == Other.K3 &&
				K4 == Other.K4 &&
				K5 == Other.K5 &&
				K6 == Other.K6 &&
				F == Other.F &&
				C == Other.C &&
				bUseFisheyeModel == Other.bUseFisheyeModel);
	}

	/** Compare two lens distortion models and return whether they are different. */
	bool operator != (const FOpenCVLensDistortionParameters& Other) const
	{
		return !(*this == Other);
	}

	/** Returns true if lens distortion parameters are for identity lens (or default parameters) */
	bool IsIdentity() const
	{
		return (K1 == 0.0f &&
				K2 == 0.0f &&
				P1 == 0.0f &&
				P2 == 0.0f &&
				K3 == 0.0f &&
				K4 == 0.0f &&
				K5 == 0.0f &&
				K6 == 0.0f &&
				F == FVector2D(1.0f, 1.0f) &&
				C == FVector2D(0.5f, 0.5f));
	}

	bool IsSet() const 
	{
		return *this != FOpenCVLensDistortionParameters();
	}

private:

#if WITH_OPENCV
	/** Convert internal coefficients to OpenCV matrix representation */
	cv::Mat ConvertToOpenCVDistortionCoefficients() const;

	/** Convert internal normalized camera matrix to OpenCV pixel scaled matrix representation. */
	cv::Mat CreateOpenCVCameraMatrix(const FVector2D& InImageSize) const;
#endif //WITH_OPENCV

public:

	/** Radial parameter #1. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K1;

	/** Radial parameter #2. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K2;

	/** Tangential parameter #1. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float P1;

	/** Tangential parameter #2. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float P2;

	/** Radial parameter #3. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K3;

	/** Radial parameter #4. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K4;

	/** Radial parameter #5. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K5;
	
	/** Radial parameter #6. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	float K6;

	/** Camera matrix's normalized Fx and Fy. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	FVector2D F;

	/** Camera matrix's normalized Cx and Cy. */
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	FVector2D C;

	/** Camera lens needs Fisheye camera model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Distortion|Parameters")
	bool bUseFisheyeModel;
};
