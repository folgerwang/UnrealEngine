// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
LandscapeEditProcedural.cpp: Landscape editing procedural mode
=============================================================================*/

#include "LandscapeEdit.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeComponent.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#include "LandscapeRenderMobile.h"

#if WITH_EDITOR
#include "LandscapeEditorModule.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Interfaces/ITargetPlatform.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Settings/EditorExperimentalSettings.h"
#endif

#include "Shader.h"
#include "GlobalShader.h"
#include "RendererInterface.h"
#include "PipelineStateCache.h"
#include "MaterialShaderType.h"
#include "EngineModule.h"
#include "ShaderParameterUtils.h"
#include "LandscapeBPCustomBrush.h"

#define LOCTEXT_NAMESPACE "Landscape"

void ALandscapeProxy::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITORONLY_DATA
	if (GetMutableDefault<UEditorExperimentalSettings>()->bProceduralLandscape)
	{
		for (auto& ItPair : RenderDataPerHeightmap)
		{
			FRenderDataPerHeightmap& HeightmapRenderData = ItPair.Value;

			if (HeightmapRenderData.HeightmapsCPUReadBack != nullptr)
			{
				BeginReleaseResource(HeightmapRenderData.HeightmapsCPUReadBack);
			}
		}

		ReleaseResourceFence.BeginFence();
	}
#endif
}

bool ALandscapeProxy::IsReadyForFinishDestroy()
{
	bool bReadyForFinishDestroy = Super::IsReadyForFinishDestroy();

#if WITH_EDITORONLY_DATA
	if (GetMutableDefault<UEditorExperimentalSettings>()->bProceduralLandscape)
	{
		if (bReadyForFinishDestroy)
		{
			bReadyForFinishDestroy = ReleaseResourceFence.IsFenceComplete();
		}
	}
#endif

	return bReadyForFinishDestroy;
}

void ALandscapeProxy::FinishDestroy()
{
	Super::FinishDestroy();

#if WITH_EDITORONLY_DATA
	if (GetMutableDefault<UEditorExperimentalSettings>()->bProceduralLandscape)
	{
		check(ReleaseResourceFence.IsFenceComplete());

		for (auto& ItPair : RenderDataPerHeightmap)
		{
			FRenderDataPerHeightmap& HeightmapRenderData = ItPair.Value;

			delete HeightmapRenderData.HeightmapsCPUReadBack;
			HeightmapRenderData.HeightmapsCPUReadBack = nullptr;
		}
	}
#endif
}

#if WITH_EDITOR

static TAutoConsoleVariable<int32> CVarOutputProceduralDebugDrawCallName(
	TEXT("landscape.OutputProceduralDebugDrawCallName"),
	0,
	TEXT("This will output the name of each draw call for Scope Draw call event. This will allow readable draw call info through RenderDoc, for example."));

static TAutoConsoleVariable<int32> CVarOutputProceduralRTContent(
	TEXT("landscape.OutputProceduralRTContent"),
	0,
	TEXT("This will output the content of render target. This is used for debugging only."));

struct FLandscapeProceduralVertex
{
	FVector2D Position;
	FVector2D UV;
};

struct FLandscapeProceduralTriangle
{
	FLandscapeProceduralVertex V0;
	FLandscapeProceduralVertex V1;
	FLandscapeProceduralVertex V2;
};

/** The filter vertex declaration resource type. */
class FLandscapeProceduralVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FLandscapeProceduralVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		uint32 Stride = sizeof(FLandscapeProceduralVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FLandscapeProceduralVertex, Position), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FLandscapeProceduralVertex, UV), VET_Float2, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};


class FLandscapeProceduralVertexBuffer : public FVertexBuffer
{
public:
	void Init(const TArray<FLandscapeProceduralTriangle>& InTriangleList)
	{
		TriangleList = InTriangleList;
	}

private:

	/** Initialize the RHI for this rendering resource */
	void InitRHI() override
	{
		TResourceArray<FLandscapeProceduralVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(TriangleList.Num() * 3);

		for (int32 i = 0; i < TriangleList.Num(); ++i)
		{
			Vertices[i * 3 + 0] = TriangleList[i].V0;
			Vertices[i * 3 + 1] = TriangleList[i].V1;
			Vertices[i * 3 + 2] = TriangleList[i].V2;
		}
		
		// Create vertex buffer. Fill buffer with initial data upon creation
		FRHIResourceCreateInfo CreateInfo(&Vertices);
		VertexBufferRHI = RHICreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}

	TArray<FLandscapeProceduralTriangle> TriangleList;
};



class FLandscapeProceduralVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLandscapeProceduralVS)

public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	FLandscapeProceduralVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TransformParam.Bind(Initializer.ParameterMap, TEXT("Transform"), SPF_Mandatory);
	}

	FLandscapeProceduralVS()
	{}

	void SetParameters(FRHICommandList& RHICmdList, const FMatrix& InProjectionMatrix)
	{
		SetShaderValue(RHICmdList, GetVertexShader(), TransformParam, InProjectionMatrix);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << TransformParam;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter TransformParam;
};

IMPLEMENT_GLOBAL_SHADER(FLandscapeProceduralVS, "/Engine/Private/LandscapeProceduralVS.usf", "VSMain", SF_Vertex);

struct FLandscapeHeightmapProceduralShaderParameters
{
	FLandscapeHeightmapProceduralShaderParameters()
		: ReadHeightmap1(nullptr)
		, ReadHeightmap2(nullptr)
		, HeightmapSize(0, 0)
		, ApplyLayerModifiers(false)
		, LayerWeight(1.0f)
		, LayerVisible(true)
		, OutputAsDelta(false)
		, GenerateNormals(false)
		, GridSize(0.0f, 0.0f, 0.0f)
		, CurrentMipHeightmapSize(0, 0)
		, ParentMipHeightmapSize(0, 0)
		, CurrentMipComponentVertexCount(0)
	{}

	UTexture* ReadHeightmap1;
	UTexture* ReadHeightmap2;
	FIntPoint HeightmapSize;
	bool ApplyLayerModifiers;
	float LayerWeight;
	bool LayerVisible;
	bool OutputAsDelta;
	bool GenerateNormals;
	FVector GridSize;
	FIntPoint CurrentMipHeightmapSize;
	FIntPoint ParentMipHeightmapSize;
	int32 CurrentMipComponentVertexCount;
};

class FLandscapeHeightmapProceduralPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLandscapeHeightmapProceduralPS);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	FLandscapeHeightmapProceduralPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ReadHeightmapTexture1Param.Bind(Initializer.ParameterMap, TEXT("ReadHeightmapTexture1"));
		ReadHeightmapTexture2Param.Bind(Initializer.ParameterMap, TEXT("ReadHeightmapTexture2"));
		ReadHeightmapTexture1SamplerParam.Bind(Initializer.ParameterMap, TEXT("ReadHeightmapTexture1Sampler"));
		ReadHeightmapTexture2SamplerParam.Bind(Initializer.ParameterMap, TEXT("ReadHeightmapTexture2Sampler"));
		LayerInfoParam.Bind(Initializer.ParameterMap, TEXT("LayerInfo"));
		OutputConfigParam.Bind(Initializer.ParameterMap, TEXT("OutputConfig"));
		TextureSizeParam.Bind(Initializer.ParameterMap, TEXT("HeightmapTextureSize"));
		LandscapeGridScaleParam.Bind(Initializer.ParameterMap, TEXT("LandscapeGridScale"));
		ComponentVertexCountParam.Bind(Initializer.ParameterMap, TEXT("CurrentMipComponentVertexCount"));
	}

	FLandscapeHeightmapProceduralPS()
	{}

	void SetParameters(FRHICommandList& RHICmdList, const FLandscapeHeightmapProceduralShaderParameters& InParams)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), ReadHeightmapTexture1Param, ReadHeightmapTexture1SamplerParam, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InParams.ReadHeightmap1->Resource->TextureRHI);

		if (InParams.ReadHeightmap2 != nullptr)
		{
			SetTextureParameter(RHICmdList, GetPixelShader(), ReadHeightmapTexture2Param, ReadHeightmapTexture2SamplerParam, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InParams.ReadHeightmap2->Resource->TextureRHI);
		}

		FVector2D LayerInfo(InParams.LayerWeight, InParams.LayerVisible ? 1.0f : 0.0f);
		FVector4 OutputConfig(InParams.ApplyLayerModifiers ? 1.0f : 0.0f, InParams.OutputAsDelta ? 1.0f : 0.0f, InParams.ReadHeightmap2 ? 1.0f : 0.0f, InParams.GenerateNormals ? 1.0f : 0.0f);
		FVector2D TextureSize(InParams.HeightmapSize.X, InParams.HeightmapSize.Y);

		SetShaderValue(RHICmdList, GetPixelShader(), LayerInfoParam, LayerInfo);
		SetShaderValue(RHICmdList, GetPixelShader(), OutputConfigParam, OutputConfig);
		SetShaderValue(RHICmdList, GetPixelShader(), TextureSizeParam, TextureSize);
		SetShaderValue(RHICmdList, GetPixelShader(), LandscapeGridScaleParam, InParams.GridSize);
		SetShaderValue(RHICmdList, GetPixelShader(), ComponentVertexCountParam, (float)InParams.CurrentMipComponentVertexCount);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ReadHeightmapTexture1Param;
		Ar << ReadHeightmapTexture2Param;
		Ar << ReadHeightmapTexture1SamplerParam;
		Ar << ReadHeightmapTexture2SamplerParam;
		Ar << LayerInfoParam;
		Ar << OutputConfigParam;
		Ar << TextureSizeParam;
		Ar << LandscapeGridScaleParam;
		Ar << ComponentVertexCountParam;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter ReadHeightmapTexture1Param;
	FShaderResourceParameter ReadHeightmapTexture2Param;
	FShaderResourceParameter ReadHeightmapTexture1SamplerParam;
	FShaderResourceParameter ReadHeightmapTexture2SamplerParam;
	FShaderParameter LayerInfoParam;
	FShaderParameter OutputConfigParam;
	FShaderParameter TextureSizeParam;
	FShaderParameter LandscapeGridScaleParam;
	FShaderParameter ComponentVertexCountParam;
};

IMPLEMENT_GLOBAL_SHADER(FLandscapeHeightmapProceduralPS, "/Engine/Private/LandscapeProceduralPS.usf", "PSMain", SF_Pixel);

class FLandscapeHeightmapMipsProceduralPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FLandscapeHeightmapMipsProceduralPS);
public:

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM4) && !IsConsolePlatform(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}

	FLandscapeHeightmapMipsProceduralPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		ReadHeightmapTexture1Param.Bind(Initializer.ParameterMap, TEXT("ReadHeightmapTexture1"));
		ReadHeightmapTexture1SamplerParam.Bind(Initializer.ParameterMap, TEXT("ReadHeightmapTexture1Sampler"));
		CurrentMipHeightmapSizeParam.Bind(Initializer.ParameterMap, TEXT("CurrentMipTextureSize"));
		ParentMipHeightmapSizeParam.Bind(Initializer.ParameterMap, TEXT("ParentMipTextureSize"));
		CurrentMipComponentVertexCountParam.Bind(Initializer.ParameterMap, TEXT("CurrentMipComponentVertexCount"));
	}

	FLandscapeHeightmapMipsProceduralPS()
	{}

	void SetParameters(FRHICommandList& RHICmdList, const FLandscapeHeightmapProceduralShaderParameters& InParams)
	{
		SetTextureParameter(RHICmdList, GetPixelShader(), ReadHeightmapTexture1Param, ReadHeightmapTexture1SamplerParam, TStaticSamplerState<SF_Point, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InParams.ReadHeightmap1->Resource->TextureRHI);

		SetShaderValue(RHICmdList, GetPixelShader(), CurrentMipHeightmapSizeParam, FVector2D(InParams.CurrentMipHeightmapSize.X, InParams.CurrentMipHeightmapSize.Y));
		SetShaderValue(RHICmdList, GetPixelShader(), ParentMipHeightmapSizeParam, FVector2D(InParams.ParentMipHeightmapSize.X, InParams.ParentMipHeightmapSize.Y));
		SetShaderValue(RHICmdList, GetPixelShader(), CurrentMipComponentVertexCountParam, (float)InParams.CurrentMipComponentVertexCount);
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ReadHeightmapTexture1Param;
		Ar << ReadHeightmapTexture1SamplerParam;
		Ar << CurrentMipHeightmapSizeParam;
		Ar << ParentMipHeightmapSizeParam;
		Ar << CurrentMipComponentVertexCountParam;

		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter ReadHeightmapTexture1Param;
	FShaderResourceParameter ReadHeightmapTexture1SamplerParam;
	FShaderParameter CurrentMipHeightmapSizeParam;
	FShaderParameter ParentMipHeightmapSizeParam;
	FShaderParameter CurrentMipComponentVertexCountParam;
};

