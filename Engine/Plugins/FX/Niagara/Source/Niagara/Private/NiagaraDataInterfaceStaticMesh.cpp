// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceStaticMesh.h"
#include "NiagaraEmitterInstance.h"
#include "Engine/StaticMeshActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystemInstance.h"
#include "Internationalization/Internationalization.h"
#include "NiagaraScript.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceStaticMesh"

FStaticMeshFilteredAreaWeightedSectionSampler::FStaticMeshFilteredAreaWeightedSectionSampler()
	: Res(nullptr)
	, Owner(nullptr)
{
}

void FStaticMeshFilteredAreaWeightedSectionSampler::Init(FStaticMeshLODResources* InRes, FNDIStaticMesh_InstanceData* InOwner)
{
	Res = InRes;
	Owner = InOwner;

	FStaticMeshAreaWeightedSectionSampler::Init(Res);
}

float FStaticMeshFilteredAreaWeightedSectionSampler::GetWeights(TArray<float>& OutWeights)
{
	check(Owner && Owner->Mesh);
	float Total = 0.0f;
	OutWeights.Empty(Owner->GetValidSections().Num());
	FStaticMeshLODResources& LODRes = Owner->Mesh->RenderData->LODResources[0];
	for (int32 i = 0; i < Owner->GetValidSections().Num(); ++i)
	{
		int32 SecIdx = Owner->GetValidSections()[i];
		float T = LODRes.AreaWeightedSectionSamplers[SecIdx].GetTotalWeight();
		OutWeights.Add(T);
		Total += T;
	}
	return Total;
}

//////////////////////////////////////////////////////////////////////////
//FNDIStaticMesh_InstanceData

void FNDIStaticMesh_InstanceData::InitVertexColorFiltering()
{
	DynamicVertexColorSampler = FNDI_StaticMesh_GeneratedData::GetDynamicColorFilterData(this);
}

bool FNDIStaticMesh_InstanceData::Init(UNiagaraDataInterfaceStaticMesh* Interface, FNiagaraSystemInstance* SystemInstance)
{
	check(SystemInstance);
	UStaticMesh* PrevMesh = Mesh;
	Component = nullptr;
	Mesh = nullptr;
	Transform = FMatrix::Identity;
	TransformInverseTransposed = FMatrix::Identity;
	PrevTransform = FMatrix::Identity;
	PrevTransformInverseTransposed = FMatrix::Identity;
	DeltaSeconds = 0.0f;
	ChangeId = Interface->ChangeId;

	if (Interface->Source)
	{
		AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Interface->Source);
		UStaticMeshComponent* SourceComp = nullptr;
		if (MeshActor != nullptr)
		{
			SourceComp = MeshActor->GetStaticMeshComponent();
		}
		else
		{
			SourceComp = Interface->Source->FindComponentByClass<UStaticMeshComponent>();
		}

		if (SourceComp)
		{
			Mesh = SourceComp->GetStaticMesh();
			Component = SourceComp;
		}
		else
		{
			Component = Interface->Source->GetRootComponent();
		}
	}
	else
	{
		if (UNiagaraComponent* SimComp = SystemInstance->GetComponent())
		{
			if (UStaticMeshComponent* ParentComp = Cast<UStaticMeshComponent>(SimComp->GetAttachParent()))
			{
				Component = ParentComp;
				Mesh = ParentComp->GetStaticMesh();
			}
			else if (UStaticMeshComponent* OuterComp = SimComp->GetTypedOuter<UStaticMeshComponent>())
			{
				Component = OuterComp;
				Mesh = OuterComp->GetStaticMesh();
			}
			else if (AActor* Owner = SimComp->GetAttachmentRootActor())
			{
				TArray<UActorComponent*> SourceComps = Owner->GetComponentsByClass(UStaticMeshComponent::StaticClass());
				for (UActorComponent* ActorComp : SourceComps)
				{
					UStaticMeshComponent* SourceComp = Cast<UStaticMeshComponent>(ActorComp);
					if (SourceComp)
					{
						UStaticMesh* PossibleMesh = SourceComp->GetStaticMesh();
						if (PossibleMesh != nullptr && PossibleMesh->bAllowCPUAccess)
						{
							Mesh = PossibleMesh;
							Component = SourceComp;
							break;
						}
					}
				}
			}

			if (!Component.IsValid())
			{
				Component = SimComp;
			}
		}
	}

	check(Component.IsValid());

	if (!Mesh && Interface->DefaultMesh)
	{
		Mesh = Interface->DefaultMesh;
	}

	if (Component.IsValid() && Mesh)
	{
		PrevTransform = Transform;
		PrevTransformInverseTransposed = TransformInverseTransposed;
		Transform = Component->GetComponentToWorld().ToMatrixWithScale();
		TransformInverseTransposed = Transform.InverseFast().GetTransposed();
	}

	if (!Mesh)
	{
		UE_LOG(LogNiagara, Log, TEXT("StaticMesh data interface has no valid mesh. Failed InitPerInstanceData - %s"), *Interface->GetFullName());
		return false;
	}

	if (!Mesh->bAllowCPUAccess)
	{
		UE_LOG(LogNiagara, Log, TEXT("StaticMesh data interface using a mesh that does not allow CPU access. Failed InitPerInstanceData - Mesh: %s"), *Mesh->GetFullName());
		return false;
	}

	if (!Component.IsValid())
	{
		UE_LOG(LogNiagara, Log, TEXT("StaticMesh data interface has no valid component. Failed InitPerInstanceData - %s"), *Interface->GetFullName());
		return false;
	}

#if WITH_EDITOR
	Mesh->GetOnMeshChanged().AddUObject(SystemInstance->GetComponent(), &UNiagaraComponent::ReinitializeSystem);
#endif

	bIsAreaWeightedSampling = Mesh->bSupportUniformlyDistributedSampling;

	//Init the instance filter
	ValidSections.Empty();
	FStaticMeshLODResources& Res = Mesh->RenderData->LODResources[0];
	for (int32 i = 0; i < Res.Sections.Num(); ++i)
	{
		if (Interface->SectionFilter.AllowedMaterialSlots.Num() == 0 || Interface->SectionFilter.AllowedMaterialSlots.Contains(Res.Sections[i].MaterialIndex))
		{
			ValidSections.Add(i);
		}
	}

	if (GetValidSections().Num() == 0)
	{
		UE_LOG(LogNiagara, Log, TEXT("StaticMesh data interface has a section filter preventing any spawning. Failed InitPerInstanceData - %s"), *Interface->GetFullName());
		return false;
	}

	Sampler.Init(&Res, this);

	return true;
}

bool FNDIStaticMesh_InstanceData::ResetRequired(UNiagaraDataInterfaceStaticMesh* Interface)const
{
	check(GetActualMesh());

	if (!Component.IsValid())
	{
		//The component we were bound to is no longer valid so we have to trigger a reset.
		return true;
	}

	if (Interface != nullptr && ChangeId != Interface->ChangeId)
	{
		return true;
	}

	bool bPrevAreaWeighted = bIsAreaWeightedSampling;
	//bool bPrevVCSampling = bSupportingVertexColorSampling;//TODO: Vertex color filtering needs more work.
	bool bReset = false;
	if (Mesh)
	{
		//bSupportingVertexColorSampling = bEnableVertexColorRangeSorting && MeshHasColors();
		bReset = !Mesh->bAllowCPUAccess || Mesh->bSupportUniformlyDistributedSampling != bPrevAreaWeighted/* || bSupportingVertexColorSampling != bPrevVCSampling*/;
	}
	return bReset;
}

bool FNDIStaticMesh_InstanceData::Tick(UNiagaraDataInterfaceStaticMesh* Interface, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	if (ResetRequired(Interface))
	{
		return true;
	}
	else
	{
		DeltaSeconds = InDeltaSeconds;
		if (Component.IsValid() && Mesh)
		{
			PrevTransform = Transform;
			PrevTransformInverseTransposed = TransformInverseTransposed;
			Transform = Component->GetComponentToWorld().ToMatrixWithScale();
			TransformInverseTransposed = Transform.InverseFast().GetTransposed();
		}
		else
		{
			PrevTransform = FMatrix::Identity;
			PrevTransformInverseTransposed = FMatrix::Identity;
			Transform = FMatrix::Identity;
			TransformInverseTransposed = FMatrix::Identity;
		}
		return false;
	}
}

//void UNiagaraDataInterfaceStaticMesh::ResetFilter()
//{
//#if WITH_EDITOR	TODO: Make editor only and serialize this stuff out.
//	SectionFilter.Init(this, UsesAreaWeighting());

//TODO: Vertex color filtering needs some more work.
// 	// If we have enabled vertex color sorting for emission, given that these are vertex colors
// 	// and is limited to a byte, we can make lookup relatively quick by just having a bucket
// 	// for every possible entry.
// 	// Will want a better strategy in the long run, but for now this is trivial for GDC..
// 	TrianglesSortedByVertexColor.Empty();
// 	if (bEnableVertexColorRangeSorting && bSupportingVertexColorSampling)
// 	{
// 		VertexColorToTriangleStart.AddDefaulted(256);
// 		if (UStaticMesh* ActualMesh = GetActualMesh())
// 		{
// 			FStaticMeshLODResources& Res = ActualMesh->RenderData->LODResources[0];
// 
// 			// Go over all triangles for each possible vertex color and add it to that bucket
// 			for (int32 i = 0; i < VertexColorToTriangleStart.Num(); i++)
// 			{
// 				uint32 MinVertexColorRed = i;
// 				uint32 MaxVertexColorRed = i + 1;
// 				VertexColorToTriangleStart[i] = TrianglesSortedByVertexColor.Num();
// 
// 				FIndexArrayView IndexView = Res.IndexBuffer.GetArrayView();
// 				for (int32 j = 0; j < SectionFilter.GetValidSections().Num(); j++)
// 				{
// 					int32 SectionIdx = SectionFilter.GetValidSections()[j];
// 					int32 TriStartIdx = Res.Sections[SectionIdx].FirstIndex;
// 					for (uint32 TriIdx = 0; TriIdx < Res.Sections[SectionIdx].NumTriangles; TriIdx++)
// 					{
// 						uint32 V0Idx = IndexView[TriStartIdx + TriIdx * 3 + 0];
// 						uint32 V1Idx = IndexView[TriStartIdx + TriIdx * 3 + 1];
// 						uint32 V2Idx = IndexView[TriStartIdx + TriIdx * 3 + 2];
// 
// 						uint8 MaxR = FMath::Max<uint8>(Res.ColorVertexBuffer.VertexColor(V0Idx).R,
// 							FMath::Max<uint8>(Res.ColorVertexBuffer.VertexColor(V1Idx).R,
// 								Res.ColorVertexBuffer.VertexColor(V2Idx).R));
// 						if (MaxR >= MinVertexColorRed && MaxR < MaxVertexColorRed)
// 						{
// 							TrianglesSortedByVertexColor.Add(TriStartIdx + TriIdx * 3);
// 						}
// 					}
// 				}
// 			}
// 		}
// 	}
//	bFilterInitialized = true;

//#endif
//}

//////////////////////////////////////////////////////////////////////////


UNiagaraDataInterfaceStaticMesh::UNiagaraDataInterfaceStaticMesh(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultMesh(nullptr)
	, Source(nullptr)	
	, ChangeId(0)
	//, bSupportingVertexColorSampling(0)//Vertex color filtering needs some more work.
	//, bFilterInitialized(false)
{

}


#if WITH_EDITOR

void UNiagaraDataInterfaceStaticMesh::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);

		//Still some issues with using custom structs. Convert node for example throws a wobbler. TODO after GDC.
		FNiagaraTypeRegistry::Register(FMeshTriCoordinate::StaticStruct(), true, true, false);
	}
}

void UNiagaraDataInterfaceStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ChangeId++;
}

#endif //WITH_EDITOR

namespace StaticMeshHelpers
{
	static const FName RandomSectionName("RandomSection");
	static const FName RandomTriCoordName("RandomTriCoord");
	static const FName RandomTriCoordOnSectionName("RandomTriCoordOnSection");
	static const FName RandomTriCoordVCFilteredName("RandomTriCoordUsingVertexColorFilter");

	static const FName GetTriPositionName("GetTriPosition");
	static const FName GetTriNormalName("GetTriNormal");
	static const FName GetTriTangentsName("GetTriTangents");

	static const FName GetTriPositionWSName("GetTriPositionWS");
	static const FName GetTriNormalWSName("GetTriNormalWS");
	static const FName GetTriTangentsWSName("GetTriTangentsWS");

	static const FName GetTriColorName("GetTriColor");
	static const FName GetTriUVName("GetTriUV");

	static const FName GetTriPositionAndVelocityName("GetTriPositionAndVelocityWS");

