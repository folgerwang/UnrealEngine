// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MixedRealityOpenCVCalibrator.h"
#include "Engine/TextureRenderTarget2D.h"

#if WITH_OPENCV
OPENCV_INCLUDES_START
#undef check // the check macro causes problems with opencv headers
#include "opencv2/calib3d.hpp"
#include "opencv2/imgproc.hpp"
OPENCV_INCLUDES_END

using namespace cv;
#endif


UMROpenCVCalibrator* UMROpenCVCalibrator::CreateCalibrator(int32 BoardWidth /*= 7*/, int32 BoardHeight /*= 5*/, float InSquareSize /*= 3.f*/)
{
	UMROpenCVCalibrator* Result = NewObject<UMROpenCVCalibrator>(UMROpenCVCalibrator::StaticClass());
	Result->Reset(BoardWidth, BoardHeight, InSquareSize);
	return Result;
}

void UMROpenCVCalibrator::Reset(int32 BoardWidth, int32 BoardHeight, float InSquareSize/*=3.f*/)
{
	SquareSize = InSquareSize;
#if WITH_OPENCV
	BoardSize = Size(BoardWidth, BoardHeight);
	// Assuming the chessboard is at the origin lying flat on the z plane, construct object coordinates for it
	BoardPoints.clear();
	BoardPoints.reserve(BoardSize.height * BoardSize.width);
	for (int i = 0; i < BoardSize.height; ++i)
		for (int j = 0; j < BoardSize.width; ++j)
			BoardPoints.push_back(Point3f(float(j*SquareSize), float(i*SquareSize), 0));

	// Reserve space for a few samples
	ImagePoints.clear();
	ImagePoints.reserve(25);
#endif
}

bool UMROpenCVCalibrator::FeedRenderTarget(UTextureRenderTarget2D* TexRT)
{
#if WITH_OPENCV

	TArray<uint8> RawData;
	FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
	EPixelFormat Format = TexRT->GetFormat();
	bool bReadSuccess = false;
	switch (Format)
	{
	case PF_FloatRGBA:
	{
		TArray<FFloat16Color> FloatColors;
		bReadSuccess = RenderTarget->ReadFloat16Pixels(FloatColors);
		if (bReadSuccess)
		{
			RawData.AddUninitialized(FloatColors.Num() * 3); // We need to convert float to uint8
			for (int I = 0; I < FloatColors.Num(); I++)
			{
				// OpenCV expects BGR ordering and we're ignoring the alpha component
				RawData[I * 3 + 0] = (uint8)(FloatColors[I].B*255.0f);
				RawData[I * 3 + 1] = (uint8)(FloatColors[I].G*255.0f);
				RawData[I * 3 + 2] = (uint8)(FloatColors[I].R*255.0f);
			}
		}
	}
	break;
	case PF_B8G8R8A8:
	{
		TArray<FColor> Colors;
		bReadSuccess = RenderTarget->ReadPixels(Colors);
		if (bReadSuccess)
		{
			RawData.AddUninitialized(Colors.Num() * 3);
			for (int I = 0; I < Colors.Num(); I++)
			{
				// Strip out the alpha component for OpenCV
				RawData[I * 3 + 0] = Colors[I].B;
				RawData[I * 3 + 1] = Colors[I].G;
				RawData[I * 3 + 2] = Colors[I].R;
			}
		}
	}
	break;
	}

	if (bReadSuccess == false) // sorry, no bread ;)
	{
		// Either invalid texture data or unsupported texture format
		return false;
	}

	Mat Image(TexRT->SizeY, TexRT->SizeX, CV_8UC3, (void*)RawData.GetData());
	return Feed(Image);
#else
	return false;
#endif
}

bool UMROpenCVCalibrator::FeedImage(const FString& FilePath)
{
#if WITH_OPENCV

	FTCHARToUTF8 FilePathUtf8(*FilePath);
	cv::String CVPath(FilePathUtf8.Get(), FilePathUtf8.Length());
	Mat Image = imread(CVPath);


	return Feed(Image);
#else
	return false;
#endif
}