IMPLEMENT_GLOBAL_SHADER(FLandscapeHeightmapMipsProceduralPS, "/Engine/Private/LandscapeProceduralPS.usf", "PSMainMips", SF_Pixel);

/** The filter vertex declaration resource type. */


DECLARE_GPU_STAT_NAMED(LandscapeProceduralRender, TEXT("Landscape Procedural Render"));

class FLandscapeProceduralCopyResource_RenderThread
{
public: 
	FLandscapeProceduralCopyResource_RenderThread(UTexture* InHeightmapRTRead, UTexture* InCopyResolveTarget, FTextureResource* InCopyResolveTargetCPUResource, FIntPoint InComponentSectionBase, int32 InSubSectionSizeQuad, int32 InNumSubSections, int32 InCurrentMip)
		: SourceResource(InHeightmapRTRead != nullptr ? InHeightmapRTRead->Resource : nullptr)
		, CopyResolveTargetResource(InCopyResolveTarget != nullptr ? InCopyResolveTarget->Resource : nullptr)
		, CopyResolveTargetCPUResource(InCopyResolveTargetCPUResource)
		, CurrentMip(InCurrentMip)
		, ComponentSectionBase(InComponentSectionBase)
		, SubSectionSizeQuad(InSubSectionSizeQuad)
		, NumSubSections(InNumSubSections)
		, SourceDebugName(SourceResource != nullptr ? InHeightmapRTRead->GetName() : TEXT(""))
		, CopyResolveDebugName(InCopyResolveTarget != nullptr ? InCopyResolveTarget->GetName() : TEXT(""))
	{}

	void CopyToResolveTarget(FRHICommandListImmediate& InRHICmdList)
	{
		if (SourceResource == nullptr || CopyResolveTargetResource == nullptr)
		{
			return;
		}

		SCOPE_CYCLE_COUNTER(STAT_LandscapeRegenerateProceduralHeightmaps_RenderThread);
		SCOPED_DRAW_EVENTF(InRHICmdList, LandscapeProceduralCopy, TEXT("LS Copy %s -> %s, Mip: %d"), *SourceDebugName, *CopyResolveDebugName, CurrentMip);
		SCOPED_GPU_STAT(InRHICmdList, LandscapeProceduralRender);

		FIntPoint SourceReadTextureSize(SourceResource->GetSizeX(), SourceResource->GetSizeY());
		FIntPoint CopyResolveWriteTextureSize(CopyResolveTargetResource->GetSizeX() >> CurrentMip, CopyResolveTargetResource->GetSizeY() >> CurrentMip);

		int32 LocalComponentSizeQuad = SubSectionSizeQuad * NumSubSections;
		FVector2D HeightmapPositionOffset(FMath::RoundToInt(ComponentSectionBase.X / LocalComponentSizeQuad), FMath::RoundToInt(ComponentSectionBase.Y / LocalComponentSizeQuad));

		FResolveParams Params;
		Params.SourceArrayIndex = 0;
		Params.DestArrayIndex = CurrentMip;

		if (SourceReadTextureSize.X <= CopyResolveWriteTextureSize.X)
		{
			Params.Rect.X1 = 0;
			Params.Rect.X2 = SourceReadTextureSize.X;
			Params.DestRect.X1 = FMath::RoundToInt(HeightmapPositionOffset.X * (((SubSectionSizeQuad + 1) * NumSubSections) >> CurrentMip));
		}
		else
		{
			Params.Rect.X1 = FMath::RoundToInt(HeightmapPositionOffset.X * (((SubSectionSizeQuad + 1) * NumSubSections) >> CurrentMip));
			Params.Rect.X2 = Params.Rect.X1 + CopyResolveWriteTextureSize.X;
			Params.DestRect.X1 = 0;
		}

		if (SourceReadTextureSize.Y <= CopyResolveWriteTextureSize.Y)
		{
			Params.Rect.Y1 = 0;
			Params.Rect.Y2 = SourceReadTextureSize.Y;
			Params.DestRect.Y1 = FMath::RoundToInt(HeightmapPositionOffset.Y * (((SubSectionSizeQuad + 1) * NumSubSections) >> CurrentMip));
		}
		else
		{
			Params.Rect.Y1 = FMath::RoundToInt(HeightmapPositionOffset.Y * (((SubSectionSizeQuad + 1) * NumSubSections) >> CurrentMip));
			Params.Rect.Y2 = Params.Rect.Y1 + CopyResolveWriteTextureSize.Y;
			Params.DestRect.Y1 = 0;
		}

		InRHICmdList.CopyToResolveTarget(SourceResource->TextureRHI, CopyResolveTargetResource->TextureRHI, Params);

		if (CopyResolveTargetCPUResource != nullptr)
		{
			InRHICmdList.CopyToResolveTarget(SourceResource->TextureRHI, CopyResolveTargetCPUResource->TextureRHI, Params);
		}
	}

private:
	FTextureResource* SourceResource;
	FTextureResource* CopyResolveTargetResource;
	FTextureResource* CopyResolveTargetCPUResource;
	FIntPoint ReadRenderTargetSize;
	int32 CurrentMip;
	FIntPoint ComponentSectionBase;
	int32 SubSectionSizeQuad;
	int32 NumSubSections;
	FString SourceDebugName;
	FString CopyResolveDebugName;
};

class FLandscapeHeightmapProceduralRender_RenderThread
{
public:

	FLandscapeHeightmapProceduralRender_RenderThread(const FString& InDebugName, UTextureRenderTarget2D* InWriteRenderTarget, const FIntPoint& InWriteRenderTargetSize, const FIntPoint& InReadRenderTargetSize, const FMatrix& InProjectionMatrix, 
													 const FLandscapeHeightmapProceduralShaderParameters& InShaderParams, int32 InCurrentMip, const TArray<FLandscapeProceduralTriangle>& InTriangleList)
		: RenderTargetResource(InWriteRenderTarget->GameThread_GetRenderTargetResource())
		, WriteRenderTargetSize(InWriteRenderTargetSize)
		, ReadRenderTargetSize(InReadRenderTargetSize)
		, ProjectionMatrix(InProjectionMatrix)
		, ShaderParams(InShaderParams)
		, PrimitiveCount(InTriangleList.Num())
		, DebugName(InDebugName)
		, CurrentMip(InCurrentMip)
	{
		VertexBufferResource.Init(InTriangleList);
	}

	virtual ~FLandscapeHeightmapProceduralRender_RenderThread()
	{}

	void Render(FRHICommandListImmediate& InRHICmdList, bool InClearRT)
	{
		SCOPE_CYCLE_COUNTER(STAT_LandscapeRegenerateProceduralHeightmaps_RenderThread);
		SCOPED_DRAW_EVENTF(InRHICmdList, LandscapeProceduralHeightmapRender, TEXT("%s"), DebugName.Len() > 0 ? *DebugName : TEXT("LandscapeProceduralHeightmapRender"));
		SCOPED_GPU_STAT(InRHICmdList, LandscapeProceduralRender);
		INC_DWORD_STAT(STAT_LandscapeRegenerateProceduralHeightmapsDrawCalls);

		check(IsInRenderingThread());

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTargetResource, NULL, FEngineShowFlags(ESFIM_Game)).SetWorldTimes(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime(), FApp::GetCurrentTime() - GStartTime));

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, WriteRenderTargetSize.X, WriteRenderTargetSize.Y));
		ViewInitOptions.ViewOrigin = FVector::ZeroVector;
		ViewInitOptions.ViewRotationMatrix = FMatrix::Identity;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.OverlayColor = FLinearColor::White;

		// Create and add the new view
		FSceneView* View = new FSceneView(ViewInitOptions);
		ViewFamily.Views.Add(View);

		// Init VB/IB Resource
		VertexDeclaration.InitResource();
		VertexBufferResource.InitResource();

		// Setup Pipeline
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		//RT0ColorWriteMask, RT0ColorBlendOp, RT0ColorSrcBlend, RT0ColorDestBlend, RT0AlphaBlendOp, RT0AlphaSrcBlend, RT0AlphaDestBlend,
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_One, BF_Zero>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		FRHIRenderPassInfo RenderPassInfo(ViewFamily.RenderTarget->GetRenderTargetTexture(), CurrentMip == 0 ? ERenderTargetActions::Clear_Store : ERenderTargetActions::Load_Store, nullptr, 0, 0);
		InRHICmdList.BeginRenderPass(RenderPassInfo, TEXT("DrawProceduralHeightmaps"));

		if (CurrentMip == 0)
		{
			// Setup Shaders
			TShaderMapRef<FLandscapeProceduralVS> VertexShader(GetGlobalShaderMap(View->GetFeatureLevel()));
			TShaderMapRef<FLandscapeHeightmapProceduralPS> PixelShader(GetGlobalShaderMap(View->GetFeatureLevel()));

			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

			InRHICmdList.SetViewport(View->UnscaledViewRect.Min.X, View->UnscaledViewRect.Min.Y, 0.0f, View->UnscaledViewRect.Max.X, View->UnscaledViewRect.Max.Y, 1.0f);

			InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit);

			// Set shader params
			VertexShader->SetParameters(InRHICmdList, ProjectionMatrix);
			PixelShader->SetParameters(InRHICmdList, ShaderParams);
		}
		else
		{
			// Setup Shaders
			TShaderMapRef<FLandscapeProceduralVS> VertexShader(GetGlobalShaderMap(View->GetFeatureLevel()));
			TShaderMapRef<FLandscapeHeightmapMipsProceduralPS> PixelShader(GetGlobalShaderMap(View->GetFeatureLevel()));

			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = GETSAFERHISHADER_VERTEX(*VertexShader);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = GETSAFERHISHADER_PIXEL(*PixelShader);

			InRHICmdList.SetViewport(0.0f, 0.0f, 0.0f, WriteRenderTargetSize.X, WriteRenderTargetSize.Y, 1.0f);

			InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit);

			// Set shader params
			VertexShader->SetParameters(InRHICmdList, ProjectionMatrix);
			PixelShader->SetParameters(InRHICmdList, ShaderParams);			
		}

		InRHICmdList.SetStencilRef(0);
		InRHICmdList.SetScissorRect(false, 0, 0, 0, 0);
		InRHICmdList.SetStreamSource(0, VertexBufferResource.VertexBufferRHI, 0);

		InRHICmdList.DrawPrimitive(0, PrimitiveCount, 1);

		InRHICmdList.EndRenderPass();

		VertexDeclaration.ReleaseResource();
		VertexBufferResource.ReleaseResource();
	}

