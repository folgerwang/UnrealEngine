// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCollisionQuery.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraWorldManager.h"
#include "ShaderParameterUtils.h"
#include "GlobalDistanceFieldParameters.h"
#include "Shader.h"

//////////////////////////////////////////////////////////////////////////
//Color Curve

FCriticalSection UNiagaraDataInterfaceCollisionQuery::CriticalSection;

UNiagaraDataInterfaceCollisionQuery::UNiagaraDataInterfaceCollisionQuery(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	TraceChannelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ECollisionChannel"), true);
}

bool UNiagaraDataInterfaceCollisionQuery::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance)
{
	CQDIPerInstanceData *PIData = new (PerInstanceData) CQDIPerInstanceData;
	PIData->SystemInstance = InSystemInstance;
	if (InSystemInstance)
	{
		PIData->CollisionBatch.Init(InSystemInstance->GetIDName(), InSystemInstance->GetComponent()->GetWorld());
	}
	return true;
}


void UNiagaraDataInterfaceCollisionQuery::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}
}

void UNiagaraDataInterfaceCollisionQuery::PostLoad()
{
	Super::PostLoad();

	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
}

void UNiagaraDataInterfaceCollisionQuery::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig3;
	Sig3.Name = TEXT("PerformCollisionQuery");
	Sig3.bMemberFunction = true;
	Sig3.bRequiresContext = false;
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	//Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("PerformQuery")));
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ReturnQueryID")));
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlePosition")));
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Direction")));
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("DeltaTime")));
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CollisionSize")));
	Sig3.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("DepthBounds")));
	Sig3.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("QueryID")));
	Sig3.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("CollisionValid")));
	Sig3.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionPos")));
	Sig3.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionNormal")));
	Sig3.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Friction")));
	Sig3.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Restitution")));
	OutFunctions.Add(Sig3);


	FNiagaraFunctionSignature Sig;
	Sig.Name = TEXT("SubmitQuery");
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlePosition")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticleVelocity")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("DeltaTime")));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("CollisionID")));
	OutFunctions.Add(Sig);

	FNiagaraFunctionSignature Sig2;
	Sig2.Name = TEXT("ReadQuery");
	Sig2.bMemberFunction = true;
	Sig2.bRequiresContext = false;
	Sig2.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	Sig2.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("CollisionID")));
	Sig2.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("CollisionValid")));
	Sig2.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionPos")));
	Sig2.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionVelocity")));
	Sig2.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionNormal")));
	OutFunctions.Add(Sig2);

	FNiagaraFunctionSignature SigGpu;
	SigGpu.Name = TEXT("PerformCollisionQueryGPUShader");
	SigGpu.bMemberFunction = true;
	SigGpu.bRequiresContext = false;
	SigGpu.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	SigGpu.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("DepthSamplePosWorld")));
	SigGpu.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("TraceEndWorld")));
	SigGpu.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SceneDepthBounds")));
	SigGpu.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ParticleRadius")));
	SigGpu.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("UseMeshDistanceField")));
	SigGpu.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("CollisionValid")));
	SigGpu.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionPosWorld")));
	SigGpu.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionNormal")));
	OutFunctions.Add(SigGpu);

	FNiagaraFunctionSignature SigDepth;
	SigDepth.Name = TEXT("QuerySceneDepthGPU");
	SigDepth.bMemberFunction = true;
	SigDepth.bRequiresContext = false;
	SigDepth.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	SigDepth.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("DepthSamplePosWorld")));
	SigDepth.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("SceneDepth")));
	SigDepth.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CameraPosWorld")));
	SigDepth.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsInsideView")));
	SigDepth.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SamplePosWorld")));
	SigDepth.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SampleWorldNormal")));
	OutFunctions.Add(SigDepth);

	FNiagaraFunctionSignature SigMeshField;
	SigMeshField.Name = TEXT("QueryMeshDistanceFieldGPU");
	SigMeshField.bMemberFunction = true;
	SigMeshField.bRequiresContext = false;
	SigMeshField.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	SigMeshField.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("FieldSamplePosWorld")));
	SigMeshField.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("DistanceToNearestSurface")));
	SigMeshField.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("FieldGradient")));
	OutFunctions.Add(SigMeshField);

	FNiagaraFunctionSignature SigCpuSync;
	SigCpuSync.Name = TEXT("PerformCollisionQuerySyncCPU");
	SigCpuSync.bMemberFunction = true;
	SigCpuSync.bRequiresContext = false;
	SigCpuSync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	SigCpuSync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("TraceStartWorld")));
	SigCpuSync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("TraceEndWorld")));
	SigCpuSync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(TraceChannelEnum), TEXT("TraceChannel")));
	SigCpuSync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("CollisionValid")));
	SigCpuSync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsTraceInsideMesh")));
	SigCpuSync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionPosWorld")));
	SigCpuSync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionNormal")));
	SigCpuSync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CollisionMaterialFriction")));
	SigCpuSync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CollisionMaterialRestitution")));
	OutFunctions.Add(SigCpuSync);

	FNiagaraFunctionSignature SigCpuAsync;
	SigCpuAsync.Name = TEXT("PerformCollisionQueryAsyncCPU");
	SigCpuAsync.bMemberFunction = true;
	SigCpuAsync.bRequiresContext = false;
	SigCpuAsync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("CollisionQuery")));
	SigCpuAsync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("PreviousFrameQueryID")));
	SigCpuAsync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("TraceStartWorld")));
	SigCpuAsync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("TraceEndWorld")));
	SigCpuAsync.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(TraceChannelEnum), TEXT("TraceChannel")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("NextFrameQueryID")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("CollisionValid")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("IsTraceInsideMesh")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionPosWorld")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("CollisionNormal")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CollisionMaterialFriction")));
	SigCpuAsync.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("CollisionMaterialRestitution")));
	OutFunctions.Add(SigCpuAsync);
}

