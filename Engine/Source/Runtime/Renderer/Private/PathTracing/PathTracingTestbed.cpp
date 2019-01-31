// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PathTracingTestbed.h"
#include "RHI.h"

#if RHI_RAYTRACING

#include "Containers/DynamicRHIResourceArray.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/SceneFilterRendering.h"

#include "RendererPrivate.h"

class FTestBrdfIntegrityCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FTestBrdfIntegrityCS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	FTestBrdfIntegrityCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		SamplesCountParameter.Bind(Initializer.ParameterMap, TEXT("SamplesCount"));
		BrdfTypeParameter.Bind(Initializer.ParameterMap, TEXT("BrdfType"));
		ResultsBufferParameter.Bind(Initializer.ParameterMap, TEXT("ResultsBuffer"));
		FloatResultsBufferParameter.Bind(Initializer.ParameterMap, TEXT("FloatResultsBuffer"));
	}

	FTestBrdfIntegrityCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		uint32 SamplesCount, 
		uint32 BrdfType,
		FUnorderedAccessViewRHIParamRef ResultsBuffer,
		FUnorderedAccessViewRHIParamRef FloatResultsBuffer)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ShaderRHI, SamplesCountParameter, SamplesCount);
		SetShaderValue(RHICmdList, ShaderRHI, BrdfTypeParameter, BrdfType);
		SetUAVParameter(RHICmdList, ShaderRHI, ResultsBufferParameter, ResultsBuffer);
		SetUAVParameter(RHICmdList, ShaderRHI, FloatResultsBufferParameter, FloatResultsBuffer);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& ResultsBuffer,
		FRWBuffer& FloatResultsBuffer,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, ResultsBuffer.UAV, Fence);
		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, FloatResultsBuffer.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << SamplesCountParameter;
		Ar << BrdfTypeParameter;
		Ar << ResultsBufferParameter;
		Ar << FloatResultsBufferParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter SamplesCountParameter;
	FShaderParameter BrdfTypeParameter;
	FShaderResourceParameter ResultsBufferParameter;
	FShaderResourceParameter FloatResultsBufferParameter;
};

IMPLEMENT_SHADER_TYPE(, FTestBrdfIntegrityCS, TEXT("/Engine/Private/PathTracing/Material/PathTracingTestBrdfs.usf"), TEXT("TestBrdfIntegrityCS"), SF_Compute)



class FTestPdfIntegratesToOneCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FTestPdfIntegratesToOneCS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	FTestPdfIntegratesToOneCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BrdfTypeParameter.Bind(Initializer.ParameterMap, TEXT("BrdfType"));
		WoParameter.Bind(Initializer.ParameterMap, TEXT("Wo"));
		NumThetaStepsParameter.Bind(Initializer.ParameterMap, TEXT("NumThetaSteps"));
		NumPhiStepsParameter.Bind(Initializer.ParameterMap, TEXT("NumPhiSteps"));
		PdfResultsBufferParameter.Bind(Initializer.ParameterMap, TEXT("PdfsResultsBuffer"));
	}

	FTestPdfIntegratesToOneCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		uint32 BrdfType,
		FVector Wo,
		uint32 NumThetaSteps,
		uint32 NumPhiSteps,
		FUnorderedAccessViewRHIParamRef PdfsResultsBuffer)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ShaderRHI, BrdfTypeParameter, BrdfType);
		SetShaderValue(RHICmdList, ShaderRHI, WoParameter, Wo);
		SetShaderValue(RHICmdList, ShaderRHI, NumThetaStepsParameter, NumThetaSteps);
		SetShaderValue(RHICmdList, ShaderRHI, NumPhiStepsParameter, NumPhiSteps);
		SetUAVParameter(RHICmdList, ShaderRHI, PdfResultsBufferParameter, PdfsResultsBuffer);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& PdfsResultsBuffer,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, PdfsResultsBuffer.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BrdfTypeParameter;
		Ar << WoParameter;
		Ar << NumThetaStepsParameter;
		Ar << NumPhiStepsParameter;
		Ar << PdfResultsBufferParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter BrdfTypeParameter;
	FShaderParameter WoParameter;
	FShaderParameter NumThetaStepsParameter;
	FShaderParameter NumPhiStepsParameter;
	FShaderResourceParameter PdfResultsBufferParameter;
};