	/** Temporary solution for exposing the transform of a mesh. Ideally this would be done by allowing interfaces to add to the uniform set for a simulation. */
	static const FName GetMeshLocalToWorldName("GetLocalToWorld");
	static const FName GetMeshLocalToWorldInverseTransposedName("GetMeshLocalToWorldInverseTransposed");
	static const FName GetMeshWorldVelocityName("GetWorldVelocity");

	static const FName GetVertexPositionName("GetVertexPosition"); 
	static const FName GetVertexPositionWSName("GetVertexPositionWS"); 
};

void UNiagaraDataInterfaceStaticMesh::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::RandomSectionName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Section")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::RandomTriCoordName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::RandomTriCoordVCFilteredName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("Start")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("Range")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		Sig.SetDescription(LOCTEXT("DataInterfaceSpline_RandomTriCoordVCFiltered", "If bSupportingVertexColorSampling is set on the data source, will randomly find a triangle whose red channel is within the Start to Start + Range color range."));
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::RandomTriCoordOnSectionName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Section")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriPositionName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriPositionAndVelocityName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriPositionWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriNormalName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//	Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriNormalWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriTangentsName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriTangentsWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Tangent")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Binormal")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriColorName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetTriUVName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Tri Index")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Bary Coord")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("UV Set")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetMeshLocalToWorldName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetMatrix4Def(), TEXT("Transform")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetMeshLocalToWorldInverseTransposedName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetMatrix4Def(), TEXT("Transform")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetMeshWorldVelocityName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		//Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FMeshTriCoordinate::StaticStruct()), TEXT("Coord")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetVertexPositionName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Vertex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetVertexPositionDesc", "Returns the local space vertex position for the passed vertex.");
#endif
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	} 
	
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = StaticMeshHelpers::GetVertexPositionWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("StaticMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Vertex")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetVertexPositionWSDesc", "Returns the world space vertex position for the passed vertex.");
#endif
		//Sig.Owner = *GetFullName();
		OutFunctions.Add(Sig);
	}
}

//External function binder choosing between template specializations based on UsesAreaWeighting
template<typename NextBinder>
struct TUsesAreaWeightingBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		FNDIStaticMesh_InstanceData* InstData = (FNDIStaticMesh_InstanceData*)InstanceData;
		UNiagaraDataInterfaceStaticMesh* MeshInterface = CastChecked<UNiagaraDataInterfaceStaticMesh>(Interface);
		if (InstData->UsesAreaWeighting())
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<bool, true>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
		else
		{
			NextBinder::template Bind<ParamTypes..., TIntegralConstant<bool, false>>(Interface, BindingInfo, InstanceData, OutFunc);
		}
	}
};

//Helper struct for accessing typed vertex data.
template<EStaticMeshVertexTangentBasisType TangentT, EStaticMeshVertexUVType UVTypeT>
struct TTypedMeshVertexAccessor
{
	const FStaticMeshVertexBuffer& Verts;
	TTypedMeshVertexAccessor(const FStaticMeshVertexBuffer& InVerts)
		: Verts(InVerts)
	{}

	FORCEINLINE FVector GetTangentX(int32 Idx)const { return Verts.VertexTangentX_Typed<TangentT>(Idx); }
	FORCEINLINE FVector GetTangentY(int32 Idx)const { return Verts.VertexTangentY_Typed<TangentT>(Idx); }
	FORCEINLINE FVector GetTangentZ(int32 Idx)const { return Verts.VertexTangentZ_Typed<TangentT>(Idx); }
	FORCEINLINE FVector2D GetUV(int32 Idx, int32 UVSet)const { return Verts.GetVertexUV_Typed<UVTypeT>(Idx, UVSet); }
};

//External function binder choosing between template specializations based on the mesh's vertex type.
template<typename NextBinder>
struct TTypedMeshAccessorBinder
{
	template<typename... ParamTypes>
	static void Bind(UNiagaraDataInterface* Interface, const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
	{
		FNDIStaticMesh_InstanceData* InstData = (FNDIStaticMesh_InstanceData*)InstanceData;
		UNiagaraDataInterfaceStaticMesh* MeshInterface = CastChecked<UNiagaraDataInterfaceStaticMesh>(Interface);
		check(InstData->Mesh);
		FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
		if (Res.VertexBuffers.StaticMeshVertexBuffer.GetUseHighPrecisionTangentBasis())			
		{
			if (Res.VertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs())
			{
				NextBinder::template Bind<ParamTypes..., TTypedMeshVertexAccessor<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::HighPrecision>>(Interface, BindingInfo, InstanceData, OutFunc);
			}
			else
			{
				NextBinder::template Bind<ParamTypes..., TTypedMeshVertexAccessor<EStaticMeshVertexTangentBasisType::HighPrecision, EStaticMeshVertexUVType::Default>>(Interface, BindingInfo, InstanceData, OutFunc);
			}
		}
		else
		{
			if (Res.VertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs())
			{
				NextBinder::template Bind<ParamTypes..., TTypedMeshVertexAccessor<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::HighPrecision>>(Interface, BindingInfo, InstanceData, OutFunc);
			}
			else
			{
				NextBinder::template Bind<ParamTypes..., TTypedMeshVertexAccessor<EStaticMeshVertexTangentBasisType::Default, EStaticMeshVertexUVType::Default>>(Interface, BindingInfo, InstanceData, OutFunc);
			}
		}
	}
};

//Final binders for all static mesh interface functions.
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomSection);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomTriCoord);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomTriCoordVertexColorFiltered);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomTriCoordOnSection);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordPosition);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordNormal);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordTangents);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordColor);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordUV);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordPositionAndVelocity);

DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetVertexPosition);

