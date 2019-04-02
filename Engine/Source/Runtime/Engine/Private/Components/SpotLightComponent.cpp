// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SpotLightComponent.cpp: LightComponent implementation.
=============================================================================*/

#include "Components/SpotLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "PointLightSceneProxy.h"


/**
 * The scene info for a spot light.
 */
class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:

	/** Outer cone angle in radians, clamped to a valid range. */
	float OuterConeAngle;

	/** Cosine of the spot light's inner cone angle. */
	float CosInnerCone;

	/** Cosine of the spot light's outer cone angle. */
	float CosOuterCone;

	/** 1 / (CosInnerCone - CosOuterCone) */
	float InvCosConeDifference;

	/** Sine of the spot light's outer cone angle. */
	float SinOuterCone;

	/** 1 / Tangent of the spot light's outer cone angle. */
	float InvTanOuterCone;

	/** Cosine of the spot light's outer light shaft cone angle. */
	float CosLightShaftConeAngle;

	/** 1 / (appCos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle) */
	float InvCosLightShaftConeDifference;

	/** Initialization constructor. */
	FSpotLightSceneProxy(const USpotLightComponent* Component)
	:	FPointLightSceneProxy(Component)
	{
		const float ClampedInnerConeAngle = FMath::Clamp(Component->InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f;
		const float ClampedOuterConeAngle = FMath::Clamp(Component->OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);
		OuterConeAngle = ClampedOuterConeAngle;
		CosOuterCone = FMath::Cos(ClampedOuterConeAngle);
		SinOuterCone = FMath::Sin(ClampedOuterConeAngle);
		CosInnerCone = FMath::Cos(ClampedInnerConeAngle);
		InvCosConeDifference = 1.0f / (CosInnerCone - CosOuterCone);
		InvTanOuterCone = 1.0f / FMath::Tan(ClampedOuterConeAngle);
		const float ClampedOuterLightShaftConeAngle = FMath::Clamp(Component->LightShaftConeAngle * (float)PI / 180.0f, 0.001f, 89.0f * (float)PI / 180.0f + 0.001f);
		// Use half the outer light shaft cone angle as the inner angle to provide a nice falloff
		// Not exposing the inner light shaft cone angle as it is probably not needed
		const float ClampedInnerLightShaftConeAngle = .5f * ClampedOuterLightShaftConeAngle;
		CosLightShaftConeAngle = FMath::Cos(ClampedOuterLightShaftConeAngle);
		InvCosLightShaftConeDifference = 1.0f / (FMath::Cos(ClampedInnerLightShaftConeAngle) - CosLightShaftConeAngle);
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetLightShaderParameters(FLightShaderParameters& LightParameters) const override
	{
		LightParameters.Position = GetOrigin();
		LightParameters.InvRadius = InvRadius;
		LightParameters.Color = FVector(GetColor());
		LightParameters.FalloffExponent = FalloffExponent;
		LightParameters.Direction = -GetDirection();
		LightParameters.Tangent = FVector(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);
		LightParameters.SpotAngles = FVector2D(CosOuterCone, InvCosConeDifference);
		LightParameters.SpecularScale = SpecularScale;
		LightParameters.SourceRadius = SourceRadius;
		LightParameters.SoftSourceRadius = SoftSourceRadius;
		LightParameters.SourceLength = SourceLength;
		LightParameters.SourceTexture = GWhiteTexture->TextureRHI;
	}

	// FLightSceneInfo interface.
	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const override
	{
		if(!FLocalLightSceneProxy::AffectsBounds(Bounds))
		{
			return false;
		}

		FVector	U = GetOrigin() - (Bounds.SphereRadius / SinOuterCone) * GetDirection(),
				D = Bounds.Origin - U;
		float	dsqr = D | D,
				E = GetDirection() | D;
		if(E > 0.0f && E * E >= dsqr * FMath::Square(CosOuterCone))
		{
			D = Bounds.Origin - GetOrigin();
			dsqr = D | D;
			E = -(GetDirection() | D);
			if(E > 0.0f && E * E >= dsqr * FMath::Square(SinOuterCone))
				return dsqr <= FMath::Square(Bounds.SphereRadius);
			else
				return true;
		}

		return false;
	}
	
	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const override
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = *new(OutInitializers) FWholeSceneProjectedShadowInitializer;
		OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
		OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
		OutInitializer.Scales = FVector(1.0f,InvTanOuterCone,InvTanOuterCone);
		OutInitializer.FaceDirection = FVector(1,0,0);

		const FSphere AbsoluteBoundingSphere = FSpotLightSceneProxy::GetBoundingSphere();
		OutInitializer.SubjectBounds = FBoxSphereBounds(
			AbsoluteBoundingSphere.Center - GetOrigin(),
			FVector(AbsoluteBoundingSphere.W, AbsoluteBoundingSphere.W, AbsoluteBoundingSphere.W),
			AbsoluteBoundingSphere.W
			);

		OutInitializer.WAxis = FVector4(0,0,1,0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.bRayTracedDistanceField = UseRayTracedDistanceFieldShadows() && DoesPlatformSupportDistanceFieldShadowing(ViewFamily.GetShaderPlatform());
		return true;
	}

	virtual float GetOuterConeAngle() const override { return OuterConeAngle; }

	virtual FVector2D GetLightShaftConeParams() const override { return FVector2D(CosLightShaftConeAngle, InvCosLightShaftConeDifference); }

	virtual FSphere GetBoundingSphere() const override
	{
		return FMath::ComputeBoundingSphereForCone(GetOrigin(), GetDirection(), Radius, CosOuterCone, SinOuterCone);
	}

	virtual float GetEffectiveScreenRadius(const FViewMatrices& ShadowViewMatrices) const override
	{
		// Heuristic: use the radius of the inscribed sphere at the cone's end as the light's effective screen radius
		// We do so because we do not want to use the light's radius directly, which will make us overestimate the shadow map resolution greatly for a spot light

		// In the correct form,
		//   InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius() / CosOuterCone
		//   InscribedSphereRadius = GetRadius() / SinOuterCone
		// Do it incorrectly to avoid division which is more expensive and risks division by zero
		const FVector InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius() * CosOuterCone;
		const float InscribedSphereRadius = GetRadius() * SinOuterCone;

		const float SphereDistanceFromViewOrigin = (InscribedSpherePosition - ShadowViewMatrices.GetViewOrigin()).Size();

		return ShadowViewMatrices.GetScreenScale() * InscribedSphereRadius / FMath::Max(SphereDistanceFromViewOrigin, 1.0f);
	}
};

USpotLightComponent::USpotLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpot"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpotMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	InnerConeAngle = 0.0f;
	OuterConeAngle = 44.0f;
}