// build the shader function HLSL; function name is passed in, as it's defined per-DI; that way, configuration could change
// the HLSL in the spirit of a static switch
// TODO: need a way to identify each specific function here

// 
bool UNiagaraDataInterfaceCollisionQuery::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	// a little tricky, since we've got two functions for submitting and retrieving a query; we store submitted queries per thread group,
	// assuming it'll usually be the same thread trying to call ReadQuery for a particular QueryID, that submitted it in the first place.
	if (DefinitionFunctionName == TEXT("PerformCollisionQuery"))
	{
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in int InQueryID, in float3 In_ParticlePos, in float3 In_ParticleVel, in float In_DeltaSeconds, float CollisionRadius, in float CollisionDepthBounds, \
			out int Out_QueryID, out bool OutCollisionValid, out float3 Out_CollisionPos, out float3 Out_CollisionNormal, out float Out_Friction, out float Out_Restitution) \n{\n");
		// get the screen position
		OutHLSL += TEXT("\
		OutCollisionValid = false;\n\
		Out_QueryID = InQueryID;\n\
		Out_CollisionPos = In_ParticlePos;\n\
		Out_CollisionNormal = float3(0.0, 0.0, 1.0);\n\
		Out_Friction = 0.0;\n\
		Out_Restitution = 1.0;\n\
		float3 DeltaPosition = In_DeltaSeconds * In_ParticleVel; \
		float3 CollisionOffset = normalize(DeltaPosition) * CollisionRadius;\
		float3 CollisionPosition = In_ParticlePos + CollisionOffset; \n\
		float3 NewPosition = In_ParticlePos.xyz + DeltaPosition; \
		float4 SamplePosition = float4(CollisionPosition + View.PreViewTranslation, 1); \n\
		float4 ClipPosition = mul(SamplePosition, View.TranslatedWorldToClip);\n\
		float2 ScreenPosition = ClipPosition.xy / ClipPosition.w;\n\
		// Don't try to collide if the particle falls outside the view.\n\
		if (all(abs(ScreenPosition.xy) <= float2(1, 1)))\n\
		{\n\
			// Sample the depth buffer to get a world position near the particle.\n\
			float2 ScreenUV = ScreenPosition * View.ScreenPositionScaleBias.xy + View.ScreenPositionScaleBias.wz;\n\
			float SceneDepth = CalcSceneDepth(ScreenUV);\n\
			if (abs(ClipPosition.w - SceneDepth) < CollisionDepthBounds)\n\
			{\n\
				// Reconstruct world position.\n\
				float3 WorldPosition = WorldPositionFromSceneDepth(ScreenPosition.xy, SceneDepth);\n\
				// Sample the normal buffer to create a plane to collide against.\n\
				float3 WorldNormal = Texture2DSampleLevel(SceneTexturesStruct.GBufferATexture, SceneTexturesStruct.GBufferATextureSampler, ScreenUV, 0).xyz * 2.0 - 1.0;\n\
				float4 CollisionPlane = float4(WorldNormal, dot(WorldPosition.xyz, WorldNormal));\n\
				// Compute the portion of velocity normal to the collision plane.\n\
				float VelocityDot = dot(CollisionPlane.xyz, DeltaPosition.xyz);\n\
				float d_back = (dot(CollisionPlane.xyz, In_ParticlePos.xyz) + CollisionRadius - CollisionPlane.w);\n\
				float d_front = (dot(CollisionPlane.xyz, NewPosition.xyz) - CollisionRadius - CollisionPlane.w);\n\
				// distance to the plane from current and predicted position\n\
				if (d_back >= 0.0f && d_front <= 0.0f && VelocityDot < 0.0f)\n\
				{\n\
					OutCollisionValid = true;\n\
					Out_CollisionPos = In_ParticlePos + (WorldNormal*d_back);\n\
					Out_CollisionNormal = WorldNormal;\n\
					Out_Friction = 0.0f;\n\
					Out_Restitution = 1.0f;\n\
					Out_QueryID = 0;\
				}\n\
				else\n\
				{\n\
					OutCollisionValid = false; \n\
				}\n\
			}\n\
		}\n}\n\n");
	}
	else if (DefinitionFunctionName == TEXT("PerformCollisionQueryGPUShader"))
	{
		FString SceneDepthFunction = InstanceFunctionName + TEXT("_SceneDepthCollision");
		FString DistanceFieldFunction = InstanceFunctionName + TEXT("_DistanceFieldCollision");

		OutHLSL += TEXT("void ") + SceneDepthFunction + TEXT("(in float3 In_SamplePos, in float3 In_TraceEndPos, in float CollisionDepthBounds, in float ParticleRadius, out bool OutCollisionValid, out float3 Out_CollisionPos, out float3 Out_CollisionNormal) \n{\n\
		OutCollisionValid = false;\n\
		Out_CollisionPos = In_SamplePos;\n\
		Out_CollisionNormal = float3(0.0, 0.0, 1.0);\n\
		float4 SamplePosition = float4(In_SamplePos + View.PreViewTranslation, 1); \n\
		float4 ClipPosition = mul(SamplePosition, View.TranslatedWorldToClip);\n\
		float2 ScreenPosition = ClipPosition.xy / ClipPosition.w;\n\
		// Don't try to collide if the particle falls outside the view.\n\
		if (all(abs(ScreenPosition.xy) <= float2(1, 1)))\n\
		{\n\
			// Sample the depth buffer to get a world position near the particle.\n\
			float2 ScreenUV = ScreenPosition * View.ScreenPositionScaleBias.xy + View.ScreenPositionScaleBias.wz;\n\
			float SceneDepth = CalcSceneDepth(ScreenUV);\n\
			if (abs(ClipPosition.w - SceneDepth) < CollisionDepthBounds)\n\
			{\n\
				// Reconstruct world position.\n\
				float3 WorldPosition = WorldPositionFromSceneDepth(ScreenPosition.xy, SceneDepth);\n\
				// Sample the normal buffer to create a plane to collide against.\n\
				float3 WorldNormal = Texture2DSampleLevel(SceneTexturesStruct.GBufferATexture, SceneTexturesStruct.GBufferATextureSampler, ScreenUV, 0).xyz * 2.0 - 1.0;\n\
				float4 CollisionPlane = float4(WorldNormal, dot(WorldPosition.xyz, WorldNormal));\n\
				// Compute the portion of velocity normal to the collision plane.\n\
				float VelocityDot = dot(CollisionPlane.xyz, (In_TraceEndPos - In_SamplePos).xyz);\n\
				float d_back = (dot(CollisionPlane.xyz, In_SamplePos.xyz) + ParticleRadius - CollisionPlane.w);\n\
				float d_front = (dot(CollisionPlane.xyz, In_TraceEndPos.xyz) - ParticleRadius - CollisionPlane.w);\n\
				// distance to the plane from current and predicted position\n\
				if (d_back >= 0.0f && d_front <= 0.0f && VelocityDot < 0.0f)\n\
				{\n\
					OutCollisionValid = true;\n\
					Out_CollisionPos = In_SamplePos + (WorldNormal*d_back);\n\
					Out_CollisionNormal = WorldNormal;\n\
				}\n\
			}\n\
		}\
		\n}\n\n");
		OutHLSL += TEXT("void ") + DistanceFieldFunction + TEXT("(in float3 InPosition, in float3 In_TraceEndPos, out bool OutCollisionValid, out float3 Out_CollisionPos, out float3 Out_CollisionNormal)\n{\n\
		float DistanceToNearestSurface = GetDistanceToNearestSurfaceGlobal(InPosition);\n\
		if (DistanceToNearestSurface < length(In_TraceEndPos - InPosition))\n\
		{\n\
			OutCollisionValid = true;\n\
			Out_CollisionNormal = normalize(GetDistanceFieldGradientGlobal(InPosition));\n\
			Out_CollisionPos = InPosition - Out_CollisionNormal * DistanceToNearestSurface;\n\
		}\n\
		else\n\
		{\n\
			OutCollisionValid = false;\n\
			Out_CollisionNormal = float3(0.0, 0.0, 1.0);\n\
			Out_CollisionPos = InPosition;\n\
		}\n}\n\n");
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_SamplePos, in float3 In_TraceEndPos, in float CollisionDepthBounds, ") +
			TEXT("in float ParticleRadius, in bool UseMeshDistanceField, out bool OutCollisionValid, out float3 Out_CollisionPos, out float3 Out_CollisionNormal) \n{\n");
		OutHLSL += TEXT("\
			if (UseMeshDistanceField)\n\
			{\n\
				") + DistanceFieldFunction + TEXT("(In_SamplePos, In_TraceEndPos, OutCollisionValid, Out_CollisionPos, Out_CollisionNormal);\n\
			}\n\
			else\n\
			{\n\
				") + SceneDepthFunction + TEXT("(In_SamplePos, In_TraceEndPos, CollisionDepthBounds, ParticleRadius, OutCollisionValid, Out_CollisionPos, Out_CollisionNormal);\n\
			}\n}\n\n");
	}
	else if (DefinitionFunctionName == TEXT("QuerySceneDepthGPU"))
	{
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_SamplePos, out float Out_SceneDepth, out float3 Out_CameraPosWorld, out bool Out_IsInsideView, out float3 Out_WorldPos, out float3 Out_WorldNormal) \n{\n");
		OutHLSL += TEXT("\
			Out_SceneDepth = -1;\n\
			Out_WorldPos = float3(0.0, 0.0, 0.0);\n\
			Out_WorldNormal = float3(0.0, 0.0, 1.0);\n\
			Out_IsInsideView = true;\n\
			Out_CameraPosWorld.xyz = View.WorldCameraOrigin.xyz;\n\
			float4 SamplePosition = float4(In_SamplePos + View.PreViewTranslation, 1);\n\
			float4 ClipPosition = mul(SamplePosition, View.TranslatedWorldToClip);\n\
			float2 ScreenPosition = ClipPosition.xy / ClipPosition.w;\n\
			// Check if the sample is inside the view.\n\
			if (all(abs(ScreenPosition.xy) <= float2(1, 1)))\n\
			{\n\
				// Sample the depth buffer to get a world position near the sample position.\n\
				float2 ScreenUV = ScreenPosition * View.ScreenPositionScaleBias.xy + View.ScreenPositionScaleBias.wz;\n\
				float SceneDepth = CalcSceneDepth(ScreenUV);\n\
				Out_SceneDepth = SceneDepth;\n\
				// Reconstruct world position.\n\
				Out_WorldPos = WorldPositionFromSceneDepth(ScreenPosition.xy, SceneDepth);\n\
				// Sample the normal buffer\n\
				Out_WorldNormal = Texture2DSampleLevel(SceneTexturesStruct.GBufferATexture, SceneTexturesStruct.GBufferATextureSampler, ScreenUV, 0).xyz * 2.0 - 1.0;\n\
			}\n\
			else\n\
			{\n\
				Out_IsInsideView = false;\n\
			}\n}\n\n");
	}
	else if (DefinitionFunctionName == TEXT("QueryMeshDistanceFieldGPU"))
	{
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_SamplePos, out float Out_DistanceToNearestSurface, out float3 Out_FieldGradient) \n{\n");
		OutHLSL += TEXT("\
			Out_DistanceToNearestSurface = GetDistanceToNearestSurfaceGlobal(In_SamplePos);\n\
			Out_FieldGradient = GetDistanceFieldGradientGlobal(In_SamplePos);\
			\n}\n\n");
	}

	return true;
}

