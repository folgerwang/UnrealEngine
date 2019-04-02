// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LightRendering.h: Light rendering declarations.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "SceneRendering.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "LightSceneInfo.h"

/** Uniform buffer for rendering deferred lights. */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FDeferredLightUniformStruct,)
	SHADER_PARAMETER(FVector4,ShadowMapChannelMask)
	SHADER_PARAMETER(FVector2D,DistanceFadeMAD)
	SHADER_PARAMETER(float, ContactShadowLength)
	SHADER_PARAMETER(float, VolumetricScatteringIntensity)
	SHADER_PARAMETER(uint32,ShadowedBits)
	SHADER_PARAMETER(uint32,LightingChannelMask)
	SHADER_PARAMETER_STRUCT_INCLUDE(FLightShaderParameters, LightParameters)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

extern uint32 GetShadowQuality();

extern float GetLightFadeFactor(const FSceneView& View, const FLightSceneProxy* Proxy);

template<typename ShaderRHIParamRef>
void SetDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FLightSceneInfo* LightSceneInfo,
	const FSceneView& View)
{
	FDeferredLightUniformStruct DeferredLightUniformsValue;
	LightSceneInfo->Proxy->GetLightShaderParameters(DeferredLightUniformsValue.LightParameters);
	
	const FVector2D FadeParams = LightSceneInfo->Proxy->GetDirectionalLightDistanceFadeParameters(View.GetFeatureLevel(), LightSceneInfo->IsPrecomputedLightingValid(), View.MaxShadowCascades);

	// use MAD for efficiency in the shader
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(FadeParams.Y, -FadeParams.X * FadeParams.Y);

	int32 ShadowMapChannel = LightSceneInfo->Proxy->GetShadowMapChannel();

	static const auto AllowStaticLightingVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.AllowStaticLighting"));
	const bool bAllowStaticLighting = (!AllowStaticLightingVar || AllowStaticLightingVar->GetValueOnRenderThread() != 0);

	if (!bAllowStaticLighting)
	{
		ShadowMapChannel = INDEX_NONE;
	}

	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(
		ShadowMapChannel == 0 ? 1 : 0,
		ShadowMapChannel == 1 ? 1 : 0,
		ShadowMapChannel == 2 ? 1 : 0,
		ShadowMapChannel == 3 ? 1 : 0);

	const bool bDynamicShadows = View.Family->EngineShowFlags.DynamicShadows && GetShadowQuality() > 0;
	const bool bHasLightFunction = LightSceneInfo->Proxy->GetLightFunctionMaterial() != NULL;
	DeferredLightUniformsValue.ShadowedBits  = LightSceneInfo->Proxy->CastsStaticShadow() || bHasLightFunction ? 1 : 0;
	DeferredLightUniformsValue.ShadowedBits |= LightSceneInfo->Proxy->CastsDynamicShadow() && View.Family->EngineShowFlags.DynamicShadows ? 3 : 0;

	DeferredLightUniformsValue.VolumetricScatteringIntensity = LightSceneInfo->Proxy->GetVolumetricScatteringIntensity();

	static auto* ContactShadowsCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ContactShadows"));
	DeferredLightUniformsValue.ContactShadowLength = 0;

	if (ContactShadowsCVar && ContactShadowsCVar->GetValueOnRenderThread() != 0 && View.Family->EngineShowFlags.ContactShadows)
	{
		DeferredLightUniformsValue.ContactShadowLength = LightSceneInfo->Proxy->GetContactShadowLength();
		// Sign indicates if contact shadow length is in world space or screen space.
		// Multiply by 2 for screen space in order to preserve old values after introducing multiply by View.ClipToView[1][1] in shader.
		DeferredLightUniformsValue.ContactShadowLength *= LightSceneInfo->Proxy->IsContactShadowLengthInWS() ? -1.0f : 2.0f;
	}

	// When rendering reflection captures, the direct lighting of the light is actually the indirect specular from the main view
	if (View.bIsReflectionCapture)
	{
		DeferredLightUniformsValue.LightParameters.Color *= LightSceneInfo->Proxy->GetIndirectLightingScale();
	}

	const ELightComponentType LightType = (ELightComponentType)LightSceneInfo->Proxy->GetLightType();
	if ((LightType == LightType_Point || LightType == LightType_Spot || LightType == LightType_Rect) && View.IsPerspectiveProjection())
	{
		DeferredLightUniformsValue.LightParameters.Color *= GetLightFadeFactor(View, LightSceneInfo->Proxy);
	}

	DeferredLightUniformsValue.LightingChannelMask = LightSceneInfo->Proxy->GetLightingChannelMask();

	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI,DeferredLightUniformBufferParameter,DeferredLightUniformsValue);
}