IMPLEMENT_SHADER_TYPE(, FTestPdfIntegratesToOneCS, TEXT("/Engine/Private/PathTracing/Material/PathTracingTestPdfIntegration.usf"), TEXT("TestPDFIntegratesToOneCS"), SF_Compute)



class FTestBrdfGenerateWiSamplesCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FTestBrdfGenerateWiSamplesCS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	FTestBrdfGenerateWiSamplesCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BrdfTypeParameter.Bind(Initializer.ParameterMap, TEXT("BrdfType"));
		NumSamplesParameter.Bind(Initializer.ParameterMap, TEXT("NumSamples"));
		WoParameter.Bind(Initializer.ParameterMap, TEXT("Wo"));
		NumThetaStepsParameter.Bind(Initializer.ParameterMap, TEXT("NumThetaSteps"));
		NumPhiStepsParameter.Bind(Initializer.ParameterMap, TEXT("NumPhiSteps"));
		WisBinDistributionResultsParameter.Bind(Initializer.ParameterMap, TEXT("WisBinDistributionResultsBuffer"));
	}

	FTestBrdfGenerateWiSamplesCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		uint32 BrdfType,
		uint32 NumSamples,
		FVector Wo,
		uint32 NumThetaSteps,
		uint32 NumPhiSteps,
		FUnorderedAccessViewRHIParamRef WisBinDistributionResultsBuffer)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ShaderRHI, BrdfTypeParameter, BrdfType);
		SetShaderValue(RHICmdList, ShaderRHI, NumSamplesParameter, NumSamples);
		SetShaderValue(RHICmdList, ShaderRHI, WoParameter, Wo);
		SetShaderValue(RHICmdList, ShaderRHI, NumThetaStepsParameter, NumThetaSteps);
		SetShaderValue(RHICmdList, ShaderRHI, NumPhiStepsParameter, NumPhiSteps);
		SetUAVParameter(RHICmdList, ShaderRHI, WisBinDistributionResultsParameter, WisBinDistributionResultsBuffer);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& PdfsResultsBuffer,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, PdfsResultsBuffer.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BrdfTypeParameter;
		Ar << NumSamplesParameter;
		Ar << WoParameter;
		Ar << NumThetaStepsParameter;
		Ar << NumPhiStepsParameter;
		Ar << WisBinDistributionResultsParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter BrdfTypeParameter;
	FShaderParameter NumSamplesParameter;
	FShaderParameter WoParameter;
	FShaderParameter NumThetaStepsParameter;
	FShaderParameter NumPhiStepsParameter;
	FShaderResourceParameter WisBinDistributionResultsParameter;
};

IMPLEMENT_SHADER_TYPE(, FTestBrdfGenerateWiSamplesCS, TEXT("/Engine/Private/PathTracing/Material/PathTracingTestGenerateWiSamples.usf"), TEXT("TestGenerateWiSamplesCS"), SF_Compute)



class FTestBrdfIntegrateHemispherePatchCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FTestBrdfIntegrateHemispherePatchCS, Global);

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return ShouldCompileRayTracingShadersForProject(Parameters.Platform);
	}

	static uint32 GetGroupSize()
	{
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}

	FTestBrdfIntegrateHemispherePatchCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		BrdfTypeParameter.Bind(Initializer.ParameterMap, TEXT("BrdfType"));
		WoParameter.Bind(Initializer.ParameterMap, TEXT("Wo"));
		NumThetaStepsParameter.Bind(Initializer.ParameterMap, TEXT("NumThetaSteps"));
		NumPhiStepsParameter.Bind(Initializer.ParameterMap, TEXT("NumPhiSteps"));
		WisBinDistributionResultsParameter.Bind(Initializer.ParameterMap, TEXT("PdfIntegralsBins"));
	}

	FTestBrdfIntegrateHemispherePatchCS()
	{
	}

	void SetParameters(
		FRHICommandList& RHICmdList,
		uint32 BrdfType,
		FVector Wo,
		uint32 NumThetaSteps,
		uint32 NumPhiSteps,
		FUnorderedAccessViewRHIParamRef PdfsResultsBuffer)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();
		SetShaderValue(RHICmdList, ShaderRHI, BrdfTypeParameter, BrdfType);
		SetShaderValue(RHICmdList, ShaderRHI, WoParameter, Wo);
		SetShaderValue(RHICmdList, ShaderRHI, NumThetaStepsParameter, NumThetaSteps);
		SetShaderValue(RHICmdList, ShaderRHI, NumPhiStepsParameter, NumPhiSteps);
		SetUAVParameter(RHICmdList, ShaderRHI, WisBinDistributionResultsParameter, PdfsResultsBuffer);
	}

	void UnsetParameters(
		FRHICommandList& RHICmdList,
		EResourceTransitionAccess TransitionAccess,
		EResourceTransitionPipeline TransitionPipeline,
		FRWBuffer& PdfsResultsBuffer,
		FComputeFenceRHIParamRef Fence)
	{
		FComputeShaderRHIParamRef ShaderRHI = GetComputeShader();

		RHICmdList.TransitionResource(TransitionAccess, TransitionPipeline, PdfsResultsBuffer.UAV, Fence);
	}

	virtual bool Serialize(FArchive& Ar)
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << BrdfTypeParameter;
		Ar << WoParameter;
		Ar << NumThetaStepsParameter;
		Ar << NumPhiStepsParameter;
		Ar << WisBinDistributionResultsParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter BrdfTypeParameter;
	FShaderParameter WoParameter;
	FShaderParameter NumThetaStepsParameter;
	FShaderParameter NumPhiStepsParameter;
	FShaderResourceParameter WisBinDistributionResultsParameter;
};

IMPLEMENT_SHADER_TYPE(, FTestBrdfIntegrateHemispherePatchCS, TEXT("/Engine/Private/PathTracing/Material/PathTracingTestIntegrateHemispherePatch.usf"), TEXT("TestIntegrateHemispherePatchCS"), SF_Compute)


TArray<FString> GetBrdfNames(void)
{
	// Order should match BrdfType parameter in TestBrdfIntegrityCS until there is a common definition
	//#dxr_todo: add support for testing different parameters for each BRDF
	TArray<FString> BrdfNames;
	BrdfNames.Add("Lambert");
	BrdfNames.Add("Glossy GGX");

	return (BrdfNames);
}

void TestPathTracingMaterials(void)
{
	TestBRDFsIntegrity();
	TestPDFsIntegrateToOne();
	TestBRDFandPDFConsistency();
}