private:
	FTextureRenderTargetResource* RenderTargetResource;
	FIntPoint WriteRenderTargetSize;
	FIntPoint ReadRenderTargetSize;
	FMatrix ProjectionMatrix;
	FLandscapeHeightmapProceduralShaderParameters ShaderParams;
	FLandscapeProceduralVertexBuffer VertexBufferResource;
	int32 PrimitiveCount;
	FLandscapeProceduralVertexDeclaration VertexDeclaration;
	FString DebugName;
	int32 CurrentMip;
};

void ALandscapeProxy::SetupProceduralLayers(int32 InNumComponentsX, int32 InNumComponentsY)
{
	ALandscape* Landscape = GetLandscapeActor();
	check(Landscape);

	ULandscapeInfo* Info = GetLandscapeInfo();

	if (Info == nullptr)
	{
		return;
	}

	TArray<ALandscapeProxy*> AllLandscapes;
	AllLandscapes.Add(Landscape);

	for (auto& It : Info->Proxies)
	{
		AllLandscapes.Add(It);
	}

	// TEMP STUFF START
	bool Layer1Exist = false;
	FProceduralLayer Layer1;
	Layer1.Name = TEXT("Layer1");

	bool Layer2Exist = false;
	FProceduralLayer Layer2;
	Layer2.Name = TEXT("Layer2");	

	for (int32 i = 0; i < Landscape->ProceduralLayers.Num(); ++i)
	{
		if (Landscape->ProceduralLayers[i].Name == Layer1.Name)
		{
			Layer1Exist = true;				
		}

		if (Landscape->ProceduralLayers[i].Name == Layer2.Name)
		{
			Layer2Exist = true;
		}
	}

	if (!Layer1Exist)
	{
		Landscape->ProceduralLayers.Add(Layer1);

		for (ALandscapeProxy* LandscapeProxy : AllLandscapes)
		{
			LandscapeProxy->ProceduralLayersData.Add(Layer1.Name, FProceduralLayerData());
		}
	}

	if (!Layer2Exist)
	{
		Landscape->ProceduralLayers.Add(Layer2);

		for (ALandscapeProxy* LandscapeProxy : AllLandscapes)
		{
			LandscapeProxy->ProceduralLayersData.Add(Layer2.Name, FProceduralLayerData());
		}
	}	
	///// TEMP STUFF END

	int32 NumComponentsX = InNumComponentsX;
	int32 NumComponentsY = InNumComponentsY;
	bool GenerateComponentCounts = NumComponentsX == INDEX_NONE || NumComponentsY == INDEX_NONE;
	FIntPoint MaxSectionBase(0, 0);

	uint32 UpdateFlags = 0;

	// Setup all Heightmap data
	for (ALandscapeProxy* LandscapeProxy : AllLandscapes)
	{
		for (ULandscapeComponent* Component : LandscapeProxy->LandscapeComponents)
		{
			UTexture2D* ComponentHeightmapTexture = Component->GetHeightmap();

			FRenderDataPerHeightmap* Data = LandscapeProxy->RenderDataPerHeightmap.Find(ComponentHeightmapTexture);

			if (Data == nullptr)
			{
				FRenderDataPerHeightmap NewData;
				NewData.Components.Add(Component);
				NewData.OriginalHeightmap = ComponentHeightmapTexture;
				NewData.HeightmapsCPUReadBack = new FLandscapeProceduralTexture2DCPUReadBackResource(ComponentHeightmapTexture->Source.GetSizeX(), ComponentHeightmapTexture->Source.GetSizeY(), ComponentHeightmapTexture->GetPixelFormat(), ComponentHeightmapTexture->Source.GetNumMips());
				BeginInitResource(NewData.HeightmapsCPUReadBack);

				LandscapeProxy->RenderDataPerHeightmap.Add(ComponentHeightmapTexture, NewData);
			}
			else
			{
				Data->Components.AddUnique(Component);
			}

			if (GenerateComponentCounts)
			{
				MaxSectionBase.X = FMath::Max(MaxSectionBase.X, Component->SectionBaseX);
				MaxSectionBase.Y = FMath::Max(MaxSectionBase.Y, Component->SectionBaseY);
			}
		}
	}

	if (GenerateComponentCounts)
	{
		NumComponentsX = (MaxSectionBase.X / ComponentSizeQuads) + 1;
		NumComponentsY = (MaxSectionBase.Y / ComponentSizeQuads) + 1;
	}

	const int32 TotalVertexCountX = (SubsectionSizeQuads * NumSubsections) * NumComponentsX + 1;
	const int32 TotalVertexCountY = (SubsectionSizeQuads * NumSubsections) * NumComponentsY + 1;

	if (Landscape->HeightmapRTList.Num() == 0)
	{
		Landscape->HeightmapRTList.Init(nullptr, EHeightmapRTType::Count);

		int32 CurrentMipSizeX = ((SubsectionSizeQuads + 1) * NumSubsections) * NumComponentsX;
		int32 CurrentMipSizeY = ((SubsectionSizeQuads + 1) * NumSubsections) * NumComponentsY;

		for (int32 i = 0; i < EHeightmapRTType::Count; ++i)
		{
			Landscape->HeightmapRTList[i] = NewObject<UTextureRenderTarget2D>(Landscape->GetOutermost());
			check(Landscape->HeightmapRTList[i]);
			Landscape->HeightmapRTList[i]->RenderTargetFormat = RTF_RGBA8;
			Landscape->HeightmapRTList[i]->AddressX = TextureAddress::TA_Clamp;
			Landscape->HeightmapRTList[i]->AddressY = TextureAddress::TA_Clamp;

			if (i < LandscapeSizeMip1) // Landscape size RT
			{
				Landscape->HeightmapRTList[i]->InitAutoFormat(FMath::RoundUpToPowerOfTwo(TotalVertexCountX), FMath::RoundUpToPowerOfTwo(TotalVertexCountY));
			}
			else // Mips
			{
				CurrentMipSizeX >>= 1;
				CurrentMipSizeY >>= 1;
				Landscape->HeightmapRTList[i]->InitAutoFormat(FMath::RoundUpToPowerOfTwo(CurrentMipSizeX), FMath::RoundUpToPowerOfTwo(CurrentMipSizeY));
			}

			Landscape->HeightmapRTList[i]->UpdateResourceImmediate(true);

			// Only generate required mips RT
			if (CurrentMipSizeX == NumComponentsX && CurrentMipSizeY == NumComponentsY)
			{
				break;
			}
		}
	}

	TArray<FVector> VertexNormals;
	TArray<uint16> EmptyHeightmapData;

	UpdateFlags |= EProceduralContentUpdateFlag::Heightmap_Render;

	// Setup all Heightmap data
	for (ALandscapeProxy* LandscapeProxy : AllLandscapes)
	{
		for (auto& ItPair : LandscapeProxy->RenderDataPerHeightmap)
		{
			FRenderDataPerHeightmap& HeightmapRenderData = ItPair.Value;
			HeightmapRenderData.TopLeftSectionBase = FIntPoint(TotalVertexCountX, TotalVertexCountY);

			for (ULandscapeComponent* Component : HeightmapRenderData.Components)
			{
				HeightmapRenderData.TopLeftSectionBase.X = FMath::Min(HeightmapRenderData.TopLeftSectionBase.X, Component->GetSectionBase().X);
				HeightmapRenderData.TopLeftSectionBase.Y = FMath::Min(HeightmapRenderData.TopLeftSectionBase.Y, Component->GetSectionBase().Y);
			}

			bool FirstLayer = true;

			for (auto& ItLayerDataPair : LandscapeProxy->ProceduralLayersData)
			{
				FProceduralLayerData& LayerData = ItLayerDataPair.Value;

				if (LayerData.Heightmaps.Find(HeightmapRenderData.OriginalHeightmap) == nullptr)
				{
					UTexture2D* Heightmap = LandscapeProxy->CreateLandscapeTexture(HeightmapRenderData.OriginalHeightmap->Source.GetSizeX(), HeightmapRenderData.OriginalHeightmap->Source.GetSizeY(), TEXTUREGROUP_Terrain_Heightmap, HeightmapRenderData.OriginalHeightmap->Source.GetFormat());
					LayerData.Heightmaps.Add(HeightmapRenderData.OriginalHeightmap, Heightmap);

					int32 MipSubsectionSizeQuads = SubsectionSizeQuads;
					int32 MipSizeU = Heightmap->Source.GetSizeX();
					int32 MipSizeV = Heightmap->Source.GetSizeY();

					UpdateFlags |= EProceduralContentUpdateFlag::Heightmap_ResolveToTexture | EProceduralContentUpdateFlag::Heightmap_BoundsAndCollision;

					// Copy data from Heightmap to first layer, after that all other layer will get init to empty layer
					if (FirstLayer)
					{
						int32 MipIndex = 0;
						TArray<uint8> MipData;
						MipData.Reserve(MipSizeU*MipSizeV * sizeof(FColor));

						while (MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1)
						{
							MipData.Reset();
							HeightmapRenderData.OriginalHeightmap->Source.GetMipData(MipData, MipIndex);

							FColor* HeightmapTextureData = (FColor*)Heightmap->Source.LockMip(MipIndex);
							FMemory::Memcpy(HeightmapTextureData, MipData.GetData(), MipData.Num());
							Heightmap->Source.UnlockMip(MipIndex);

							MipSizeU >>= 1;
							MipSizeV >>= 1;

							MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
							++MipIndex;
						}
					}
					else
					{
						TArray<FColor*> HeightmapMipMapData;

						while (MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1)
						{
							int32 MipIndex = HeightmapMipMapData.Num();
							FColor* HeightmapTextureData = (FColor*)Heightmap->Source.LockMip(MipIndex);
							FMemory::Memzero(HeightmapTextureData, MipSizeU*MipSizeV * sizeof(FColor));
							HeightmapMipMapData.Add(HeightmapTextureData);

							MipSizeU >>= 1;
							MipSizeV >>= 1;

							MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
						}

						// Initialize blank heightmap data as if ALL components were in the same heightmap to prevent creating many allocations
						if (EmptyHeightmapData.Num() == 0)
						{
							EmptyHeightmapData.Init(32768, TotalVertexCountX * TotalVertexCountY);
						}

						const FVector DrawScale3D = GetRootComponent()->RelativeScale3D;

						// Init vertex normal data if required
						if (VertexNormals.Num() == 0)
						{
							VertexNormals.AddZeroed(TotalVertexCountX * TotalVertexCountY);
							for (int32 QuadY = 0; QuadY < TotalVertexCountY - 1; QuadY++)
							{
								for (int32 QuadX = 0; QuadX < TotalVertexCountX - 1; QuadX++)
								{
									const FVector Vert00 = FVector(0.0f, 0.0f, ((float)EmptyHeightmapData[FMath::Clamp<int32>(QuadY + 0, 0, TotalVertexCountY) * TotalVertexCountX + FMath::Clamp<int32>(QuadX + 0, 0, TotalVertexCountX)] - 32768.0f)*LANDSCAPE_ZSCALE) * DrawScale3D;
									const FVector Vert01 = FVector(0.0f, 1.0f, ((float)EmptyHeightmapData[FMath::Clamp<int32>(QuadY + 1, 0, TotalVertexCountY) * TotalVertexCountX + FMath::Clamp<int32>(QuadX + 0, 0, TotalVertexCountX)] - 32768.0f)*LANDSCAPE_ZSCALE) * DrawScale3D;
									const FVector Vert10 = FVector(1.0f, 0.0f, ((float)EmptyHeightmapData[FMath::Clamp<int32>(QuadY + 0, 0, TotalVertexCountY) * TotalVertexCountX + FMath::Clamp<int32>(QuadX + 1, 0, TotalVertexCountX)] - 32768.0f)*LANDSCAPE_ZSCALE) * DrawScale3D;
									const FVector Vert11 = FVector(1.0f, 1.0f, ((float)EmptyHeightmapData[FMath::Clamp<int32>(QuadY + 1, 0, TotalVertexCountY) * TotalVertexCountX + FMath::Clamp<int32>(QuadX + 1, 0, TotalVertexCountX)] - 32768.0f)*LANDSCAPE_ZSCALE) * DrawScale3D;

									const FVector FaceNormal1 = ((Vert00 - Vert10) ^ (Vert10 - Vert11)).GetSafeNormal();
									const FVector FaceNormal2 = ((Vert11 - Vert01) ^ (Vert01 - Vert00)).GetSafeNormal();

									// contribute to the vertex normals.
									VertexNormals[(QuadX + 1 + TotalVertexCountX * (QuadY + 0))] += FaceNormal1;
									VertexNormals[(QuadX + 0 + TotalVertexCountX * (QuadY + 1))] += FaceNormal2;
									VertexNormals[(QuadX + 0 + TotalVertexCountX * (QuadY + 0))] += FaceNormal1 + FaceNormal2;
									VertexNormals[(QuadX + 1 + TotalVertexCountX * (QuadY + 1))] += FaceNormal1 + FaceNormal2;
								}
							}
						}

						for (ULandscapeComponent* Component : HeightmapRenderData.Components)
						{
							int32 HeightmapComponentOffsetX = FMath::RoundToInt((float)Heightmap->Source.GetSizeX() * Component->HeightmapScaleBias.Z);
							int32 HeightmapComponentOffsetY = FMath::RoundToInt((float)Heightmap->Source.GetSizeY() * Component->HeightmapScaleBias.W);

							for (int32 SubsectionY = 0; SubsectionY < NumSubsections; SubsectionY++)
							{
								for (int32 SubsectionX = 0; SubsectionX < NumSubsections; SubsectionX++)
								{
									for (int32 SubY = 0; SubY <= SubsectionSizeQuads; SubY++)
									{
										for (int32 SubX = 0; SubX <= SubsectionSizeQuads; SubX++)
										{
											// X/Y of the vertex we're looking at in component's coordinates.
											const int32 CompX = SubsectionSizeQuads * SubsectionX + SubX;
											const int32 CompY = SubsectionSizeQuads * SubsectionY + SubY;

											// X/Y of the vertex we're looking indexed into the texture data
											const int32 TexX = (SubsectionSizeQuads + 1) * SubsectionX + SubX;
											const int32 TexY = (SubsectionSizeQuads + 1) * SubsectionY + SubY;

											const int32 HeightTexDataIdx = (HeightmapComponentOffsetX + TexX) + (HeightmapComponentOffsetY + TexY) * Heightmap->Source.GetSizeX();

											// copy height and normal data
											int32 Value = FMath::Clamp<int32>(CompY + Component->GetSectionBase().Y, 0, TotalVertexCountY) * TotalVertexCountX + FMath::Clamp<int32>(CompX + Component->GetSectionBase().X, 0, TotalVertexCountX);
											const uint16 HeightValue = EmptyHeightmapData[Value];
											const FVector Normal = VertexNormals[CompX + Component->GetSectionBase().X + TotalVertexCountX * (CompY + Component->GetSectionBase().Y)].GetSafeNormal();

											HeightmapMipMapData[0][HeightTexDataIdx].R = HeightValue >> 8;
											HeightmapMipMapData[0][HeightTexDataIdx].G = HeightValue & 255;
											HeightmapMipMapData[0][HeightTexDataIdx].B = FMath::RoundToInt(127.5f * (Normal.X + 1.0f));
											HeightmapMipMapData[0][HeightTexDataIdx].A = FMath::RoundToInt(127.5f * (Normal.Y + 1.0f));
										}
									}
								}
							}

							bool IsBorderComponentX = (Component->GetSectionBase().X + 1 * NumSubsections) * InNumComponentsX == TotalVertexCountX;
							bool IsBorderComponentY = (Component->GetSectionBase().Y + 1 * NumSubsections) * InNumComponentsY == TotalVertexCountY;

							Component->GenerateHeightmapMips(HeightmapMipMapData, IsBorderComponentX ? MAX_int32 : 0, IsBorderComponentY ? MAX_int32 : 0);
						}

						// Add remaining mips down to 1x1 to heightmap texture.These do not represent quads and are just a simple averages of the previous mipmaps.
						// These mips are not used for sampling in the vertex shader but could be sampled in the pixel shader.
						int32 Mip = HeightmapMipMapData.Num();
						MipSizeU = (Heightmap->Source.GetSizeX()) >> Mip;
						MipSizeV = (Heightmap->Source.GetSizeY()) >> Mip;
						while (MipSizeU > 1 && MipSizeV > 1)
						{
							HeightmapMipMapData.Add((FColor*)Heightmap->Source.LockMip(Mip));
							const int32 PrevMipSizeU = (Heightmap->Source.GetSizeX()) >> (Mip - 1);
							const int32 PrevMipSizeV = (Heightmap->Source.GetSizeY()) >> (Mip - 1);

							for (int32 Y = 0; Y < MipSizeV; Y++)
							{
								for (int32 X = 0; X < MipSizeU; X++)
								{
									FColor* const TexData = &(HeightmapMipMapData[Mip])[X + Y * MipSizeU];

									const FColor* const PreMipTexData00 = &(HeightmapMipMapData[Mip - 1])[(X * 2 + 0) + (Y * 2 + 0) * PrevMipSizeU];
									const FColor* const PreMipTexData01 = &(HeightmapMipMapData[Mip - 1])[(X * 2 + 0) + (Y * 2 + 1) * PrevMipSizeU];
									const FColor* const PreMipTexData10 = &(HeightmapMipMapData[Mip - 1])[(X * 2 + 1) + (Y * 2 + 0) * PrevMipSizeU];
									const FColor* const PreMipTexData11 = &(HeightmapMipMapData[Mip - 1])[(X * 2 + 1) + (Y * 2 + 1) * PrevMipSizeU];

									TexData->R = (((int32)PreMipTexData00->R + (int32)PreMipTexData01->R + (int32)PreMipTexData10->R + (int32)PreMipTexData11->R) >> 2);
									TexData->G = (((int32)PreMipTexData00->G + (int32)PreMipTexData01->G + (int32)PreMipTexData10->G + (int32)PreMipTexData11->G) >> 2);
									TexData->B = (((int32)PreMipTexData00->B + (int32)PreMipTexData01->B + (int32)PreMipTexData10->B + (int32)PreMipTexData11->B) >> 2);
									TexData->A = (((int32)PreMipTexData00->A + (int32)PreMipTexData01->A + (int32)PreMipTexData10->A + (int32)PreMipTexData11->A) >> 2);
								}
							}
							Mip++;
							MipSizeU >>= 1;
							MipSizeV >>= 1;
						}

						for (int32 i = 0; i < HeightmapMipMapData.Num(); i++)
						{
							Heightmap->Source.UnlockMip(i);
						}
					}

					Heightmap->BeginCachePlatformData();
					Heightmap->ClearAllCachedCookedPlatformData();					
				}

				FirstLayer = false;
			}
		}
	}

	// Setup all Weightmap data
	// TODO

	// Fix Owning actor for Brushes. It can happen after save as operation, for example
	for (FProceduralLayer& Layer : Landscape->ProceduralLayers)
	{
		for (int32 i = 0; i < Layer.Brushes.Num(); ++i)
		{
			FLandscapeProceduralLayerBrush& Brush = Layer.Brushes[i];

			if (Brush.BPCustomBrush->GetOwningLandscape() == nullptr)
			{
				Brush.BPCustomBrush->SetOwningLandscape(Landscape);
			}
		}

		// TEMP stuff
		if (Layer.HeightmapBrushOrderIndices.Num() == 0)
		{
			for (int32 i = 0; i < Layer.Brushes.Num(); ++i)
			{
				FLandscapeProceduralLayerBrush& Brush = Layer.Brushes[i];

				if (Brush.BPCustomBrush->IsAffectingHeightmap())
				{
					Layer.HeightmapBrushOrderIndices.Add(i);
				}
			}
		}

		if (Layer.WeightmapBrushOrderIndices.Num() == 0)
		{
			for (int32 i = 0; i < Layer.Brushes.Num(); ++i)
			{
				FLandscapeProceduralLayerBrush& Brush = Layer.Brushes[i];

				if (Brush.BPCustomBrush->IsAffectingWeightmap())
				{
					Layer.WeightmapBrushOrderIndices.Add(i);
				}
			}
		}		
		// TEMP stuff
	}

	Landscape->RequestProceduralContentUpdate(UpdateFlags);
}

