// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PostProcessDOF.cpp: Post process Depth of Field implementation.
=============================================================================*/

#include "PostProcess/DiaphragmDOF.h"
#include "PostProcess/PostProcessCircleDOF.h"


namespace 
{

TAutoConsoleVariable<float> CVarMaxForegroundRadius(
	TEXT("r.DOF.Kernel.MaxForegroundRadius"),
	0.025f,
	TEXT("Maximum size of the foreground bluring radius in screen space (default=0.025)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarMaxBackgroundRadius(
	TEXT("r.DOF.Kernel.MaxBackgroundRadius"),
	0.025f,
	TEXT("Maximum size of the background bluring radius in screen space (default=0.025)."),
	ECVF_Scalability | ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarDiaphragmBladeCount(
	TEXT("r.DOF.DiaphragmBladeCount"),
	0,
	TEXT("Number of diaphragm blades to simulate (default = 0 for circle)."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarDiaphragmRotation(
	TEXT("r.DOF.DiaphragmRotation"),
	0.0f,
	TEXT("Rotation of the diaphragm in degrees."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarMaxAperture(
	TEXT("r.DOF.MaxAperture"),
	1.0f,
	TEXT("Max aperture (F-stop unit) used to model rounded diaphragm blades on the bokeh."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarBokehShape(
	TEXT("r.DOF.BokehShape"),
	0,
	TEXT("Shape of the bokeh.\n")
	TEXT(" 0: Always keep the bokeh as a circle (default);\n")
	TEXT(" 1: Model diaphragm blades straight;\n")
	TEXT(" 2: Model diaphragm blades' curvature according to max aperture."),
	ECVF_RenderThreadSafe);

} // namespace

void DiaphragmDOF::FPhysicalCocModel::Compile(const FViewInfo& View)
{
	// Fetches DOF settings.
	{
		FocusDistance = View.FinalPostProcessSettings.DepthOfFieldFocalDistance;

		// -because foreground Coc are negative.
		MinForegroundCocRadius = -CVarMaxForegroundRadius.GetValueOnRenderThread();
		MaxBackgroundCocRadius = CVarMaxBackgroundRadius.GetValueOnRenderThread();
	}

	// Compile coc model equation.
	{

		float FocalLengthInMM = ComputeFocalLengthFromFov(View);

		// Convert focal distance in world position to mm (from cm to mm)
		float FocalDistanceInMM = View.FinalPostProcessSettings.DepthOfFieldFocalDistance * 10.0f;

		// Convert mm to pixels.
		float const SensorWidthInMM = View.FinalPostProcessSettings.DepthOfFieldSensorWidth;

		// Convert f-stop, focal length, and focal distance to
		// projected circle of confusion size at infinity in mm.
		//
		// coc = f * f / (n * (d - f))
		// where,
		//   f = focal length
		//   d = focal distance
		//   n = fstop (where n is the "n" in "f/n")
		float DiameterInMM = FMath::Square(FocalLengthInMM) / (View.FinalPostProcessSettings.DepthOfFieldFstop * (FocalDistanceInMM - FocalLengthInMM));

		// Convert diameter in mm to resolution less radius on the filmback.
		InfinityBackgroundCocRadius = DiameterInMM * 0.5f / SensorWidthInMM;
	}
}

float DiaphragmDOF::FPhysicalCocModel::DepthToResCocRadius(float SceneDepth, float HorizontalResolution) const
{
	return HorizontalResolution * FMath::Clamp(((SceneDepth - FocusDistance) / SceneDepth) * InfinityBackgroundCocRadius, MinForegroundCocRadius, MaxBackgroundCocRadius);
}

void DiaphragmDOF::FBokehModel::Compile(const FViewInfo& View)
{
	// TODO: Should move that to FPostProcessSettings, but Diaphragm DOF is still experimental.
	{
		DiaphragmBladeCount = FMath::Min(CVarDiaphragmBladeCount.GetValueOnRenderThread(), 16);
		if (DiaphragmBladeCount < 4)
		{
			DiaphragmBladeCount = 0;
		}

		DiaphragmRotation = FMath::DegreesToRadians(CVarDiaphragmRotation.GetValueOnRenderThread());
	}

	float Aperture = View.FinalPostProcessSettings.DepthOfFieldFstop;
	float MaxAparture = FMath::Max(CVarMaxAperture.GetValueOnRenderThread(), Aperture);

	{
		BokehShape = EBokehShape::Circle;

		int32 BokehSetting = CVarBokehShape.GetValueOnRenderThread();
		if (BokehSetting == 1)
		{
			BokehShape = EBokehShape::StraightBlades;
		}
		else if (BokehSetting == 2)
		{
			BokehShape = EBokehShape::RoundedBlades;
		}
	}

	const float CircumscribedRadius = 1.0f;

	// Target a constant bokeh area to be eenergy preservative.
	const float TargetedBokehArea = PI * (CircumscribedRadius * CircumscribedRadius);

	if (BokehShape == EBokehShape::Circle ||
		DiaphragmBladeCount == 0 ||
		false) //(BokehShape == EBokehShape::RoundedBlades  && Aperture == MaxAparture))
	{
		CocRadiusToCircumscribedRadius = 1.0f;
		CocRadiusToIncircleRadius = 1.0f;
		BokehShape = EBokehShape::Circle;
		DiaphragmBladeCount = 0;
	}
	else if (BokehShape == EBokehShape::StraightBlades)
	{
		const float BladeCoverageAngle = PI / DiaphragmBladeCount;

		// Compute CocRadiusToCircumscribedRadius coc that the area of the boked remains identical,
		// to be energy conservative acorss the DiaphragmBladeCount.
		const float TriangleArea = ((CircumscribedRadius * CircumscribedRadius) *
			FMath::Cos(BladeCoverageAngle) *
			FMath::Sin(BladeCoverageAngle));
		const float CircleRadius = FMath::Sqrt(DiaphragmBladeCount * TriangleArea / TargetedBokehArea);

		CocRadiusToCircumscribedRadius = CircumscribedRadius / CircleRadius;
		CocRadiusToIncircleRadius = CocRadiusToCircumscribedRadius * FMath::Cos(PI / DiaphragmBladeCount);
	}
	else // if (BokehShape == EBokehShape::RoundedBlades)
	{
		// Angle covered by a single blade in the bokeh.
		float BladeCoverageAngle = PI / DiaphragmBladeCount;

		// Blade radius for CircumscribedRadius == 1.0.
		// TODO: this computation is not very accurate.
		float BladeRadius = CircumscribedRadius * MaxAparture / Aperture;

		// Visible angle of a single blade.
		float BladeVisibleAngle = FMath::Asin((CircumscribedRadius / BladeRadius) * FMath::Sin(BladeCoverageAngle));

		// Distance between the center of the blade's circle and center of the bokeh.
		float BladeCircleOffset = BladeRadius * FMath::Cos(BladeVisibleAngle) - CircumscribedRadius * FMath::Cos(BladeCoverageAngle);

		// Area of the triangle inscribed in the circle radius=CircumscribedRadius.
		float InscribedTriangleArea = ((CircumscribedRadius * CircumscribedRadius) *
			FMath::Cos(BladeCoverageAngle) *
			FMath::Sin(BladeCoverageAngle));

		// Area of the triangle inscribed in the circle radius=BladeRadius.
		float BladeInscribedTriangleArea = ((BladeRadius * BladeRadius) *
			FMath::Cos(BladeVisibleAngle) *
			FMath::Sin(BladeVisibleAngle));

		// Additional area added by the fact the blade has a circle shape and not a straight.
		float AdditonalCircleArea = PI * BladeRadius * BladeRadius * (BladeVisibleAngle / PI) - BladeInscribedTriangleArea;

		// Total area of the bokeh inscribed in circle radius=CircumscribedRadius.
		float InscribedBokedArea = DiaphragmBladeCount * (InscribedTriangleArea + AdditonalCircleArea);

		// Geometric upscale factor for to do target the desired bokeh area.
		float UpscaleFactor = FMath::Sqrt(TargetedBokehArea / InscribedBokedArea);

		RoundedBlades.DiaphragmBladeRadius = UpscaleFactor * BladeRadius;
		RoundedBlades.DiaphragmBladeCenterOffset = UpscaleFactor * BladeCircleOffset;

		CocRadiusToCircumscribedRadius = UpscaleFactor * CircumscribedRadius;
		CocRadiusToIncircleRadius = UpscaleFactor * (BladeRadius - BladeCircleOffset);
	}
}