void UNiagaraDataInterfaceStaticMesh::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	FNDIStaticMesh_InstanceData* InstData = (FNDIStaticMesh_InstanceData*)InstanceData;
	check(InstData && InstData->Mesh && InstData->Component.IsValid());

	bool bNeedsVertexPositions = false;
	bool bNeedsVertexColors = false;
	bool bNeedsVertMain = true;//Assuming we always need this?
	
	if (BindingInfo.Name == StaticMeshHelpers::RandomSectionName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
		TUsesAreaWeightingBinder<NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomSection)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::RandomTriCoordName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4);
		TUsesAreaWeightingBinder<NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomTriCoord)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	//TODO: Vertex color filtering needs more work.
	else if (BindingInfo.Name == StaticMeshHelpers::RandomTriCoordVCFilteredName)
	{
		InstData->InitVertexColorFiltering();
		check(BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 4);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomTriCoordVertexColorFiltered)::Bind(this, OutFunc);
	}	
	else if (BindingInfo.Name == StaticMeshHelpers::RandomTriCoordOnSectionName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4);
		TUsesAreaWeightingBinder<NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, RandomTriCoordOnSection)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriPositionName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3);
		bNeedsVertexPositions = true;
		TNDIExplicitBinder<FNDITransformHandlerNoop, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriPositionWSName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3);
		bNeedsVertexPositions = true;
		TNDIExplicitBinder<FNDITransformHandler, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriNormalName)
	{
		check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 3);
		bNeedsVertMain = true;
		TNDIExplicitBinder<FNDITransformHandlerNoop, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriNormalWSName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 3);
		bNeedsVertMain = true;
		TNDIExplicitBinder<FNDITransformHandler, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriTangentsName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 9);
		bNeedsVertMain = true;
		TTypedMeshAccessorBinder<TNDIExplicitBinder<FNDITransformHandlerNoop, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordTangents)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriTangentsWSName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 9);
		bNeedsVertMain = true;
		TTypedMeshAccessorBinder<TNDIExplicitBinder<FNDITransformHandler, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordTangents)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriColorName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 4);
		bNeedsVertexColors = true;
		NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordColor)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriUVName)
	{
		check(BindingInfo.GetNumInputs() == 6 && BindingInfo.GetNumOutputs() == 2);
		bNeedsVertMain = true;
		TTypedMeshAccessorBinder<NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordUV)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetTriPositionAndVelocityName)
	{
		check(BindingInfo.GetNumInputs() == 5 && BindingInfo.GetNumOutputs() == 6);
		bNeedsVertMain = true;
		bNeedsVertexPositions = true;  
		NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetTriCoordPositionAndVelocity)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetMeshLocalToWorldName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 16);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceStaticMesh::GetLocalToWorld);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetMeshLocalToWorldInverseTransposedName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 16);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceStaticMesh::GetLocalToWorldInverseTransposed);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetMeshWorldVelocityName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 3);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceStaticMesh::GetWorldVelocity);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetVertexPositionName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3);
		bNeedsVertexPositions = true;
		TNDIExplicitBinder<FNDITransformHandlerNoop, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetVertexPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == StaticMeshHelpers::GetVertexPositionWSName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3);
		bNeedsVertexPositions = true;
		TNDIExplicitBinder<FNDITransformHandler, NDI_FUNC_BINDER(UNiagaraDataInterfaceStaticMesh, GetVertexPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}

	if (bNeedsVertexPositions && !InstData->MeshHasPositions())
	{
		UE_LOG(LogNiagara, Log, TEXT("Static Mesh data interface is cannot run as it's reading position data on a mesh that does not provide it. - Mesh:%s  "), *InstData->Mesh->GetFullName());
	}
	if (bNeedsVertexColors && !InstData->MeshHasColors())
	{
		UE_LOG(LogNiagara, Log, TEXT("Static Mesh data interface is cannot run as it's reading color data on a mesh that does not provide it. - Mesh:%s  "), *InstData->Mesh->GetFullName());
	}
	if (bNeedsVertMain && !InstData->MeshHasVerts())
	{
		UE_LOG(LogNiagara, Log, TEXT("Static Mesh data interface is cannot run as it's reading vertex data on a mesh with no vertex data. - Mesh:%s  "), *InstData->Mesh->GetFullName());
	}
}

bool UNiagaraDataInterfaceStaticMesh::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceStaticMesh* OtherTyped = CastChecked<UNiagaraDataInterfaceStaticMesh>(Destination);
	OtherTyped->Source = Source;
	OtherTyped->DefaultMesh = DefaultMesh;
	//OtherTyped->bEnableVertexColorRangeSorting = bEnableVertexColorRangeSorting;//TODO: Vertex color filtering needs more work.
	OtherTyped->SectionFilter = SectionFilter;
	return true;
}

bool UNiagaraDataInterfaceStaticMesh::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceStaticMesh* OtherTyped = CastChecked<const UNiagaraDataInterfaceStaticMesh>(Other);
	return OtherTyped->Source == Source &&
		OtherTyped->DefaultMesh == DefaultMesh &&
		//OtherTyped->bEnableVertexColorRangeSorting == bEnableVertexColorRangeSorting &&//TODO: Vertex color filtering needs more work.
		OtherTyped->SectionFilter.AllowedMaterialSlots == SectionFilter.AllowedMaterialSlots;
}

bool UNiagaraDataInterfaceStaticMesh::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIStaticMesh_InstanceData* Inst = new (PerInstanceData) FNDIStaticMesh_InstanceData();
	return Inst->Init(this, SystemInstance);
}

void UNiagaraDataInterfaceStaticMesh::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIStaticMesh_InstanceData* Inst = (FNDIStaticMesh_InstanceData*)PerInstanceData;

#if WITH_EDITOR
	if (Inst->Mesh)
	{
		Inst->Mesh->GetOnMeshChanged().RemoveAll(SystemInstance->GetComponent());
	}
#endif

	Inst->~FNDIStaticMesh_InstanceData();
}

bool UNiagaraDataInterfaceStaticMesh::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	FNDIStaticMesh_InstanceData* Inst = (FNDIStaticMesh_InstanceData*)PerInstanceData;
	return Inst->Tick(this, SystemInstance, InDeltaSeconds);
}

#if WITH_EDITOR	
TArray<FNiagaraDataInterfaceError> UNiagaraDataInterfaceStaticMesh::GetErrors()
{
	TArray<FNiagaraDataInterfaceError> Errors;
	if (Source == nullptr && DefaultMesh != nullptr && !DefaultMesh->bAllowCPUAccess)
	{
		FNiagaraDataInterfaceError CPUAccessNotAllowedError(FText::Format(LOCTEXT("CPUAccessNotAllowedError", "This mesh needs CPU access in order to be used properly.({0})"), FText::FromString(DefaultMesh->GetName())),
			LOCTEXT("CPUAccessNotAllowedErrorSummary", "CPU access error"),
			FNiagaraDataInterfaceFix::CreateLambda([=]()
		{
			DefaultMesh->Modify();
			DefaultMesh->bAllowCPUAccess = true;
			return true;
		}));

		Errors.Add(CPUAccessNotAllowedError);
	}
	return Errors;
}
#endif

