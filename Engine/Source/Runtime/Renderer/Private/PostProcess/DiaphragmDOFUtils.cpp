// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

} // namespace

void DiaphragmDOF::FPhysicalCocModel::Compile(const FViewInfo& View)
{
	// Fetches DOF settings.
	{
		FocusDistance = View.FinalPostProcessSettings.DepthOfFieldFocalDistance;

		// -because foreground Coc are negative.
		MinForegroundCocRadius = -CVarMaxForegroundRadius.GetValueOnRenderThread();
		MaxBackgroundCocRadius = CVarMaxBackgroundRadius.GetValueOnRenderThread();

		MaxDepthBlurRadius = View.FinalPostProcessSettings.DepthOfFieldDepthBlurRadius / 1920.0f;

		// Circle DOF was actually computing in this depth blur radius in half res.
		MaxDepthBlurRadius *= 2.0f;

		DepthBlurExponent = 1.0f / (View.FinalPostProcessSettings.DepthOfFieldDepthBlurAmount * 100000.0f);
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
	float CocRadius = ((SceneDepth - FocusDistance) / SceneDepth) * InfinityBackgroundCocRadius;

	// Depth blur based.
	float DepthBlurAbsRadius = (1.0 - FMath::Exp2(-SceneDepth * DepthBlurExponent)) * MaxDepthBlurRadius;

	float ReturnCoc = FMath::Max(FMath::Abs(CocRadius), DepthBlurAbsRadius);
	if (CocRadius < 0.0)
	{
		// near CoC is using negative values
		ReturnCoc = -ReturnCoc;
	}
	return HorizontalResolution * FMath::Clamp(ReturnCoc, MinForegroundCocRadius, MaxBackgroundCocRadius);
}

void DiaphragmDOF::FBokehModel::Compile(const FViewInfo& View)
{
	{
		DiaphragmBladeCount = FMath::Clamp(View.FinalPostProcessSettings.DepthOfFieldBladeCount, 4, 16);
	}

	float Fstop = View.FinalPostProcessSettings.DepthOfFieldFstop;
	float MinFstop = View.FinalPostProcessSettings.DepthOfFieldMinFstop > 0 ? View.FinalPostProcessSettings.DepthOfFieldMinFstop : 0;

	const float CircumscribedRadius = 1.0f;

	// Target a constant bokeh area to be eenergy preservative.
	const float TargetedBokehArea = PI * (CircumscribedRadius * CircumscribedRadius);

	// Always uses circle if max aparture is smaller or equal to aperture. 
	if (Fstop <= MinFstop)
	{
		BokehShape = EBokehShape::Circle;

		CocRadiusToCircumscribedRadius = 1.0f;
		CocRadiusToIncircleRadius = 1.0f;
		DiaphragmBladeCount = 0;
		DiaphragmRotation = 0;
	}
	// Uses straight blades when max aperture is infinitely large. 
	else if (MinFstop == 0.0)
	{
		BokehShape = EBokehShape::StraightBlades;

		const float BladeCoverageAngle = PI / DiaphragmBladeCount;

		// Compute CocRadiusToCircumscribedRadius coc that the area of the boked remains identical,
		// to be energy conservative acorss the DiaphragmBladeCount.
		const float TriangleArea = ((CircumscribedRadius * CircumscribedRadius) *
			FMath::Cos(BladeCoverageAngle) *
			FMath::Sin(BladeCoverageAngle));
		const float CircleRadius = FMath::Sqrt(DiaphragmBladeCount * TriangleArea / TargetedBokehArea);

		CocRadiusToCircumscribedRadius = CircumscribedRadius / CircleRadius;
		CocRadiusToIncircleRadius = CocRadiusToCircumscribedRadius * FMath::Cos(PI / DiaphragmBladeCount);
		DiaphragmRotation = 0; // TODO.
	}
	else // if (BokehShape == EBokehShape::RoundedBlades)
	{
		BokehShape = EBokehShape::RoundedBlades;

		// Angle covered by a single blade in the bokeh.
		float BladeCoverageAngle = PI / DiaphragmBladeCount;

		// Blade radius for CircumscribedRadius == 1.0.
		// TODO: this computation is not very accurate.
		float BladeRadius = CircumscribedRadius * Fstop / MinFstop;

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

		// Compute the coordinate where the blade rotate.
		float BladePivotCenterX = 0.5 * (BladeRadius - CircumscribedRadius);
		float BladePivotCenterY = FMath::Sqrt(BladeRadius * BladeRadius - BladePivotCenterX * BladePivotCenterX);

		DiaphragmRotation = FMath::Atan2(BladePivotCenterX, BladePivotCenterY);

		RoundedBlades.DiaphragmBladeRadius = UpscaleFactor * BladeRadius;
		RoundedBlades.DiaphragmBladeCenterOffset = UpscaleFactor * BladeCircleOffset;

		CocRadiusToCircumscribedRadius = UpscaleFactor * CircumscribedRadius;
		CocRadiusToIncircleRadius = UpscaleFactor * (BladeRadius - BladeCircleOffset);
	}
}