template<typename ShaderRHIParamRef>
void SetSimpleDeferredLightParameters(
	FRHICommandList& RHICmdList, 
	const ShaderRHIParamRef ShaderRHI, 
	const TShaderUniformBufferParameter<FDeferredLightUniformStruct>& DeferredLightUniformBufferParameter, 
	const FSimpleLightEntry& SimpleLight,
	const FSimpleLightPerViewEntry &SimpleLightPerViewData,
	const FSceneView& View)
{
	FDeferredLightUniformStruct DeferredLightUniformsValue;
	DeferredLightUniformsValue.LightParameters.Position = SimpleLightPerViewData.Position;
	DeferredLightUniformsValue.LightParameters.InvRadius = 1.0f / FMath::Max(SimpleLight.Radius, KINDA_SMALL_NUMBER);
	DeferredLightUniformsValue.LightParameters.Color = SimpleLight.Color;
	DeferredLightUniformsValue.LightParameters.FalloffExponent = SimpleLight.Exponent;
	DeferredLightUniformsValue.LightParameters.Direction = FVector(1, 0, 0);
	DeferredLightUniformsValue.LightParameters.Tangent = FVector(1, 0, 0);
	DeferredLightUniformsValue.LightParameters.SpotAngles = FVector2D(-2, 1);
	DeferredLightUniformsValue.LightParameters.SpecularScale = 1.0f;
	DeferredLightUniformsValue.LightParameters.SourceRadius = 0.0f;
	DeferredLightUniformsValue.LightParameters.SoftSourceRadius = 0.0f;
	DeferredLightUniformsValue.LightParameters.SourceLength = 0.0f;
	DeferredLightUniformsValue.LightParameters.SourceTexture = GWhiteTexture->TextureRHI;
	DeferredLightUniformsValue.ContactShadowLength = 0.0f;
	DeferredLightUniformsValue.DistanceFadeMAD = FVector2D(0, 0);
	DeferredLightUniformsValue.ShadowMapChannelMask = FVector4(0, 0, 0, 0);
	DeferredLightUniformsValue.ShadowedBits = 0;
	DeferredLightUniformsValue.LightingChannelMask = 0;

	SetUniformBufferParameterImmediate(RHICmdList, ShaderRHI, DeferredLightUniformBufferParameter, DeferredLightUniformsValue);
}

/** Shader parameters needed to render a light function. */
class FLightFunctionSharedParameters
{
public:

	void Bind(const FShaderParameterMap& ParameterMap)
	{
		LightFunctionParameters.Bind(ParameterMap,TEXT("LightFunctionParameters"));
	}

	template<typename ShaderRHIParamRef>
	void Set(FRHICommandList& RHICmdList, const ShaderRHIParamRef ShaderRHI, const FLightSceneInfo* LightSceneInfo, float ShadowFadeFraction) const
	{
		const bool bIsSpotLight = LightSceneInfo->Proxy->GetLightType() == LightType_Spot;
		const bool bIsPointLight = LightSceneInfo->Proxy->GetLightType() == LightType_Point;
		const float TanOuterAngle = bIsSpotLight ? FMath::Tan(LightSceneInfo->Proxy->GetOuterConeAngle()) : 1.0f;

		SetShaderValue( 
			RHICmdList, 
			ShaderRHI, 
			LightFunctionParameters, 
			FVector4(TanOuterAngle, ShadowFadeFraction, bIsSpotLight ? 1.0f : 0.0f, bIsPointLight ? 1.0f : 0.0f));
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FLightFunctionSharedParameters& P)
	{
		Ar << P.LightFunctionParameters;
		return Ar;
	}

private:

	FShaderParameter LightFunctionParameters;
};

/** Utility functions for drawing a sphere */
namespace StencilingGeometry
{
	/**
	* Draws a sphere using RHIDrawIndexedPrimitive, useful as approximate bounding geometry for deferred passes.
	* Note: The sphere will be of unit size unless transformed by the shader. 
	*/
	extern void DrawSphere(FRHICommandList& RHICmdList);
	/**
	 * Draws exactly the same as above, but uses FVector rather than FVector4 vertex data.
	 */
	extern void DrawVectorSphere(FRHICommandList& RHICmdList);
	/** Renders a cone with a spherical cap, used for rendering spot lights in deferred passes. */
	extern void DrawCone(FRHICommandList& RHICmdList);