//RandomSection specializations.
//Each combination for AreaWeighted and Section filtered.
template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomSection<TIntegralConstant<bool, true>, true>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	checkSlow(InstData->GetValidSections().Num() > 0);
	int32 Idx = InstData->GetAreaWeigtedSampler().GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	return InstData->GetValidSections()[Idx];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomSection<TIntegralConstant<bool, true>, false>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	return Res.AreaWeightedSampler.GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomSection<TIntegralConstant<bool, false>, true>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	checkSlow(InstData->GetValidSections().Num() > 0);
	int32 Idx = RandStream.RandRange(0, InstData->GetValidSections().Num() - 1);
	return InstData->GetValidSections()[Idx];
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomSection<TIntegralConstant<bool, false>, false>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	return RandStream.RandRange(0, Res.Sections.Num() - 1);
}

template<typename TAreaWeighted>
void UNiagaraDataInterfaceStaticMesh::RandomSection(FVectorVMContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutSection(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutSection.GetDest() = RandomSection<TAreaWeighted, true>(Context.RandStream, Res, InstData);
		OutSection.Advance();
	}
}

//RandomTriIndex specializations.
//Each combination for AreaWeighted and Section filtered.
template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomTriIndex<TIntegralConstant<bool, true>, true>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	int32 SecIdx = RandomSection<TIntegralConstant<bool, true>, true>(RandStream, Res, InstData);
	FStaticMeshSection&  Sec = Res.Sections[SecIdx];
	int32 Tri = Res.AreaWeightedSectionSamplers[SecIdx].GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	return Sec.FirstIndex + Tri * 3;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomTriIndex<TIntegralConstant<bool, true>, false>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	int32 SecIdx = RandomSection<TIntegralConstant<bool, true>, false>(RandStream, Res, InstData);
	FStaticMeshSection&  Sec = Res.Sections[SecIdx];
	int32 Tri = Res.AreaWeightedSectionSamplers[SecIdx].GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
	return Sec.FirstIndex + Tri * 3;
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomTriIndex<TIntegralConstant<bool, false>, true>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	int32 SecIdx = RandomSection<TIntegralConstant<bool, false>, true>(RandStream, Res, InstData);
	FStaticMeshSection&  Sec = Res.Sections[SecIdx];
	int32 Tri = RandStream.RandRange(0, Sec.NumTriangles - 1);
	return Sec.FirstIndex + Tri * 3;	
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomTriIndex<TIntegralConstant<bool, false>, false>(FRandomStream& RandStream, FStaticMeshLODResources& Res, FNDIStaticMesh_InstanceData* InstData)
{
	int32 SecIdx = RandomSection<TIntegralConstant<bool, false>, false>(RandStream, Res, InstData);
	FStaticMeshSection&  Sec = Res.Sections[SecIdx];
	int32 Tri = RandStream.RandRange(0, Sec.NumTriangles - 1);
	return Sec.FirstIndex + Tri * 3;
}