void TestBRDFsIntegrity(void)
{
	// This function checks the following:
	// - The BRDF returned by Sample() and Eval() are the same and always positive
	// - The PDF returned by Sample() and Pdf() are the same and always positive
	// - The BRDF is symetric: BRDF(wo,wi) == BRDF(wi,wo)
	// Note: this test is not meant to be used with BRDFs/PDFs that contain delta terms such as pure specular/transmissive

	uint32 NumSamples = 1024;
	uint32 NumTests = 7; 
	TArray<FString> BrdfNames = GetBrdfNames(); 

	enum BrdfTests// Should match TestBrdfIntegrityCS until there is a common definition
	{
		TEST_NEGATIVE_BRDF_INDEX					= 0,
		TEST_SAMPLED_VS_EVAL_BRDF_MISMATCH_INDEX	= 1,
		TEST_NEGATIVE_PDF_INDEX						= 2,
		TEST_SAMPLED_VS_PDF_MISMATCH_INDEX			= 3,
		TEST_SAMPLED_VS_EVAL_PDF_MISMATCH_INDEX		= 4,
		TEST_NON_SYMMETRIC_BRDF						= 5,
		TEST_BRDF_AND_PDF_OK						= 6,
		TEST_COUNT									= 7,
	};


	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	const auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FTestBrdfIntegrityCS> BrdfIntegrityComputeShader(ShaderMap);
	RHICmdList.SetComputeShader(BrdfIntegrityComputeShader->GetComputeShader());

	for (uint32 BrdfType = 0; BrdfType < (uint32) BrdfNames.Num(); ++BrdfType)
	{
		//#dxr_todo: check thread safety of UE_LOG() when tests are moved to the EngineTest framework (safety depends because FOutputDevice can overload CanBeUsedOnAnyThread() )
		UE_LOG(LogShaders, Display, TEXT("Executed validation test for BRDF: %s"), *BrdfNames[BrdfType]);

		FRWBufferStructured BrdsResultsBuffer;
		BrdsResultsBuffer.Initialize(sizeof(uint32), TEST_COUNT, BUF_Static);

		FRWBufferStructured FloatBrdsResultsBuffer; // For testing
		FloatBrdsResultsBuffer.Initialize(sizeof(float), TEST_COUNT, BUF_Static);

		BrdfIntegrityComputeShader->SetParameters(RHICmdList, NumSamples, BrdfType, BrdsResultsBuffer.UAV, FloatBrdsResultsBuffer.UAV);
		
		uint32 NumCSGroups = FMath::DivideAndRoundUp(NumSamples, FTestBrdfIntegrityCS::GetGroupSize());
		DispatchComputeShader(RHICmdList, *BrdfIntegrityComputeShader, NumCSGroups, 1, 1);

		GDynamicRHI->RHISubmitCommandsAndFlushGPU();
		GDynamicRHI->RHIBlockUntilGPUIdle();

		uint32* BrdsResultsBufferOutput = (uint32*)RHILockStructuredBuffer(BrdsResultsBuffer.Buffer, 0, BrdsResultsBuffer.Buffer->GetSize(), RLM_ReadOnly);
		uint32 NumNegativeBrdfs			= BrdsResultsBufferOutput[TEST_NEGATIVE_BRDF_INDEX];
		uint32 NumMismatchedBrdfs		= BrdsResultsBufferOutput[TEST_SAMPLED_VS_EVAL_BRDF_MISMATCH_INDEX];
		uint32 NumNegativePdfs			= BrdsResultsBufferOutput[TEST_NEGATIVE_PDF_INDEX];
		uint32 NumMismatchedPdfs		= BrdsResultsBufferOutput[TEST_SAMPLED_VS_PDF_MISMATCH_INDEX];
		uint32 NumMismatchedSampledPdfs = BrdsResultsBufferOutput[TEST_SAMPLED_VS_EVAL_PDF_MISMATCH_INDEX];
		uint32 NumNonSymmetricBrdfs		= BrdsResultsBufferOutput[TEST_NON_SYMMETRIC_BRDF];
		uint32 NumGoodBrdfs				= BrdsResultsBufferOutput[TEST_BRDF_AND_PDF_OK];
		RHIUnlockStructuredBuffer(BrdsResultsBuffer.Buffer);

		//float* FloatBrdsResultsBufferOutput = (float*)RHILockStructuredBuffer(FloatBrdsResultsBuffer.Buffer, 0, FloatBrdsResultsBuffer.Buffer->GetSize(), RLM_ReadOnly);
		//RHIUnlockStructuredBuffer(FloatBrdsResultsBuffer.Buffer);

		UE_LOG(LogShaders, Display, TEXT("Samples: %d"), NumSamples);
		if (NumGoodBrdfs == NumSamples)
		{
			UE_LOG(LogShaders, Display, TEXT("All samples passed the tests."));
		}
		else
		{
			UE_LOG(LogShaders, Error, TEXT("Some samples did not pass all the tests. Num. invalid samples: %d."), NumSamples - NumGoodBrdfs);
		}		
		
		if (NumNegativeBrdfs > 0)
		{
			UE_LOG(LogShaders, Error, TEXT("Num. negative BRDFs: %d."), NumNegativeBrdfs);
		}

		if (NumMismatchedBrdfs > 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("Some BRDFs are significantly different when computed through Sample and Eval methods. Num. mismatches: %d."), NumMismatchedBrdfs);
		}

		if (NumNegativePdfs > 0)
		{
			UE_LOG(LogShaders, Error, TEXT("Some PDFs are negative. Num. negative PDFs: %d."), NumNegativePdfs);
		}

		if (NumMismatchedPdfs > 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("Some PDFs are significantly different when computed through Sample and Eval methods. Num. mismatches: %d."), NumMismatchedPdfs);
		}

		if (NumMismatchedSampledPdfs > 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("Some PDFs are significantly different when computed through Pdf and Eval methods. Num. mismatches: %d."), NumMismatchedSampledPdfs);
		}

		if (NumNonSymmetricBrdfs > 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("Num. non symmetric BRDFs: %d."), NumNonSymmetricBrdfs);
		}
	}
}