	/** 
	* Vertex buffer for a sphere of unit size. Used for drawing a sphere as approximate bounding geometry for deferred passes.
	*/
	template<int32 NumSphereSides, int32 NumSphereRings, typename VectorType>
	class TStencilSphereVertexBuffer : public FVertexBuffer
	{
	public:

		int32 GetNumRings() const
		{
			return NumSphereRings;
		}

		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			const int32 NumSides = NumSphereSides;
			const int32 NumRings = NumSphereRings;
			const int32 NumVerts = (NumSides + 1) * (NumRings + 1);

			const float RadiansPerRingSegment = PI / (float)NumRings;
			float Radius = 1;

			TArray<VectorType, TInlineAllocator<NumRings + 1> > ArcVerts;
			ArcVerts.Empty(NumRings + 1);
			// Calculate verts for one arc
			for (int32 i = 0; i < NumRings + 1; i++)
			{
				const float Angle = i * RadiansPerRingSegment;
				ArcVerts.Add(FVector(0.0f, FMath::Sin(Angle), FMath::Cos(Angle)));
			}

			TResourceArray<VectorType, VERTEXBUFFER_ALIGNMENT> Verts;
			Verts.Empty(NumVerts);
			// Then rotate this arc NumSides + 1 times.
			const FVector Center = FVector(0,0,0);
			for (int32 s = 0; s < NumSides + 1; s++)
			{
				FRotator ArcRotator(0, 360.f * ((float)s / NumSides), 0);
				FRotationMatrix ArcRot( ArcRotator );

				for (int32 v = 0; v < NumRings + 1; v++)
				{
					const int32 VIx = (NumRings + 1) * s + v;
					Verts.Add(Center + Radius * ArcRot.TransformPosition(ArcVerts[v]));
				}
			}

			NumSphereVerts = Verts.Num();
			uint32 Size = Verts.GetResourceDataSize();

			// Create vertex buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Verts);
			VertexBufferRHI = RHICreateVertexBuffer(Size,BUF_Static,CreateInfo);
		}

		int32 GetVertexCount() const { return NumSphereVerts; }

	/** 
	* Calculates the world transform for a sphere.
	* @param OutTransform - The output world transform.
	* @param Sphere - The sphere to generate the transform for.
	* @param PreViewTranslation - The pre-view translation to apply to the transform.
	* @param bConservativelyBoundSphere - when true, the sphere that is drawn will contain all positions in the analytical sphere,
	*		 Otherwise the sphere vertices will lie on the analytical sphere and the positions on the faces will lie inside the sphere.
	*/
	void CalcTransform(FVector4& OutPosAndScale, const FSphere& Sphere, const FVector& PreViewTranslation, bool bConservativelyBoundSphere = true)
	{
		float Radius = Sphere.W;
		if (bConservativelyBoundSphere)
		{
			const int32 NumRings = NumSphereRings;
			const float RadiansPerRingSegment = PI / (float)NumRings;

			// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
			Radius /= FMath::Cos(RadiansPerRingSegment);
		}

		const FVector Translate(Sphere.Center + PreViewTranslation);
		OutPosAndScale = FVector4(Translate, Radius);
	}