void ALandscape::CopyProceduralTargetToResolveTarget(UTexture* InHeightmapRTRead, UTexture* InCopyResolveTarget, FTextureResource* InCopyResolveTargetCPUResource, const FIntPoint& InFirstComponentSectionBase, int32 InCurrentMip) const
{
	FLandscapeProceduralCopyResource_RenderThread CopyResource(InHeightmapRTRead, InCopyResolveTarget, InCopyResolveTargetCPUResource, InFirstComponentSectionBase, SubsectionSizeQuads, NumSubsections, InCurrentMip);

	ENQUEUE_RENDER_COMMAND(FLandscapeProceduralCopyResultCommand)(
		[CopyResource](FRHICommandListImmediate& RHICmdList) mutable
		{
			CopyResource.CopyToResolveTarget(RHICmdList);
		});
}

void ALandscape::DrawHeightmapComponentsToRenderTargetMips(TArray<ULandscapeComponent*>& InComponentsToDraw, UTexture* InReadHeightmap, bool InClearRTWrite, struct FLandscapeHeightmapProceduralShaderParameters& InShaderParams) const
{
	bool OutputDebugName = CVarOutputProceduralDebugDrawCallName.GetValueOnAnyThread() == 1 || CVarOutputProceduralRTContent.GetValueOnAnyThread() == 1 ? true : false;
	int32 CurrentMip = 1;
	UTexture* ReadMipRT = InReadHeightmap;

	for (int32 MipRTIndex = EHeightmapRTType::LandscapeSizeMip1; MipRTIndex < EHeightmapRTType::Count; ++MipRTIndex)
	{
		UTextureRenderTarget2D* WriteMipRT = HeightmapRTList[MipRTIndex];

		if (WriteMipRT != nullptr)
		{
			DrawHeightmapComponentsToRenderTarget(OutputDebugName ? FString::Printf(TEXT("LS Height: %s = -> %s CombinedAtlasWithMips %d"), *ReadMipRT->GetName(), *WriteMipRT->GetName(), CurrentMip) : TEXT(""),
												  InComponentsToDraw, ReadMipRT, nullptr, WriteMipRT, ERTDrawingType::RTMips, InClearRTWrite, InShaderParams, CurrentMip++);
		}

		ReadMipRT = HeightmapRTList[MipRTIndex];
	}
}

