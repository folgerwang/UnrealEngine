// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "NiagaraCustomVersion.h"


#define LOCTEXT_NAMESPACE "UNiagaraDataInterfaceTexture"

const FName UNiagaraDataInterfaceTexture::SampleTexture2DName(TEXT("SampleTexture2D"));
const FName UNiagaraDataInterfaceTexture::SampleVolumeTextureName(TEXT("SampleVolumeTexture"));
const FName UNiagaraDataInterfaceTexture::SamplePseudoVolumeTextureName(TEXT("SamplePseudoVolumeTexture"));
const FName UNiagaraDataInterfaceTexture::TextureDimsName(TEXT("TextureDimensions2D"));
const FString UNiagaraDataInterfaceTexture::TextureName(TEXT("Texture_"));
const FString UNiagaraDataInterfaceTexture::SamplerName(TEXT("Sampler_"));
const FString UNiagaraDataInterfaceTexture::DimensionsBaseName(TEXT("Dimensions_"));

UNiagaraDataInterfaceTexture::UNiagaraDataInterfaceTexture(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Texture(nullptr)
{

}

void UNiagaraDataInterfaceTexture::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
	}
}

void UNiagaraDataInterfaceTexture::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	const int32 NiagaraVer = GetLinkerCustomVersion(FNiagaraCustomVersion::GUID);
	if (NiagaraVer < FNiagaraCustomVersion::TextureDataInterfaceUsesCustomSerialize)
	{
		if (Texture != nullptr)
		{
			Texture->ConditionalPostLoad();
		}
	}
#endif
}

#if WITH_EDITOR

void UNiagaraDataInterfaceTexture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}



#endif

void UNiagaraDataInterfaceTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() == false || Ar.CustomVer(FNiagaraCustomVersion::GUID) >= FNiagaraCustomVersion::TextureDataInterfaceUsesCustomSerialize)
	{
		TArray<uint8> StreamData;
		Ar << StreamData;
	}
	Ar.UsingCustomVersion(FNiagaraCustomVersion::GUID);
}

bool UNiagaraDataInterfaceTexture::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceTexture* DestinationTexture = CastChecked<UNiagaraDataInterfaceTexture>(Destination);
	DestinationTexture->Texture = Texture;

	return true;
}

bool UNiagaraDataInterfaceTexture::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceTexture* OtherTexture = CastChecked<const UNiagaraDataInterfaceTexture>(Other);
	return OtherTexture->Texture == Texture;
}

void UNiagaraDataInterfaceTexture::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = SampleTexture2DName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Texture")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("UV")));
		Sig.SetDescription(LOCTEXT("TextureSampleTexture2DDesc", "Sample mip level 0 of the input 2d texture at the specified UV coordinates. The UV origin (0,0) is in the upper left hand corner of the image."));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));
		//Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}

	//{
	//	FNiagaraFunctionSignature Sig;
	//	Sig.Name = SampleVolumeTextureName;
	//	Sig.bMemberFunction = true;
	//	Sig.bRequiresContext = false;
	//	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Texture")));
	//	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("UVW")));
	//	Sig.SetDescription(LOCTEXT("TextureSampleVolumeTextureDesc", "Sample mip level 0 of the input 3d texture at the specified UVW coordinates. The UVW origin (0,0) is in the bottom left hand corner of the volume."));
	//	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));
	//	//Sig.Owner = *GetName();

	//	OutFunctions.Add(Sig);
	//}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = SamplePseudoVolumeTextureName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Texture")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("UVW")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("XYNumFrames")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("TotalNumFrames")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MipMode")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("MipLevel")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("DDX")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("DDY")));
		
		Sig.SetDescription(LOCTEXT("TextureSamplePseudoVolumeTextureDesc", "Return a pseudovolume texture sample.\n"
			"Useful for simulating 3D texturing with a 2D texture or as a texture flipbook with lerped transitions.\n"
			"Treats 2d layout of frames as a 3d texture and performs bilinear filtering by blending with an offset Z frame.\n"
			"Texture = Input Texture Object storing Volume Data\n"
			"UVW = Input float3 for Position, 0 - 1\n"
			"XYNumFrames = Input float for num frames in x, y directions\n"
			"TotalNumFrames = Input float for num total frames\n"
			"MipMode = Sampling mode : 0 = use miplevel, 1 = use UV computed gradients, 2 = Use gradients(default = 0)\n"
			"MipLevel = MIP level to use in mipmode = 0 (default 0)\n"
			"DDX, DDY = Texture gradients in mipmode = 2\n"));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("Value")));
		//Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = TextureDimsName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Texture")));
		Sig.SetDescription(LOCTEXT("TextureDimsDesc", "Get the dimensions of mip 0 of the texture."));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Dimensions2D")));
		//Sig.Owner = *GetName();

		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceTexture, SampleTexture);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceTexture, SamplePseudoVolumeTexture)
void UNiagaraDataInterfaceTexture::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == SampleTexture2DName)
	{
		check(BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceTexture, SampleTexture)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == SamplePseudoVolumeTextureName)
	{
		check(BindingInfo.GetNumInputs() == 12 && BindingInfo.GetNumOutputs() == 4);
		NDI_FUNC_BINDER(UNiagaraDataInterfaceTexture, SamplePseudoVolumeTexture)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == TextureDimsName)
	{
		check(BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 2);
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceTexture::GetTextureDimensions);
	}
}

void UNiagaraDataInterfaceTexture::GetTextureDimensions(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<float> OutWidth(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutHeight(Context);

	if (Texture == nullptr)
	{
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			*OutWidth.GetDestAndAdvance() = 0.0f;
			*OutHeight.GetDestAndAdvance() = 0.0f;
		}
	}
	else
	{
		float Width = Texture->GetSurfaceWidth();
		float Height = Texture->GetSurfaceHeight();
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			*OutWidth.GetDestAndAdvance() = Width;
			*OutHeight.GetDestAndAdvance() = Height;
		}
	}
}

void UNiagaraDataInterfaceTexture::SampleTexture(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncInputHandler<float> XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> YParam(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleB(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleA(Context);

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

void UNiagaraDataInterfaceTexture::SamplePseudoVolumeTexture(FVectorVMContext& Context)
{
	// Noop handler which just returns magenta since this doesn't run on CPU.
	VectorVM::FExternalFuncInputHandler<float> UVW_UParam(Context);
	VectorVM::FExternalFuncInputHandler<float> UVW_VParam(Context);
	VectorVM::FExternalFuncInputHandler<float> UVW_WParam(Context);

	VectorVM::FExternalFuncInputHandler<float> XYNumFrames_XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> XYNumFrames_YParam(Context);
	
	VectorVM::FExternalFuncInputHandler<float> TotalNumFramesParam(Context);

	VectorVM::FExternalFuncInputHandler<int32> MipModeParam(Context);

	VectorVM::FExternalFuncInputHandler<float> MipLevelParam(Context);

	VectorVM::FExternalFuncInputHandler<float> DDX_XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> DDX_YParam(Context);

	VectorVM::FExternalFuncInputHandler<float> DDY_XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> DDY_YParam(Context);

	VectorVM::FExternalFuncRegisterHandler<float> OutSampleR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleB(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleA(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		UVW_UParam.Advance();
		UVW_VParam.Advance();
		UVW_WParam.Advance();

		XYNumFrames_XParam.Advance();
		XYNumFrames_YParam.Advance();

		TotalNumFramesParam.Advance();

		MipModeParam.Advance();

		MipLevelParam.Advance();

		DDX_XParam.Advance();
		DDX_YParam.Advance();

		DDY_XParam.Advance();
		DDY_YParam.Advance();

		*OutSampleR.GetDestAndAdvance() = 1.0;
		*OutSampleG.GetDestAndAdvance() = 0.0;
		*OutSampleB.GetDestAndAdvance() = 1.0;
		*OutSampleA.GetDestAndAdvance() = 1.0;
	}
}

bool UNiagaraDataInterfaceTexture::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	if (DefinitionFunctionName == SampleTexture2DName)
	{
		FString HLSLTextureName = TextureName + ParamInfo.DataInterfaceHLSLSymbol;
		FString HLSLSamplerName = SamplerName + ParamInfo.DataInterfaceHLSLSymbol;
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float2 In_UV, out float4 Out_Value) \n{\n");
		OutHLSL += TEXT("\t Out_Value = ") + HLSLTextureName + TEXT(".SampleLevel(") + HLSLSamplerName + TEXT(", In_UV, 0);\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	/*else if (DefinitionFunctionName == SampleVolumeTextureName)
	{
		FString HLSLTextureName = TextureName + ParamInfo.DataInterfaceHLSLSymbol;
		FString HLSLSamplerName = SamplerName + ParamInfo.DataInterfaceHLSLSymbol;
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_UV, out float4 Out_Value) \n{\n");
		OutHLSL += TEXT("\t Out_Value = ") + HLSLTextureName + TEXT(".SampleLevel(") + HLSLSamplerName + TEXT(", In_UV, 0);\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}*/
	else if (DefinitionFunctionName == SamplePseudoVolumeTextureName)
	{
		FString HLSLTextureName = TextureName + ParamInfo.DataInterfaceHLSLSymbol;
		FString HLSLSamplerName = SamplerName + ParamInfo.DataInterfaceHLSLSymbol;
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(in float3 In_UVW, in float2 In_XYNumFrames, in float In_TotalNumFrames, in int In_MipMode, in float In_MipLevel, in float2 In_DDX, in float2 In_DDY, out float4 Out_Value) \n{\n");
		OutHLSL += TEXT("\t Out_Value = PseudoVolumeTexture(") + HLSLTextureName + TEXT(", ") + HLSLSamplerName + TEXT(", In_UVW, In_XYNumFrames, In_TotalNumFrames, (uint) In_MipMode, In_MipLevel, In_DDX, In_DDY); \n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	else if (DefinitionFunctionName == TextureDimsName)
	{
		FString DimsVar = DimensionsBaseName + ParamInfo.DataInterfaceHLSLSymbol;
		OutHLSL += TEXT("void ") + InstanceFunctionName + TEXT("(out float2 Out_Value) \n{\n");
		OutHLSL += TEXT("\t Out_Value = ") + DimsVar + TEXT(";\n");
		OutHLSL += TEXT("\n}\n");
		return true;
	}
	return false;
}

void UNiagaraDataInterfaceTexture::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	FString HLSLTextureName = TextureName + ParamInfo.DataInterfaceHLSLSymbol;
	FString HLSLSamplerName = SamplerName + ParamInfo.DataInterfaceHLSLSymbol;
	OutHLSL += TEXT("Texture2D ") + HLSLTextureName + TEXT(";\n");
	OutHLSL += TEXT("SamplerState ") + HLSLSamplerName + TEXT(";\n");
	OutHLSL += TEXT("float2 ") + DimensionsBaseName + ParamInfo.DataInterfaceHLSLSymbol + TEXT(";\n");

}



struct FNiagaraDataInterfaceParametersCS_Texture : public FNiagaraDataInterfaceParametersCS
{
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		FString TexName = UNiagaraDataInterfaceTexture::TextureName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol;
		FString SampleName = (UNiagaraDataInterfaceTexture::SamplerName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol);
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

		Dimensions.Bind(ParameterMap, *(UNiagaraDataInterfaceTexture::DimensionsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));

	}

	virtual void Serialize(FArchive& Ar)override
	{
		Ar << TextureParam;
		Ar << SamplerParam;
		if (Ar.IsLoading() == false || Ar.CustomVer(FNiagaraCustomVersion::GUID) >= FNiagaraCustomVersion::TextureDataInterfaceSizeSerialize)
		{
			Ar << Dimensions;
		}
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface) const override
	{
		check(IsInRenderingThread());

		FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		UNiagaraDataInterfaceTexture* TextureDI = CastChecked<UNiagaraDataInterfaceTexture>(DataInterface);
		UTexture *Texture = TextureDI->Texture;
		float TexDims[2];
		if (!Texture)
		{
			TexDims[0] = 0.0f;
			TexDims[1] = 0.0f;
			SetShaderValue(RHICmdList, ComputeShaderRHI, Dimensions, TexDims);
			return;
		}
		FTextureRHIParamRef TextureRHI = Texture->TextureReference.TextureReferenceRHI->GetReferencedTexture();
		SetTextureParameter(
			RHICmdList,
			ComputeShaderRHI,
			TextureParam,
			SamplerParam,
			Texture->Resource->SamplerStateRHI,
			TextureRHI
		);
		TexDims[0] = TextureDI->Texture->GetSurfaceWidth();
		TexDims[1] = TextureDI->Texture->GetSurfaceHeight();
		SetShaderValue(RHICmdList, ComputeShaderRHI, Dimensions, TexDims);
	}


private:

	FShaderResourceParameter TextureParam;
	FShaderResourceParameter SamplerParam;
	FShaderParameter Dimensions;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceTexture::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_Texture();
}

#undef LOCTEXT_NAMESPACE