	private:
		int32 NumSphereVerts;
	};

	/** 
	* Stenciling sphere index buffer
	*/
	template<int32 NumSphereSides, int32 NumSphereRings>
	class TStencilSphereIndexBuffer : public FIndexBuffer
	{
	public:
		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			const int32 NumSides = NumSphereSides;
			const int32 NumRings = NumSphereRings;
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

			// Add triangles for all the vertices generated
			for (int32 s = 0; s < NumSides; s++)
			{
				const int32 a0start = (s + 0) * (NumRings + 1);
				const int32 a1start = (s + 1) * (NumRings + 1);

				for (int32 r = 0; r < NumRings; r++)
				{
					Indices.Add(a0start + r + 0);
					Indices.Add(a1start + r + 0);
					Indices.Add(a0start + r + 1);
					Indices.Add(a1start + r + 0);
					Indices.Add(a1start + r + 1);
					Indices.Add(a0start + r + 1);
				}
			}

			NumIndices = Indices.Num();
			const uint32 Size = Indices.GetResourceDataSize();
			const uint32 Stride = sizeof(uint16);

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Indices);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static, CreateInfo);
		}

		int32 GetIndexCount() const { return NumIndices; }; 
	
	private:
		int32 NumIndices;
	};

	class FStencilConeIndexBuffer : public FIndexBuffer
	{
	public:
		// A side is a line of vertices going from the cone's origin to the edge of its SphereRadius
		static const int32 NumSides = 18;
		// A slice is a circle of vertices in the cone's XY plane
		static const int32 NumSlices = 12;

		static const uint32 NumVerts = NumSides * NumSlices * 2;

		void InitRHI() override
		{
			TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> Indices;

			Indices.Empty((NumSlices - 1) * NumSides * 12);
			// Generate triangles for the vertices of the cone shape
			for (int32 SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
			{
				for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
				{
					const int32 CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides;
					const int32 NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides;
					const int32 NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides;
					const int32 NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides;

					Indices.Add(CurrentIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceAndSideIndex);
				}
			}

			// Generate triangles for the vertices of the spherical cap
			const int32 CapIndexStart = NumSides * NumSlices;

			for (int32 SliceIndex = 0; SliceIndex < NumSlices - 1; SliceIndex++)
			{
				for (int32 SideIndex = 0; SideIndex < NumSides; SideIndex++)
				{
					const int32 CurrentIndex = SliceIndex * NumSides + SideIndex % NumSides + CapIndexStart;
					const int32 NextSideIndex = SliceIndex * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;
					const int32 NextSliceIndex = (SliceIndex + 1) * NumSides + SideIndex % NumSides + CapIndexStart;
					const int32 NextSliceAndSideIndex = (SliceIndex + 1) * NumSides + (SideIndex + 1) % NumSides + CapIndexStart;

					Indices.Add(CurrentIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSideIndex);
					Indices.Add(NextSliceIndex);
					Indices.Add(NextSliceAndSideIndex);
				}
			}

			const uint32 Size = Indices.GetResourceDataSize();
			const uint32 Stride = sizeof(uint16);

			NumIndices = Indices.Num();

			// Create index buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Indices);
			IndexBufferRHI = RHICreateIndexBuffer(Stride, Size, BUF_Static,CreateInfo);
		}

		int32 GetIndexCount() const { return NumIndices; } 

	protected:
		int32 NumIndices;
	};

	/** 
	* Vertex buffer for a cone. It holds zero'd out data since the actual math is done on the shader
	*/
	class FStencilConeVertexBuffer : public FVertexBuffer
	{
	public:
		static const int32 NumVerts = FStencilConeIndexBuffer::NumSides * FStencilConeIndexBuffer::NumSlices * 2;

		/** 
		* Initialize the RHI for this rendering resource 
		*/
		void InitRHI() override
		{
			TResourceArray<FVector4, VERTEXBUFFER_ALIGNMENT> Verts;
			Verts.Empty(NumVerts);
			for (int32 s = 0; s < NumVerts; s++)
			{
				Verts.Add(FVector4(0, 0, 0, 0));
			}

			uint32 Size = Verts.GetResourceDataSize();

			// Create vertex buffer. Fill buffer with initial data upon creation
			FRHIResourceCreateInfo CreateInfo(&Verts);
			VertexBufferRHI = RHICreateVertexBuffer(Size, BUF_Static, CreateInfo);
		}

		int32 GetVertexCount() const { return NumVerts; }
	};

	extern TGlobalResource<TStencilSphereVertexBuffer<18, 12, FVector4> >	GStencilSphereVertexBuffer;
	extern TGlobalResource<TStencilSphereVertexBuffer<18, 12, FVector> >	GStencilSphereVectorBuffer;
	extern TGlobalResource<TStencilSphereIndexBuffer<18, 12> >				GStencilSphereIndexBuffer;
	extern TGlobalResource<TStencilSphereVertexBuffer<4, 4, FVector4> >		GLowPolyStencilSphereVertexBuffer;
	extern TGlobalResource<TStencilSphereIndexBuffer<4, 4> >				GLowPolyStencilSphereIndexBuffer;
	extern TGlobalResource<FStencilConeVertexBuffer>						GStencilConeVertexBuffer;
	extern TGlobalResource<FStencilConeIndexBuffer>							GStencilConeIndexBuffer;

}; //End StencilingGeometry

