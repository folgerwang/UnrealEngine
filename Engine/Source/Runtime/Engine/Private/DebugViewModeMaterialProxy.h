// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugViewModeMaterialProxy.h : Contains definitions the debug view mode material shaders.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Engine/TextureStreamingTypes.h"
#include "DebugViewModeHelpers.h"

class FMaterialCompiler;
class UTexture;

#if WITH_EDITORONLY_DATA

/**
 * Material proxy for debug viewmode. Used to prevent debug view mode shaders from being stored in the default material map.
 */
class FDebugViewModeMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:

	FDebugViewModeMaterialProxy()
		: FMaterial()
		, MaterialInterface(nullptr)
		, Material(nullptr)
		, Usage(EMaterialShaderMapUsage::Default)
		, DebugViewMode(DVSM_None)
		, PixelShaderName(nullptr)
		, bValid(true)
		, bIsDefaultMaterial(false)
		, bSynchronousCompilation(true)
	{
		SetQualityLevelProperties(EMaterialQualityLevel::High, false, GMaxRHIFeatureLevel);
	}

	FDebugViewModeMaterialProxy(
		UMaterialInterface* InMaterialInterface, 
		EMaterialQualityLevel::Type QualityLevel, 
		ERHIFeatureLevel::Type FeatureLevel, 
		bool bSynchronousCompilation, 
		EDebugViewShaderMode InDebugViewMode
	);

	void MarkAsInvalid() { bValid = false; }
	bool IsValid() const { return bValid; }
	
	virtual bool RequiresSynchronousCompilation() const override;

	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual bool IsPersistent() const override { return false; }

	// Normally this would cause a bug as the shader map would try to be shared by both, but GetShaderMapUsage() allows this to work
	virtual FGuid GetMaterialId() const override { return Material->StateId; }

	virtual EMaterialShaderMapUsage::Type GetShaderMapUsage() const override { return Usage; }


	virtual bool ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const override
	{
		const FString ShaderTypeName = ShaderType->GetName();
		if (Usage == EMaterialShaderMapUsage::DebugViewMode)
		{
			if (ShaderTypeName.Contains(TEXT("DebugViewMode")))
			{
				return true;
			}
			if (PixelShaderName && ShaderTypeName.Contains(PixelShaderName))
			{
				return true;
			}
		}
		return  false;
	}

	virtual const TArray<UTexture*>& GetReferencedTextures() const override
	{
		return ReferencedTextures;
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual int32 CompilePropertyAndSetMaterialProperty(EMaterialProperty Property, FMaterialCompiler* Compiler, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime) const override
	{
		return MaterialInterface ? MaterialInterface->GetMaterialResource(GMaxRHIFeatureLevel)->CompilePropertyAndSetMaterialProperty(Property, Compiler, OverrideShaderFrequency, bUsePreviousFrameTime) : INDEX_NONE;
	}

#if HANDLE_CUSTOM_OUTPUTS_AS_MATERIAL_ATTRIBUTES
	virtual int32 CompileCustomAttribute(const FGuid& AttributeID, FMaterialCompiler* Compiler) const override
	{
		return MaterialInterface ? MaterialInterface->CompilePropertyEx(Compiler, AttributeID) : INDEX_NONE;
	}
#endif

	virtual FString GetMaterialUsageDescription() const override
	{
		return FString::Printf(TEXT("FDebugViewModeMaterialProxy (%s) %s"), PixelShaderName ? PixelShaderName : TEXT("Undefined"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("null"));
	}

	virtual FString GetFriendlyName() const override 
	{ 
		return FString::Printf(TEXT("DebugViewMode %s"), PixelShaderName ? PixelShaderName : TEXT("Undefined")); 
	}

	virtual UMaterialInterface* GetMaterialInterface() const override
	{
		return MaterialInterface;
	}

	virtual bool IsDefaultMaterial() const 
	{ 
		return bIsDefaultMaterial; 
	};

	friend FArchive& operator<< ( FArchive& Ar, FDebugViewModeMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial& GetMaterialWithFallback(ERHIFeatureLevel::Type InFeatureLevel, const FMaterialRenderProxy*& OutFallbackMaterialRenderProxy) const override;
	virtual bool GetVectorValue(const FMaterialParameterInfo& ParameterInfo, FLinearColor* OutValue, const FMaterialRenderContext& Context) const override;
	virtual bool GetScalarValue(const FMaterialParameterInfo& ParameterInfo, float* OutValue, const FMaterialRenderContext& Context) const override;
	virtual bool GetTextureValue(const FMaterialParameterInfo& ParameterInfo,const UTexture** OutValue, const FMaterialRenderContext& Context) const override;

	virtual EMaterialDomain GetMaterialDomain() const override;
	virtual bool IsTwoSided() const  override;
	virtual bool IsDitheredLODTransition() const  override;
	virtual bool IsLightFunction() const override;
	virtual bool IsDeferredDecal() const override;
	virtual bool IsVolumetricPrimitive() const override { return false; }
	virtual bool IsSpecialEngineMaterial() const override;
	virtual bool IsWireframe() const override;
	virtual bool IsMasked() const override;
	virtual enum EBlendMode GetBlendMode() const override;
	virtual enum EMaterialShadingModel GetShadingModel() const override;
	virtual float GetOpacityMaskClipValue() const override;
	virtual bool GetCastDynamicShadowAsMasked() const override;
	virtual void GatherCustomOutputExpressions(TArray<class UMaterialExpressionCustomOutput*>& OutCustomOutputs) const override;
	virtual void GatherExpressionsForCustomInterpolators(TArray<class UMaterialExpression*>& OutExpressions) const override;

	// Cached material usage.
	virtual bool IsUsedWithSkeletalMesh() const override { return bIsUsedWithSkeletalMesh; }
	virtual bool IsUsedWithLandscape() const override { return bIsUsedWithLandscape; }
	virtual bool IsUsedWithParticleSystem() const override { return bIsUsedWithParticleSystem; }
	virtual bool IsUsedWithParticleSprites() const override { return bIsUsedWithParticleSprites; }
	virtual bool IsUsedWithBeamTrails() const override { return bIsUsedWithBeamTrails; }
	virtual bool IsUsedWithMeshParticles() const override { return bIsUsedWithMeshParticles; }
	virtual bool IsUsedWithNiagaraSprites() const override { return bIsUsedWithNiagaraSprites; }
	virtual bool IsUsedWithNiagaraRibbons() const override { return bIsUsedWithNiagaraRibbons; }
	virtual bool IsUsedWithNiagaraMeshParticles() const override { return bIsUsedWithNiagaraMeshParticles; }
	virtual bool IsUsedWithMorphTargets() const override { return bIsUsedWithMorphTargets; }
	virtual bool IsUsedWithSplineMeshes() const override { return bIsUsedWithSplineMeshes; }
	virtual bool IsUsedWithInstancedStaticMeshes() const override { return bIsUsedWithInstancedStaticMeshes; }
	virtual bool IsUsedWithAPEXCloth() const override { return bIsUsedWithAPEXCloth; }

	virtual EMaterialShaderMapUsage::Type GetMaterialShaderMapUsage() const { return Usage; }

	////////////////

	static void AddShader(
		UMaterialInterface* InMaterialInterface, 
		EMaterialQualityLevel::Type QualityLevel, 
		ERHIFeatureLevel::Type FeatureLevel, 
		bool bSynchronousCompilation, 
		EDebugViewShaderMode InDebugViewMode
	);
	static bool GetShader(const UMaterialInterface* InMaterialInterface, EDebugViewShaderMode InDebugViewMode, ERHIFeatureLevel::Type InFeatureLevel, const FMaterialRenderProxy*& OutMaterialRenderProxy, const FMaterial*& OutMaterial);
	static void ClearAllShaders(UMaterialInterface* InMaterialInterface);
	static bool HasAnyShaders() { return DebugMaterialShaderMap.Num() > 0; }
	static void ValidateAllShaders(TSet<UMaterialInterface*>& Materials);
	static bool MissingShadersChanged() { return bMissingShadersChanged; }
	static void ClearMissingShadersFlag() { bMissingShadersChanged = false; }

private:

	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	UMaterial* Material;	
	TArray<UTexture*> ReferencedTextures;
	EMaterialShaderMapUsage::Type Usage;
	EDebugViewShaderMode DebugViewMode;
	const TCHAR* PixelShaderName;

	/** Cached material usage. */
	union
	{
		uint32 CachedMaterialUsage;
		struct 
		{
			uint32 bIsUsedWithSkeletalMesh : 1;
			uint32 bIsUsedWithLandscape : 1;
			uint32 bIsUsedWithParticleSystem : 1;
			uint32 bIsUsedWithParticleSprites : 1;
			uint32 bIsUsedWithBeamTrails : 1;
			uint32 bIsUsedWithMeshParticles : 1;
			uint32 bIsUsedWithNiagaraSprites : 1;
			uint32 bIsUsedWithNiagaraRibbons : 1;
			uint32 bIsUsedWithNiagaraMeshParticles : 1;
			uint32 bIsUsedWithMorphTargets : 1;
			uint32 bIsUsedWithSplineMeshes : 1;
			uint32 bIsUsedWithInstancedStaticMeshes : 1;
			uint32 bIsUsedWithAPEXCloth : 1;
		};
	};

	/** Whether this debug material should be used or not. */
	bool bValid;
	bool bIsDefaultMaterial;
	bool bSynchronousCompilation;

	struct FMaterialKey
	{
		FMaterialKey(const UMaterialInterface* InMaterialInterface, EDebugViewShaderMode InDebugViewMode, ERHIFeatureLevel::Type InFeatureLevel) : MaterialInterface(InMaterialInterface), DebugViewMode((uint32)InDebugViewMode), FeatureLevel((uint32)InFeatureLevel) {}

		const UMaterialInterface* MaterialInterface;
		uint32 DebugViewMode;
		uint32 FeatureLevel;

		friend bool operator==(const FMaterialKey& Lhs, const FMaterialKey& Rhs)
		{
			return Lhs.MaterialInterface == Rhs.MaterialInterface
				&& Lhs.DebugViewMode == Rhs.DebugViewMode
				&& Lhs.FeatureLevel == Rhs.FeatureLevel;
		}

		friend uint32 GetTypeHash(const FMaterialKey& Key)
		{
			const uint32 DebugViewModeShift = 6;
			static_assert((2 ^ DebugViewModeShift) < DVSM_MAX, "Bit shift too small!");

			return GetTypeHash(Key.MaterialInterface) ^ GetTypeHash((Key.FeatureLevel << DebugViewModeShift) ^ Key.DebugViewMode);
		}
	};

	static volatile bool bReentrantCall;
	static TMap<FMaterialKey, FDebugViewModeMaterialProxy*> DebugMaterialShaderMap;
	static TSet<FMaterialKey> MissingShaders;
	static bool bMissingShadersChanged;
};

#endif
