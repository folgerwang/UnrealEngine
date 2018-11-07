// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceSkeletalMesh.h"
#include "NiagaraComponent.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "SkeletalMeshTypes.h"
#include "NiagaraStats.h"
#include "Templates/AlignmentTemplates.h"
#include "NDISkeletalMeshCommon.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceSkeletalMesh_BoneSampling"

DECLARE_CYCLE_STAT(TEXT("Skel Mesh Skeleton Sampling"), STAT_NiagaraSkel_Bone_Sample, STATGROUP_Niagara);

//Final binders for all static mesh interface functions.
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSkinnedBoneData)
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, IsValidBone)
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSpecificBoneAt)

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSpecificSocketBoneAt)

static const FName RandomSpecificBoneName("RandomSpecificBone");
static const FName IsValidBoneName("IsValidBoneName");
static const FName GetSkinnedBoneDataName("GetSkinnedBoneData");
static const FName GetSkinnedBoneDataWSName("GetSkinnedBoneDataWS");
static const FName GetSpecificBoneCountName("GetSpecificBoneCount");
static const FName GetSpecificBoneAtName("GetSpecificBone");

static const FName RandomSpecificSocketBoneName("RandomSpecificSocketBone");
static const FName GetSpecificSocketCountName("GetSpecificSocketCount");
static const FName GetSpecificSocketBoneAtName("GetSpecificSocketBone");

void UNiagaraDataInterfaceSkeletalMesh::GetSkeletonSamplingFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	//////////////////////////////////////////////////////////////////////////
	// Bone functions.

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = RandomSpecificBoneName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef() , TEXT("Bone")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = IsValidBoneName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Bone")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsValid")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("IsValidBoneDesc", "Determine if this bone index is valid for this mesh's skeleton.");
#endif
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSkinnedBoneDataName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Bone")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetSkinnedBoneDataDesc", "Returns skinning dependant data for the pased bone in local space. All outputs are optional and you will incur zero to minimal cost if they are not connected.");
#endif
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSkinnedBoneDataWSName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Bone")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetSkinnedBoneDataWSDesc", "Returns skinning dependant data for the pased bone in world space. All outputs are optional and you will incur zero to minimal cost if they are not connected.");
#endif
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSpecificBoneCountName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetSpecificBoneCountDesc", "Returns the number of specific bones in the DI list.");
#endif
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSpecificBoneAtName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Bone Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Bone")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetSpecificBoneAtDesc", "Gets the bone at the passed index in the DI's specfic bones list.");
#endif
		OutFunctions.Add(Sig);
	}

	//////////////////////////////////////////////////////////////////////////
	//Socket functions

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = RandomSpecificSocketBoneName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Socket Bone")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("RandomSpecificSocketBoneDesc", "Gets the bone for a random socket in the DI's specific socket list.");
#endif
		OutFunctions.Add(Sig);
	}
	
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSpecificSocketCountName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetSpecificSocketCountDesc", "Returns the number of specific Sockets in the DI list.");
#endif
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetSpecificSocketBoneAtName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("SkeletalMesh")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Socket Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Socket Bone")));
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("GetSpecificSocketBoneAtDesc", "Gets the bone for the socket at the passed index in the DI's specfic socket list.");
#endif
		OutFunctions.Add(Sig);
	}
}

void UNiagaraDataInterfaceSkeletalMesh::BindSkeletonSamplingFunction(const FVMExternalFunctionBindingInfo& BindingInfo, FNDISkeletalMesh_InstanceData* InstanceData, FVMExternalFunction &OutFunc)
{
	//Bone Functions
	if (BindingInfo.Name == RandomSpecificBoneName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
		auto Lambda = [this](FVectorVMContext& Context) { this->RandomSpecificBone(Context); }; 
		OutFunc = FVMExternalFunction::CreateLambda(Lambda); 
	}
	else if (BindingInfo.Name == IsValidBoneName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, IsValidBone)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetSkinnedBoneDataName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 6);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandlerNoop, NDI_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSkinnedBoneData)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetSkinnedBoneDataWSName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 6);
		TSkinningModeBinder<TNDIExplicitBinder<FNDITransformHandler, NDI_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSkinnedBoneData)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetSpecificBoneCountName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
		auto Lambda = [this](FVectorVMContext& Context) { this->GetSpecificBoneCount(Context); };
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);
	}
	else if (BindingInfo.Name == GetSpecificBoneAtName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSpecificBoneAt)::Bind(this, OutFunc);
	}
	//Socket Functions
	else if (BindingInfo.Name == RandomSpecificSocketBoneName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
		auto Lambda = [this](FVectorVMContext& Context) { this->RandomSpecificSocketBone(Context); };
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);
	}
	else if (BindingInfo.Name == GetSpecificSocketCountName)
	{
		check(BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 1);
		auto Lambda = [this](FVectorVMContext& Context) { this->GetSpecificSocketCount(Context); };
		OutFunc = FVMExternalFunction::CreateLambda(Lambda);
	}
	else if (BindingInfo.Name == GetSpecificSocketBoneAtName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceSkeletalMesh, GetSpecificSocketBoneAt)::Bind(this, OutFunc);
	}
}