/** 
* Stencil geometry parameters used by multiple shaders. 
*/
class FStencilingGeometryShaderParameters
{
public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		StencilGeometryPosAndScale.Bind(ParameterMap, TEXT("StencilingGeometryPosAndScale"));
		StencilConeParameters.Bind(ParameterMap, TEXT("StencilingConeParameters"));
		StencilConeTransform.Bind(ParameterMap, TEXT("StencilingConeTransform"));
		StencilPreViewTranslation.Bind(ParameterMap, TEXT("StencilingPreViewTranslation"));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FVector4& InStencilingGeometryPosAndScale) const
	{
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilGeometryPosAndScale, InStencilingGeometryPosAndScale);
		SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilConeParameters, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
	}

	void Set(FRHICommandList& RHICmdList, FShader* Shader, const FSceneView& View, const FLightSceneInfo* LightSceneInfo) const
	{
		FVector4 GeometryPosAndScale;
		if( LightSceneInfo->Proxy->GetLightType() == LightType_Point ||
			LightSceneInfo->Proxy->GetLightType() == LightType_Rect )
		{
			StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(GeometryPosAndScale, LightSceneInfo->Proxy->GetBoundingSphere(), View.ViewMatrices.GetPreViewTranslation());
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilGeometryPosAndScale, GeometryPosAndScale);
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilConeParameters, FVector4(0.0f, 0.0f, 0.0f, 0.0f));
		}
		else if(LightSceneInfo->Proxy->GetLightType() == LightType_Spot)
		{
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilConeTransform, LightSceneInfo->Proxy->GetLightToWorld());
			SetShaderValue(
				RHICmdList, 
				Shader->GetVertexShader(),
				StencilConeParameters,
				FVector4(
					StencilingGeometry::FStencilConeIndexBuffer::NumSides,
					StencilingGeometry::FStencilConeIndexBuffer::NumSlices,
					LightSceneInfo->Proxy->GetOuterConeAngle(),
					LightSceneInfo->Proxy->GetRadius()));
			SetShaderValue(RHICmdList, Shader->GetVertexShader(), StencilPreViewTranslation, View.ViewMatrices.GetPreViewTranslation());
		}
	}

	/** Serializer. */ 
	friend FArchive& operator<<(FArchive& Ar,FStencilingGeometryShaderParameters& P)
	{
		Ar << P.StencilGeometryPosAndScale;
		Ar << P.StencilConeParameters;
		Ar << P.StencilConeTransform;
		Ar << P.StencilPreViewTranslation;
		return Ar;
	}

private:
	FShaderParameter StencilGeometryPosAndScale;
	FShaderParameter StencilConeParameters;
	FShaderParameter StencilConeTransform;
	FShaderParameter StencilPreViewTranslation;
};


/** A vertex shader for rendering the light in a deferred pass. */
template<bool bRadialLight>
class TDeferredLightVS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(TDeferredLightVS,Global);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return bRadialLight ? IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) : true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	TDeferredLightVS()	{}
	TDeferredLightVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
		StencilingGeometryParameters.Bind(Initializer.ParameterMap);
	}

	void SetParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FLightSceneInfo* LightSceneInfo)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);
		StencilingGeometryParameters.Set(RHICmdList, this, View, LightSceneInfo);
	}

	void SetSimpleLightParameters(FRHICommandList& RHICmdList, const FViewInfo& View, const FSphere& LightBounds)
	{
		FGlobalShader::SetParameters<FViewUniformShaderParameters>(RHICmdList, GetVertexShader(),View.ViewUniformBuffer);

		FVector4 StencilingSpherePosAndScale;
		StencilingGeometry::GStencilSphereVertexBuffer.CalcTransform(StencilingSpherePosAndScale, LightBounds, View.ViewMatrices.GetPreViewTranslation());
		StencilingGeometryParameters.Set(RHICmdList, this, StencilingSpherePosAndScale);
	}

	//~ Begin FShader Interface
	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << StencilingGeometryParameters;
		return bShaderHasOutdatedParameters;
	}
	//~ End FShader Interface

private:

	FStencilingGeometryShaderParameters StencilingGeometryParameters;
};

extern void SetBoundingGeometryRasterizerAndDepthState(FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FSphere& LightBounds);

enum class FLightOcclusionType : uint8
{
	Shadowmap,
	Raytraced,
};
FLightOcclusionType GetLightOcclusionType(const FLightSceneProxy& Proxy);
FLightOcclusionType GetLightOcclusionType(const FLightSceneInfoCompact& LightInfo);

