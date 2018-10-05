// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCollisionQuery.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraWorldManager.h"
#include "ShaderParameterUtils.h"
#include "Shader.h"

//////////////////////////////////////////////////////////////////////////
//Color Curve

FCriticalSection UNiagaraDataInterfaceCollisionQuery::CriticalSection;

UNiagaraDataInterfaceCollisionQuery::UNiagaraDataInterfaceCollisionQuery(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
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

#if WITH_EDITOR

void UNiagaraDataInterfaceCollisionQuery::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif


bool UNiagaraDataInterfaceCollisionQuery::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	return true;
}

bool UNiagaraDataInterfaceCollisionQuery::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	return true;
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
		OutHLSL += TEXT("float3 WorldPositionFromSceneDepth(float2 ScreenPosition, float SceneDepth)\n{\n\tfloat4 HomogeneousWorldPosition = mul(float4(ScreenPosition * SceneDepth, SceneDepth, 1), View.ScreenToWorld);\n\treturn HomogeneousWorldPosition.xyz / HomogeneousWorldPosition.w;\n}\n");
		OutHLSL += TEXT("\n");
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in int InQueryID, in float3 In_ParticlePos, in float3 In_ParticleVel, in float In_DeltaSeconds, float CollisionRadius, in float CollisionDepthBounds, int In_InstanceData, \
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
					Out_QueryID = 0.0f;\
				}\n\
				else\n\
				{\n\
					OutCollisionValid = false; \n\
				}\n\
			}\n\
		}\n\
		\n\
		\n");

		OutHLSL += TEXT("}\n");
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
		PassUniformBuffer.Bind(ParameterMap, FSceneTexturesUniformParameters::StaticStruct.GetShaderVariableName());
		check(PassUniformBuffer.IsBound());
	}

	virtual void Serialize(FArchive& Ar)override
	{
		Ar << PassUniformBuffer;
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface) const override
	{
		check(IsInRenderingThread());

		const FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();

		TUniformBufferRef<FSceneTexturesUniformParameters> SceneTextureUniformParams = GNiagaraViewDataManager.GetSceneTextureUniformParameters();
		SetUniformBufferParameter(RHICmdList, ComputeShaderRHI, PassUniformBuffer/*Shader->GetUniformBufferParameter(SceneTexturesUniformBufferStruct)*/, SceneTextureUniformParams);
	}

private:

	/** The SceneDepthTexture parameter for depth buffer collision. */
	FShaderUniformBufferParameter PassUniformBuffer;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceCollisionQuery::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_CollisionQuery();
}