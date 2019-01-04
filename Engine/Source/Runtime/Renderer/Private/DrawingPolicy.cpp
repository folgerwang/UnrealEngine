// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DrawingPolicy.cpp: Base drawing policy implementation.
=============================================================================*/

#include "DrawingPolicy.h"
#include "SceneUtils.h"
#include "SceneRendering.h"
#include "Logging/LogMacros.h"
#include "MaterialShader.h"
#include "DebugViewModeRendering.h"
#include "SceneCore.h"
#include "ScenePrivate.h"

int32 GEmitMeshDrawEvent = 0;
static FAutoConsoleVariableRef CVarEmitMeshDrawEvent(
	TEXT("r.EmitMeshDrawEvents"),
	GEmitMeshDrawEvent,
	TEXT("Emits a GPU event around each drawing policy draw call.  /n")
	TEXT("Useful for seeing stats about each draw call, however it greatly distorts total time and time per draw call."),
	ECVF_RenderThreadSafe
	);

FMeshDrawingPolicy::FMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	const FMeshDrawingPolicyOverrideSettings& InOverrideSettings
	):
	VertexFactory(InVertexFactory),
	MaterialRenderProxy(InMaterialRenderProxy),
	MaterialResource(&InMaterialResource),
	MeshPrimitiveType(InOverrideSettings.MeshPrimitiveType),
	bIsDitheredLODTransitionMaterial(InMaterialResource.IsDitheredLODTransition() || !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::DitheredLODTransition))
{
	// using this saves a virtual function call
	bool bMaterialResourceIsTwoSided = InMaterialResource.IsTwoSided();
	
	const bool bIsWireframeMaterial = InMaterialResource.IsWireframe() || !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::Wireframe);
	MeshFillMode = bIsWireframeMaterial ? FM_Wireframe : FM_Solid;

	const bool bInTwoSidedOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::TwoSided);
	const bool bInReverseCullModeOverride = !!(InOverrideSettings.MeshOverrideFlags & EDrawingPolicyOverrideFlags::ReverseCullMode);
	const bool bIsTwoSided = (bMaterialResourceIsTwoSided || bInTwoSidedOverride);
	const bool bMeshRenderTwoSided = bIsTwoSided || bInTwoSidedOverride;
	MeshCullMode = (bMeshRenderTwoSided) ? CM_None : (bInReverseCullModeOverride ? CM_CCW : CM_CW);

	bUsePositionOnlyVS = false;
}

void FMeshDrawingPolicy::OnlyApplyDitheredLODTransitionState(FDrawingPolicyRenderState& DrawRenderState, const FViewInfo& ViewInfo, const FStaticMesh& Mesh, const bool InAllowStencilDither)
{
	DrawRenderState.SetDitheredLODTransitionAlpha(0.0f);

	if (Mesh.bDitheredLODTransition && !InAllowStencilDither)
	{
		if (ViewInfo.StaticMeshFadeOutDitheredLODMap[Mesh.Id])
		{
			DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition());
		}
		else if (ViewInfo.StaticMeshFadeInDitheredLODMap[Mesh.Id])
		{
			DrawRenderState.SetDitheredLODTransitionAlpha(ViewInfo.GetTemporalLODTransition() - 1.0f);
		}
	}
}

void FMeshDrawingPolicy::SetInstanceParameters(FRHICommandList& RHICmdList, const FSceneView& View, uint32 InInstanceOffset, uint32 InInstanceCount) const
{
	BaseVertexShader->SetInstanceParameters(RHICmdList, InInstanceOffset, InInstanceCount);
}

void FMeshDrawingPolicy::SetPrimitiveIdStream(
	FRHICommandList& RHICmdList,
	const FSceneView& View,
	const FPrimitiveSceneProxy* PrimitiveSceneProxy,
	EPrimitiveIdMode PrimitiveIdMode,
	uint32 DynamicPrimitiveShaderDataIndex
	) const
{
	if (UseGPUScene(GMaxRHIShaderPlatform, View.GetFeatureLevel()))
	{
		const int8 PrimitiveIdStreamIndex = VertexFactory->GetPrimitiveIdStreamIndex(GetUsePositionOnlyVS());

		if (PrimitiveIdStreamIndex >= 0)
		{
			const FScene* Scene = ((const FScene*)View.Family->Scene);

			if (Scene)
			{
				int32 PrimitiveId = 0;

				if (PrimitiveIdMode == PrimID_FromPrimitiveSceneInfo)
				{
					PrimitiveId = PrimitiveSceneProxy->GetPrimitiveSceneInfo()->GetIndex();
				}
				else if (PrimitiveIdMode == PrimID_DynamicPrimitiveShaderData)
				{
					PrimitiveId = Scene->Primitives.Num() + DynamicPrimitiveShaderDataIndex;
				}

				// PrimitiveIdBufferEmulation is setup so the value at a given offset is equal to the offset, this allows passing a different PrimitiveId to each draw without updating any constant buffers
				RHICmdList.SetStreamSource(PrimitiveIdStreamIndex, ((const FViewInfo&)View).OneFramePrimitiveIdBufferEmulation.Buffer, PrimitiveId * sizeof(int32));
			}
			else
			{
				check(PrimitiveIdMode == PrimID_ForceZero);
				// Note: DrawTileMesh is relying on the shader getting a PrimitiveId of 0 when Scene is null
				RHICmdList.SetStreamSource(PrimitiveIdStreamIndex, GPrimitiveIdDummy.VertexBufferRHI, 0);
			}
		}
	}
}

void FMeshDrawingPolicy::DrawMesh(FRHICommandList& RHICmdList, const FSceneView& View, const FMeshBatch& Mesh, int32 BatchElementIndex, const bool bIsInstancedStereo) const
{
	DEFINE_LOG_CATEGORY_STATIC(LogFMeshDrawingPolicyDrawMesh, Warning, All);
	INC_DWORD_STAT(STAT_MeshDrawCalls);
	SCOPED_CONDITIONAL_DRAW_EVENTF(RHICmdList, MeshEvent, GEmitMeshDrawEvent != 0, TEXT("Mesh Draw"));

	const FMeshBatchElement& BatchElement = Mesh.Elements[BatchElementIndex];

	if (BatchElement.IndexBuffer)
	{
		UE_CLOG(!BatchElement.IndexBuffer->IndexBufferRHI, LogFMeshDrawingPolicyDrawMesh, Fatal,
			TEXT("FMeshDrawingPolicy::DrawMesh - BatchElement has an index buffer object with null RHI resource (drawing using material \"%s\")"),
			MaterialRenderProxy ? *MaterialRenderProxy->GetFriendlyName() : TEXT("null"));
		check(BatchElement.IndexBuffer->IsInitialized());

		if (BatchElement.bIsInstanceRuns)
		{
			if (!GRHISupportsFirstInstance)
			{
				if (GetUsePositionOnlyVS())
				{
					for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
					{
						const uint32 InstanceOffset = BatchElement.InstanceRuns[Run * 2];
						const uint32 InstanceCount = (1 + BatchElement.InstanceRuns[Run * 2 + 1] - InstanceOffset);
						SetInstanceParameters(RHICmdList, View, InstanceOffset, InstanceCount);
						Mesh.VertexFactory->OffsetPositionInstanceStreams(RHICmdList, InstanceOffset);

						RHICmdList.DrawIndexedPrimitive(
							BatchElement.IndexBuffer->IndexBufferRHI,
							BatchElement.BaseVertexIndex,
							0,
							BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
							BatchElement.FirstIndex,
							BatchElement.NumPrimitives,
							InstanceCount * GetInstanceFactor()
						);
					}
				}
				else
				{
					for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
					{
						const uint32 InstanceOffset = BatchElement.InstanceRuns[Run * 2];
						const uint32 InstanceCount = (1 + BatchElement.InstanceRuns[Run * 2 + 1] - InstanceOffset);
						SetInstanceParameters(RHICmdList, View, InstanceOffset, InstanceCount);
						Mesh.VertexFactory->OffsetInstanceStreams(RHICmdList, InstanceOffset);

						RHICmdList.DrawIndexedPrimitive(
							BatchElement.IndexBuffer->IndexBufferRHI,
							BatchElement.BaseVertexIndex,
							0,
							BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
							BatchElement.FirstIndex,
							BatchElement.NumPrimitives,
							InstanceCount * GetInstanceFactor()
						);
					}
				}
			}
			else
			{
				for (uint32 Run = 0; Run < BatchElement.NumInstances; Run++)
				{
					const uint32 InstanceOffset = BatchElement.InstanceRuns[Run * 2];
					const uint32 InstanceCount = (1 + BatchElement.InstanceRuns[Run * 2 + 1] - InstanceOffset);
					SetInstanceParameters(RHICmdList, View, InstanceOffset, InstanceCount);

					RHICmdList.DrawIndexedPrimitive(
						BatchElement.IndexBuffer->IndexBufferRHI,
						BatchElement.BaseVertexIndex,
						InstanceOffset,
						BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
						BatchElement.FirstIndex,
						BatchElement.NumPrimitives,
						InstanceCount * GetInstanceFactor()
					);
				}
			}
		}
		else
		{
			if (BatchElement.IndirectArgsBuffer)
			{
				RHICmdList.DrawIndexedPrimitiveIndirect(
					BatchElement.IndexBuffer->IndexBufferRHI, 
					BatchElement.IndirectArgsBuffer, 
					0
				);
			}
			else
			{
				// Currently only supporting this path for instanced stereo.
				const uint32 InstanceCount = ((bIsInstancedStereo && !BatchElement.bIsInstancedMesh) ? 2 : BatchElement.NumInstances);
				SetInstanceParameters(RHICmdList, View, 0, InstanceCount);

				RHICmdList.DrawIndexedPrimitive(
					BatchElement.IndexBuffer->IndexBufferRHI,
					BatchElement.BaseVertexIndex,
					0,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives,
					InstanceCount * GetInstanceFactor()
				);
			}
		}
	}
	else
	{
		SetInstanceParameters(RHICmdList, View, 0, 1);

		RHICmdList.DrawPrimitive(
			BatchElement.BaseVertexIndex + BatchElement.FirstIndex,
			BatchElement.NumPrimitives,
			BatchElement.NumInstances
		);
	}
}

void FMeshDrawingPolicy::SetSharedState(FRHICommandList& RHICmdList, const FDrawingPolicyRenderState& DrawRenderState, const FSceneView* View, const FMeshDrawingPolicy::ContextDataType PolicyContext) const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->SetStreams(View->FeatureLevel, RHICmdList);
}

/**
* Get the decl and stream strides for this mesh policy type and vertexfactory
* @param VertexDeclaration - output decl 
*/
const FVertexDeclarationRHIRef& FMeshDrawingPolicy::GetVertexDeclaration() const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	const FVertexDeclarationRHIRef& VertexDeclaration = VertexFactory->GetDeclaration();
	check(VertexFactory->NeedsDeclaration()==false || IsValidRef(VertexDeclaration));

	return VertexDeclaration;
}