//////////////////////////////////////////////////////////////////////////
// Direct sampling from listed sockets and bones.

void UNiagaraDataInterfaceSkeletalMesh::GetSpecificBoneCount(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);
	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutCount(Context);

	int32 Num = SpecificBones.Num();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutCount.GetDestAndAdvance() = Num;
	}
}

void UNiagaraDataInterfaceSkeletalMesh::GetSpecificBoneAt(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);

	VectorVM::FExternalFuncInputHandler<int32> BoneParam(Context);
	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutBone(Context);
	const TArray<int32>& SpecificBonesArray = InstData->SpecificBones;

	int32 Max = SpecificBones.Num() - 1;
	if (Max != INDEX_NONE)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 BoneIndex = FMath::Clamp(BoneParam.GetAndAdvance(), 0, Max);
			*OutBone.GetDestAndAdvance() = SpecificBonesArray[BoneIndex];
		}
	}
	else
	{
		FMemory::Memset(OutBone.GetDest(), 0xFF, Context.NumInstances * sizeof(int32));
	}
}

void UNiagaraDataInterfaceSkeletalMesh::RandomSpecificBone(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);

	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutBone(Context);
	const TArray<int32>& SpecificBonesArray = InstData->SpecificBones;

	int32 Max = SpecificBones.Num() - 1;
	if (Max != INDEX_NONE)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 BoneIndex = Context.RandStream.RandRange(0, Max);
			*OutBone.GetDestAndAdvance() = SpecificBonesArray[BoneIndex];
		}
	}
	else
	{
		FMemory::Memset(OutBone.GetDest(), 0xFF, Context.NumInstances * sizeof(int32));
	}
}

void UNiagaraDataInterfaceSkeletalMesh::IsValidBone(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);

	VectorVM::FExternalFuncInputHandler<int32> BoneParam(Context);

	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);
	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	VectorVM::FExternalFuncRegisterHandler<FNiagaraBool> OutValid(Context);

	FSkeletalMeshAccessorHelper MeshAccessor;
	MeshAccessor.Init<TIntegralConstant<int32, 0>, TIntegralConstant<int32, 0>>(InstData);
	const FReferenceSkeleton& RefSkeleton = MeshAccessor.Mesh->RefSkeleton;
	int32 NumBones = RefSkeleton.GetNum();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 RequestedIndex = BoneParam.GetAndAdvance();

		FNiagaraBool Value;
		Value.SetValue(RequestedIndex >= 0 && RequestedIndex < NumBones);
		*OutValid.GetDestAndAdvance() = Value;
	}
}

struct FBoneSocketSkinnedDataOutputHandler
{
	FBoneSocketSkinnedDataOutputHandler(FVectorVMContext& Context)
		: PosX(Context), PosY(Context), PosZ(Context)
		, VelX(Context), VelY(Context), VelZ(Context)
		, bNeedsPosition(PosX.IsValid() || PosY.IsValid() || PosZ.IsValid())
		, bNeedsVelocity(VelX.IsValid() || VelY.IsValid() || VelZ.IsValid())
	{
	}

	VectorVM::FExternalFuncRegisterHandler<float> PosX; VectorVM::FExternalFuncRegisterHandler<float> PosY; VectorVM::FExternalFuncRegisterHandler<float> PosZ;
	VectorVM::FExternalFuncRegisterHandler<float> VelX; VectorVM::FExternalFuncRegisterHandler<float> VelY; VectorVM::FExternalFuncRegisterHandler<float> VelZ;

	//TODO: Rotation + Scale too? Use quats so we can get proper interpolation between bone and parent.

	const bool bNeedsPosition;
	const bool bNeedsVelocity;

	FORCEINLINE void SetPosition(FVector Position)
	{
		*PosX.GetDestAndAdvance() = Position.X;
		*PosY.GetDestAndAdvance() = Position.Y;
		*PosZ.GetDestAndAdvance() = Position.Z;
	}