void UNiagaraDataInterfaceCollisionQuery::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	// we don't need to add these to hlsl, as they're already in common.ush
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, SubmitQuery);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, ReadQuery);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQuery);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQuerySyncCPU);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQueryAsyncCPU);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQueryGPU);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, QuerySceneDepth);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, QueryMeshDistanceField);

void UNiagaraDataInterfaceCollisionQuery::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	CQDIPerInstanceData *InstData = (CQDIPerInstanceData *)InstanceData;
	if (BindingInfo.Name == TEXT("SubmitQuery") /*&& BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 1*/)
	{
		 NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, SubmitQuery)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("ReadQuery") /*&& BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4*/)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, ReadQuery)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("PerformCollisionQuery") /*&& BindingInfo.GetNumInputs() == 1 && BindingInfo.GetNumOutputs() == 4*/)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQuery)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("PerformCollisionQuerySyncCPU"))
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQuerySyncCPU)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("PerformCollisionQueryAsyncCPU"))
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQueryAsyncCPU)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("PerformCollisionQueryGPUShader"))
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, PerformQueryGPU)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("QuerySceneDepthGPU"))
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, QuerySceneDepth)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TEXT("QueryMeshDistanceFieldGPU"))
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceCollisionQuery, QueryMeshDistanceField)::Bind(this, OutFunc);
	}
	else
	{
		UE_LOG(LogNiagara, Error, TEXT("Could not find data interface external function. %s\n"),
			*BindingInfo.Name.ToString());
	}
}

