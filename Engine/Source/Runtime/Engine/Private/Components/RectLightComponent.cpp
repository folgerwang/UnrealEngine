// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightComponent.cpp: PointLightComponent implementation.
=============================================================================*/

#include "Components/RectLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "RenderingThread.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "PointLightSceneProxy.h"

extern int32 GAllowPointLightCubemapShadows;

class FRectLightSceneProxy : public FLocalLightSceneProxy
{
public:
	float		SourceWidth;
	float		SourceHeight;
	UTexture*	SourceTexture;

	FRectLightSceneProxy(const URectLightComponent* Component)
	:	FLocalLightSceneProxy(Component)
	,	SourceWidth(Component->SourceWidth)
	,	SourceHeight(Component->SourceHeight)
	,	SourceTexture(Component->SourceTexture)
	{}

	virtual bool IsRectLight() const override
	{
		return true;
	}

	virtual bool HasSourceTexture() const override
	{
		return SourceTexture != nullptr;
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetParameters(FLightParameters& LightParameters) const override
	{
		LightParameters.LightPositionAndInvRadius = FVector4(
			GetOrigin(),
			InvRadius);

		FLinearColor LightColor = GetColor();
		LightColor /= 0.5f * SourceWidth * SourceHeight;
		
		LightParameters.LightColorAndFalloffExponent = FVector4(
			LightColor.R,
			LightColor.G,
			LightColor.B,
			0.0f);

		const FVector ZAxis(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);

		LightParameters.NormalizedLightDirection = -GetDirection();
		LightParameters.NormalizedLightTangent = ZAxis;
		LightParameters.SpotAngles = FVector2D( -2.0f, 1.0f );
		LightParameters.SpecularScale = SpecularScale;
		LightParameters.LightSourceRadius = SourceWidth * 0.5f;
		LightParameters.LightSoftSourceRadius = 0.0f;
		LightParameters.LightSourceLength = SourceHeight * 0.5f;
		LightParameters.SourceTexture = SourceTexture ? SourceTexture->Resource : GWhiteTexture;
	}

	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
	{
		if (ViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::SM4
			&& GAllowPointLightCubemapShadows != 0)
		{
			FWholeSceneProjectedShadowInitializer& OutInitializer = *new(OutInitializers) FWholeSceneProjectedShadowInitializer;
			OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
			OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
			OutInitializer.Scales = FVector(1, 1, 1);
			OutInitializer.FaceDirection = FVector(0,0,1);
			OutInitializer.SubjectBounds = FBoxSphereBounds(FVector(0, 0, 0),FVector(Radius,Radius,Radius),Radius);
			OutInitializer.WAxis = FVector4(0,0,1,0);
			OutInitializer.MinLightW = 0.1f;
			OutInitializer.MaxDistanceToCastInLightW = Radius;
			OutInitializer.bOnePassPointLightShadow = true;
			OutInitializer.bRayTracedDistanceField = UseRayTracedDistanceFieldShadows() && DoesPlatformSupportDistanceFieldShadowing(ViewFamily.GetShaderPlatform());
			return true;
		}
		
		return false;
	}
};


URectLightComponent::URectLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightPoint"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightPointMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	SourceWidth = 64.0f;
	SourceHeight = 64.0f;
	SourceTexture = nullptr;
}

FLightSceneProxy* URectLightComponent::CreateSceneProxy() const
{
	return new FRectLightSceneProxy(this);
}

void URectLightComponent::SetSourceWidth(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceWidth != NewValue)
	{
		SourceWidth = NewValue;
		MarkRenderStateDirty();
	}
}

void URectLightComponent::SetSourceHeight(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceHeight != NewValue)
	{
		SourceHeight = NewValue;
		MarkRenderStateDirty();
	}
}

float URectLightComponent::ComputeLightBrightness() const
{
	float LightBrightness = Super::ComputeLightBrightness();

	if (IntensityUnits == ELightUnits::Candelas)
	{
		LightBrightness *= (100.f * 100.f); // Conversion from cm2 to m2
	}
	else if (IntensityUnits == ELightUnits::Lumens)
	{
		LightBrightness *= (100.f * 100.f / PI); // Conversion from cm2 to m2 and PI from the cosine distribution
	}
	else
	{
		LightBrightness *= 16; // Legacy scale of 16
	}

	return LightBrightness;
}

#if WITH_EDITOR
void URectLightComponent::SetLightBrightness(float InBrightness)
{
	if (IntensityUnits == ELightUnits::Candelas)
	{
		Super::SetLightBrightness(InBrightness / (100.f * 100.f)); // Conversion from cm2 to m2
	}
	else if (IntensityUnits == ELightUnits::Lumens)
	{
		Super::SetLightBrightness(InBrightness / (100.f * 100.f / PI)); // Conversion from cm2 to m2 and PI from the cosine distribution
	}
	else
	{
		Super::SetLightBrightness(InBrightness / 16); // Legacy scale of 16
	}
}
#endif // WITH_EDITOR

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType URectLightComponent::GetLightType() const
{
	return LightType_Rect;
}

float URectLightComponent::GetUniformPenumbraSize() const
{
	if (LightmassSettings.bUseAreaShadowsForStationaryLight)
	{
		// Interpret distance as shadow factor directly
		return 1.0f;
	}
	else
	{
		float SourceRadius = FMath::Sqrt( SourceWidth * SourceHeight );
		// Heuristic to derive uniform penumbra size from light source radius
		return FMath::Clamp(SourceRadius == 0 ? .05f : SourceRadius * .005f, .0001f, 1.0f);
	}
}

#if WITH_EDITOR
/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	UProperty that has been changed, NULL if unknown
 */
void URectLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SourceWidth  = FMath::Max(1.0f, SourceWidth);
	SourceHeight = FMath::Max(1.0f, SourceHeight);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR