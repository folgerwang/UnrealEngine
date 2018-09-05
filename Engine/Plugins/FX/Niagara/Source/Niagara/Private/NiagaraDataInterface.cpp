// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterface.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveFloat.h"
#include "NiagaraTypes.h"
#include "ShaderParameterUtils.h"
#include "NiagaraShader.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterface"

UNiagaraDataInterface::UNiagaraDataInterface(FObjectInitializer const& ObjectInitializer)
{
}

void UNiagaraDataInterface::PostLoad()
{
	Super::PostLoad();
	SetFlags(RF_Public);
}

bool UNiagaraDataInterface::CopyTo(UNiagaraDataInterface* Destination) const 
{
	bool result = CopyToInternal(Destination);
#if WITH_EDITOR
	Destination->OnChanged().Broadcast();
#endif
	return result;
}

bool UNiagaraDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
	if (Other == nullptr || Other->GetClass() != GetClass())
	{
		return false;
	}
	return true;
}

bool UNiagaraDataInterface::IsDataInterfaceType(const FNiagaraTypeDefinition& TypeDef)
{
	const UClass* Class = TypeDef.GetClass();
	if (Class && Class->IsChildOf(UNiagaraDataInterface::StaticClass()))
	{
		return true;
	}
	return false;
}

bool UNiagaraDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (Destination == nullptr || Destination->GetClass() != GetClass())
	{
		return false;
	}
	return true;
}

#if WITH_EDITOR

void UNiagaraDataInterface::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	TArray<FNiagaraFunctionSignature> DIFuncs;
	GetFunctions(DIFuncs);

	if (!DIFuncs.Contains(Function))
	{
		//We couldn't find this signature in the list of available functions.
		//Lets try to find one with the same name whose parameters may have changed.
		int32 ExistingSigIdx = DIFuncs.IndexOfByPredicate([&](const FNiagaraFunctionSignature& Sig) { return Sig.GetName() == Function.GetName(); });;
		if (ExistingSigIdx != INDEX_NONE)
		{
			OutValidationErrors.Add(FText::Format(LOCTEXT("DI Function Parameter Mismatch!", "Data Interface function called but it's parameters do not match any available function!\nThe API for this data interface function has likely changed and you need to update your graphs.\nInterface: {0}\nFunction: {1}\n"), FText::FromString(GetClass()->GetName()), FText::FromString(Function.GetName())));
		}
		else
		{
			OutValidationErrors.Add(FText::Format(LOCTEXT("Unknown DI Function", "Unknown Data Interface function called!\nThe API for this data interface has likely changed and you need to update your graphs.\nInterface: {0}\nFunction: {1}\n"), FText::FromString(GetClass()->GetName()), FText::FromString(Function.GetName())));
		}
	}
}

#endif

//////////////////////////////////////////////////////////////////////////

bool UNiagaraDataInterfaceCurveBase::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceCurveBase* DestinationTyped = CastChecked<UNiagaraDataInterfaceCurveBase>(Destination);
	DestinationTyped->bUseLUT = bUseLUT;
	return true;
}