void ALandscape::DrawHeightmapComponentsToRenderTarget(const FString& InDebugName, TArray<ULandscapeComponent*>& InComponentsToDraw, UTexture* InHeightmapRTRead, UTextureRenderTarget2D* InOptionalHeightmapRTRead2, UTextureRenderTarget2D* InHeightmapRTWrite,
													  ERTDrawingType InDrawType, bool InClearRTWrite, FLandscapeHeightmapProceduralShaderParameters& InShaderParams, int32 InMipRender) const
{
	check(InHeightmapRTRead != nullptr);
	check(InHeightmapRTWrite != nullptr);

	FIntPoint HeightmapWriteTextureSize(InHeightmapRTWrite->SizeX, InHeightmapRTWrite->SizeY);
	FIntPoint HeightmapReadTextureSize(InHeightmapRTRead->Source.GetSizeX(), InHeightmapRTRead->Source.GetSizeY());
	UTextureRenderTarget2D* HeightmapRTRead = Cast<UTextureRenderTarget2D>(InHeightmapRTRead);

	if (HeightmapRTRead != nullptr)
	{
		HeightmapReadTextureSize.X = HeightmapRTRead->SizeX;
		HeightmapReadTextureSize.Y = HeightmapRTRead->SizeY;
	}

	// Quad Setup
	TArray<FLandscapeProceduralTriangle> TriangleList;
	TriangleList.Reserve(InComponentsToDraw.Num() * 2 * NumSubsections);

	switch (InDrawType)
	{
		case ERTDrawingType::RTAtlas:
		{
			for (ULandscapeComponent* Component : InComponentsToDraw)
			{
				FVector2D HeightmapScaleBias(Component->HeightmapScaleBias.Z, Component->HeightmapScaleBias.W);
				GenerateHeightmapQuadsAtlas(Component->GetSectionBase(), HeightmapScaleBias, SubsectionSizeQuads, HeightmapReadTextureSize, HeightmapWriteTextureSize, TriangleList);
			}
		} break;

		case ERTDrawingType::RTAtlasToNonAtlas:
		{
			for (ULandscapeComponent* Component : InComponentsToDraw)
			{
				FVector2D HeightmapScaleBias(Component->HeightmapScaleBias.Z, Component->HeightmapScaleBias.W);
				GenerateHeightmapQuadsAtlasToNonAtlas(Component->GetSectionBase(), HeightmapScaleBias, SubsectionSizeQuads, HeightmapReadTextureSize, HeightmapWriteTextureSize, TriangleList);
			}
		} break;

		case ERTDrawingType::RTNonAtlas:
		{
			for (ULandscapeComponent* Component : InComponentsToDraw)
			{
				FVector2D HeightmapScaleBias(Component->HeightmapScaleBias.Z, Component->HeightmapScaleBias.W);
				GenerateHeightmapQuadsNonAtlas(Component->GetSectionBase(), HeightmapScaleBias, SubsectionSizeQuads, HeightmapReadTextureSize, HeightmapWriteTextureSize, TriangleList);
			}
		} break;

		case ERTDrawingType::RTNonAtlasToAtlas:
		{
			for (ULandscapeComponent* Component : InComponentsToDraw)
			{
				FVector2D HeightmapScaleBias(Component->HeightmapScaleBias.Z, Component->HeightmapScaleBias.W);
				GenerateHeightmapQuadsNonAtlasToAtlas(Component->GetSectionBase(), HeightmapScaleBias, SubsectionSizeQuads, HeightmapReadTextureSize, HeightmapWriteTextureSize, TriangleList);
			}
		} break;

		case ERTDrawingType::RTMips:
		{
			for (ULandscapeComponent* Component : InComponentsToDraw)
			{
				FVector2D HeightmapScaleBias(Component->HeightmapScaleBias.Z, Component->HeightmapScaleBias.W);
				GenerateHeightmapQuadsMip(Component->GetSectionBase(), HeightmapScaleBias, SubsectionSizeQuads, HeightmapReadTextureSize, HeightmapWriteTextureSize, InMipRender, TriangleList);
			}			
		} break;

		default:
		{
			check(false);
			return;
		}
	}

	InShaderParams.ReadHeightmap1 = InHeightmapRTRead;
	InShaderParams.ReadHeightmap2 = InOptionalHeightmapRTRead2;
	InShaderParams.HeightmapSize = HeightmapReadTextureSize;
	InShaderParams.CurrentMipComponentVertexCount = (((SubsectionSizeQuads + 1) * NumSubsections) >> InMipRender);

	if (InMipRender > 0)
	{
		InShaderParams.CurrentMipHeightmapSize = HeightmapWriteTextureSize;
		InShaderParams.ParentMipHeightmapSize = HeightmapReadTextureSize;
	}

	FMatrix ProjectionMatrix = AdjustProjectionMatrixForRHI(FTranslationMatrix(FVector(0, 0, 0)) *
															FMatrix(FPlane(1.0f / (FMath::Max<uint32>(HeightmapWriteTextureSize.X, 1.f) / 2.0f), 0.0, 0.0f, 0.0f), FPlane(0.0f, -1.0f / (FMath::Max<uint32>(HeightmapWriteTextureSize.Y, 1.f) / 2.0f), 0.0f, 0.0f), FPlane(0.0f, 0.0f, 1.0f, 0.0f), FPlane(-1.0f, 1.0f, 0.0f, 1.0f)));

	FLandscapeHeightmapProceduralRender_RenderThread ProceduralRender(InDebugName, InHeightmapRTWrite, HeightmapWriteTextureSize, HeightmapReadTextureSize, ProjectionMatrix, InShaderParams, InMipRender, TriangleList);

	ENQUEUE_RENDER_COMMAND(FDrawSceneCommand)(
		[ProceduralRender, ClearRT = InClearRTWrite](FRHICommandListImmediate& RHICmdList) mutable
		{
			ProceduralRender.Render(RHICmdList, ClearRT);
		});
	
	PrintDebugRTHeightmap(InDebugName, InHeightmapRTWrite, InMipRender, InShaderParams.GenerateNormals);
}

void ALandscape::GenerateHeightmapQuad(const FIntPoint& InVertexPosition, const float InVertexSize, const FVector2D& InUVStart, const FVector2D& InUVSize, TArray<FLandscapeProceduralTriangle>& OutTriangles) const
{
	FLandscapeProceduralTriangle Tri1;
	
	Tri1.V0.Position = FVector2D(InVertexPosition.X, InVertexPosition.Y);
	Tri1.V1.Position = FVector2D(InVertexPosition.X + InVertexSize, InVertexPosition.Y);
	Tri1.V2.Position = FVector2D(InVertexPosition.X + InVertexSize, InVertexPosition.Y + InVertexSize);

	Tri1.V0.UV = FVector2D(InUVStart.X, InUVStart.Y);
	Tri1.V1.UV = FVector2D(InUVStart.X + InUVSize.X, InUVStart.Y);
	Tri1.V2.UV = FVector2D(InUVStart.X + InUVSize.X, InUVStart.Y + InUVSize.Y);
	OutTriangles.Add(Tri1);

	FLandscapeProceduralTriangle Tri2;
	Tri2.V0.Position = FVector2D(InVertexPosition.X + InVertexSize, InVertexPosition.Y + InVertexSize);
	Tri2.V1.Position = FVector2D(InVertexPosition.X, InVertexPosition.Y + InVertexSize);
	Tri2.V2.Position = FVector2D(InVertexPosition.X, InVertexPosition.Y);

	Tri2.V0.UV = FVector2D(InUVStart.X + InUVSize.X, InUVStart.Y + InUVSize.Y);
	Tri2.V1.UV = FVector2D(InUVStart.X, InUVStart.Y + InUVSize.Y);
	Tri2.V2.UV = FVector2D(InUVStart.X, InUVStart.Y);

	OutTriangles.Add(Tri2);
}

void ALandscape::GenerateHeightmapQuadsAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, TArray<FLandscapeProceduralTriangle>& OutTriangles) const
{
	FIntPoint ComponentSectionBase = InSectionBase;
	FIntPoint UVComponentSectionBase = InSectionBase;

	int32 LocalComponentSizeQuad = InSubSectionSizeQuad * NumSubsections;
	int32 SubsectionSizeVerts = InSubSectionSizeQuad + 1;

	FVector2D HeightmapPositionOffset(FMath::RoundToInt(ComponentSectionBase.X / LocalComponentSizeQuad), FMath::RoundToInt(ComponentSectionBase.Y / LocalComponentSizeQuad));
	FVector2D ComponentsPerTexture(FMath::RoundToInt(InWriteSize.X / LocalComponentSizeQuad), FMath::RoundToInt(InWriteSize.Y / LocalComponentSizeQuad));

	if (InReadSize.X >= InWriteSize.X)
	{
		if (InReadSize.X == InWriteSize.X)
		{
			if (ComponentsPerTexture.X > 1.0f)
			{
				UVComponentSectionBase.X = HeightmapPositionOffset.X * (SubsectionSizeVerts * NumSubsections);
			}
			else
			{
				UVComponentSectionBase.X -= (UVComponentSectionBase.X + LocalComponentSizeQuad > InWriteSize.X) ? FMath::FloorToInt((HeightmapPositionOffset.X / ComponentsPerTexture.X)) * ComponentsPerTexture.X * LocalComponentSizeQuad : 0;
			}
		}

		ComponentSectionBase.X -= (ComponentSectionBase.X + LocalComponentSizeQuad > InWriteSize.X) ? FMath::FloorToInt((HeightmapPositionOffset.X / ComponentsPerTexture.X)) * ComponentsPerTexture.X * LocalComponentSizeQuad : 0;
		HeightmapPositionOffset.X = ComponentSectionBase.X / LocalComponentSizeQuad;
	}

	if (InReadSize.Y >= InWriteSize.Y)
	{
		if (InReadSize.Y == InWriteSize.Y)
		{
			if (ComponentsPerTexture.Y > 1.0f)
			{
				UVComponentSectionBase.Y = HeightmapPositionOffset.Y * (SubsectionSizeVerts * NumSubsections);
			}
			else
			{
				UVComponentSectionBase.Y -= (UVComponentSectionBase.Y + LocalComponentSizeQuad > InWriteSize.Y) ? FMath::FloorToInt((HeightmapPositionOffset.Y / ComponentsPerTexture.Y)) * ComponentsPerTexture.Y * LocalComponentSizeQuad : 0;
			}
		}

		ComponentSectionBase.Y -= (ComponentSectionBase.Y + LocalComponentSizeQuad > InWriteSize.Y) ? FMath::FloorToInt((HeightmapPositionOffset.Y / ComponentsPerTexture.Y)) * ComponentsPerTexture.Y * LocalComponentSizeQuad : 0;
		HeightmapPositionOffset.Y = ComponentSectionBase.Y / LocalComponentSizeQuad;
	}

	ComponentSectionBase.X = HeightmapPositionOffset.X * (SubsectionSizeVerts * NumSubsections);
	ComponentSectionBase.Y = HeightmapPositionOffset.Y * (SubsectionSizeVerts * NumSubsections);

	FVector2D HeightmapUVSize((float)SubsectionSizeVerts / (float)InReadSize.X, (float)SubsectionSizeVerts / (float)InReadSize.Y);
	FIntPoint SubSectionSectionBase;

	for (int8 SubY = 0; SubY < NumSubsections; ++SubY)
	{
		for (int8 SubX = 0; SubX < NumSubsections; ++SubX)
		{
			SubSectionSectionBase.X = ComponentSectionBase.X + SubsectionSizeVerts * SubX;
			SubSectionSectionBase.Y = ComponentSectionBase.Y + SubsectionSizeVerts * SubY;

			// Offset for this component's data in heightmap texture
			FVector2D HeightmapUVStart;

			if (InReadSize.X >= InWriteSize.X)
			{
				HeightmapUVStart.X = ((float)UVComponentSectionBase.X / (float)InReadSize.X) + HeightmapUVSize.X * (float)SubX;
			}
			else
			{
				HeightmapUVStart.X = InScaleBias.X + HeightmapUVSize.X * (float)SubX;
			}

			if (InReadSize.Y >= InWriteSize.Y)
			{
				HeightmapUVStart.Y = ((float)UVComponentSectionBase.Y / (float)InReadSize.Y) + HeightmapUVSize.Y * (float)SubY;
			}
			else
			{
				HeightmapUVStart.Y = InScaleBias.Y + HeightmapUVSize.Y * (float)SubY;
			}

			GenerateHeightmapQuad(SubSectionSectionBase, SubsectionSizeVerts, HeightmapUVStart, HeightmapUVSize, OutTriangles);
		}
	}
}

