// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ByteBuffer.h"
#include "RHI.h"
#include "ShaderParameters.h"
#include "Shader.h"
#include "ShaderParameterUtils.h"
#include "GlobalShader.h"

class FMemsetBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMemsetBufferCS,Global)
	
	FMemsetBufferCS() {}
	
	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return /*IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM5 )*/
			Parameters.Platform == SP_PS4 || Parameters.Platform == SP_PCD3D_SM5 || Parameters.Platform == SP_XBOXONE_D3D12;
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREADGROUP_SIZE"), ThreadGroupSize );
	}

public:
	enum { ThreadGroupSize = 64 };

	FShaderParameter			Value;
	FShaderParameter			Size;
	FShaderParameter			DstOffset;
	FShaderResourceParameter	DstBuffer;

	FMemsetBufferCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		Value.Bind(		Initializer.ParameterMap, TEXT("Value") );
		Size.Bind(		Initializer.ParameterMap, TEXT("Size") );
		DstOffset.Bind(	Initializer.ParameterMap, TEXT("DstOffset") );
		DstBuffer.Bind(	Initializer.ParameterMap, TEXT("DstBuffer") );
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Value;
		Ar << Size;
		Ar << DstOffset;
		Ar << DstBuffer;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FMemsetBufferCS, TEXT("/Engine/Private/ByteBuffer.usf"), TEXT("MemsetBufferCS"), SF_Compute );


// Must be aligned to 4 bytes
void MemsetBuffer( FRHICommandList& RHICmdList, const FRWByteAddressBuffer& DstBuffer, uint32 Value, uint32 NumBytes, uint32 DstOffset )
{
	check( (NumBytes & 3) == 0 );
	check( (DstOffset & 3) == 0 );

	auto ShaderMap = GetGlobalShaderMap( GMaxRHIFeatureLevel );

	TShaderMapRef< FMemsetBufferCS > ComputeShader( ShaderMap );

	const FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
	RHICmdList.SetComputeShader( ShaderRHI );

	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->Value, Value );
	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->Size, NumBytes / 4 );
	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->DstOffset, DstOffset / 4 );
	SetUAVParameter( RHICmdList, ShaderRHI, ComputeShader->DstBuffer, DstBuffer.UAV );

	RHICmdList.DispatchComputeShader( FMath::DivideAndRoundUp< uint32 >( NumBytes / 4, FMemsetBufferCS::ThreadGroupSize * 4 ), 1, 1 );

	SetUAVParameter( RHICmdList, ShaderRHI, ComputeShader->DstBuffer, FUnorderedAccessViewRHIRef() );
}


class FMemcpyBufferCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FMemcpyBufferCS,Global)
	
	FMemcpyBufferCS() {}
	
	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return /*IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM5 )*/
			Parameters.Platform == SP_PS4 || Parameters.Platform == SP_PCD3D_SM5 || Parameters.Platform == SP_XBOXONE_D3D12;
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREADGROUP_SIZE"), ThreadGroupSize );
	}

public:
	enum { ThreadGroupSize = 64 };

	FShaderParameter			Size;
	FShaderParameter			SrcOffset;
	FShaderParameter			DstOffset;
	FShaderResourceParameter	SrcBuffer;
	FShaderResourceParameter	DstBuffer;

	FMemcpyBufferCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		Size.Bind(		Initializer.ParameterMap, TEXT("Size") );
		SrcOffset.Bind(	Initializer.ParameterMap, TEXT("SrcOffset") );
		DstOffset.Bind(	Initializer.ParameterMap, TEXT("DstOffset") );
		SrcBuffer.Bind(	Initializer.ParameterMap, TEXT("SrcBuffer") );
		DstBuffer.Bind(	Initializer.ParameterMap, TEXT("DstBuffer") );
	}

	virtual bool Serialize(FArchive& Ar) override
	{		
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << Size;
		Ar << SrcOffset;
		Ar << DstOffset;
		Ar << SrcBuffer;
		Ar << DstBuffer;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FMemcpyBufferCS, TEXT("/Engine/Private/ByteBuffer.usf"), TEXT("MemcpyBufferCS"), SF_Compute );