bool UNiagaraDataInterfaceCurveBase::CompareLUTS(const TArray<float>& OtherLUT) const
{
	if (ShaderLUT.Num() == OtherLUT.Num())
	{
		for (int32 i = 0; i < ShaderLUT.Num(); i++)
		{
			if (false == FMath::IsNearlyEqual(ShaderLUT[i], OtherLUT[i]))
			{
				return false;
			}
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool UNiagaraDataInterfaceCurveBase::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}

	const UNiagaraDataInterfaceCurveBase* OtherTyped = CastChecked<UNiagaraDataInterfaceCurveBase>(Other);
	bool bEqual = OtherTyped->bUseLUT == bUseLUT;
	return bEqual;
}


void UNiagaraDataInterfaceCurveBase::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	OutHLSL += TEXT("\n");
	FString MinTimeStr = TEXT("MinTime_") + ParamInfo.DataInterfaceHLSLSymbol;
	FString MaxTimeStr = TEXT("MaxTime_") + ParamInfo.DataInterfaceHLSLSymbol;
	FString InvTimeRangeStr = TEXT("InvTimeRange_") + ParamInfo.DataInterfaceHLSLSymbol;

	FString BufferName = "CurveLUT_" + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Buffer<float> ") + BufferName + TEXT(";\n");

	OutHLSL += TEXT("float ") + MinTimeStr + TEXT(";\n");
	OutHLSL += TEXT("float ") + MaxTimeStr + TEXT(";\n");
	OutHLSL += TEXT("float ") + InvTimeRangeStr + TEXT(";\n");
	OutHLSL += TEXT("\n");

	//TODO: Create a Unitiliy/Common funcitons hlsl def shared between all instances of the same data interface class for these.
	OutHLSL += FString::Printf(TEXT("float TimeToLUTFraction_%s(float T)\n{\n\treturn saturate((T - %s) * %s);\n}\n"), *ParamInfo.DataInterfaceHLSLSymbol, *MinTimeStr, *InvTimeRangeStr);
	OutHLSL += FString::Printf(TEXT("float SampleCurve_%s(float T)\n{\n\treturn %s[(uint)T];\n}\n"), *ParamInfo.DataInterfaceHLSLSymbol, *BufferName);
	OutHLSL += TEXT("\n");
}

FRWBuffer& UNiagaraDataInterfaceCurveBase::GetCurveLUTGPUBuffer()
{
	//TODO: This isn't really very thread safe. Need to move to a proxy like system where DIs can push data to the RT safely.
	if (GPUBufferDirty)
	{
		int32 ElemSize = GetCurveNumElems();
		CurveLUT.Release();
		CurveLUT.Initialize(sizeof(float), CurveLUTWidth * ElemSize, EPixelFormat::PF_R32_FLOAT, BUF_Static);
		uint32 BufferSize = ShaderLUT.Num() * sizeof(float);
		int32 *BufferData = static_cast<int32*>(RHILockVertexBuffer(CurveLUT.Buffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly));
		FPlatformMemory::Memcpy(BufferData, ShaderLUT.GetData(), BufferSize);
		RHIUnlockVertexBuffer(CurveLUT.Buffer);
		GPUBufferDirty = false;
	}

	return CurveLUT;
}

struct FNiagaraDataInterfaceParametersCS_Curve : public FNiagaraDataInterfaceParametersCS
{
	virtual ~FNiagaraDataInterfaceParametersCS_Curve() {}
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		MinTime.Bind(ParameterMap, *(TEXT("MinTime_") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		MaxTime.Bind(ParameterMap, *(TEXT("MaxTime_") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		InvTimeRange.Bind(ParameterMap, *(TEXT("InvTimeRange_") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		CurveLUT.Bind(ParameterMap, *(TEXT("CurveLUT_") + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
	}

	virtual void Serialize(FArchive& Ar) override
	{
		Ar << MinTime;
		Ar << MaxTime;
		Ar << InvTimeRange;
		Ar << CurveLUT;
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface) const override
	{
		check(IsInRenderingThread());

		const FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		UNiagaraDataInterfaceCurveBase* CurveDI = CastChecked<UNiagaraDataInterfaceCurveBase>(DataInterface);
		FRWBuffer& CurveLUTBuffer = CurveDI->GetCurveLUTGPUBuffer();

		SetShaderValue(RHICmdList, ComputeShaderRHI, MinTime, CurveDI->GetMinTime());
		SetShaderValue(RHICmdList, ComputeShaderRHI, MaxTime, CurveDI->GetMaxTime());
		SetShaderValue(RHICmdList, ComputeShaderRHI, InvTimeRange, CurveDI->GetInvTimeRange());
		RHICmdList.SetShaderResourceViewParameter(ComputeShaderRHI, CurveLUT.GetBaseIndex(), CurveLUTBuffer.SRV);
	}

	FShaderParameter MinTime;
	FShaderParameter MaxTime;
	FShaderParameter InvTimeRange;
	FShaderResourceParameter CurveLUT;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceCurveBase::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_Curve();
}


#undef LOCTEXT_NAMESPACE