void ALandscape::GenerateHeightmapQuadsMip(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InReadSize, const FIntPoint& InWriteSize, int32 CurrentMip, TArray<FLandscapeProceduralTriangle>& OutTriangles) const
{
	int32 LocalComponentSizeQuad = InSubSectionSizeQuad * NumSubsections;
	int32 SubsectionSizeVerts = InSubSectionSizeQuad + 1;
	int32 MipSubsectionSizeVerts = SubsectionSizeVerts >> CurrentMip;

	FVector2D HeightmapPositionOffset(FMath::RoundToInt(InSectionBase.X / LocalComponentSizeQuad), FMath::RoundToInt(InSectionBase.Y / LocalComponentSizeQuad));
	FVector2D ComponentsPerTexture(FMath::RoundToInt(InWriteSize.X / LocalComponentSizeQuad), FMath::RoundToInt(InWriteSize.Y / LocalComponentSizeQuad));

	FIntPoint ComponentSectionBase(HeightmapPositionOffset.X * (MipSubsectionSizeVerts * NumSubsections), HeightmapPositionOffset.Y * (MipSubsectionSizeVerts * NumSubsections));
	FIntPoint UVComponentSectionBase(HeightmapPositionOffset.X * (SubsectionSizeVerts * NumSubsections), HeightmapPositionOffset.Y * (SubsectionSizeVerts * NumSubsections));
	FVector2D HeightmapUVSize((float)(SubsectionSizeVerts >> (CurrentMip - 1)) / (float)InReadSize.X, (float)(SubsectionSizeVerts >> (CurrentMip - 1)) / (float)InReadSize.Y);
	FIntPoint SubSectionSectionBase;

	for (int8 SubY = 0; SubY < NumSubsections; ++SubY)
	{
		for (int8 SubX = 0; SubX < NumSubsections; ++SubX)
		{
			SubSectionSectionBase.X = ComponentSectionBase.X + MipSubsectionSizeVerts * SubX;
			SubSectionSectionBase.Y = ComponentSectionBase.Y + MipSubsectionSizeVerts * SubY;

			// Offset for this component's data in heightmap texture
			FVector2D HeightmapUVStart;
			HeightmapUVStart.X = ((float)(UVComponentSectionBase.X >> (CurrentMip - 1)) / (float)InReadSize.X) + HeightmapUVSize.X * (float)SubX;
			HeightmapUVStart.Y = ((float)(UVComponentSectionBase.Y >> (CurrentMip - 1)) / (float)InReadSize.Y) + HeightmapUVSize.Y * (float)SubY;

			GenerateHeightmapQuad(SubSectionSectionBase, MipSubsectionSizeVerts, HeightmapUVStart, HeightmapUVSize, OutTriangles);
		}
	}
}

void ALandscape::GenerateHeightmapQuadsAtlasToNonAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InHeightmapReadTextureSize, const FIntPoint& InHeightmapWriteTextureSize, TArray<FLandscapeProceduralTriangle>& OutTriangles) const
{
	FIntPoint ComponentSectionBase = InSectionBase;
	int32 LocalComponentSizeQuad = InSubSectionSizeQuad * NumSubsections;
	int32 HeightmapPositionOffsetX = ComponentSectionBase.X / LocalComponentSizeQuad;
	int32 HeightmapPositionOffsetY = ComponentSectionBase.Y / LocalComponentSizeQuad;
	int32 SubsectionSizeVerts = InSubSectionSizeQuad + 1;

	FIntPoint UVComponentSectionBase = InSectionBase;
	UVComponentSectionBase.X = HeightmapPositionOffsetX * (SubsectionSizeVerts * NumSubsections);
	UVComponentSectionBase.Y = HeightmapPositionOffsetY * (SubsectionSizeVerts * NumSubsections);

	ComponentSectionBase.X = HeightmapPositionOffsetX * (InSubSectionSizeQuad * NumSubsections);
	ComponentSectionBase.Y = HeightmapPositionOffsetY * (InSubSectionSizeQuad * NumSubsections);

	FVector2D HeightmapUVSize((float)SubsectionSizeVerts / (float)InHeightmapReadTextureSize.X, (float)SubsectionSizeVerts / (float)InHeightmapReadTextureSize.Y);
	FIntPoint SubSectionSectionBase;

	for (int8 SubY = 0; SubY < NumSubsections; ++SubY)
	{
		for (int8 SubX = 0; SubX < NumSubsections; ++SubX)
		{
			SubSectionSectionBase.X = ComponentSectionBase.X + InSubSectionSizeQuad * SubX;
			SubSectionSectionBase.Y = ComponentSectionBase.Y + InSubSectionSizeQuad * SubY;

			// Offset for this component's data in heightmap texture
			FVector2D HeightmapUVStart;

			if (InHeightmapReadTextureSize.X >= InHeightmapWriteTextureSize.X)
			{
				HeightmapUVStart.X = ((float)UVComponentSectionBase.X / (float)InHeightmapReadTextureSize.X) + HeightmapUVSize.X * (float)SubX;
			}
			else
			{
				HeightmapUVStart.X = InScaleBias.X + HeightmapUVSize.X * (float)SubX;
			}

			if (InHeightmapReadTextureSize.Y >= InHeightmapWriteTextureSize.Y)
			{
				HeightmapUVStart.Y = ((float)UVComponentSectionBase.Y / (float)InHeightmapReadTextureSize.Y) + HeightmapUVSize.Y * (float)SubY;
			}
			else
			{
				HeightmapUVStart.Y = InScaleBias.Y + HeightmapUVSize.Y * (float)SubY;
			}

			GenerateHeightmapQuad(SubSectionSectionBase, SubsectionSizeVerts, HeightmapUVStart, HeightmapUVSize, OutTriangles);
		}
	}
}

void ALandscape::GenerateHeightmapQuadsNonAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InHeightmapReadTextureSize, const FIntPoint& InHeightmapWriteTextureSize, TArray<FLandscapeProceduralTriangle>& OutTriangles) const
{
	// We currently only support drawing in non atlas mode with the same texture size
	check(InHeightmapReadTextureSize.X == InHeightmapWriteTextureSize.X && InHeightmapReadTextureSize.Y == InHeightmapWriteTextureSize.Y);

	FIntPoint ComponentSectionBase = InSectionBase;
	int32 LocalComponentSizeQuad = InSubSectionSizeQuad * NumSubsections;
	int32 HeightmapPositionOffsetX = ComponentSectionBase.X / LocalComponentSizeQuad;
	int32 HeightmapPositionOffsetY = ComponentSectionBase.Y / LocalComponentSizeQuad;
	int32 SubsectionSizeVerts = InSubSectionSizeQuad + 1;

	FIntPoint UVComponentSectionBase = InSectionBase;
	UVComponentSectionBase.X = HeightmapPositionOffsetX * (InSubSectionSizeQuad * NumSubsections);
	UVComponentSectionBase.Y = HeightmapPositionOffsetY * (InSubSectionSizeQuad * NumSubsections);

	FVector2D HeightmapUVSize((float)SubsectionSizeVerts / (float)InHeightmapReadTextureSize.X, (float)SubsectionSizeVerts / (float)InHeightmapReadTextureSize.Y);
	FIntPoint SubSectionSectionBase;

	for (int8 SubY = 0; SubY < NumSubsections; ++SubY)
	{
		for (int8 SubX = 0; SubX < NumSubsections; ++SubX)
		{
			SubSectionSectionBase.X = ComponentSectionBase.X + InSubSectionSizeQuad * SubX;
			SubSectionSectionBase.Y = ComponentSectionBase.Y + InSubSectionSizeQuad * SubY;

			// Offset for this component's data in heightmap texture
			FVector2D HeightmapUVStart(((float)UVComponentSectionBase.X / (float)InHeightmapReadTextureSize.X) + HeightmapUVSize.X * (float)SubX, ((float)UVComponentSectionBase.Y / (float)InHeightmapReadTextureSize.Y) + HeightmapUVSize.Y * (float)SubY);
			GenerateHeightmapQuad(SubSectionSectionBase, SubsectionSizeVerts, HeightmapUVStart, HeightmapUVSize, OutTriangles);
		}
	}
}

void ALandscape::GenerateHeightmapQuadsNonAtlasToAtlas(const FIntPoint& InSectionBase, const FVector2D& InScaleBias, float InSubSectionSizeQuad, const FIntPoint& InHeightmapReadTextureSize, const FIntPoint& InHeightmapWriteTextureSize, TArray<FLandscapeProceduralTriangle>& OutTriangles) const
{
	FIntPoint ComponentSectionBase = InSectionBase;
	int32 LocalComponentSizeQuad = InSubSectionSizeQuad * NumSubsections;
	int32 HeightmapPositionOffsetX = ComponentSectionBase.X / LocalComponentSizeQuad;
	int32 HeightmapPositionOffsetY = ComponentSectionBase.Y / LocalComponentSizeQuad;
	int32 SubsectionSizeVerts = InSubSectionSizeQuad + 1;

	ComponentSectionBase.X = HeightmapPositionOffsetX * (SubsectionSizeVerts * NumSubsections);
	ComponentSectionBase.Y = HeightmapPositionOffsetY * (SubsectionSizeVerts * NumSubsections);

	FVector2D HeightmapUVSize((float)SubsectionSizeVerts / (float)InHeightmapReadTextureSize.X, (float)SubsectionSizeVerts / (float)InHeightmapReadTextureSize.Y);
	FIntPoint SubSectionSectionBase;

	for (int8 SubY = 0; SubY < NumSubsections; ++SubY)
	{
		for (int8 SubX = 0; SubX < NumSubsections; ++SubX)
		{
			SubSectionSectionBase.X = ComponentSectionBase.X + SubsectionSizeVerts * SubX;
			SubSectionSectionBase.Y = ComponentSectionBase.Y + SubsectionSizeVerts * SubY;

			// Offset for this component's data in heightmap texture
			float HeightmapScaleBiasZ = (float)InSectionBase.X / (float)InHeightmapReadTextureSize.X;
			float HeightmapScaleBiasW = (float)InSectionBase.Y / (float)InHeightmapReadTextureSize.Y;
			FVector2D HeightmapUVStart(HeightmapScaleBiasZ + ((float)InSubSectionSizeQuad / (float)InHeightmapReadTextureSize.X) * (float)SubX, HeightmapScaleBiasW + ((float)InSubSectionSizeQuad / (float)InHeightmapReadTextureSize.Y) * (float)SubY);

			GenerateHeightmapQuad(SubSectionSectionBase, SubsectionSizeVerts, HeightmapUVStart, HeightmapUVSize, OutTriangles);
		}
	}
}