// Must be aligned to 4 bytes
void MemcpyBuffer( FRHICommandList& RHICmdList, const FRWByteAddressBuffer& DstBuffer, const FRWByteAddressBuffer& SrcBuffer, uint32 NumBytes, uint32 DstOffset, uint32 SrcOffset )
{
	check( (NumBytes & 3) == 0 );
	check( (SrcOffset & 3) == 0 );
	check( (DstOffset & 3) == 0 );

	auto ShaderMap = GetGlobalShaderMap( GMaxRHIFeatureLevel );

	TShaderMapRef< FMemcpyBufferCS > ComputeShader( ShaderMap );
		
	const FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
	RHICmdList.SetComputeShader( ShaderRHI );

	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->Size, NumBytes / 4 );
	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->SrcOffset, SrcOffset / 4 );
	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->DstOffset, DstOffset / 4 );
	SetSRVParameter( RHICmdList, ShaderRHI, ComputeShader->SrcBuffer, SrcBuffer.SRV );
	SetUAVParameter( RHICmdList, ShaderRHI, ComputeShader->DstBuffer, DstBuffer.UAV );

	RHICmdList.DispatchComputeShader( FMath::DivideAndRoundUp< uint32 >( NumBytes / 4, FMemcpyBufferCS::ThreadGroupSize * 4 ), 1, 1 );

	SetUAVParameter( RHICmdList, ShaderRHI, ComputeShader->DstBuffer, FUnorderedAccessViewRHIRef() );

#if 0
	void* SrcBufferCopy = RHILockStructuredBuffer( SrcBuffer.Buffer, SrcOffset, NumBytes, RLM_ReadOnly );
	void* DstBufferCopy = RHILockStructuredBuffer( DstBuffer.Buffer, DstOffset, NumBytes, RLM_ReadOnly );

	check( FMemory::Memcmp( SrcBufferCopy, DstBufferCopy, NumBytes ) == 0 );

	RHIUnlockStructuredBuffer( SrcBuffer.Buffer );
	RHIUnlockStructuredBuffer( DstBuffer.Buffer );
#endif
}


void ResizeBuffer( FRHICommandList& RHICmdList, FRWByteAddressBuffer& Buffer, uint32 NumBytes )
{
	if( Buffer.NumBytes == 0 )
	{
		Buffer.Initialize( NumBytes );
	}
	else if( NumBytes != Buffer.NumBytes )
	{
		FRWByteAddressBuffer NewBuffer;
		NewBuffer.Initialize( NumBytes );

		// Copy data to new buffer
		uint32 CopyBytes = FMath::Min( NumBytes, Buffer.NumBytes );
		MemcpyBuffer( RHICmdList, NewBuffer, Buffer, CopyBytes );

		Buffer.Buffer	= NewBuffer.Buffer;
		Buffer.UAV		= NewBuffer.UAV;
		Buffer.SRV		= NewBuffer.SRV;
	}
}






class FScatterCopyCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FScatterCopyCS,Global)
	
	FScatterCopyCS() {}
	
	static bool ShouldCompilePermutation( const FGlobalShaderPermutationParameters& Parameters )
	{
		return /*IsFeatureLevelSupported( Parameters.Platform, ERHIFeatureLevel::SM5 )*/
			Parameters.Platform == SP_PS4 || Parameters.Platform == SP_PCD3D_SM5 || Parameters.Platform == SP_XBOXONE_D3D12;
	}

	static void ModifyCompilationEnvironment( const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment )
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine( TEXT("THREADGROUP_SIZE"), ThreadGroupSize );
	}

public:
	enum { ThreadGroupSize = 64 };

	FShaderParameter			NumScatters;
	FShaderResourceParameter	ScatterBuffer;
	FShaderResourceParameter	SrcBuffer;
	FShaderResourceParameter	DstBuffer;
	
	FScatterCopyCS( const ShaderMetaType::CompiledShaderInitializerType& Initializer )
		: FGlobalShader(Initializer)
	{
		NumScatters.Bind(	Initializer.ParameterMap, TEXT("NumScatters") );
		ScatterBuffer.Bind(	Initializer.ParameterMap, TEXT("ScatterBuffer") );
		SrcBuffer.Bind(		Initializer.ParameterMap, TEXT("SrcBuffer") );
		DstBuffer.Bind(		Initializer.ParameterMap, TEXT("DstBuffer") );
	}

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParameters = FGlobalShader::Serialize(Ar);
		Ar << NumScatters;
		Ar << ScatterBuffer;
		Ar << SrcBuffer;
		Ar << DstBuffer;
		return bShaderHasOutdatedParameters;
	}
};