	FORCEINLINE void SetVelocity(FVector Velocity)
	{
		*VelX.GetDestAndAdvance() = Velocity.X;
		*VelY.GetDestAndAdvance() = Velocity.Y;
		*VelZ.GetDestAndAdvance() = Velocity.Z;
	}
};

template<typename SkinningHandlerType, typename TransformHandlerType>
void UNiagaraDataInterfaceSkeletalMesh::GetSkinnedBoneData(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);
	SkinningHandlerType SkinningHandler;
	TransformHandlerType TransformHandler;
	VectorVM::FExternalFuncInputHandler<int32> BoneParam(Context);

	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	checkfSlow(InstData.Get(), TEXT("Skeletal Mesh Interface has invalid instance data. %s"), *GetPathName());
	checkfSlow(InstData->Mesh, TEXT("Skeletal Mesh Interface has invalid mesh. %s"), *GetPathName());

	//TODO: Replace this by storing off FTransforms and doing a proper lerp to get a final transform.
	//Also need to pull in a per particle interpolation factor.
	const FMatrix& Transform = InstData->Transform;
	const FMatrix& PrevTransform = InstData->PrevTransform;

	FBoneSocketSkinnedDataOutputHandler Output(Context);

	FSkinWeightVertexBuffer* SkinWeightBuffer;
	FSkeletalMeshLODRenderData& LODData = InstData->GetLODRenderDataAndSkinWeights(SkinWeightBuffer);

	FSkeletalMeshAccessorHelper Accessor;
	Accessor.Init<TIntegralConstant<int32, 0>, TIntegralConstant<int32, 0>>(InstData);

	const FReferenceSkeleton& RefSkel = Accessor.Mesh->RefSkeleton;

	int32 BoneMax = RefSkel.GetNum() - 1;
	float InvDt = 1.0f / InstData->DeltaSeconds;

	FVector BonePos;
	FVector BonePrev;

	FVector Pos;
	FVector Prev;
	FVector Velocity;

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		int32 Bone = FMath::Clamp(BoneParam.GetAndAdvance(), 0, BoneMax);

		//No parent bone, just spawn at bone.
		if (Output.bNeedsPosition || Output.bNeedsVelocity)
		{
			Pos = SkinningHandler.GetSkinnedBonePosition(Accessor, Bone);
			TransformHandler.TransformPosition(Pos, Transform);
			Output.SetPosition(Pos);
		}

		if (Output.bNeedsVelocity)
		{
			Prev = SkinningHandler.GetSkinnedBonePreviousPosition(Accessor, Bone);
			TransformHandler.TransformPosition(Prev, PrevTransform);
			Velocity = (Pos - Prev) * InvDt;
			Output.SetVelocity(Velocity);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// Sockets

void UNiagaraDataInterfaceSkeletalMesh::GetSpecificSocketCount(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);
	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutCount(Context);

	int32 Num = SpecificSockets.Num();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutCount.GetDestAndAdvance() = Num;
	}
}

void UNiagaraDataInterfaceSkeletalMesh::GetSpecificSocketBoneAt(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);

	VectorVM::FExternalFuncInputHandler<int32> SocketParam(Context);
	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutSocketBone(Context);
	const TArray<int32>& SpecificSocketsArray = InstData->SpecificSocketBones;

	int32 Max = SpecificSockets.Num() - 1;
	if (Max != INDEX_NONE)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 SocketIndex = FMath::Clamp(SocketParam.GetAndAdvance(), 0, Max);
			*OutSocketBone.GetDestAndAdvance() = SpecificSocketsArray[SocketIndex];
		}
	}
	else
	{
		FMemory::Memset(OutSocketBone.GetDest(), 0xFF, Context.NumInstances * sizeof(int32));
	}
}

void UNiagaraDataInterfaceSkeletalMesh::RandomSpecificSocketBone(FVectorVMContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraSkel_Bone_Sample);

	VectorVM::FUserPtrHandler<FNDISkeletalMesh_InstanceData> InstData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutSocketBone(Context);
	const TArray<int32>& SpecificSocketsArray = InstData->SpecificSocketBones;

	int32 Max = SpecificSockets.Num() - 1;
	if (Max != INDEX_NONE)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			int32 SocketIndex = Context.RandStream.RandRange(0, Max);
			*OutSocketBone.GetDestAndAdvance() = SpecificSocketsArray[SocketIndex];
		}
	}
	else
	{
		FMemory::Memset(OutSocketBone.GetDest(), 0xFF, Context.NumInstances * sizeof(int32));
	}
}

#undef LOCTEXT_NAMESPACE