float USpotLightComponent::GetHalfConeAngle() const
{
	const float ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle, 0.0f, 89.0f) * (float)PI / 180.0f;
	const float ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)PI / 180.0f, ClampedInnerConeAngle + 0.001f, 89.0f * (float)PI / 180.0f + 0.001f);
	return ClampedOuterConeAngle;
}

float USpotLightComponent::GetCosHalfConeAngle() const
{
	return FMath::Cos(GetHalfConeAngle());
}

void USpotLightComponent::SetInnerConeAngle(float NewInnerConeAngle)
{
	if (AreDynamicDataChangesAllowed(false)
		&& NewInnerConeAngle != InnerConeAngle)
	{
		InnerConeAngle = NewInnerConeAngle;
		MarkRenderStateDirty();
	}
}

void USpotLightComponent::SetOuterConeAngle(float NewOuterConeAngle)
{
	if (AreDynamicDataChangesAllowed(false)
		&& NewOuterConeAngle != OuterConeAngle)
	{
		OuterConeAngle = NewOuterConeAngle;
		MarkRenderStateDirty();
	}
}

float USpotLightComponent::ComputeLightBrightness() const
{
	float LightBrightness = ULightComponent::ComputeLightBrightness();

	if (bUseInverseSquaredFalloff)
	{
		if (IntensityUnits == ELightUnits::Candelas)
		{
			LightBrightness *= (100.f * 100.f); // Conversion from cm2 to m2
		}
		else if (IntensityUnits == ELightUnits::Lumens)
		{
			LightBrightness *= (100.f * 100.f / 2.f / PI / (1.f - GetCosHalfConeAngle())); // Conversion from cm2 to m2 and cone remapping.
		}
		else
		{
			LightBrightness *= 16; // Legacy scale of 16
		}
	}
	return LightBrightness;
}