void TestPDFsIntegrateToOne(void)
{
	// This function checks the PDF integrates to 1 in a hemisphere for a given set of input directions
	// A non zero amount of errors is acceptable due precision issues or insufficient amount of samples
	
	float ValidPdfThreshold = 0.05; // Could be increased if N samples is higher
	uint32 NumTests = 10;
	TArray<FString> BrdfNames = GetBrdfNames();

	// Integrate with constant solid angle in a hemisphere
	// Note: if segments are too small float precision is not enough
	uint32 NumThetaSteps = 128;
	uint32 NumPhiSteps = 128;
	uint32 NSamples = NumThetaSteps * NumPhiSteps;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	const auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FTestPdfIntegratesToOneCS> PdfIntegrateToOneCS(ShaderMap);
	RHICmdList.SetComputeShader(PdfIntegrateToOneCS->GetComputeShader());

	TArray<FVector> Wos;
	FRandomStream RandomStream(0);
	for (uint32 i = 0; i < NumTests; ++i)
	{
		const float U1 = RandomStream.GetFraction();
		const float U2 = RandomStream.GetFraction();

		double Z = U1;
		float r = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
		float phi = 2.0f * (float)PI * U2;
		FVector Wo(r * FMath::Cos(phi), r * FMath::Sin(phi), Z);
		Wos.Add(Wo);
	}

	for (uint32 BrdfType = 0; BrdfType < (uint32)BrdfNames.Num(); ++BrdfType)
	{
		uint32 Nfails = 0;

		FIntVector Dimensions = FIntVector(NumPhiSteps, NumThetaSteps, 1);
		FIntVector NumCSGroups = FIntVector::DivideAndRoundUp(Dimensions, FTestPdfIntegratesToOneCS::GetGroupSize());
		
		FRWBufferStructured PdfsResultsBuffer; 
		PdfsResultsBuffer.Initialize(sizeof(float), NSamples, BUF_Static);

		for (FVector Wo : Wos)
		{
			PdfIntegrateToOneCS->SetParameters(RHICmdList, BrdfType, Wo, NumThetaSteps, NumPhiSteps, PdfsResultsBuffer.UAV);

			DispatchComputeShader(RHICmdList, *PdfIntegrateToOneCS, NumCSGroups.X, NumCSGroups.Y, 1);

			GDynamicRHI->RHISubmitCommandsAndFlushGPU();
			GDynamicRHI->RHIBlockUntilGPUIdle();

			float* PdfResults = (float*)RHILockStructuredBuffer(PdfsResultsBuffer.Buffer, 0, PdfsResultsBuffer.Buffer->GetSize(), RLM_ReadOnly);

			float PdfAccum = 0.0f;
			for (uint32 PdfIt = 0; PdfIt < NSamples; ++PdfIt)
			{
				PdfAccum += PdfResults[PdfIt];
			}

			float IntegralRes = PdfAccum / NSamples;
			bool bPdfOK = fabsf(IntegralRes - 1.0f) < ValidPdfThreshold;
			if (!bPdfOK)
			{
				Nfails++;
			}

			RHIUnlockStructuredBuffer(PdfsResultsBuffer.Buffer);
		}

		UE_LOG(LogShaders, Display, TEXT("Executed PDF integration test for BRDF: %s."), *BrdfNames[BrdfType]);
		UE_LOG(LogShaders, Display, TEXT("Num. incoming direction tested: %d."), Wos.Num());

		if (Nfails > 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("PDF for BRDF: %s does not integrate to 1 in %d tests."), *BrdfNames[BrdfType], Nfails);
		}
		else
		{
			UE_LOG(LogShaders, Display, TEXT("PDF for BRDF: %s integrates to 1 in all cases."), *BrdfNames[BrdfType]);
		}
	}  	
}