template<typename TAreaWeighted>
void UNiagaraDataInterfaceStaticMesh::RandomTriCoord(FVectorVMContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutTri(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryZ(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	FIndexArrayView Indices = Res.IndexBuffer.GetArrayView();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutTri.GetDest() = RandomTriIndex<TAreaWeighted, true>(Context.RandStream, Res, InstData);
		FVector Bary = RandomBarycentricCoord(Context.RandStream);
		*OutBaryX.GetDest() = Bary.X;
		*OutBaryY.GetDest() = Bary.Y;
		*OutBaryZ.GetDest() = Bary.Z;
		
		OutTri.Advance();
		OutBaryX.Advance();
		OutBaryY.Advance();
		OutBaryZ.Advance();
	}
}

void UNiagaraDataInterfaceStaticMesh::RandomTriCoordVertexColorFiltered(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<int32> MinValue(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> RangeValue(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutTri(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryZ(Context);
	
	FDynamicVertexColorFilterData* VCFData = InstData->DynamicVertexColorSampler.Get();

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	FIndexArrayView Indices = Res.IndexBuffer.GetArrayView();

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		uint32 StartIdx = (uint32)(MinValue.Get()*255.0f);
		uint32 Range = (uint32)(RangeValue.Get()*255.0f + 0.5f);
		uint32 EndIdx = StartIdx + Range;
		// Iterate over the bucketed range and find the total number of triangles in the list.
		uint32 NumTris = 0;

		// Unfortunately, there's always the chance that the user gave us a range and value that don't have any vertex color matches.
		// In this case (hopefully rare), we keep expanding the search space until we find a valid value.
		while (NumTris == 0)
		{
			StartIdx = FMath::Clamp<uint32>(StartIdx, 0, (uint32)VCFData->VertexColorToTriangleStart.Num() - 1);
			EndIdx = FMath::Clamp<uint32>(EndIdx, StartIdx, (uint32)VCFData->VertexColorToTriangleStart.Num() - 1);
			NumTris = (EndIdx < (uint32)VCFData->VertexColorToTriangleStart.Num() - 1) ? (VCFData->VertexColorToTriangleStart[EndIdx + 1] - VCFData->VertexColorToTriangleStart[StartIdx]) :
				(uint32)VCFData->TrianglesSortedByVertexColor.Num() - VCFData->VertexColorToTriangleStart[StartIdx];

			if (NumTris == 0)
			{
				if (StartIdx > 0)
				{
					StartIdx -= 1;
				}
				Range += 1;
				EndIdx = StartIdx + Range;
			}
		}

		// Select a random triangle from the list.
		uint32 RandomTri = Context.RandStream.GetFraction()*NumTris;

		// Now emit that triangle...
		*OutTri.GetDest() = VCFData->TrianglesSortedByVertexColor[VCFData->VertexColorToTriangleStart[StartIdx] + RandomTri];

		FVector Bary = RandomBarycentricCoord(Context.RandStream);
		*OutBaryX.GetDest() = Bary.X;
		*OutBaryY.GetDest() = Bary.Y;
		*OutBaryZ.GetDest() = Bary.Z;

		MinValue.Advance();
		RangeValue.Advance();
		OutTri.Advance();
		OutBaryX.Advance();
		OutBaryY.Advance();
		OutBaryZ.Advance();
	}
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomTriIndexOnSection<TIntegralConstant<bool, true>>(FRandomStream& RandStream, FStaticMeshLODResources& Res, int32 SecIdx, FNDIStaticMesh_InstanceData* InstData)
{
	return Res.AreaWeightedSectionSamplers[SecIdx].GetEntryIndex(RandStream.GetFraction(), RandStream.GetFraction());
}

template<>
FORCEINLINE int32 UNiagaraDataInterfaceStaticMesh::RandomTriIndexOnSection<TIntegralConstant<bool, false>>(FRandomStream& RandStream, FStaticMeshLODResources& Res, int32 SecIdx, FNDIStaticMesh_InstanceData* InstData)
{
	FStaticMeshSection&  Sec = Res.Sections[SecIdx];
	int32 Tri = RandStream.RandRange(0, Sec.NumTriangles - 1);
	return Sec.FirstIndex + Tri * 3;
}

template<typename TAreaWeighted>
void UNiagaraDataInterfaceStaticMesh::RandomTriCoordOnSection(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> SectionIdxParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutTri(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBaryZ(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	FIndexArrayView Indices = Res.IndexBuffer.GetArrayView();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 SecIdx = SectionIdxParam.Get();
		*OutTri.GetDest() = RandomTriIndexOnSection<TAreaWeighted>(Context.RandStream, Res, SecIdx, InstData);
		FVector Bary = RandomBarycentricCoord(Context.RandStream);
		*OutBaryX.GetDest() = Bary.X;
		*OutBaryY.GetDest() = Bary.Y;
		*OutBaryZ.GetDest() = Bary.Z;

		SectionIdxParam.Advance();
		OutTri.Advance();
		OutBaryX.Advance();
		OutBaryY.Advance();
		OutBaryZ.Advance();
	}
}

template<typename TransformHandlerType>
void UNiagaraDataInterfaceStaticMesh::GetTriCoordPosition(FVectorVMContext& Context)
{
	TransformHandlerType TransformHandler;
	VectorVM::FExternalFuncInputHandler<int32> TriParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryXParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryYParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryZParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FIndexArrayView& Indices = Res.IndexBuffer.GetArrayView();
	const FPositionVertexBuffer& Positions = Res.VertexBuffers.PositionVertexBuffer;

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		int32 Idx0 = Indices[Tri];
		int32 Idx1 = Indices[Tri + 1];
		int32 Idx2 = Indices[Tri + 2];

		FVector Pos = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Positions.VertexPosition(Idx0), Positions.VertexPosition(Idx1), Positions.VertexPosition(Idx2));
		TransformHandler.TransformPosition(Pos, InstData->Transform);

		*OutPosX.GetDest() = Pos.X;
		*OutPosY.GetDest() = Pos.Y;
		*OutPosZ.GetDest() = Pos.Z;

		TriParam.Advance();
		BaryXParam.Advance();
		BaryYParam.Advance();
		BaryZParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
	}
}

template<typename TransformHandlerType>
void UNiagaraDataInterfaceStaticMesh::GetTriCoordNormal(FVectorVMContext& Context)
{
	TransformHandlerType TransformHandler;

	VectorVM::FExternalFuncInputHandler<int32> TriParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryXParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryYParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryZParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutNormZ(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FIndexArrayView& Indices = Res.IndexBuffer.GetArrayView();
	const FStaticMeshVertexBuffer& Verts = Res.VertexBuffers.StaticMeshVertexBuffer;

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		int32 Idx0 = Indices[Tri];
		int32 Idx1 = Indices[Tri + 1];
		int32 Idx2 = Indices[Tri + 2];

		FVector Norm = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Verts.VertexTangentZ(Idx0), Verts.VertexTangentZ(Idx1), Verts.VertexTangentZ(Idx2));
		TransformHandler.TransformVector(Norm, InstData->TransformInverseTransposed);

		*OutNormX.GetDest() = Norm.X;
		*OutNormY.GetDest() = Norm.Y;
		*OutNormZ.GetDest() = Norm.Z;
		TriParam.Advance();
		BaryXParam.Advance();
		BaryYParam.Advance();
		BaryZParam.Advance();
		OutNormX.Advance();
		OutNormY.Advance();
		OutNormZ.Advance();
	}
}

template<typename VertexAccessorType, typename TransformHandlerType>
void UNiagaraDataInterfaceStaticMesh::GetTriCoordTangents(FVectorVMContext& Context)
{
	TransformHandlerType TransformHandler;	

	VectorVM::FExternalFuncInputHandler<int32> TriParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryXParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryYParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryZParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FIndexArrayView& Indices = Res.IndexBuffer.GetArrayView();
	const VertexAccessorType Verts(Res.VertexBuffers.StaticMeshVertexBuffer);

	VectorVM::FExternalFuncRegisterHandler<float> OutTangentX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutTangentY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutTangentZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBinormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBinormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutBinormZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutNormZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		int32 Idx0 = Indices[Tri];
		int32 Idx1 = Indices[Tri + 1];
		int32 Idx2 = Indices[Tri + 2];
		FVector Tangent = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Verts.GetTangentX(Idx0), Verts.GetTangentX(Idx1), Verts.GetTangentX(Idx2));
		FVector Binorm = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Verts.GetTangentY(Idx0), Verts.GetTangentY(Idx1), Verts.GetTangentY(Idx2));
		FVector Norm = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Verts.GetTangentZ(Idx0), Verts.GetTangentZ(Idx1), Verts.GetTangentZ(Idx2));
		TransformHandler.TransformVector(Tangent, InstData->TransformInverseTransposed);
		TransformHandler.TransformVector(Binorm, InstData->TransformInverseTransposed);
		TransformHandler.TransformVector(Norm, InstData->TransformInverseTransposed);
		*OutTangentX.GetDest() = Tangent.X;
		*OutTangentY.GetDest() = Tangent.Y;
		*OutTangentZ.GetDest() = Tangent.Z;
		*OutBinormX.GetDest() = Binorm.X;
		*OutBinormY.GetDest() = Binorm.Y;
		*OutBinormZ.GetDest() = Binorm.Z;
		*OutNormX.GetDest() = Norm.X;
		*OutNormY.GetDest() = Norm.Y;
		*OutNormZ.GetDest() = Norm.Z;

		TriParam.Advance();
		BaryXParam.Advance();
		BaryYParam.Advance();
		BaryZParam.Advance();
		OutTangentX.Advance();
		OutTangentY.Advance();
		OutTangentZ.Advance();
		OutBinormX.Advance();
		OutBinormY.Advance();
		OutBinormZ.Advance();
		OutNormX.Advance();
		OutNormY.Advance();
		OutNormZ.Advance();
	}
}