void UNiagaraDataInterfaceCollisionQuery::PerformQuery(FVectorVMContext& Context)
{
	//PerformType PerformParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> InIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> PosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> PosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> PosParamZ(Context);

	VectorVM::FExternalFuncInputHandler<float> DirParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> DirParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> DirParamZ(Context);

	VectorVM::FExternalFuncInputHandler<float> DTParam(Context);
	VectorVM::FExternalFuncInputHandler<float> SizeParam(Context);
	VectorVM::FExternalFuncInputHandler<float> DepthBoundsParam(Context);

	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryID(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryValid(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Friction(Context);
	VectorVM::FExternalFuncRegisterHandler<float> Restitution(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		// submit a query, get query id in return
		//if (PerformParam.Get())
		{
			FVector Pos(PosParamX.Get(), PosParamY.Get(), PosParamZ.Get());
			FVector Dir(DirParamX.Get(), DirParamY.Get(), DirParamZ.Get());
			ensure(!Pos.ContainsNaN());
			float Dt = DTParam.Get();
			float Size = SizeParam.Get();
			*OutQueryID.GetDest() = InstanceData->CollisionBatch.SubmitQuery(Pos, Dir, Size, Dt);
		}

		// try to retrieve a query with the supplied query ID
		FNiagaraDICollsionQueryResult Res;
		int32 ID = InIDParam.Get();
		bool Valid = InstanceData->CollisionBatch.GetQueryResult(ID, Res);
		if (Valid)
		{
			*OutQueryValid.GetDest() = 0xFFFFFFFF; //->SetValue(true);
			*OutCollisionPosX.GetDest() = Res.CollisionPos.X;
			*OutCollisionPosY.GetDest() = Res.CollisionPos.Y;
			*OutCollisionPosZ.GetDest() = Res.CollisionPos.Z;
			*OutCollisionNormX.GetDest() = Res.CollisionNormal.X;
			*OutCollisionNormY.GetDest() = Res.CollisionNormal.Y;
			*OutCollisionNormZ.GetDest() = Res.CollisionNormal.Z;
			*Friction.GetDest() = Res.Friction;
			*Restitution.GetDest() = Res.Restitution;
		}
		else
		{
			*OutQueryValid.GetDest() = 0;// ->SetValue(false);
			*OutCollisionPosX.GetDest() =  0.0f;
			*OutCollisionPosY.GetDest() =  0.0f;
			*OutCollisionPosZ.GetDest() =  0.0f;
			*OutCollisionNormX.GetDest() = 0.0f;
			*OutCollisionNormY.GetDest() = 0.0f;
			*OutCollisionNormZ.GetDest() = 0.0f;
			*Friction.GetDest() = 0.0f;
			*Restitution.GetDest() = 0.0f;
		}

		//PerformParam.Advance();
		InIDParam.Advance();
		OutQueryValid.Advance();
		OutCollisionPosX.Advance();
		OutCollisionPosY.Advance();
		OutCollisionPosZ.Advance();
		OutCollisionNormX.Advance();
		OutCollisionNormY.Advance();
		OutCollisionNormZ.Advance();
		Friction.Advance();
		Restitution.Advance();

		PosParamX.Advance();
		PosParamY.Advance();
		PosParamZ.Advance();
		DirParamX.Advance();
		DirParamY.Advance();
		DirParamZ.Advance();
		DTParam.Advance();
		SizeParam.Advance();
		DepthBoundsParam.Advance();
		OutQueryID.Advance();
	}

}

void UNiagaraDataInterfaceCollisionQuery::PerformQuerySyncCPU(FVectorVMContext & Context)
{
	VectorVM::FExternalFuncInputHandler<float> StartPosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamZ(Context);

	VectorVM::FExternalFuncInputHandler<float> EndPosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamZ(Context);

	VectorVM::FExternalFuncInputHandler<ECollisionChannel> TraceChannelParam(Context);

	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryValid(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutInsideMesh(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutFriction(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutRestitution(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		FVector Pos(StartPosParamX.GetAndAdvance(), StartPosParamY.GetAndAdvance(), StartPosParamZ.GetAndAdvance());
		FVector Dir(EndPosParamX.GetAndAdvance(), EndPosParamY.GetAndAdvance(), EndPosParamZ.GetAndAdvance());
		ECollisionChannel TraceChannel = TraceChannelParam.GetAndAdvance();
		ensure(!Pos.ContainsNaN());
		FNiagaraDICollsionQueryResult Res;
		bool Valid = InstanceData->CollisionBatch.PerformQuery(Pos, Dir, Res, TraceChannel);
		if (Valid)
		{
			*OutQueryValid.GetDestAndAdvance() = 0xFFFFFFFF; //->SetValue(true);
			*OutInsideMesh.GetDestAndAdvance() = Res.IsInsideMesh ? 0xFFFFFFFF : 0;
			*OutCollisionPosX.GetDestAndAdvance() = Res.CollisionPos.X;
			*OutCollisionPosY.GetDestAndAdvance() = Res.CollisionPos.Y;
			*OutCollisionPosZ.GetDestAndAdvance() = Res.CollisionPos.Z;
			*OutCollisionNormX.GetDestAndAdvance() = Res.CollisionNormal.X;
			*OutCollisionNormY.GetDestAndAdvance() = Res.CollisionNormal.Y;
			*OutCollisionNormZ.GetDestAndAdvance() = Res.CollisionNormal.Z;
			*OutFriction.GetDestAndAdvance() = Res.Friction;
			*OutRestitution.GetDestAndAdvance() = Res.Restitution;
		}
		else
		{
			*OutQueryValid.GetDestAndAdvance() = 0; //->SetValue(false);
			*OutInsideMesh.GetDestAndAdvance() = 0;
			*OutCollisionPosX.GetDestAndAdvance() = 0.0f;
			*OutCollisionPosY.GetDestAndAdvance() = 0.0f;
			*OutCollisionPosZ.GetDestAndAdvance() = 0.0f;
			*OutCollisionNormX.GetDestAndAdvance() = 0.0f;
			*OutCollisionNormY.GetDestAndAdvance() = 0.0f;
			*OutCollisionNormZ.GetDestAndAdvance() = 0.0f;
			*OutFriction.GetDestAndAdvance() = 0.0f;
			*OutRestitution.GetDestAndAdvance() = 0.0f;
		}
	}
}

void UNiagaraDataInterfaceCollisionQuery::PerformQueryAsyncCPU(FVectorVMContext & Context)
{
	VectorVM::FExternalFuncInputHandler<int32> InIDParam(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamZ(Context);

	VectorVM::FExternalFuncInputHandler<float> EndPosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamZ(Context);

	VectorVM::FExternalFuncInputHandler<ECollisionChannel> TraceChannelParam(Context);

	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryID(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryValid(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutInsideMesh(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutFriction(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutRestitution(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		FVector Pos(StartPosParamX.GetAndAdvance(), StartPosParamY.GetAndAdvance(), StartPosParamZ.GetAndAdvance());
		FVector Dir(EndPosParamX.GetAndAdvance(), EndPosParamY.GetAndAdvance(), EndPosParamZ.GetAndAdvance());
		ECollisionChannel TraceChannel = TraceChannelParam.GetAndAdvance();
		ensure(!Pos.ContainsNaN());
		*OutQueryID.GetDestAndAdvance() = InstanceData->CollisionBatch.SubmitQuery(Pos, Dir, TraceChannel);

		// try to retrieve a query with the supplied query ID
		FNiagaraDICollsionQueryResult Res;
		int32 ID = InIDParam.GetAndAdvance();
		bool Valid = InstanceData->CollisionBatch.GetQueryResult(ID, Res);
		if (Valid)
		{
			*OutQueryValid.GetDestAndAdvance() = 0xFFFFFFFF; //->SetValue(true);
			*OutInsideMesh.GetDestAndAdvance() = Res.IsInsideMesh ? 0xFFFFFFFF : 0;
			*OutCollisionPosX.GetDestAndAdvance() = Res.CollisionPos.X;
			*OutCollisionPosY.GetDestAndAdvance() = Res.CollisionPos.Y;
			*OutCollisionPosZ.GetDestAndAdvance() = Res.CollisionPos.Z;
			*OutCollisionNormX.GetDestAndAdvance() = Res.CollisionNormal.X;
			*OutCollisionNormY.GetDestAndAdvance() = Res.CollisionNormal.Y;
			*OutCollisionNormZ.GetDestAndAdvance() = Res.CollisionNormal.Z;
			*OutFriction.GetDestAndAdvance() = Res.Friction;
			*OutRestitution.GetDestAndAdvance() = Res.Restitution;
		}
		else
		{
			*OutQueryValid.GetDestAndAdvance() = 0; //->SetValue(false);
			*OutInsideMesh.GetDestAndAdvance() = 0;
			*OutCollisionPosX.GetDestAndAdvance() = 0.0f;
			*OutCollisionPosY.GetDestAndAdvance() = 0.0f;
			*OutCollisionPosZ.GetDestAndAdvance() = 0.0f;
			*OutCollisionNormX.GetDestAndAdvance() = 0.0f;
			*OutCollisionNormY.GetDestAndAdvance() = 0.0f;
			*OutCollisionNormZ.GetDestAndAdvance() = 0.0f;
			*OutFriction.GetDestAndAdvance() = 0.0f;
			*OutRestitution.GetDestAndAdvance() = 0.0f;
		}
	}
}


void UNiagaraDataInterfaceCollisionQuery::PerformQueryGPU(FVectorVMContext& Context)
{
	UE_LOG(LogNiagara, Error, TEXT("GPU only function 'PerformQueryGPU' called on CPU VM, check your module code to fix."));

	VectorVM::FExternalFuncInputHandler<float> StartPosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> StartPosParamZ(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> EndPosParamZ(Context);
	VectorVM::FExternalFuncInputHandler<float> SceneDepthBoundsParam(Context);
	VectorVM::FExternalFuncInputHandler<float> ParticleRadiusParam(Context);
	VectorVM::FExternalFuncInputHandler<int32> UseDistanceFieldParam(Context);

	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryValid(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormZ(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutQueryValid.GetDestAndAdvance() = 0;// ->SetValue(false);
		*OutCollisionPosX.GetDestAndAdvance() = 0.0f;
		*OutCollisionPosY.GetDestAndAdvance() = 0.0f;
		*OutCollisionPosZ.GetDestAndAdvance() = 0.0f;
		*OutCollisionNormX.GetDestAndAdvance() = 0.0f;
		*OutCollisionNormY.GetDestAndAdvance() = 0.0f;
		*OutCollisionNormZ.GetDestAndAdvance() = 1.0f;
	}
}

void UNiagaraDataInterfaceCollisionQuery::QuerySceneDepth(FVectorVMContext & Context)
{
	UE_LOG(LogNiagara, Error, TEXT("GPU only function 'QuerySceneDepthGPU' called on CPU VM, check your module code to fix."));

	VectorVM::FExternalFuncInputHandler<float> SamplePosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> SamplePosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> SamplePosParamZ(Context);
	
	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutSceneDepth(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCameraPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCameraPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCameraPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutIsInsideView(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWorldPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWorldPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWorldPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWorldNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWorldNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutWorldNormZ(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutSceneDepth.GetDestAndAdvance() = -1;
		*OutIsInsideView.GetDestAndAdvance() = 0;
		*OutWorldPosX.GetDestAndAdvance() = 0.0f;
		*OutWorldPosY.GetDestAndAdvance() = 0.0f;
		*OutWorldPosZ.GetDestAndAdvance() = 0.0f;
		*OutWorldNormX.GetDestAndAdvance() = 0.0f;
		*OutWorldNormY.GetDestAndAdvance() = 0.0f;
		*OutWorldNormZ.GetDestAndAdvance() = 1.0f;
		*OutCameraPosX.GetDestAndAdvance() = 0.0f;
		*OutCameraPosY.GetDestAndAdvance() = 0.0f;
		*OutCameraPosZ.GetDestAndAdvance() = 0.0f;
	}
}

void UNiagaraDataInterfaceCollisionQuery::QueryMeshDistanceField(FVectorVMContext& Context)
{
	UE_LOG(LogNiagara, Error, TEXT("GPU only function 'QueryMeshDistanceFieldGPU' called on CPU VM, check your module code to fix."));

	VectorVM::FExternalFuncInputHandler<float> SamplePosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> SamplePosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> SamplePosParamZ(Context);

	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutSurfaceDistance(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutFieldGradientX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutFieldGradientY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutFieldGradientZ(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutSurfaceDistance.GetDestAndAdvance() = -1;
		*OutFieldGradientX.GetDestAndAdvance() = 0.0f;
		*OutFieldGradientY.GetDestAndAdvance() = 0.0f;
		*OutFieldGradientZ.GetDestAndAdvance() = 1.0f;
	}
}

void UNiagaraDataInterfaceCollisionQuery::SubmitQuery(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<float> PosParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> PosParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> PosParamZ(Context);
	VectorVM::FExternalFuncInputHandler<float> VelParamX(Context);
	VectorVM::FExternalFuncInputHandler<float> VelParamY(Context);
	VectorVM::FExternalFuncInputHandler<float> VelParamZ(Context);

	VectorVM::FExternalFuncInputHandler<float> DTParam(Context);
	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);

	FScopeLock ScopeLock(&CriticalSection);

	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryID(Context);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		FVector Pos(PosParamX.Get(), PosParamY.Get(), PosParamZ.Get());
		FVector Vel(VelParamX.Get(), VelParamY.Get(), VelParamZ.Get());
		ensure(!Pos.ContainsNaN());
		ensure(!Vel.ContainsNaN());
		float Dt = DTParam.Get();

		*OutQueryID.GetDest() = InstanceData->CollisionBatch.SubmitQuery(Pos, Vel, 0.0f, Dt);

		PosParamX.Advance();
		PosParamY.Advance();
		PosParamZ.Advance();
		VelParamX.Advance();
		VelParamY.Advance();
		VelParamZ.Advance();
		DTParam.Advance();
		OutQueryID.Advance();
	}

}

void UNiagaraDataInterfaceCollisionQuery::ReadQuery(FVectorVMContext& Context)
{	
	VectorVM::FExternalFuncInputHandler<int32> IDParam(Context);
	VectorVM::FUserPtrHandler<CQDIPerInstanceData> InstanceData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutQueryValid(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionPosZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionVelX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionVelY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionVelZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutCollisionNormZ(Context);

	FScopeLock ScopeLock(&CriticalSection);
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		FNiagaraDICollsionQueryResult Res;
		int32 ID = IDParam.Get();
		bool Valid = InstanceData->CollisionBatch.GetQueryResult(ID, Res);

		if (Valid)
		{
			*OutQueryValid.GetDest() = 0xFFFFFFFF; //->SetValue(true);
			*OutCollisionPosX.GetDest() = Res.CollisionPos.X;
			*OutCollisionPosY.GetDest() = Res.CollisionPos.Y;
			*OutCollisionPosZ.GetDest() = Res.CollisionPos.Z;
			*OutCollisionVelX.GetDest() = Res.CollisionVelocity.X;
			*OutCollisionVelY.GetDest() = Res.CollisionVelocity.Y;
			*OutCollisionVelZ.GetDest() = Res.CollisionVelocity.Z;
			*OutCollisionNormX.GetDest() = Res.CollisionNormal.X;
			*OutCollisionNormY.GetDest() = Res.CollisionNormal.Y;
			*OutCollisionNormZ.GetDest() = Res.CollisionNormal.Z;
		}
		else
		{
			*OutQueryValid.GetDest() = 0;// ->SetValue(false);
		}

		IDParam.Advance();
		OutQueryValid.Advance();
		OutCollisionPosX.Advance();
		OutCollisionPosY.Advance();
		OutCollisionPosZ.Advance();
		OutCollisionVelX.Advance();
		OutCollisionVelY.Advance();
		OutCollisionVelZ.Advance();
		OutCollisionNormX.Advance();
		OutCollisionNormY.Advance();
		OutCollisionNormZ.Advance();
	}
}

bool UNiagaraDataInterfaceCollisionQuery::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance, float DeltaSeconds)
{
	return false;
}

bool UNiagaraDataInterfaceCollisionQuery::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance, float DeltaSeconds)
{
	CQDIPerInstanceData *PIData = static_cast<CQDIPerInstanceData*>(PerInstanceData);
	PIData->CollisionBatch.Tick(ENiagaraSimTarget::CPUSim);
	PIData->CollisionBatch.ClearWrite();
	return false;
}

//////////////////////////////////////////////////////////////////////////

struct FNiagaraDataInterfaceParametersCS_CollisionQuery : public FNiagaraDataInterfaceParametersCS
{
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		PassUniformBuffer.Bind(ParameterMap, FSceneTexturesUniformParameters::StaticStructMetadata.GetShaderVariableName());
		
		GlobalDistanceFieldParameters.Bind(ParameterMap);
		if (GlobalDistanceFieldParameters.IsBound())
		{
			GNiagaraViewDataManager.SetGlobalDistanceFieldUsage();
		}
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << PassUniformBuffer;
		Ar << GlobalDistanceFieldParameters;
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface, void* PerInstanceData) const override
	{
		check(IsInRenderingThread());

		const FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		
		TUniformBufferRef<FSceneTexturesUniformParameters> SceneTextureUniformParams = GNiagaraViewDataManager.GetSceneTextureUniformParameters();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, PassUniformBuffer/*Shader->GetUniformBufferParameter(SceneTexturesUniformBufferStruct)*/, SceneTextureUniformParams);
		if (GlobalDistanceFieldParameters.IsBound())
		{
			GNiagaraViewDataManager.SetGlobalDistanceFieldUsage();
			GlobalDistanceFieldParameters.Set(RHICmdList, ComputeShaderRHI, *GNiagaraViewDataManager.GetGlobalDistanceFieldParameters());
		}		
	}

private:

	/** The SceneDepthTexture parameter for depth buffer collision. */
	FShaderUniformBufferParameter PassUniformBuffer;

	FGlobalDistanceFieldParameters GlobalDistanceFieldParameters;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceCollisionQuery::ConstructComputeParameters() const
{
	return new FNiagaraDataInterfaceParametersCS_CollisionQuery();
}