void TestBRDFandPDFConsistency(void)
{
	// This function checks that the directions returned by the Sample() method
	// are consistent with the distribution of output directions given by Pdf()

	float ValidConsistencyThreshold = 0.05;
	float ValidPdfThreshold = 0.05; 
	TArray<FString> BrdfNames = GetBrdfNames();

	uint32 WosCount = 10;
	TArray<FVector> Wos;
	FRandomStream RandomStream(0);
	for (uint32 i = 0; i < WosCount; ++i)
	{
		//#dxr_todo: use cos weight instead of uniform for this test
		const float U1 = RandomStream.GetFraction();
		const float U2 = RandomStream.GetFraction();

		double Z = U1;
		float r = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
		float phi = 2.0f * (float)PI * U2;
		FVector Wo(r * FMath::Cos(phi), r * FMath::Sin(phi), Z);
		Wos.Add(Wo);
	}

	uint32 NumThetaSteps = 128;
	uint32 NumPhiSteps = 128;
	uint32 WiSamplesCount = 10000;

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	const auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	
	TShaderMapRef<FTestBrdfGenerateWiSamplesCS> BrdfGenerateWiSamplesCS(ShaderMap);
	RHICmdList.SetComputeShader(BrdfGenerateWiSamplesCS->GetComputeShader());

	TShaderMapRef<FTestBrdfIntegrateHemispherePatchCS> IntegrateHemispherePatchCS(ShaderMap);
	RHICmdList.SetComputeShader(IntegrateHemispherePatchCS->GetComputeShader());

	for (uint32 BrdfType = 0; BrdfType < (uint32)BrdfNames.Num(); ++BrdfType)
	{
		uint32 ForbiddenSamplesFails = 0;
		uint32 BinMismatches = 0;

		for (FVector Wo : Wos)
		{
			TArray<float> BinSampledDistribution;
			TArray<float> PdfPatchesDistribution;

			BinSampledDistribution.Init(0.0, NumThetaSteps*NumPhiSteps);
			PdfPatchesDistribution.Init(0.0, NumThetaSteps*NumPhiSteps);

			// Generate samples calling BRDF::Sample() and classify them in bins
			{
				FRWBufferStructured SampledWisResultsBuffer; 
				SampledWisResultsBuffer.Initialize(sizeof(float)*3, WiSamplesCount, BUF_Static);

				BrdfGenerateWiSamplesCS->SetParameters(RHICmdList, BrdfType, WiSamplesCount, Wo, NumThetaSteps, NumPhiSteps, SampledWisResultsBuffer.UAV);

				uint32 NumCSGroupsSamplesCS = FMath::DivideAndRoundUp(WiSamplesCount, FTestBrdfGenerateWiSamplesCS::GetGroupSize());
				DispatchComputeShader(RHICmdList, *BrdfGenerateWiSamplesCS, NumCSGroupsSamplesCS, 1, 1);

				GDynamicRHI->RHISubmitCommandsAndFlushGPU();
				GDynamicRHI->RHIBlockUntilGPUIdle();

				float* SampledWosResults = (float*)RHILockStructuredBuffer(SampledWisResultsBuffer.Buffer, 0, SampledWisResultsBuffer.Buffer->GetSize(), RLM_ReadOnly);
				for (uint32 SamplesIt = 0; SamplesIt < WiSamplesCount; SamplesIt++)
				{
					FVector Wi(SampledWosResults[3*SamplesIt + 0], SampledWosResults[3 * SamplesIt + 1], SampledWosResults[3 * SamplesIt + 2]);
				
					// Sample with constant CosTheta instead constant Theta
					// to divide the hemisphere in patches of equal area
					float ThetaLength = 1.0 / NumThetaSteps; //Hemisphere only so far
					float PhiLength = 2 * PI / NumPhiSteps;

					// Patches are not equally distributed in theta but in cos(theta) so they all have the same area
					float CosTheta = FMath::Clamp(Wi.Z, 0.0f, 0.9999f);
					uint32 InversedBin = FMath::FloorToInt(CosTheta / ThetaLength);
					uint32 WiThetaBin = NumThetaSteps - InversedBin - 1;

					float WiPhi = atan2(Wi.Y, Wi.X);
					if (WiPhi < 0.0)
					{
						WiPhi = 2.0 * PI + WiPhi;
					}

					uint32 WiPhiBin = FMath::FloorToInt(WiPhi / PhiLength);

					BinSampledDistribution[WiThetaBin * NumPhiSteps + WiPhiBin] += 1;
				}

				RHIUnlockStructuredBuffer(SampledWisResultsBuffer.Buffer);
			}

			//	Integrate PDF in each patch
			{
				FRWBufferStructured PdfPatchesIntegrationResultsBuffer;
				PdfPatchesIntegrationResultsBuffer.Initialize(sizeof(float), NumThetaSteps * NumPhiSteps, BUF_Static);

				IntegrateHemispherePatchCS->SetParameters(RHICmdList, BrdfType, Wo, NumThetaSteps, NumPhiSteps, PdfPatchesIntegrationResultsBuffer.UAV);

				FIntVector HemispherePatchCSDimensions = FIntVector(NumPhiSteps, NumThetaSteps, 1);
				FIntVector HemispherePatchCSNumGroups = FIntVector::DivideAndRoundUp(HemispherePatchCSDimensions, FTestBrdfIntegrateHemispherePatchCS::GetGroupSize());

				DispatchComputeShader(RHICmdList, *IntegrateHemispherePatchCS, HemispherePatchCSNumGroups.X, HemispherePatchCSNumGroups.Y, 1);

				GDynamicRHI->RHISubmitCommandsAndFlushGPU();
				GDynamicRHI->RHIBlockUntilGPUIdle();

				// Watch the watchman: check the hemisphere returns 1, otherwise the PDF integration routine is wrong
				float HemisphereIntegral = 0.0;
				float* PatchesIntegrationResults = (float*)RHILockStructuredBuffer(PdfPatchesIntegrationResultsBuffer.Buffer, 0, PdfPatchesIntegrationResultsBuffer.Buffer->GetSize(), RLM_ReadOnly);
				for (uint32 SamplesIt = 0; SamplesIt < NumThetaSteps*NumPhiSteps; ++SamplesIt)
				{
					float PatchIntegral = PatchesIntegrationResults[SamplesIt];
					PdfPatchesDistribution.Add(PatchIntegral);
					HemisphereIntegral += PatchIntegral;
				}

				if (FMath::Abs(HemisphereIntegral - 1.0f) > ValidPdfThreshold)
				{
					UE_LOG(LogShaders, Warning, TEXT("The sum of the integral of the PDF for BRDF: %s for all the patches is different to 1."), *BrdfNames[BrdfType]);
				}
				else
				{
					UE_LOG(LogShaders, Display, TEXT("The sum of the integral of the PDF for BRDF: %s for all the patches is close enough to 1."), *BrdfNames[BrdfType]);
				}
			}

			//	// Compare count in each bin with expected count
			for (uint32 iBin = 0; iBin < NumThetaSteps*NumPhiSteps; iBin++)
			{
				float SampledPercent = BinSampledDistribution[iBin] * 100.0 / WiSamplesCount;
				float ExpectedPercent = PdfPatchesDistribution[iBin] * 100.0;

				if (ExpectedPercent > 0)
				{
					uint32 diff = fabsf(SampledPercent - ExpectedPercent);
					float Err = (float)diff;
					if (Err > 20.0) //#dxr_todo: adjust threshold
					{
						BinMismatches++;
					}
				}
				else
				{
					if (SampledPercent > 0.01*WiSamplesCount)
					{
						// Samples in a bin where there should not be any
						ForbiddenSamplesFails++;
					}
				}
			}

		} // End Wo

		UE_LOG(LogShaders, Display, TEXT("Executed BRDF vs PDF consistency test for BRDF: %s."), *BrdfNames[BrdfType]);
		UE_LOG(LogShaders, Display, TEXT("Num. incoming direction tested: %d."), Wos.Num());


		float TestedBinsCount = (float)WosCount * NumThetaSteps * NumPhiSteps;
		float MismatchesRatio = (float)(BinMismatches / TestedBinsCount);
		
		if (ForbiddenSamplesFails > 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("BRDF: %s has generated invalid samples in %d tests."), *BrdfNames[BrdfType], ForbiddenSamplesFails);
		}
		else
		{
			UE_LOG(LogShaders, Display, TEXT("All samples generated by BRDF: %s are valid"), *BrdfNames[BrdfType]);
		}

		if (MismatchesRatio > ValidConsistencyThreshold)
		{
			UE_LOG(LogShaders, Warning, TEXT("BRDF: %s has generated too many samples inconsistent with the distribution given by the PDF. Mismatches ratio: %f."), *BrdfNames[BrdfType], MismatchesRatio);
		}
		else
		{
			UE_LOG(LogShaders, Display, TEXT("All samples generated by BRDF: %s are valid"), *BrdfNames[BrdfType]);
		}

	} // End traversing Brdfs	


	//	// Check the sum of all patches is equal to the integration of the whole domain at hone (and equal to 1)
	//	if (abs(HemisphereIntegral - HemisphereIntegralAccum) > ValidIntegralThreshold)
	//	{
	//		PatchIntegrationFails++;
	//	}

	//}

	//// Watch the watchman: check the hemisphere returns 1, otherwise the PDF integration routine is wrong
	//Vector3D woFixed(normalized(Vector3D(1.0, 1.0, 1.0)));
	//float HemisphereIntegral = IntegratePdfInSphericalPatch(0, 0, PI / 2.0, 2.0*PI, 2 * PI, woFixed, (BxDF*)(this));
	//if (abs(HemisphereIntegral - 1.0) > ValidIntegralThreshold)
	//{
	//	return false;
	//}
}

#else // RHI_RAYTRACING

void TestPathTracingMaterials(void)
{
	// Nothing to do when ray tracing is disabled
}

void TestBRDFsIntegrity(void)
{
	// Nothing to do when ray tracing is disabled
}

void TestPDFsIntegrateToOne(void)
{
	// Nothing to do when ray tracing is disabled
}

void TestBRDFandPDFConsistency(void)
{
	// Nothing to do when ray tracing is disabled
}

#endif // RHI_RAYTRACING