void ALandscape::PrintDebugHeightData(const FString& InContext, const TArray<FColor>& InHeightmapData, const FIntPoint& InDataSize, int32 InMipRender, bool InOutputNormals) const
{
	bool DisplayDebugPrint = CVarOutputProceduralRTContent.GetValueOnAnyThread() == 1 ? true : false;
	bool DisplayHeightAsDelta = false;

	if (!DisplayDebugPrint)
	{
		return;
	}

	TArray<uint16> HeightData;
	TArray<FVector> NormalData;
	HeightData.Reserve(InHeightmapData.Num());
	NormalData.Reserve(InHeightmapData.Num());

	for (FColor Color : InHeightmapData)
	{
		uint16 Height = ((Color.R << 8) | Color.G);
		HeightData.Add(Height);

		if (InOutputNormals)
		{
			FVector Normal;
			Normal.X = Color.B > 0.0f ? ((float)Color.B / 127.5f - 1.0f) : 0.0f;
			Normal.Y = Color.A > 0.0f ? ((float)Color.A / 127.5f - 1.0f) : 0.0f;
			Normal.Z = 0.0f;

			NormalData.Add(Normal);
		}
	}

	UE_LOG(LogLandscapeBP, Display, TEXT("Context: %s"), *InContext);

	int32 MipSize = ((SubsectionSizeQuads + 1) >> InMipRender);

	for (int32 Y = 0; Y < InDataSize.Y; ++Y)
	{
		FString HeightmapHeightOutput;

		for (int32 X = 0; X < InDataSize.X; ++X)
		{
			int32 HeightDelta = HeightData[X + Y * InDataSize.X];

			if (DisplayHeightAsDelta)
			{
				HeightDelta = HeightDelta >= 32768 ? HeightDelta - 32768 : HeightDelta;
			}

			if (X > 0 && MipSize > 0 && X % MipSize == 0)
			{
				HeightmapHeightOutput += FString::Printf(TEXT("  "));
			}

			FString HeightStr = FString::Printf(TEXT("%d"), HeightDelta);

			int32 PadCount = 5 - HeightStr.Len();
			if (PadCount > 0)
			{
				HeightStr = FString::ChrN(PadCount, '0') + HeightStr;
			}

			HeightmapHeightOutput += HeightStr + TEXT(" ");
		}

		if (Y > 0 && MipSize > 0 && Y % MipSize == 0)
		{
			UE_LOG(LogLandscapeBP, Display, TEXT(""));
		}

		UE_LOG(LogLandscapeBP, Display, TEXT("%s"), *HeightmapHeightOutput);
	}

	if (InOutputNormals)
	{
		UE_LOG(LogLandscapeBP, Display, TEXT(""));

		for (int32 Y = 0; Y < InDataSize.Y; ++Y)
		{
			FString HeightmapNormaltOutput;

			for (int32 X = 0; X < InDataSize.X; ++X)
			{
				FVector Normal = NormalData[X + Y * InDataSize.X];

				if (X > 0 && MipSize > 0 && X % MipSize == 0)
				{
					HeightmapNormaltOutput += FString::Printf(TEXT("  "));
				}

				HeightmapNormaltOutput += FString::Printf(TEXT(" %s"), *Normal.ToString());
			}

			if (Y > 0 && MipSize > 0 && Y % MipSize == 0)
			{
				UE_LOG(LogLandscapeBP, Display, TEXT(""));
			}

			UE_LOG(LogLandscapeBP, Display, TEXT("%s"), *HeightmapNormaltOutput);
		}
	}
}

void ALandscape::PrintDebugRTHeightmap(FString Context, UTextureRenderTarget2D* InDebugRT, int32 InMipRender, bool InOutputNormals) const
{
	bool DisplayDebugPrint = CVarOutputProceduralRTContent.GetValueOnAnyThread() == 1 ? true : false;

	if (!DisplayDebugPrint)
	{
		return;
	}

	FTextureRenderTargetResource* RenderTargetResource = InDebugRT->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(HeightmapRTCanvasRenderTargetResolveCommand)(
		[RenderTargetResource](FRHICommandListImmediate& RHICmdList)
		{
			// Copy (resolve) the rendered image from the frame buffer to its render target texture
			RHICmdList.CopyToResolveTarget(RenderTargetResource->GetRenderTargetTexture(), RenderTargetResource->TextureRHI, FResolveParams());
		});

	FlushRenderingCommands();
	int32 MinX, MinY, MaxX, MaxY;
	const ULandscapeInfo* LandscapeInfo = GetLandscapeInfo();
	LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY);
	FIntRect SampleRect = FIntRect(0, 0, InDebugRT->SizeX, InDebugRT->SizeY);

	FReadSurfaceDataFlags Flags(RCM_UNorm, CubeFace_MAX);

	TArray<FColor> OutputRTHeightmap;
	OutputRTHeightmap.Reserve(SampleRect.Width() * SampleRect.Height());

	InDebugRT->GameThread_GetRenderTargetResource()->ReadPixels(OutputRTHeightmap, Flags, SampleRect);

	PrintDebugHeightData(Context, OutputRTHeightmap, FIntPoint(SampleRect.Width(), SampleRect.Height()), InMipRender, InOutputNormals);
}

void ALandscape::RegenerateProceduralHeightmaps()
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeRegenerateProceduralHeightmaps);

	ULandscapeInfo* Info = GetLandscapeInfo();

	if (ProceduralContentUpdateFlags == 0 || Info == nullptr)
	{
		return;
	}

	TArray<ALandscapeProxy*> AllLandscapes;
	AllLandscapes.Add(this);

	for (auto& It : Info->Proxies)
	{
		AllLandscapes.Add(It);
	}

	for (ALandscapeProxy* Landscape : AllLandscapes)
	{
		for (auto& ItLayerDataPair : Landscape->ProceduralLayersData)
		{
			for (auto& ItHeightmapPair : ItLayerDataPair.Value.Heightmaps)
			{
				UTexture2D* OriginalHeightmap = ItHeightmapPair.Key;
				UTexture2D* LayerHeightmap = ItHeightmapPair.Value;

				if (!LayerHeightmap->IsAsyncCacheComplete() || !OriginalHeightmap->IsFullyStreamedIn())
				{
					return;
				}

				if (LayerHeightmap->Resource == nullptr)
				{
					LayerHeightmap->FinishCachePlatformData();

					LayerHeightmap->Resource = LayerHeightmap->CreateResource();
					if (LayerHeightmap->Resource)
					{
						BeginInitResource(LayerHeightmap->Resource);
					}
				}

				if (!LayerHeightmap->Resource->IsInitialized() || !LayerHeightmap->IsFullyStreamedIn())
				{
					return;
				}
			}
		}
	}

	TArray<ULandscapeComponent*> AllLandscapeComponents;

	for (ALandscapeProxy* Landscape : AllLandscapes)
	{
		AllLandscapeComponents.Append(Landscape->LandscapeComponents);
	}

	if ((ProceduralContentUpdateFlags & EProceduralContentUpdateFlag::Heightmap_Render) != 0 && HeightmapRTList.Num() > 0)
	{
		FLandscapeHeightmapProceduralShaderParameters ShaderParams;

		bool FirstLayer = true;
		UTextureRenderTarget2D* CombinedHeightmapAtlasRT = HeightmapRTList[EHeightmapRTType::LandscapeSizeCombinedAtlas];
		UTextureRenderTarget2D* CombinedHeightmapNonAtlasRT = HeightmapRTList[EHeightmapRTType::LandscapeSizeCombinedNonAtlas];
		UTextureRenderTarget2D* LandscapeScratchRT1 = HeightmapRTList[EHeightmapRTType::LandscapeSizeScratch1];
		UTextureRenderTarget2D* LandscapeScratchRT2 = HeightmapRTList[EHeightmapRTType::LandscapeSizeScratch2];
		UTextureRenderTarget2D* LandscapeScratchRT3 = HeightmapRTList[EHeightmapRTType::LandscapeSizeScratch3];

		bool OutputDebugName = CVarOutputProceduralDebugDrawCallName.GetValueOnAnyThread() == 1 || CVarOutputProceduralRTContent.GetValueOnAnyThread() == 1 ? true : false;

		for (FProceduralLayer& Layer : ProceduralLayers)
		{
			//Draw Layer heightmap to Combined RT Atlas
			ShaderParams.ApplyLayerModifiers = true;
			ShaderParams.LayerVisible = Layer.Visible;
			ShaderParams.LayerWeight = Layer.Weight;

			for (ALandscapeProxy* Landscape : AllLandscapes)
			{
				FProceduralLayerData* LayerData = Landscape->ProceduralLayersData.Find(Layer.Name);

				if (LayerData != nullptr)
				{
					for (auto& ItPair : LayerData->Heightmaps)
					{
						FRenderDataPerHeightmap& HeightmapRenderData = *Landscape->RenderDataPerHeightmap.Find(ItPair.Key);
						UTexture2D* Heightmap = ItPair.Value;

						CopyProceduralTargetToResolveTarget(Heightmap, LandscapeScratchRT1, nullptr, HeightmapRenderData.TopLeftSectionBase, 0);

						PrintDebugRTHeightmap(OutputDebugName ? FString::Printf(TEXT("LS Height: %s Component %s += -> CombinedAtlas %s"), *Layer.Name.ToString(), *Heightmap->GetName(), *LandscapeScratchRT1->GetName()) : TEXT(""), LandscapeScratchRT1);
					}
				}
			}

			// NOTE: From this point on, we always work in non atlas, we'll convert back at the end to atlas only
			DrawHeightmapComponentsToRenderTarget(OutputDebugName ? FString::Printf(TEXT("LS Height: %s += -> NonAtlas %s"), *Layer.Name.ToString(), *LandscapeScratchRT1->GetName(), *LandscapeScratchRT2->GetName()) : TEXT(""),
												  AllLandscapeComponents, LandscapeScratchRT1, nullptr, LandscapeScratchRT2, ERTDrawingType::RTAtlasToNonAtlas, true, ShaderParams);

			// Combine Current layer with current result
			DrawHeightmapComponentsToRenderTarget(OutputDebugName ? FString::Printf(TEXT("LS Height: %s += -> CombinedNonAtlas %s"), *Layer.Name.ToString(), *LandscapeScratchRT2->GetName(), *CombinedHeightmapNonAtlasRT->GetName()) : TEXT(""),
												AllLandscapeComponents, LandscapeScratchRT2, FirstLayer ? nullptr : LandscapeScratchRT3, CombinedHeightmapNonAtlasRT, ERTDrawingType::RTNonAtlas, FirstLayer, ShaderParams);

			ShaderParams.ApplyLayerModifiers = false;

			if (Layer.Visible)
			{
				// Draw each Combined RT into a Non Atlas RT format to be use as base for all brush rendering
				if (Layer.Brushes.Num() > 0)
				{
					CopyProceduralTargetToResolveTarget(CombinedHeightmapNonAtlasRT, LandscapeScratchRT1, nullptr, FIntPoint(0,0), 0);
					PrintDebugRTHeightmap(OutputDebugName ? FString::Printf(TEXT("LS Height: %s Component %s += -> CombinedNonAtlas %s"), *Layer.Name.ToString(), *CombinedHeightmapNonAtlasRT->GetName(), *LandscapeScratchRT1->GetName()) : TEXT(""), LandscapeScratchRT1);
				}

				// Draw each brushes				
				for (int32 i = 0; i < Layer.HeightmapBrushOrderIndices.Num(); ++i)
				{
					// TODO: handle conversion from float to RG8 by using material params to write correct values
					// TODO: handle conversion/handling of RT not same size as internal size

					FLandscapeProceduralLayerBrush& Brush = Layer.Brushes[Layer.HeightmapBrushOrderIndices[i]];

					if (Brush.BPCustomBrush == nullptr)
					{
						continue;
					}

					check(Brush.BPCustomBrush->IsAffectingHeightmap());

					if (!Brush.IsInitialized())
					{
						Brush.Initialize(GetBoundingRect(), FIntPoint(CombinedHeightmapNonAtlasRT->SizeX, CombinedHeightmapNonAtlasRT->SizeY));
					}

					UTextureRenderTarget2D* BrushOutputNonAtlasRT = Brush.Render(true, CombinedHeightmapNonAtlasRT);

					if (BrushOutputNonAtlasRT == nullptr || BrushOutputNonAtlasRT->SizeX != CombinedHeightmapNonAtlasRT->SizeX || BrushOutputNonAtlasRT->SizeY != CombinedHeightmapNonAtlasRT->SizeY)
					{
						continue;
					}

					INC_DWORD_STAT(STAT_LandscapeRegenerateProceduralHeightmapsDrawCalls); // Brush Render

					PrintDebugRTHeightmap(OutputDebugName ? FString::Printf(TEXT("LS Height: %s %s -> BrushNonAtlas %s"), *Layer.Name.ToString(), *Brush.BPCustomBrush->GetName(), *BrushOutputNonAtlasRT->GetName()) : TEXT(""), BrushOutputNonAtlasRT);

					// Resolve back to Combined heightmap
					CopyProceduralTargetToResolveTarget(BrushOutputNonAtlasRT, CombinedHeightmapNonAtlasRT, nullptr, FIntPoint(0, 0), 0);
					PrintDebugRTHeightmap(OutputDebugName ? FString::Printf(TEXT("LS Height: %s Component %s += -> CombinedNonAtlas %s"), *Layer.Name.ToString(), *BrushOutputNonAtlasRT->GetName(), *CombinedHeightmapNonAtlasRT->GetName()) : TEXT(""), CombinedHeightmapNonAtlasRT);
				}
			}

			CopyProceduralTargetToResolveTarget(CombinedHeightmapNonAtlasRT, LandscapeScratchRT3, nullptr, FIntPoint(0, 0), 0);
			PrintDebugRTHeightmap(OutputDebugName ? FString::Printf(TEXT("LS Height: %s Component %s += -> CombinedNonAtlas %s"), *Layer.Name.ToString(), *CombinedHeightmapNonAtlasRT->GetName(), *LandscapeScratchRT3->GetName()) : TEXT(""), LandscapeScratchRT3);

			FirstLayer = false;
		}

		ShaderParams.GenerateNormals = true;
		ShaderParams.GridSize = GetRootComponent()->RelativeScale3D;

		DrawHeightmapComponentsToRenderTarget(OutputDebugName ? FString::Printf(TEXT("LS Height: %s = -> CombinedNonAtlasNormals : %s"), *CombinedHeightmapNonAtlasRT->GetName(), *LandscapeScratchRT1->GetName()) : TEXT(""),
											  AllLandscapeComponents, CombinedHeightmapNonAtlasRT, nullptr, LandscapeScratchRT1, ERTDrawingType::RTNonAtlas, true, ShaderParams);

		ShaderParams.GenerateNormals = false;

		DrawHeightmapComponentsToRenderTarget(OutputDebugName ? FString::Printf(TEXT("LS Height: %s = -> CombinedAtlasFinal : %s"), *LandscapeScratchRT1->GetName(), *CombinedHeightmapAtlasRT->GetName()) : TEXT(""),
											  AllLandscapeComponents, LandscapeScratchRT1, nullptr, CombinedHeightmapAtlasRT, ERTDrawingType::RTNonAtlasToAtlas, true, ShaderParams);

		DrawHeightmapComponentsToRenderTargetMips(AllLandscapeComponents, CombinedHeightmapAtlasRT, true, ShaderParams);

		// Copy back all Mips to original heightmap data
		for (ALandscapeProxy* Landscape : AllLandscapes)
		{
			for (auto& ItPair : Landscape->RenderDataPerHeightmap)
			{
				int32 CurrentMip = 0;
				FRenderDataPerHeightmap& HeightmapRenderData = ItPair.Value;

				CopyProceduralTargetToResolveTarget(CombinedHeightmapAtlasRT, HeightmapRenderData.OriginalHeightmap, HeightmapRenderData.HeightmapsCPUReadBack, HeightmapRenderData.TopLeftSectionBase, CurrentMip++);

				for (int32 MipRTIndex = EHeightmapRTType::LandscapeSizeMip1; MipRTIndex < EHeightmapRTType::Count; ++MipRTIndex)
				{
					if (HeightmapRTList[MipRTIndex] != nullptr)
					{
						CopyProceduralTargetToResolveTarget(HeightmapRTList[MipRTIndex], HeightmapRenderData.OriginalHeightmap, HeightmapRenderData.HeightmapsCPUReadBack, HeightmapRenderData.TopLeftSectionBase, CurrentMip++);
					}
				}
			}
		}
	}

	if ((ProceduralContentUpdateFlags & EProceduralContentUpdateFlag::Heightmap_ResolveToTexture) != 0 || (ProceduralContentUpdateFlags & EProceduralContentUpdateFlag::Heightmap_ResolveToTextureDDC) != 0)
	{
		ResolveProceduralHeightmapTexture((ProceduralContentUpdateFlags & EProceduralContentUpdateFlag::Heightmap_ResolveToTextureDDC) != 0);
	}

	if ((ProceduralContentUpdateFlags & EProceduralContentUpdateFlag::Heightmap_BoundsAndCollision) != 0)
	{
		for (ULandscapeComponent* Component : AllLandscapeComponents)
		{
			Component->UpdateCachedBounds();
			Component->UpdateComponentToWorld();

			Component->UpdateCollisionData(false);
		}
	}

	ProceduralContentUpdateFlags = 0;

	// If doing rendering debug, keep doing the render only
	if (CVarOutputProceduralDebugDrawCallName.GetValueOnAnyThread() == 1)
	{
		ProceduralContentUpdateFlags = EProceduralContentUpdateFlag::Heightmap_Render;
	}
}