void UNiagaraDataInterfaceStaticMesh::GetTriCoordColor(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> TriParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryXParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryYParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryZParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutColorR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutColorG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutColorB(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutColorA(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FIndexArrayView& Indices = Res.IndexBuffer.GetArrayView();
	const FColorVertexBuffer& Colors = Res.VertexBuffers.ColorVertexBuffer;

	if (Colors.GetNumVertices() > 0)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 Tri = TriParam.Get();
			int32 Idx0 = Indices[Tri];
			int32 Idx1 = Indices[Tri + 1];
			int32 Idx2 = Indices[Tri + 2];

			FLinearColor Color = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(),
				Colors.VertexColor(Idx0).ReinterpretAsLinear(), Colors.VertexColor(Idx1).ReinterpretAsLinear(), Colors.VertexColor(Idx2).ReinterpretAsLinear());

			*OutColorR.GetDest() = Color.R;
			*OutColorG.GetDest() = Color.G;
			*OutColorB.GetDest() = Color.B;
			*OutColorA.GetDest() = Color.A;
			TriParam.Advance();
			BaryXParam.Advance();
			BaryYParam.Advance();
			BaryZParam.Advance();
			OutColorR.Advance();
			OutColorG.Advance();
			OutColorB.Advance();
			OutColorA.Advance();
		}
	}
	else
	{
		// This mesh doesn't have color information so set the color to white.
		FLinearColor Color = FLinearColor::White;
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			*OutColorR.GetDest() = Color.R;
			*OutColorG.GetDest() = Color.G;
			*OutColorB.GetDest() = Color.B;
			*OutColorA.GetDest() = Color.A;
			TriParam.Advance();
			BaryXParam.Advance();
			BaryYParam.Advance();
			BaryZParam.Advance();
			OutColorR.Advance();
			OutColorG.Advance();
			OutColorB.Advance();
			OutColorA.Advance();
		}
	}
}

template<typename VertexAccessorType>
void UNiagaraDataInterfaceStaticMesh::GetTriCoordUV(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> TriParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryXParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryYParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryZParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> UVSetParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutU(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutV(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FIndexArrayView& Indices = Res.IndexBuffer.GetArrayView();
	const VertexAccessorType Verts(Res.VertexBuffers.StaticMeshVertexBuffer);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		int32 Idx0 = Indices[Tri];
		int32 Idx1 = Indices[Tri + 1];
		int32 Idx2 = Indices[Tri + 2];

		int32 UVSet = UVSetParam.Get();
		FVector2D UV = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(),	Verts.GetUV(Idx0, UVSet), Verts.GetUV(Idx1, UVSet),	Verts.GetUV(Idx2, UVSet));

		*OutU.GetDest() = UV.X;
		*OutV.GetDest() = UV.Y;

		TriParam.Advance();
		BaryXParam.Advance();
		BaryYParam.Advance();
		BaryZParam.Advance();
		UVSetParam.Advance();
		OutU.Advance();
		OutV.Advance();
	}
}

void UNiagaraDataInterfaceStaticMesh::GetTriCoordPositionAndVelocity(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<int32> TriParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryXParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryYParam(Context);
	VectorVM::FExternalFuncInputHandler<float> BaryZParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelZ(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FIndexArrayView& Indices = Res.IndexBuffer.GetArrayView();
	const FPositionVertexBuffer& Positions = Res.VertexBuffers.PositionVertexBuffer;

	float InvDt = 1.0f / InstData->DeltaSeconds;
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Tri = TriParam.Get();
		int32 Idx0 = Indices[Tri];
		int32 Idx1 = Indices[Tri + 1];
		int32 Idx2 = Indices[Tri + 2];

		FVector Pos = BarycentricInterpolate(BaryXParam.Get(), BaryYParam.Get(), BaryZParam.Get(), Positions.VertexPosition(Idx0), Positions.VertexPosition(Idx1), Positions.VertexPosition(Idx2));

		FVector PrevWSPos = InstData->PrevTransform.TransformPosition(Pos);
		FVector WSPos = InstData->Transform.TransformPosition(Pos);

		FVector Vel = (WSPos - PrevWSPos) * InvDt;
		*OutPosX.GetDest() = WSPos.X;
		*OutPosY.GetDest() = WSPos.Y;
		*OutPosZ.GetDest() = WSPos.Z;
		*OutVelX.GetDest() = Vel.X;
		*OutVelY.GetDest() = Vel.Y;
		*OutVelZ.GetDest() = Vel.Z;
		TriParam.Advance();
		BaryXParam.Advance();
		BaryYParam.Advance();
		BaryZParam.Advance();
		OutPosX.Advance();
		OutPosY.Advance();
		OutPosZ.Advance();
		OutVelX.Advance();
		OutVelY.Advance();
		OutVelZ.Advance();
	}
}

