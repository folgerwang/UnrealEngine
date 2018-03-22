// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityLensDistortion.h"
#include "Engine/Texture2D.h"

#if WITH_OPENCV
OPENCV_INCLUDES_START
#undef check // the check macro causes problems with opencv headers
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"
OPENCV_INCLUDES_END

using namespace cv;

FMRLensDistortion::FMRLensDistortion(const cv::Mat & DistCoeffs, const cv::Mat & CameraMatrix)
{
	// The DistCoeffs matrix is a one row matrix:
	check(DistCoeffs.rows == 1);
	K1 = DistCoeffs.at<double>(0);
	K2 = DistCoeffs.at<double>(1);
	P1 = DistCoeffs.at<double>(2);
	P2 = DistCoeffs.at<double>(3);

	// The docs make it sound like third (and fourth and fifth) radial coefficients may be optional
	K3 = DistCoeffs.cols >= 5 ? DistCoeffs.at<double>(4) : .0f;
	K4 = DistCoeffs.cols >= 6 ? DistCoeffs.at<double>(5) : .0f;
	K5 = DistCoeffs.cols >= 7 ? DistCoeffs.at<double>(6) : .0f;
	K6 = DistCoeffs.cols >= 8 ? DistCoeffs.at<double>(7) : .0f;

	check(CameraMatrix.rows == 3 && CameraMatrix.cols == 3);
	F.X = CameraMatrix.at<double>(0, 0);
	F.Y = CameraMatrix.at<double>(1, 1);
	C.X = CameraMatrix.at<double>(0, 2);
	C.Y = CameraMatrix.at<double>(1, 2);
}

Mat FMRLensDistortion::GetDistCoeffs()
{
	Mat DistCoeffs(1, 8, CV_64F);
	DistCoeffs.at<double>(0) = K1;
	DistCoeffs.at<double>(1) = K2;
	DistCoeffs.at<double>(2) = P1;
	DistCoeffs.at<double>(3) = P2;
	DistCoeffs.at<double>(4) = K3;
	DistCoeffs.at<double>(5) = K4;
	DistCoeffs.at<double>(6) = K5;
	DistCoeffs.at<double>(7) = K6;
	return DistCoeffs;
}

Mat FMRLensDistortion::GetCameraMatrix()
{
	Mat CameraMatrix = Mat::eye(3, 3, CV_64F);
	CameraMatrix.at<double>(0, 0) = F.X;
	CameraMatrix.at<double>(1, 1) = F.Y;
	CameraMatrix.at<double>(0, 2) = C.X;
	CameraMatrix.at<double>(1, 2) = C.Y;
	return CameraMatrix;
}


#endif

UTexture2D * FMRLensDistortion::CreateUndistortUVMap(FIntPoint ImageSize, float Alpha, float & UndistortedHFOV, float & UndistortedVFOV, float & UndistortedAspectRatio)
{
#if WITH_OPENCV
	if (IsSet())
	{

		Mat Map1(ImageSize.X, ImageSize.Y, CV_32FC1);
		Mat Map2(ImageSize.X, ImageSize.Y, CV_32FC1);

		// Use OpenCV to generate the map
		{
			Size ImgSzCV(ImageSize.X, ImageSize.Y);

			Mat Identity = Mat::eye(3, 3, CV_64F);
			Mat CameraMatrix = GetCameraMatrix();
			Mat DistCoeffs = GetDistCoeffs();

			// Calculate a new camera matrix based on the camera distortion coefficients and the passed in Alpha parameter
			Mat NewCameraMatrix = getOptimalNewCameraMatrix(CameraMatrix, DistCoeffs, ImgSzCV, Alpha);

			// Create UV lookup arrays that do the undistortion
			initUndistortRectifyMap(CameraMatrix, DistCoeffs, Identity, NewCameraMatrix, ImgSzCV, Map1.type(), Map1, Map2);

			// Estimate field of view of the undistorted image
			double AspectRatio, FovX, FovY, FocalLength_Unused;
			Point2d PrincipalPoint_Unused;
			// We pass in zero aperture size as it is unknown. (It is only required for calculating focal length and the principal point)
			calibrationMatrixValues(NewCameraMatrix, ImgSzCV, 0.0, 0.0, FovX, FovY, FocalLength_Unused, PrincipalPoint_Unused, AspectRatio);
			UndistortedHFOV = FovX;
			UndistortedVFOV = FovY;
			UndistortedAspectRatio = AspectRatio;
		}

		// Now convert the raw map arrays to an unreal texture
		UTexture2D* Result = UTexture2D::CreateTransient(
			ImageSize.X,
			ImageSize.Y,
			PF_G32R32F
		);

		// Lock the texture so it can be modified
		FTexture2DMipMap& Mip = Result->PlatformData->Mips[0];
		float* MipData = static_cast<float*>(Mip.BulkData.Lock(LOCK_READ_WRITE));

		// Generate pixel data
		for (int32 Y = 0; Y < ImageSize.Y; Y++)
		{
			float* Row = &MipData[Y * ImageSize.X * 2];
			for (int32 X = 0; X < ImageSize.X; X++)
			{
				Row[X * 2 + 0] = Map1.at<float>(Y, X) / static_cast<float>(ImageSize.X);
				Row[X * 2 + 1] = Map2.at<float>(Y, X) / static_cast<float>(ImageSize.Y);
			}
		}
		// Unlock the texture data
		Mip.BulkData.Unlock();
		Result->UpdateResource();

		return Result;
	}
	else
#endif
	{
		return nullptr;
	}
}