#if WITH_EDITOR
void USpotLightComponent::SetLightBrightness(float InBrightness)
{
	if (bUseInverseSquaredFalloff)
	{
		if (IntensityUnits == ELightUnits::Candelas)
		{
			ULightComponent::SetLightBrightness(InBrightness / (100.f * 100.f)); // Conversion from cm2 to m2
		}
		else if (IntensityUnits == ELightUnits::Lumens)
		{
			ULightComponent::SetLightBrightness(InBrightness / (100.f * 100.f / 2.f / PI / (1.f - GetCosHalfConeAngle()))); // Conversion from cm2 to m2 and cone remapping
		}
		else
		{
			ULightComponent::SetLightBrightness(InBrightness / 16); // Legacy scale of 16
		}
	}
	else
	{
		ULightComponent::SetLightBrightness(InBrightness);
	}
}
#endif // WITH_EDITOR


// Disable for now
//void USpotLightComponent::SetLightShaftConeAngle(float NewLightShaftConeAngle)
//{
//	if (NewLightShaftConeAngle != LightShaftConeAngle)
//	{
//		LightShaftConeAngle = NewLightShaftConeAngle;
//		MarkRenderStateDirty();
//	}
//}

FLightSceneProxy* USpotLightComponent::CreateSceneProxy() const
{
	return new FSpotLightSceneProxy(this);
}

FSphere USpotLightComponent::GetBoundingSphere() const
{
	float ConeAngle = GetHalfConeAngle();
	float CosConeAngle = FMath::Cos(ConeAngle);
	float SinConeAngle = FMath::Sin(ConeAngle);
	return FMath::ComputeBoundingSphereForCone(GetComponentTransform().GetLocation(), GetDirection(), AttenuationRadius, CosConeAngle, SinConeAngle);
}

bool USpotLightComponent::AffectsBounds(const FBoxSphereBounds& InBounds) const
{
	if(!Super::AffectsBounds(InBounds))
	{
		return false;
	}

	float	ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle,0.0f,89.0f) * (float)PI / 180.0f,
			ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)PI / 180.0f + 0.001f);

	float	Sin = FMath::Sin(ClampedOuterConeAngle),
			Cos = FMath::Cos(ClampedOuterConeAngle);

	FVector	U = GetComponentLocation() - (InBounds.SphereRadius / Sin) * GetDirection(),
			D = InBounds.Origin - U;
	float	dsqr = D | D,
			E = GetDirection() | D;
	if(E > 0.0f && E * E >= dsqr * FMath::Square(Cos))
	{
		D = InBounds.Origin - GetComponentLocation();
		dsqr = D | D;
		E = -(GetDirection() | D);
		if(E > 0.0f && E * E >= dsqr * FMath::Square(Sin))
			return dsqr <= FMath::Square(InBounds.SphereRadius);
		else
			return true;
	}

	return false;
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType USpotLightComponent::GetLightType() const
{
	return LightType_Spot;
}

#if WITH_EDITOR

void USpotLightComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == TEXT("InnerConeAngle"))
		{
			OuterConeAngle = FMath::Max(InnerConeAngle, OuterConeAngle);
		}
		else if (PropertyChangedEvent.Property->GetFName() == TEXT("OuterConeAngle"))
		{
			InnerConeAngle = FMath::Min(InnerConeAngle, OuterConeAngle);
		}
	}

	UPointLightComponent::PostEditChangeProperty(PropertyChangedEvent);
}

#endif	// WITH_EDITOR
