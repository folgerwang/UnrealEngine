// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "MixedRealityLensDistortion.h"

#if WITH_OPENCV
#include "OpenCVHelper.h"
OPENCV_INCLUDES_START
#undef check // the check macro causes problems with opencv headers
#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
OPENCV_INCLUDES_END
#endif
#include "MixedRealityOpenCVCalibrator.generated.h"

UCLASS(meta = (BlueprintSpawnableComponent))
class UMROpenCVCalibrator : public UObject
{
	GENERATED_BODY()

private:
	/**
	 * Default constructor for a CV calibration object.
	 */
	UMROpenCVCalibrator(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: UObject(ObjectInitializer)
	{}
public:

	/**
	 * @param BoardWidth The width of the checkerboard used to calibrate the camera counted as number of inner edges.
	 * @param BoardHeight The height of the checkerboard used to calibrate the camera counted as number of inner edges.
	 * @param InSquareSize The width of each square in (potentially arbitrary) world units.
	 */
	UFUNCTION(BlueprintCallable, Category = "MixedReality|LensCalibration")
	static UMROpenCVCalibrator* CreateCalibrator(int32 BoardWidth = 7, int32 BoardHeight = 5, float InSquareSize = 3.f);

	/** 
	 * @param BoardWidth The width of the checkerboard used to calibrate the camera counted as number of inner edges.
	 * @param BoardHeight The height of the checkerboard used to calibrate the camera counted as number of inner edges.
	 * @param InSquareSize The width of each square in (potentially arbitrary) world units.
	 */
	void Reset(int32 BoardWidth, int32 BoardHeight, float InSquareSize=3.f);

	/**
	 * Feeds a render target to the calibration. It must contain a checkerboard somewhere in the image.
	 * The images fed in should come from the same camera.
	 * @return true if the calibrator found a checkerboard in the image.
	 */
	UFUNCTION(BlueprintCallable, Category = "MixedReality|LensCalibration")
	bool FeedRenderTarget(class UTextureRenderTarget2D* TexRT);

	/**
	 * Feeds an image to the calibration. It must contain a checkerboard somewhere in the image.
	 * The images fed in should come from the same camera.
	 * @return true if the calibrator found a checkerboard in the image.
	 */
	UFUNCTION(BlueprintCallable, Category = "MixedReality|LensCalibration")
	bool FeedImage(const FString& FilePath);

	/** 
	 * @param OutLensDistortion the calculated distortion data from the images passed into the calibrator.
	 * @return true if there was enough data to calculate the distortion
	 */
	UFUNCTION(BlueprintCallable, Category = "MixedReality|LensCalibration")
	bool CalculateLensParameters(FMRLensDistortion& LensDistortion, float& HFOV, float& VFOV, float& AspectRatio);

private:
#if WITH_OPENCV
	bool Feed(const cv::Mat& Image);

	std::vector<std::vector<cv::Point2f>> ImagePoints;
	std::vector<cv::Point3f> BoardPoints;
	cv::Size ImageSize;
	cv::Size BoardSize;
#endif
	float SquareSize;
};