void ALandscape::ResolveProceduralHeightmapTexture(bool InUpdateDDC)
{
	SCOPE_CYCLE_COUNTER(STAT_LandscapeResolveProceduralHeightmap);

	ULandscapeInfo* Info = GetLandscapeInfo();

	TArray<ALandscapeProxy*> AllLandscapes;
	AllLandscapes.Add(this);

	for (auto& It : Info->Proxies)
	{
		AllLandscapes.Add(It);
	}

	TArray<UTexture2D*> PendingDDCUpdateTextureList;

	for (ALandscapeProxy* Landscape : AllLandscapes)
	{
		TArray<TArray<FColor>> MipData;

		for (auto& ItPair : Landscape->RenderDataPerHeightmap)
		{
			FRenderDataPerHeightmap& HeightmapRenderData = ItPair.Value;

			if (HeightmapRenderData.HeightmapsCPUReadBack == nullptr)
			{
				continue;
			}

			if (MipData.Num() == 0)
			{
				MipData.AddDefaulted(HeightmapRenderData.HeightmapsCPUReadBack->TextureRHI->GetNumMips());
			}

			int32 MipSizeU = HeightmapRenderData.HeightmapsCPUReadBack->GetSizeX();
			int32 MipSizeV = HeightmapRenderData.HeightmapsCPUReadBack->GetSizeY();
			int32 MipIndex = 0;

			while (MipSizeU >= 1 && MipSizeV >= 1)
			{
				MipData[MipIndex].Reset();

				FReadSurfaceDataFlags Flags(RCM_UNorm, CubeFace_MAX);
				Flags.SetMip(MipIndex);
				FIntRect Rect(0, 0, MipSizeU, MipSizeV);

				{
					TArray<FColor>* OutData = &MipData[MipIndex];
					FTextureRHIRef SourceTextureRHI = HeightmapRenderData.HeightmapsCPUReadBack->TextureRHI;
					ENQUEUE_RENDER_COMMAND(ReadSurfaceCommand)(
						[SourceTextureRHI, Rect, OutData, Flags](FRHICommandListImmediate& RHICmdList)
						{
							RHICmdList.ReadSurfaceData(SourceTextureRHI, Rect, *OutData, Flags);
						});
				}

				MipSizeU >>= 1;
				MipSizeV >>= 1;
				++MipIndex;
			}

			FlushRenderingCommands();

			for (MipIndex = 0; MipIndex < MipData.Num(); ++MipIndex)
			{
				if (MipData[MipIndex].Num() > 0)
				{
					PrintDebugHeightData(CVarOutputProceduralRTContent.GetValueOnAnyThread() == 1 ? FString::Printf(TEXT("CPUReadBack -> Source Heightmap %s, Mip: %d"), *HeightmapRenderData.OriginalHeightmap->GetName(), MipIndex) : TEXT(""),
										 MipData[MipIndex], FIntPoint(HeightmapRenderData.HeightmapsCPUReadBack->GetSizeX() >> MipIndex, HeightmapRenderData.HeightmapsCPUReadBack->GetSizeY() >> MipIndex), MipIndex, true);

					FColor* HeightmapTextureData = (FColor*)HeightmapRenderData.OriginalHeightmap->Source.LockMip(MipIndex);
					FMemory::Memzero(HeightmapTextureData, MipData[MipIndex].Num() * sizeof(FColor));
					FMemory::Memcpy(HeightmapTextureData, MipData[MipIndex].GetData(), MipData[MipIndex].Num() * sizeof(FColor));
					HeightmapRenderData.OriginalHeightmap->Source.UnlockMip(MipIndex);
				}
			}

			if (InUpdateDDC)
			{
				HeightmapRenderData.OriginalHeightmap->BeginCachePlatformData();
				HeightmapRenderData.OriginalHeightmap->ClearAllCachedCookedPlatformData();
				PendingDDCUpdateTextureList.Add(HeightmapRenderData.OriginalHeightmap);
				HeightmapRenderData.OriginalHeightmap->MarkPackageDirty();
			}
		}
	}	

	if (InUpdateDDC)
	{
		// Wait for all texture to be finished, do them async, since we can have many to update but we still need to wait for all of them to be finish before continuing
		for (UTexture* PendingDDCUpdateTexture : PendingDDCUpdateTextureList)
		{
			PendingDDCUpdateTexture->FinishCachePlatformData();

			PendingDDCUpdateTexture->Resource = PendingDDCUpdateTexture->CreateResource();
			if (PendingDDCUpdateTexture->Resource)
			{
				BeginInitResource(PendingDDCUpdateTexture->Resource);
			}
		}
	}
}

void ALandscape::RegenerateProceduralWeightmaps()
{

}

void ALandscape::RequestProceduralContentUpdate(uint32 InDataFlags)
{
	ProceduralContentUpdateFlags = InDataFlags;
}

void ALandscape::RegenerateProceduralContent()
{
	if ((ProceduralContentUpdateFlags & Heightmap_Setup) != 0 || (ProceduralContentUpdateFlags & Weightmap_Setup) != 0)
	{
		SetupProceduralLayers();
	}

	RegenerateProceduralHeightmaps();
	RegenerateProceduralWeightmaps();
}

void ALandscape::OnPreSaveWorld(uint32 SaveFlags, UWorld* World)
{
	if (GetMutableDefault<UEditorExperimentalSettings>()->bProceduralLandscape)
	{
		// Need to perform setup here, as it's possible to get here with the data not setup, when doing a Save As on a level
		if (PreviousExperimentalLandscapeProcedural != GetMutableDefault<UEditorExperimentalSettings>()->bProceduralLandscape)
		{
			PreviousExperimentalLandscapeProcedural = GetMutableDefault<UEditorExperimentalSettings>()->bProceduralLandscape;
			RequestProceduralContentUpdate(EProceduralContentUpdateFlag::All_Setup | EProceduralContentUpdateFlag::All_WithDDCUpdate);
		}
		else
		{
			RequestProceduralContentUpdate(EProceduralContentUpdateFlag::Heightmap_ResolveToTextureDDC | EProceduralContentUpdateFlag::Weightmap_ResolveToTextureDDC);
		}

		RegenerateProceduralContent();
		ProceduralContentUpdateFlags = 0; // Force reset so we don't end up performing save info at the next Tick
	}
}

void ALandscape::OnPostSaveWorld(uint32 SaveFlags, UWorld* World, bool bSuccess)
{	
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE