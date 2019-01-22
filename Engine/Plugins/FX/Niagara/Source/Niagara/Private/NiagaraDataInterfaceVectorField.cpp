// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceVectorField.h"
#include "VectorField/VectorFieldStatic.h"
#include "VectorField/VectorFieldAnimated.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
//#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceVectorField"

// Global HLSL variable base names, used by HLSL.
static const FString SamplerBaseName(TEXT("VectorFieldSampler_"));
static const FString TextureBaseName(TEXT("VectorFieldTexture_"));
static const FString TilingAxesBaseName(TEXT("TilingAxes_"));
static const FString DimensionsBaseName(TEXT("Dimensions_"));
static const FString MinBoundsBaseName(TEXT("MinBounds_"));
static const FString MaxBoundsBaseName(TEXT("MaxBounds_"));

// Global VM function names, also used by the shaders code generation methods.
static const FName SampleVectorFieldName("SampleField");
static const FName GetVectorFieldTilingAxesName("FieldTilingAxes");
static const FName GetVectorFieldDimensionsName("FieldDimensions");
static const FName GetVectorFieldBoundsName("FieldBounds");

/*--------------------------------------------------------------------------------------------------------------------------*/

UNiagaraDataInterfaceVectorField::UNiagaraDataInterfaceVectorField(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Field(nullptr)
	, bTileX(false)
	, bTileY(false)
	, bTileZ(false)
{
}

/*--------------------------------------------------------------------------------------------------------------------------*/


#if WITH_EDITOR
void UNiagaraDataInterfaceVectorField::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	
	// Flush the rendering thread before making any changes to make sure the 
	// data read by the compute shader isn't subject to a race condition.
	// TODO(mv): Solve properly using something like a RT Proxy.
	FlushRenderingCommands();
}

void UNiagaraDataInterfaceVectorField::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif //WITH_EDITOR


void UNiagaraDataInterfaceVectorField::PostLoad()
{
	Super::PostLoad();
	if (Field)
	{
		Field->ConditionalPostLoad();
	}
}

void UNiagaraDataInterfaceVectorField::PostInitProperties()
{
	Super::PostInitProperties();

	//Can we register data interfaces as regular types and fold them into the FNiagaraVariable framework for UI and function calls etc?
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), /*bCanBeParameter*/ true, /*bCanBePayload*/ false, /*bIsUserDefined*/ false);
	}
}

/*--------------------------------------------------------------------------------------------------------------------------*/

void UNiagaraDataInterfaceVectorField::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = SampleVectorFieldName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Vector Field")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Sample Point")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Sampled Value")));

		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorFieldDimensionsName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Vector Field")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Dimensions")));

		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorFieldTilingAxesName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Vector Field")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("TilingAxes")));

		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVectorFieldBoundsName;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Vector Field")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("MinBounds")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("MaxBounds")));

		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceVectorField, SampleVectorField);
void UNiagaraDataInterfaceVectorField::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == SampleVectorFieldName && BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceVectorField, SampleVectorField)::Bind(this, OutFunc);
	}
	else if (BindingInfo.Name == GetVectorFieldDimensionsName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 3)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceVectorField::GetFieldDimensions);
	}
	else if (BindingInfo.Name == GetVectorFieldBoundsName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 6)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceVectorField::GetFieldBounds);
	}
	else if (BindingInfo.Name == GetVectorFieldTilingAxesName && BindingInfo.GetNumInputs() == 0 && BindingInfo.GetNumOutputs() == 3)
	{
		OutFunc = FVMExternalFunction::CreateUObject(this, &UNiagaraDataInterfaceVectorField::GetFieldTilingAxes);
	}
}

bool UNiagaraDataInterfaceVectorField::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceVectorField* OtherTyped = CastChecked<const UNiagaraDataInterfaceVectorField>(Other);
	return OtherTyped->Field == Field 
		&& OtherTyped->bTileX == bTileX
		&& OtherTyped->bTileY == bTileY
		&& OtherTyped->bTileZ == bTileZ;
}

bool UNiagaraDataInterfaceVectorField::CanExecuteOnTarget(ENiagaraSimTarget Target) const
{
	return true;
}

/*--------------------------------------------------------------------------------------------------------------------------*/

#if WITH_EDITOR	
TArray<FNiagaraDataInterfaceError> UNiagaraDataInterfaceVectorField::GetErrors()
{
	UVectorFieldStatic* StaticVectorField = Cast<UVectorFieldStatic>(Field);
	UVectorFieldAnimated* AnimatedVectorField = Cast<UVectorFieldAnimated>(Field);
	
	// TODO(mv): Improve error messages?
	TArray<FNiagaraDataInterfaceError> Errors;
	if (StaticVectorField != nullptr && !StaticVectorField->bAllowCPUAccess)
	{
		FNiagaraDataInterfaceError CPUAccessNotAllowedError(
			FText::Format(
				LOCTEXT("CPUAccessNotAllowedError", "This Vector Field needs CPU access in order to be used properly.({0})"), 
				FText::FromString(StaticVectorField->GetName())
			),
			LOCTEXT("CPUAccessNotAllowedErrorSummary", "CPU access error"),
			FNiagaraDataInterfaceFix::CreateLambda(
				[=]()
				{
					StaticVectorField->SetCPUAccessEnabled();
					return true;
				}
			)
		);
		Errors.Add(CPUAccessNotAllowedError);
	}
	else if (AnimatedVectorField != nullptr)
	{
		FNiagaraDataInterfaceError AnimatedVectorFieldsNotSupportedError(
			LOCTEXT("AnimatedVectorFieldsNotSupportedErrorSummary", "Invalid vector field type."),
			LOCTEXT("AnimatedVectorFieldsNotSupportedError", "Animated vector fields are not supported."),
			nullptr
		);
		Errors.Add(AnimatedVectorFieldsNotSupportedError);
	}
	else if (Field == nullptr)
	{
		FNiagaraDataInterfaceError VectorFieldNotLoadedError(
			LOCTEXT("VectorFieldNotLoadedErrorSummary", "No Vector Field is loaded."),
			LOCTEXT("VectorFieldNotLoadedError", "No Vector Field is loaded."),
			nullptr
		);
		Errors.Add(VectorFieldNotLoadedError);
	}
	return Errors;
}
#endif // WITH_EDITOR	

/*--------------------------------------------------------------------------------------------------------------------------*/

void UNiagaraDataInterfaceVectorField::GetParameterDefinitionHLSL(FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	static const TCHAR *FormatDeclarations = TEXT(R"(
		float3 {TilingAxesName};
		float3 {DimensionsName};
		float3 {MinBoundsName};
		float3 {MaxBoundsName};
		Texture3D {TextureName};
		SamplerState {SamplerName};
	)");
	TMap<FString, FStringFormatArg> ArgsDeclarations = {
		{ TEXT("TilingAxesName"), TilingAxesBaseName + ParamInfo.DataInterfaceHLSLSymbol },
		{ TEXT("DimensionsName"), DimensionsBaseName + ParamInfo.DataInterfaceHLSLSymbol },
		{ TEXT("MinBoundsName"),  MinBoundsBaseName + ParamInfo.DataInterfaceHLSLSymbol },
		{ TEXT("MaxBoundsName"),  MaxBoundsBaseName + ParamInfo.DataInterfaceHLSLSymbol },
		{ TEXT("TextureName"),    TextureBaseName + ParamInfo.DataInterfaceHLSLSymbol },
		{ TEXT("SamplerName"),    SamplerBaseName + ParamInfo.DataInterfaceHLSLSymbol }
	};
	OutHLSL += FString::Format(FormatDeclarations, ArgsDeclarations);
}

bool UNiagaraDataInterfaceVectorField::GetFunctionHLSL(const FName& DefinitionFunctionName, FString InstanceFunctionName, FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	if (DefinitionFunctionName == SampleVectorFieldName)
	{
		static const TCHAR *FormatSample = TEXT(R"(
			void {FunctionName}(float3 In_SamplePoint, out float3 Out_Sample)
			{
				float3 SamplePoint = (In_SamplePoint - {MinBoundsName}) / ({MaxBoundsName} - {MinBoundsName});
				Out_Sample = Texture3DSample({TextureName}, {SamplerName}, SamplePoint).xyz;
			}
		)");
		TMap<FString, FStringFormatArg> ArgsSample = {
			{TEXT("FunctionName"), InstanceFunctionName},
			{TEXT("TextureName"), TextureBaseName + ParamInfo.DataInterfaceHLSLSymbol},
			{TEXT("MinBoundsName"), MinBoundsBaseName + ParamInfo.DataInterfaceHLSLSymbol},
			{TEXT("MaxBoundsName"), MaxBoundsBaseName + ParamInfo.DataInterfaceHLSLSymbol},
			{TEXT("SamplerName"), SamplerBaseName + ParamInfo.DataInterfaceHLSLSymbol}
		};
		OutHLSL += FString::Format(FormatSample, ArgsSample);
		return true;
	}
	else if (DefinitionFunctionName == GetVectorFieldTilingAxesName)
	{
		static const TCHAR *FormatTilingAxes = TEXT(R"(
			void {FunctionName}(out float3 Out_TilingAxes)
			{
				Out_TilingAxes = {TilingAxesName};
			}
		)");
		TMap<FString, FStringFormatArg> ArgsTilingAxes = {
			{TEXT("FunctionName"), InstanceFunctionName},
			{TEXT("TilingAxesName"), TilingAxesBaseName + ParamInfo.DataInterfaceHLSLSymbol}
		};
		OutHLSL += FString::Format(FormatTilingAxes, ArgsTilingAxes);
		return true;
	}
	else if (DefinitionFunctionName == GetVectorFieldDimensionsName)
	{
		static const TCHAR *FormatDimensions = TEXT(R"(
			void {FunctionName}(out float3 Out_Dimensions)
			{
				Out_Dimensions = {DimensionsName};
			}
		)");
		TMap<FString, FStringFormatArg> ArgsDimensions = {
			{TEXT("FunctionName"), InstanceFunctionName},
			{TEXT("DimensionsName"), DimensionsBaseName + ParamInfo.DataInterfaceHLSLSymbol}
		};
		OutHLSL += FString::Format(FormatDimensions, ArgsDimensions);
		return true;
	}
	else if (DefinitionFunctionName == GetVectorFieldBoundsName)
	{
		static const TCHAR *FormatBounds = TEXT(R"(
			void {FunctionName}(out float3 Out_MinBounds, out float3 Out_MaxBounds)
			{
				Out_MinBounds = {MinBoundsName};
				Out_MaxBounds = {MaxBoundsName};
			}
		)");
		TMap<FString, FStringFormatArg> ArgsBounds = {
			{TEXT("FunctionName"), InstanceFunctionName},
			{TEXT("MinBoundsName"), MinBoundsBaseName + ParamInfo.DataInterfaceHLSLSymbol},
			{TEXT("MaxBoundsName"), MaxBoundsBaseName + ParamInfo.DataInterfaceHLSLSymbol}
		};
		OutHLSL += FString::Format(FormatBounds, ArgsBounds);
		return true;
	}
	return false;
}