void UNiagaraDataInterfaceStaticMesh::WriteTransform(const FMatrix& ToWrite, FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<float> Out00(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out01(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out02(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out03(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out04(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out05(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out06(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out07(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out08(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out09(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out10(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out11(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out12(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out13(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out14(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Out15(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*Out00.GetDest() = ToWrite.M[0][0]; Out00.Advance();
		*Out01.GetDest() = ToWrite.M[0][0]; Out01.Advance();
		*Out02.GetDest() = ToWrite.M[0][0]; Out02.Advance();
		*Out03.GetDest() = ToWrite.M[0][0]; Out03.Advance();
		*Out04.GetDest() = ToWrite.M[0][0]; Out04.Advance();
		*Out05.GetDest() = ToWrite.M[0][0]; Out05.Advance();
		*Out06.GetDest() = ToWrite.M[0][0]; Out06.Advance();
		*Out07.GetDest() = ToWrite.M[0][0]; Out07.Advance();
		*Out08.GetDest() = ToWrite.M[0][0]; Out08.Advance();
		*Out09.GetDest() = ToWrite.M[0][0]; Out09.Advance();
		*Out10.GetDest() = ToWrite.M[0][0]; Out10.Advance();
		*Out11.GetDest() = ToWrite.M[0][0]; Out11.Advance();
		*Out12.GetDest() = ToWrite.M[0][0]; Out12.Advance();
		*Out13.GetDest() = ToWrite.M[0][0]; Out13.Advance();
		*Out14.GetDest() = ToWrite.M[0][0]; Out14.Advance();
		*Out15.GetDest() = ToWrite.M[0][0]; Out15.Advance();
	}
}

void UNiagaraDataInterfaceStaticMesh::GetLocalToWorld(FVectorVMContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);
	WriteTransform(InstData->Transform, Context);
}

void UNiagaraDataInterfaceStaticMesh::GetLocalToWorldInverseTransposed(FVectorVMContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);
	WriteTransform(InstData->TransformInverseTransposed, Context);
}

void UNiagaraDataInterfaceStaticMesh::GetWorldVelocity(FVectorVMContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutVelX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutVelZ(Context);

	FVector Velocity(0.0f, 0.0f, 0.0f);
	float InvDeltaTime = 1.0f / InstData->DeltaSeconds;
	if (InstData->DeltaSeconds > 0.0f)
	{
		Velocity = (FVector(InstData->Transform.M[3][0], InstData->Transform.M[3][1], InstData->Transform.M[3][2]) - 
			FVector(InstData->PrevTransform.M[3][0], InstData->PrevTransform.M[3][1], InstData->PrevTransform.M[3][2])) * InvDeltaTime;
	}

	for (int32 i = 0; i < Context.NumInstances; i++)
	{
		*OutVelX.GetDest() = Velocity.X;
		*OutVelY.GetDest() = Velocity.Y;
		*OutVelZ.GetDest() = Velocity.Z;
		OutVelX.Advance();
		OutVelY.Advance();
		OutVelZ.Advance();
	}
}

template<typename TransformHandlerType>
void UNiagaraDataInterfaceStaticMesh::GetVertexPosition(FVectorVMContext& Context)
{
	TransformHandlerType TransformHandler;
	VectorVM::FExternalFuncInputHandler<int32> VertexIndexParam(Context);
	VectorVM::FUserPtrHandler<FNDIStaticMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutPosZ(Context);

	FStaticMeshLODResources& Res = InstData->Mesh->RenderData->LODResources[0];
	const FPositionVertexBuffer& Positions = Res.VertexBuffers.PositionVertexBuffer;

	const int32 NumVerts = Positions.GetNumVertices();
	FVector Pos;
	for (int32 i = 0; i < Context.NumInstances; i++)
	{
		int32 VertexIndex = VertexIndexParam.Get() % NumVerts;
		Pos = Positions.VertexPosition(VertexIndex);		
		TransformHandler.TransformPosition(Pos, InstData->Transform);
		VertexIndexParam.Advance();
		*OutPosX.GetDestAndAdvance() = Pos.X;
		*OutPosY.GetDestAndAdvance() = Pos.Y;
		*OutPosZ.GetDestAndAdvance() = Pos.Z;
	}
}

//////////////////////////////////////////////////////////////////////////

bool FDynamicVertexColorFilterData::Init(FNDIStaticMesh_InstanceData* Owner)
{
	TrianglesSortedByVertexColor.Empty();
	VertexColorToTriangleStart.AddDefaulted(256);
	check(Owner->Mesh);

	FStaticMeshLODResources& Res = Owner->Mesh->RenderData->LODResources[0];

	if (Res.VertexBuffers.ColorVertexBuffer.GetNumVertices() == 0)
	{
		UE_LOG(LogNiagara, Log, TEXT("Cannot initialize vertex color filter data for a mesh with no color data - %s"), *Owner->Mesh->GetFullName());
		return false;
	}

	// Go over all triangles for each possible vertex color and add it to that bucket
	for (int32 i = 0; i < VertexColorToTriangleStart.Num(); i++)
	{
		uint32 MinVertexColorRed = i;
		uint32 MaxVertexColorRed = i + 1;
		VertexColorToTriangleStart[i] = TrianglesSortedByVertexColor.Num();

		FIndexArrayView IndexView = Res.IndexBuffer.GetArrayView();
		for (int32 j = 0; j < Owner->GetValidSections().Num(); j++)
		{
			int32 SectionIdx = Owner->GetValidSections()[j];
			int32 TriStartIdx = Res.Sections[SectionIdx].FirstIndex;
			for (uint32 TriIdx = 0; TriIdx < Res.Sections[SectionIdx].NumTriangles; TriIdx++)
			{
				uint32 V0Idx = IndexView[TriStartIdx + TriIdx * 3 + 0];
				uint32 V1Idx = IndexView[TriStartIdx + TriIdx * 3 + 1];
				uint32 V2Idx = IndexView[TriStartIdx + TriIdx * 3 + 2];

				uint8 MaxR = FMath::Max<uint8>(Res.VertexBuffers.ColorVertexBuffer.VertexColor(V0Idx).R,
					FMath::Max<uint8>(Res.VertexBuffers.ColorVertexBuffer.VertexColor(V1Idx).R,
						Res.VertexBuffers.ColorVertexBuffer.VertexColor(V2Idx).R));
				if (MaxR >= MinVertexColorRed && MaxR < MaxVertexColorRed)
				{
					TrianglesSortedByVertexColor.Add(TriStartIdx + TriIdx * 3);
				}
			}
		}
	}
	return true;
}

TMap<uint32, TSharedPtr<FDynamicVertexColorFilterData>> FNDI_StaticMesh_GeneratedData::DynamicVertexColorFilters;
FCriticalSection FNDI_StaticMesh_GeneratedData::CriticalSection;

TSharedPtr<FDynamicVertexColorFilterData> FNDI_StaticMesh_GeneratedData::GetDynamicColorFilterData(FNDIStaticMesh_InstanceData* Instance)
{
	FScopeLock Lock(&CriticalSection);

	check(Instance);
	check(Instance->Mesh);

	TSharedPtr<FDynamicVertexColorFilterData> Ret = nullptr;

	uint32 FilterDataHash = GetTypeHash(Instance->Mesh);
	for (int32 ValidSec : Instance->GetValidSections())
	{
		FilterDataHash = HashCombine(GetTypeHash(ValidSec), FilterDataHash);
	}

	if (TSharedPtr<FDynamicVertexColorFilterData>* Existing = DynamicVertexColorFilters.Find(FilterDataHash))
	{
		check(Existing->IsValid());//We shouldn't be able to have an invalid ptr here.
		Ret = *Existing;		
	}
	else
	{
		Ret = MakeShared<FDynamicVertexColorFilterData>();
		if (Ret->Init(Instance))
		{
			DynamicVertexColorFilters.Add(FilterDataHash) = Ret;
		}
		else
		{
			Ret = nullptr;
		}
	}

	return Ret;
}

void FNDI_StaticMesh_GeneratedData::CleanupDynamicColorFilterData()
{
	TArray<uint32, TInlineAllocator<64>> ToRemove;
	for (TPair<uint32, TSharedPtr<FDynamicVertexColorFilterData>>& Pair : DynamicVertexColorFilters)
	{
		TSharedPtr<FDynamicVertexColorFilterData>& Ptr = Pair.Value;
		if (Ptr.IsUnique())
		{
			//If we're the only ref left then destroy this data
			ToRemove.Add(Pair.Key);
		}
	}

	for (uint32 Key : ToRemove)
	{
		DynamicVertexColorFilters.Remove(Key);
	}
}

#undef LOCTEXT_NAMESPACE

