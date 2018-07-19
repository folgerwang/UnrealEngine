// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceTexture2D.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"


#define LOCTEXT_NAMESPACE "UNiagaraDataInterfaceTexture2D"

const FName UNiagaraDataInterfaceTexture2D::SampleTextureName(TEXT("SampleTexture2D"));
const FString UNiagaraDataInterfaceTexture2D::TextureName(TEXT("Texture_"));
const FString UNiagaraDataInterfaceTexture2D::SamplerName(TEXT("Sampler_"));

UNiagaraDataInterfaceTexture2D::UNiagaraDataInterfaceTexture2D(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Texture(nullptr)
{

}

void UNiagaraDataInterfaceTexture2D::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}
}

void UNiagaraDataInterfaceTexture2D::PostLoad()
{
	Super::PostLoad();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceTexture2D::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceTexture2D, Texture))
	{
		CopyTextureToCPUBackup(Texture, CPUTextureData);
	}
}

bool UNiagaraDataInterfaceTexture2D::CopyTextureToCPUBackup(UTexture* SourceTexture, TArray<uint8>& TargetBuffer)
{
	if (SourceTexture == nullptr)
	{
		TargetBuffer.Empty();
		return true;
	}

	FTextureSource& SourceData = SourceTexture->Source;
	FIntPoint SourceSize = FIntPoint(SourceData.GetSizeX(), SourceData.GetSizeY());

	{
		const int32 BytesPerPixel = 4;
		TargetBuffer.Empty();
		TargetBuffer.AddZeroed(SourceSize.X * SourceSize.Y * BytesPerPixel);
	}

	
	if (SourceData.GetFormat() == TSF_BGRA8)
	{
		uint32 BytesPerPixel = SourceData.GetBytesPerPixel();
		uint8* OffsetSource = SourceData.LockMip(0);
		uint8* OffsetDest = TargetBuffer.GetData();
		CopyTextureData(OffsetSource, OffsetDest, SourceSize.X, SourceSize.Y, BytesPerPixel, SourceData.GetSizeX() * BytesPerPixel, SourceSize.X * BytesPerPixel);
		SourceData.UnlockMip(0);
	}
	else 
	{
		UE_LOG(LogNiagara, Error, TEXT("Texture %s is not BGRA8, which isn't supported in data interfaces yet"), *SourceTexture->GetName());
	}

	return true;
}

void UNiagaraDataInterfaceTexture2D::CopyTextureData(const uint8* Source, uint8* Dest, uint32 SizeX, uint32 SizeY, uint32 BytesPerPixel, uint32 SourceStride, uint32 DestStride)
{
	const uint32 NumBytesPerRow = SizeX * BytesPerPixel;

	for (uint32 Y = 0; Y < SizeY; ++Y)
	{
		FMemory::Memcpy(
			Dest + (DestStride * Y),
			Source + (SourceStride * Y),
			NumBytesPerRow
		);
	}
}

#endif

bool UNiagaraDataInterfaceTexture2D::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceTexture2D* DestinationTexture = CastChecked<UNiagaraDataInterfaceTexture2D>(Destination);
	DestinationTexture->Texture = Texture;
	DestinationTexture->CPUTextureData = CPUTextureData;

	return true;
}

bool UNiagaraDataInterfaceTexture2D::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceTexture2D* OtherTexture = CastChecked<const UNiagaraDataInterfaceTexture2D>(Other);
	return OtherTexture->Texture == Texture;
}

void UNiagaraDataInterfaceTexture2D::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = SampleTextureName;
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Texture")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
	Sig.SetDescription(LOCTEXT("TextureSampleDesc", "Sample mip level 0 of the input 2d texture at the specified UV coordinates. The UV origin (0,0) is in the upper left hand corner of the image."));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));
	//Sig.Owner = *GetName();

	OutFunctions.Add(Sig);
}

DEFINE_NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceTexture2D, SampleTexture);
void UNiagaraDataInterfaceTexture2D::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	check(BindingInfo.Name == SampleTextureName);
	check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4);
	TNDIParamBinder<0, float, TNDIParamBinder<1, float, NDI_RAW_FUNC_BINDER(UNiagaraDataInterfaceTexture2D, SampleTexture)>>::Bind(this, BindingInfo, InstanceData, OutFunc);
}

template<typename XType, typename YType>
void UNiagaraDataInterfaceTexture2D::SampleTexture(FVectorVMContext& Context)
{
	XType XParam(Context);
	YType YParam(Context);
	FRegisterHandler<float> OutSampleR(Context);
	FRegisterHandler<float> OutSampleG(Context);
	FRegisterHandler<float> OutSampleB(Context);
	FRegisterHandler<float> OutSampleA(Context);

	if (CPUTextureData.GetAllocatedSize() == 0 || Texture == nullptr)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			float X = XParam.GetAndAdvance();
			float Y = YParam.GetAndAdvance();
			*OutSampleR.GetDestAndAdvance() = 1.0;
			*OutSampleG.GetDestAndAdvance() = 0.0;
			*OutSampleB.GetDestAndAdvance() = 1.0;
			*OutSampleA.GetDestAndAdvance() = 1.0;
		}
	}
	else
	{
		const int32 BytesPerPixel = 4;
		int32 IntSizeX = Texture->GetSizeX();
		int32 IntSizeY = Texture->GetSizeY();
		float SizeX = (float)Texture->GetSizeX();
		float SizeY = (float)Texture->GetSizeY();

		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			float X = fmodf(XParam.GetAndAdvance() * SizeX, SizeX);
			float Y = fmodf(YParam.GetAndAdvance() * SizeY, SizeY);
			int32 XInt = trunc(X);
			int32 YInt = trunc(Y);
			int32 SampleIdx = YInt * IntSizeX * BytesPerPixel + XInt * BytesPerPixel;
			ensure(CPUTextureData.Num() > SampleIdx);
			uint8 B0 = CPUTextureData[SampleIdx + 0];
			uint8 G0 = CPUTextureData[SampleIdx + 1];
			uint8 R0 = CPUTextureData[SampleIdx + 2];
			uint8 A0 = CPUTextureData[SampleIdx + 3];

			*OutSampleR.GetDestAndAdvance() = ((float)R0) / 255.0f;
			*OutSampleG.GetDestAndAdvance() = ((float)G0) / 255.0f;
			*OutSampleB.GetDestAndAdvance() = ((float)B0) / 255.0f;
			*OutSampleA.GetDestAndAdvance() = ((float)A0) / 255.0f;
		}
	}

}

bool UNiagaraDataInterfaceTexture2D::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	FString HLSLTextureName = TextureName + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLSamplerName = SamplerName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float2 In_UV, out float4 Out_Value) \n{\n");
	OutHLSL += TEXT("\t Out_Value = ") + HLSLTextureName + TEXT(".SampleLevel(") + HLSLSamplerName + TEXT(", In_UV, 0);\n");
	OutHLSL += TEXT("\n}\n");
	return true;
}

void UNiagaraDataInterfaceTexture2D::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	FString HLSLTextureName = TextureName + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLSamplerName = SamplerName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Texture2D ") + HLSLTextureName + TEXT(";\n");
	OutHLSL += TEXT("SamplerState ") + HLSLSamplerName + TEXT(";\n");
}



struct FNiagaraDataInterfaceParametersCS_Texture : public FNiagaraDataInterfaceParametersCS
{
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		FString TexName = UNiagaraDataInterfaceTexture2D::TextureName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;
		FString SampleName = (UNiagaraDataInterfaceTexture2D::SamplerName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol);
		TextureParam.Bind(ParameterMap, *TexName);
		SamplerParam.Bind(ParameterMap, *SampleName);
		
		if (!TextureParam.IsBound())
		{
			UE_LOG(LogNiagara, Warning, TEXT("Binding failed for FNiagaraDataInterfaceParametersCS_Texture Texture %s. Was it optimized out?"), *TexName)
		}

		if (!SamplerParam.IsBound())
		{
			UE_LOG(LogNiagara, Warning, TEXT("Binding failed for FNiagaraDataInterfaceParametersCS_Texture Sampler %s. Was it optimized out?"), *SampleName)
		}
	}

	virtual void Serialize(FArchive& Ar)override
	{
		Ar << TextureParam;
		Ar << SamplerParam;
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface) const override
	{
		check(IsInRenderingThread());

		FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		UNiagaraDataInterfaceTexture2D* TextureDI = CastChecked<UNiagaraDataInterfaceTexture2D>(DataInterface);
		UTexture *Texture = TextureDI->Texture;
		if (!Texture)
		{
			return;
		}
		FTextureRHIParamRef TextureRHI = Texture->Resource->TextureRHI;
		SetTextureParameter(
			RHICmdList,
			ComputeShaderRHI,
			TextureParam,
			SamplerParam,
			TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI(),
			TextureRHI
		);
	}


private:

	FShaderResourceParameter TextureParam;
	FShaderResourceParameter SamplerParam;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceTexture2D::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_Texture();
}