#if WITH_OPENCV
bool UMROpenCVCalibrator::Feed(const Mat& Image)
{
	std::vector<Point2f> Corners;
	Corners.reserve(BoardSize.height * BoardSize.width);
	ImageSize = Image.size();
	if (ImageSize.empty())
	{
		return false;
	}

	bool bFound = findChessboardCorners(Image, BoardSize, Corners, CV_CALIB_CB_ADAPTIVE_THRESH | CV_CALIB_CB_FAST_CHECK | CV_CALIB_CB_NORMALIZE_IMAGE);
	if (bFound)
	{
		Mat Gray;
		cvtColor(Image, Gray, CV_BGR2GRAY);
		cornerSubPix(Gray, Corners, Size(11, 11), Size(-1, -1), TermCriteria(CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));
		ImagePoints.push_back(Corners);
	}
	return bFound;
}
#endif

bool UMROpenCVCalibrator::CalculateLensParameters(struct FMRLensDistortion& OutLensDistortion, float& OutHFOV, float& OutVFOV, float& OutAspectRatio)
{
#if WITH_OPENCV

	if (ImagePoints.empty())
	{
		return false;
	}

	Mat DistCoeffs;
	Mat CameraMatrix = Mat::eye(3, 3, CV_64F);

	{
		// calibrateCamera returns rotation and translation vectors, even though we don't use them, we need to 
		// reserve space for them and pass in these arrays by reference.
		std::vector<Mat> Rvecs, Tvecs;
		Rvecs.reserve(ImagePoints.size());
		Tvecs.reserve(ImagePoints.size());

		// calibrateCamera requires object points for each image capture, even though they're all the same object
		// (the chessboard) in all cases.
		std::vector<std::vector<Point3f>> ObjectPoints;
		ObjectPoints.resize(ImagePoints.size(), BoardPoints);

		calibrateCamera(ObjectPoints, ImagePoints, ImageSize, CameraMatrix, DistCoeffs, Rvecs, Tvecs);

	}

	if (!checkRange(CameraMatrix) || !checkRange(DistCoeffs))
	{
		return false;
	}

	// Convert the params to a UE4 struct
	{
		// The DistCoeffs matrix is a one row matrix:
		check(DistCoeffs.rows == 1);
		OutLensDistortion.K1 = DistCoeffs.at<double>(0);
		OutLensDistortion.K2 = DistCoeffs.at<double>(1);
		OutLensDistortion.P1 = DistCoeffs.at<double>(2);
		OutLensDistortion.P2 = DistCoeffs.at<double>(3);

		// The docs make it sound like third (and fourth and fifth) radial coefficients may be optional
		OutLensDistortion.K3 = DistCoeffs.cols >= 5 ? DistCoeffs.at<double>(4) : .0f;

		check(CameraMatrix.rows == 3 && CameraMatrix.cols == 3);
		OutLensDistortion.F.X = CameraMatrix.at<double>(0, 0);
		OutLensDistortion.F.Y = CameraMatrix.at<double>(1, 1);
		OutLensDistortion.C.X = CameraMatrix.at<double>(0, 2);
		OutLensDistortion.C.Y = CameraMatrix.at<double>(1, 2);
	}

	// Estimate field of view
	{
		double AspectRatio, FovX, FovY, FocalLength_Unused;
		Point2d PrincipalPoint_Unused;
		// We pass in zero aperture size as it is unknown. (It is only required for calculating focal length and the principal point)
		calibrationMatrixValues(CameraMatrix, ImageSize, 0.0, 0.0, FovX, FovY, FocalLength_Unused, PrincipalPoint_Unused, AspectRatio);
		OutHFOV = FovX;
		OutVFOV = FovY;
		OutAspectRatio = AspectRatio;
	}
	return true;
#else
	return false;
#endif
}