struct FNiagaraDataInterfaceParametersCS_VectorField : public FNiagaraDataInterfaceParametersCS
{
	virtual void Bind(const FNiagaraDataInterfaceParamRef& ParamRef, const class FShaderParameterMap& ParameterMap) override
	{
		VectorFieldSampler.Bind(ParameterMap, *(SamplerBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		VectorFieldTexture.Bind(ParameterMap, *(TextureBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		TilingAxes.Bind(ParameterMap, *(TilingAxesBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		Dimensions.Bind(ParameterMap, *(DimensionsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		MinBounds.Bind(ParameterMap, *(MinBoundsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
		MaxBounds.Bind(ParameterMap, *(MaxBoundsBaseName + ParamRef.ParameterInfo.DataInterfaceHLSLSymbol));
	}

	virtual void Serialize(FArchive& Ar)override
	{
		Ar << VectorFieldSampler;
		Ar << VectorFieldTexture;
		Ar << TilingAxes;
		Ar << Dimensions;
		Ar << MinBounds;
		Ar << MaxBounds;
	}

	virtual void Set(FRHICommandList& RHICmdList, FNiagaraShader* Shader, class UNiagaraDataInterface* DataInterface, void* PerInstanceData) const override
	{
		check(IsInRenderingThread());

		// Different sampler states used by the computer shader to sample 3D vector field. 
		// Encoded as bitflags. To sample: 
		//     1st bit: X-axis tiling flag
		//     2nd bit: Y-axis tiling flag
		//     3rd bit: Z-axis tiling flag
		static FSamplerStateRHIParamRef SamplerStates[8] = { nullptr };
		if (SamplerStates[0] == nullptr)
		{
			SamplerStates[0] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
			SamplerStates[1] = TStaticSamplerState<SF_Bilinear, AM_Wrap,  AM_Clamp, AM_Clamp>::GetRHI();
			SamplerStates[2] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Wrap,  AM_Clamp>::GetRHI();
			SamplerStates[3] = TStaticSamplerState<SF_Bilinear, AM_Wrap,  AM_Wrap,  AM_Clamp>::GetRHI();
			SamplerStates[4] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Wrap>::GetRHI();
			SamplerStates[5] = TStaticSamplerState<SF_Bilinear, AM_Wrap,  AM_Clamp, AM_Wrap>::GetRHI();
			SamplerStates[6] = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Wrap,  AM_Wrap>::GetRHI();
			SamplerStates[7] = TStaticSamplerState<SF_Bilinear, AM_Wrap,  AM_Wrap,  AM_Wrap>::GetRHI();
		}

		// Get shader and DI
		const FComputeShaderRHIParamRef ComputeShaderRHI = Shader->GetComputeShader();
		UNiagaraDataInterfaceVectorField* VFDI = CastChecked<UNiagaraDataInterfaceVectorField>(DataInterface);
		
		// Note: There is a flush in PreEditChange to make sure everything is synced up at this point 

		// Get and set 3D texture handle from the currently bound vector field.
		UVectorFieldStatic* StaticVectorField = Cast<UVectorFieldStatic>(VFDI->Field); 
		FRHITexture* VolumeTextureRHI = StaticVectorField ? (FRHITexture*)StaticVectorField->GetVolumeTextureRef() : (FRHITexture*)GBlackVolumeTexture->TextureRHI;
		SetTextureParameter(RHICmdList, ComputeShaderRHI, VectorFieldTexture, VolumeTextureRHI); 
		
		// Get and set sampler state
		FSamplerStateRHIParamRef SamplerState = SamplerStates[int(VFDI->bTileX) + 2 * int(VFDI->bTileY) + 4 * int(VFDI->bTileZ)];
		SetSamplerParameter(RHICmdList, ComputeShaderRHI, VectorFieldSampler, SamplerState);

		//
		SetShaderValue(RHICmdList, ComputeShaderRHI, TilingAxes, VFDI->GetTilingAxes());
		SetShaderValue(RHICmdList, ComputeShaderRHI, Dimensions, VFDI->GetDimensions());
		SetShaderValue(RHICmdList, ComputeShaderRHI, MinBounds, VFDI->GetMinBounds());
		SetShaderValue(RHICmdList, ComputeShaderRHI, MaxBounds, VFDI->GetMaxBounds());
	}

private:

	FShaderResourceParameter VectorFieldSampler;
	FShaderResourceParameter VectorFieldTexture;
	FShaderParameter TilingAxes;
	FShaderParameter Dimensions;
	FShaderParameter MinBounds;
	FShaderParameter MaxBounds;
};

FNiagaraDataInterfaceParametersCS* UNiagaraDataInterfaceVectorField::ConstructComputeParameters()const
{
	return new FNiagaraDataInterfaceParametersCS_VectorField();
}

/*--------------------------------------------------------------------------------------------------------------------------*/

void UNiagaraDataInterfaceVectorField::GetFieldTilingAxes(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<float> OutSizeX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSizeY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSizeZ(Context);

	FVector Tilings = GetTilingAxes();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutSizeX.GetDest() = Tilings.X;
		*OutSizeY.GetDest() = Tilings.Y;
		*OutSizeZ.GetDest() = Tilings.Z;

		OutSizeX.Advance();
		OutSizeY.Advance();
		OutSizeZ.Advance();
	}
}

void UNiagaraDataInterfaceVectorField::GetFieldDimensions(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<float> OutSizeX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSizeY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSizeZ(Context);

	FVector Dim = GetDimensions();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutSizeX.GetDest() = Dim.X;
		*OutSizeY.GetDest() = Dim.Y;
		*OutSizeZ.GetDest() = Dim.Z;

		OutSizeX.Advance();
		OutSizeY.Advance();
		OutSizeZ.Advance();
	}
}

void UNiagaraDataInterfaceVectorField::GetFieldBounds(FVectorVMContext& Context)
{
	VectorVM::FExternalFuncRegisterHandler<float> OutMinX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutMinY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutMinZ(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutMaxX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutMaxY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutMaxZ(Context);

	FVector MinBounds = GetMinBounds();
	FVector MaxBounds = GetMaxBounds();
	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		*OutMinX.GetDest() = MinBounds.X;
		*OutMinY.GetDest() = MinBounds.Y;
		*OutMinZ.GetDest() = MinBounds.Z;
		*OutMaxX.GetDest() = MaxBounds.X;
		*OutMaxY.GetDest() = MaxBounds.Y;
		*OutMaxZ.GetDest() = MaxBounds.Z;

		OutMinX.Advance();
		OutMinY.Advance();
		OutMinZ.Advance();
		OutMaxX.Advance();
		OutMaxY.Advance();
		OutMaxZ.Advance();
	}
}

void UNiagaraDataInterfaceVectorField::SampleVectorField(FVectorVMContext& Context)
{
	// Input arguments...
	VectorVM::FExternalFuncInputHandler<float> XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> YParam(Context);
	VectorVM::FExternalFuncInputHandler<float> ZParam(Context);

	// Outputs...
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	UVectorFieldStatic* StaticVectorField = Cast<UVectorFieldStatic>(Field);
	UVectorFieldAnimated* AnimatedVectorField = Cast<UVectorFieldAnimated>(Field);

	bool bSuccess = false;

	if (StaticVectorField != nullptr && StaticVectorField->bAllowCPUAccess)
	{
		const FVector4 TilingAxes = FVector4(bTileX ? 1.0f : 0.0f, bTileY ? 1.0f : 0.0f, bTileZ ? 1.0f : 0.0f, 0.0);

		const uint32 SizeX = (uint32)StaticVectorField->SizeX;
		const uint32 SizeY = (uint32)StaticVectorField->SizeY;
		const uint32 SizeZ = (uint32)StaticVectorField->SizeZ;
		const FVector4 Size(SizeX, SizeY, SizeZ, 1.0f);

		const FVector4 MinBounds(StaticVectorField->Bounds.Min.X, StaticVectorField->Bounds.Min.Y, StaticVectorField->Bounds.Min.Z, 0.f);
		const FVector BoundSize = StaticVectorField->Bounds.GetSize();

		const FVector4 *Data = StaticVectorField->CPUData.GetData();

		if (ensure(Data && FMath::Min3(SizeX, SizeY, SizeZ) > 0 && BoundSize.GetMin() > SMALL_NUMBER))
		{
			const FVector4 OneOverBoundSize(FVector::OneVector / BoundSize, 1.0f);

			// Math helper
			static auto FVector4Clamp = [](FVector4 v, FVector4 a, FVector4 b) {
				return FVector4(FMath::Clamp(v.X, a.X, b.X),
					FMath::Clamp(v.Y, a.Y, b.Y),
					FMath::Clamp(v.Z, a.Z, b.Z),
					FMath::Clamp(v.W, a.W, b.W));
			};

			static auto FVector4Floor = [](FVector4 v) {
				return FVector4(FGenericPlatformMath::FloorToFloat(v.X),
					FGenericPlatformMath::FloorToFloat(v.Y),
					FGenericPlatformMath::FloorToFloat(v.Z),
					FGenericPlatformMath::FloorToFloat(v.W));
			};

			for (int32 InstanceIdx = 0; InstanceIdx < Context.NumInstances; ++InstanceIdx)
			{
				// Position in Volume Space
				FVector4 Pos(XParam.Get(), YParam.Get(), ZParam.Get(), 0.0f);

				// Normalize position

				Pos = (Pos - MinBounds) * OneOverBoundSize;

				// Scaled position
				Pos = Pos * Size;

				// Offset by half a cell size due to sample being in the center of its cell
				Pos = Pos - FVector4(0.5f, 0.5f, 0.5f, 0.0f);

				// 
				FVector4 Index0 = FVector4Floor(Pos);
				FVector4 Index1 = Index0 + FVector4(1.0f, 1.0f, 1.0f, 0.0f);

				// 
				FVector4 Fraction = Pos - Index0;

				Index0 = Index0 - TilingAxes*FVector4Floor(Index0 / Size)*Size;
				Index1 = Index1 - TilingAxes*FVector4Floor(Index1 / Size)*Size;

				Index0 = FVector4Clamp(Index0, FVector4(0.0f), Size - FVector4(1.0f, 1.0f, 1.0f, 0.0f));
				Index1 = FVector4Clamp(Index1, FVector4(0.0f), Size - FVector4(1.0f, 1.0f, 1.0f, 0.0f));

				// Sample by regular trilinear interpolation:

				// TODO(mv): Optimize indexing for cache? Periodicity is problematic...
				// TODO(mv): Vectorize?
				// Fetch corners
				FVector4 V000 = Data[int(Index0.X + SizeX * Index0.Y + SizeX * SizeY * Index0.Z)];
				FVector4 V100 = Data[int(Index1.X + SizeX * Index0.Y + SizeX * SizeY * Index0.Z)];
				FVector4 V010 = Data[int(Index0.X + SizeX * Index1.Y + SizeX * SizeY * Index0.Z)];
				FVector4 V110 = Data[int(Index1.X + SizeX * Index1.Y + SizeX * SizeY * Index0.Z)];
				FVector4 V001 = Data[int(Index0.X + SizeX * Index0.Y + SizeX * SizeY * Index1.Z)];
				FVector4 V101 = Data[int(Index1.X + SizeX * Index0.Y + SizeX * SizeY * Index1.Z)];
				FVector4 V011 = Data[int(Index0.X + SizeX * Index1.Y + SizeX * SizeY * Index1.Z)];
				FVector4 V111 = Data[int(Index1.X + SizeX * Index1.Y + SizeX * SizeY * Index1.Z)];

				// Blend x-axis
				FVector4 V00 = FMath::Lerp(V000, V100, Fraction.X);
				FVector4 V01 = FMath::Lerp(V001, V101, Fraction.X);
				FVector4 V10 = FMath::Lerp(V010, V110, Fraction.X);
				FVector4 V11 = FMath::Lerp(V011, V111, Fraction.X);

				// Blend y-axis
				FVector4 V0 = FMath::Lerp(V00, V10, Fraction.Y);
				FVector4 V1 = FMath::Lerp(V01, V11, Fraction.Y);

				// Blend z-axis
				FVector4 V = FMath::Lerp(V0, V1, Fraction.Z);

				// Write final output...
				*OutSampleX.GetDest() = V.X;
				*OutSampleY.GetDest() = V.Y;
				*OutSampleZ.GetDest() = V.Z;

				XParam.Advance();
				YParam.Advance();
				ZParam.Advance();
				OutSampleX.Advance();
				OutSampleY.Advance();
				OutSampleZ.Advance();
			}

			bSuccess = true;
		}
	}

	if (!bSuccess)
	{
		// TODO(mv): Add warnings?
		if (StaticVectorField != nullptr && !StaticVectorField->bAllowCPUAccess)
		{
			// No access to static vector data
		}
		else if (AnimatedVectorField != nullptr)
		{
			// Animated vector field not supported
		}
		else if (Field == nullptr)
		{
			// Vector field not loaded
		}

		// Set the default vector to positive X axis corresponding to a velocity of 100 cm/s
		// Rationale: Setting to the zero vector can be visually confusing and likely to cause problems elsewhere
		for (int32 InstanceIdx = 0; InstanceIdx < Context.NumInstances; ++InstanceIdx)
		{
			*OutSampleX.GetDest() = 0.0f;
			*OutSampleY.GetDest() = 0.0f;
			*OutSampleZ.GetDest() = 0.0f;

			XParam.Advance();
			YParam.Advance();
			ZParam.Advance();	
			OutSampleX.Advance();
			OutSampleY.Advance();
			OutSampleZ.Advance();
		}
	}

}

/*--------------------------------------------------------------------------------------------------------------------------*/

FVector UNiagaraDataInterfaceVectorField::GetTilingAxes() const
{
	return FVector(float(bTileX), float(bTileY), float(bTileZ));
}

FVector UNiagaraDataInterfaceVectorField::GetDimensions() const
{
	UVectorFieldStatic* StaticVectorField = Cast<UVectorFieldStatic>(Field);
	if (StaticVectorField)
	{
		return FVector(StaticVectorField->SizeX, StaticVectorField->SizeY, StaticVectorField->SizeZ);
	}
	return FVector{ 1.0f, 1.0f, 1.0f }; // Matches GBlackVolumeTexture
}

FVector UNiagaraDataInterfaceVectorField::GetMinBounds() const
{
	UVectorFieldStatic* StaticVectorField = Cast<UVectorFieldStatic>(Field);
	if (StaticVectorField)
	{
		return StaticVectorField->Bounds.Min;
	}
	return FVector{-1.0f, -1.0f, -1.0f};
}

FVector UNiagaraDataInterfaceVectorField::GetMaxBounds() const
{
	UVectorFieldStatic* StaticVectorField = Cast<UVectorFieldStatic>(Field);
	if (StaticVectorField)
	{
		return StaticVectorField->Bounds.Max;
	}
	return FVector{1.0f, 1.0f, 1.0f};
}

/*--------------------------------------------------------------------------------------------------------------------------*/

bool UNiagaraDataInterfaceVectorField::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceVectorField* OtherTyped = CastChecked<UNiagaraDataInterfaceVectorField>(Destination);
	OtherTyped->Field = Field;
	OtherTyped->bTileX = bTileX;
	OtherTyped->bTileY = bTileY;
	OtherTyped->bTileZ = bTileZ;
	return true;
}

#undef LOCTEXT_NAMESPACE