IMPLEMENT_SHADER_TYPE(, FScatterCopyCS, TEXT("/Engine/Private/ByteBuffer.usf"), TEXT("ScatterCopyCS"), SF_Compute );



FScatterUploadBuffer::FScatterUploadBuffer()
	: ScatterData( nullptr )
	, UploadData( nullptr )
{}

void FScatterUploadBuffer::Init( uint32 NumUploads, uint32 Stride )
{
	check( (Stride & 3) == 0 );

	// uint4 copies if aligned
	//CopySize = (Stride & 15) == 0 ? 4 : 1;
	CopySize = 1;
	CopyNum = Stride / ( 4 * CopySize );

	NumScatters = NumUploads * CopyNum;

	uint32 ScatterBytes = NumScatters * 4;
	if( ScatterBytes > ScatterBuffer.NumBytes )
	{
		// Resize Scatter Buffer
		ScatterBuffer.Release();
		ScatterBuffer.Initialize( FMath::RoundUpToPowerOfTwo( FMath::Max( ScatterBytes, 256u ) ), BUF_Volatile );
	}

	uint32 UploadBytes = NumUploads * Stride;
	if( UploadBytes > UploadBuffer.NumBytes )
	{
		// Resize Upload Buffer
		UploadBuffer.Release();
		UploadBuffer.Initialize( FMath::RoundUpToPowerOfTwo( FMath::Max( UploadBytes, 256u ) ), BUF_Volatile );
	}
	
	// This flushes the RHI thread!
	ScatterData = (uint32*)RHILockStructuredBuffer( ScatterBuffer.Buffer, 0, ScatterBytes, RLM_WriteOnly );
	UploadData  = (uint32*)RHILockStructuredBuffer( UploadBuffer.Buffer, 0, UploadBytes, RLM_WriteOnly );

	// Track the actual number of scatters added
	NumScatters = 0;
}

void FScatterUploadBuffer::UploadTo( FRHICommandList& RHICmdList, FRWByteAddressBuffer& DstBuffer )
{
	RHIUnlockStructuredBuffer( ScatterBuffer.Buffer );
	RHIUnlockStructuredBuffer( UploadBuffer.Buffer );

	ScatterData = nullptr;
	UploadData  = nullptr;

	auto ShaderMap = GetGlobalShaderMap( GMaxRHIFeatureLevel );

	TShaderMapRef< FScatterCopyCS > ComputeShader( ShaderMap );
		
	const FComputeShaderRHIParamRef ShaderRHI = ComputeShader->GetComputeShader();
	RHICmdList.SetComputeShader( ShaderRHI );

	SetShaderValue( RHICmdList, ShaderRHI, ComputeShader->NumScatters, NumScatters );
	SetSRVParameter( RHICmdList, ShaderRHI, ComputeShader->ScatterBuffer, ScatterBuffer.SRV );
	SetSRVParameter( RHICmdList, ShaderRHI, ComputeShader->SrcBuffer, UploadBuffer.SRV );
	SetUAVParameter( RHICmdList, ShaderRHI, ComputeShader->DstBuffer, DstBuffer.UAV );

	// 4 scatters per thread
	//RHICmdList.DispatchComputeShader( FMath::DivideAndRoundUp< uint32 >( NumScatters, FScatterCopyCS::ThreadGroupSize * 4 ), 1, 1 );
	RHICmdList.DispatchComputeShader( FMath::DivideAndRoundUp< uint32 >( NumScatters, FScatterCopyCS::ThreadGroupSize ), 1, 1 );

	SetUAVParameter( RHICmdList, ShaderRHI, ComputeShader->DstBuffer, FUnorderedAccessViewRHIRef() );
}