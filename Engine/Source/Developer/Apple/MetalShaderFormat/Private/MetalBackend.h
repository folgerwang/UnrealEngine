// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "hlslcc.h"
THIRD_PARTY_INCLUDES_START
	#include "ir.h"
THIRD_PARTY_INCLUDES_END
#include "PackUniformBuffers.h"
#include "LanguageSpec.h"


class FMetalLanguageSpec : public ILanguageSpec
{
public:
	uint8 Version;
	uint32 ClipDistanceCount;
	uint32 ClipDistancesUsed;
	
	FMetalLanguageSpec(uint8 InVersion) : Version(InVersion), ClipDistanceCount(0), ClipDistancesUsed(0) {}
	
	uint32 GetClipDistanceCount() const { return ClipDistanceCount; }

	virtual bool SupportsDeterminantIntrinsic() const override { return true; }

	virtual bool SupportsTransposeIntrinsic() const override { return true; }
	
	virtual bool SupportsIntegerModulo() const override { return true; }

	virtual bool SupportsMatrixConversions() const override { return false; }

	virtual void SetupLanguageIntrinsics(_mesa_glsl_parse_state* State, exec_list* ir) override;

	virtual bool AllowsSharingSamplers() const override { return true; }

	virtual bool UseSamplerInnerType() const override { return true; }

	virtual bool CanConvertBetweenHalfAndFloat() const override { return false; }

	virtual bool NeedsAtomicLoadStore() const override { return true; }
	
	virtual bool SplitInputVariableStructs() const { return false; }
	
	virtual bool SupportsFusedMultiplyAdd() const { return true; }
	
	virtual bool SupportsSaturateIntrinsic() const { return true; }

    virtual bool SupportsSinCosIntrinsic() const { return true; }
    
    virtual bool SupportsMatrixIntrinsics() const { return (Version < 2); }

	virtual bool AllowsAllTextureOperationsOnDepthTextures() const { return true; }
    
    virtual bool AllowsInvariantBufferTypes() const { return true; }
};

struct FBuffers;
struct FMetalTessellationOutputs;

enum EMetalAccess
{
    EMetalAccessRead = 1,
    EMetalAccessWrite = 2,
    EMetalAccessReadWrite = 3,
};

enum EMetalGPUSemantics
{
	EMetalGPUSemanticsMobile, // Mobile shaders for TBDR GPUs
	EMetalGPUSemanticsTBDRDesktop, // Desktop shaders for TBDR GPUs
	EMetalGPUSemanticsImmediateDesktop // Desktop shaders for Immediate GPUs
};

enum EMetalTypeBufferMode
{
	EMetalTypeBufferModeRaw = 0, // No typed buffers
	EMetalTypeBufferMode2DSRV = 1, // Buffer<> SRVs are typed via 2D textures, RWBuffer<> UAVs are raw buffers
	EMetalTypeBufferModeTBSRV = 2, // Buffer<> SRVs are typed via texture-buffers, RWBuffer<> UAVs are raw buffers
    EMetalTypeBufferMode2D = 3, // Buffer<> SRVs & RWBuffer<> UAVs are typed via 2D textures
    EMetalTypeBufferModeTB = 4, // Buffer<> SRVs & RWBuffer<> UAVs are typed via texture-buffers
};

// Metal supports 16 across all HW
static const int32 MaxMetalSamplers = 16;

// Generates Metal compliant code from IR tokens
struct FMetalCodeBackend : public FCodeBackend
{
	FMetalCodeBackend(FMetalTessellationOutputs& Attribs, unsigned int InHlslCompileFlags, EHlslCompileTarget InTarget, uint8 Version, EMetalGPUSemantics bInDesktop, EMetalTypeBufferMode InTypedMode, uint32 MaxUnrollLoops, bool bInZeroInitialise, bool bInBoundsChecks, bool bInAllFastIntriniscs, bool bForceInvariance);

	virtual char* GenerateCode(struct exec_list* ir, struct _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	virtual bool GenerateMain(EHlslShaderFrequency Frequency, const char* EntryPoint, exec_list* Instructions, _mesa_glsl_parse_state* ParseState) override;

	void CallPatchConstantFunction(_mesa_glsl_parse_state* ParseState, ir_variable* OutputPatchVar, ir_variable* internalPatchIDVar, ir_function_signature* PatchConstantSig, exec_list& DeclInstructions, exec_list &PostCallInstructions, int &onAttribute);

	// Return false if there were restrictions that made compilation fail
	virtual bool ApplyAndVerifyPlatformRestrictions(exec_list* Instructions, _mesa_glsl_parse_state* ParseState, EHlslShaderFrequency Frequency) override;

	struct glsl_type const* create_iab_type(_mesa_glsl_parse_state* ParseState, struct glsl_type const* UBType, char const* n, FBuffers const& Buffers);
	void build_iab_fields(_mesa_glsl_parse_state* ParseState, char const* n, struct glsl_type const* t, TArray<struct glsl_struct_field>& Fields, unsigned& FieldIndex, unsigned& BufferIndex, bool top, FBuffers const& Buffers);
	void InsertArgumentBuffers(exec_list* ir, _mesa_glsl_parse_state* state, FBuffers& Buffers);
	void PackInputsAndOutputs(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency, exec_list& InputVars);
	void MovePackedUniformsToMain(exec_list* ir, _mesa_glsl_parse_state* state, FBuffers& OutBuffers);
	void FixIntrinsics(exec_list* ir, _mesa_glsl_parse_state* state);
	void RemovePackedVarReferences(exec_list* ir, _mesa_glsl_parse_state* State);
	void PromoteInputsAndOutputsGlobalHalfToFloat(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency);
	void ConvertHalfToFloatUniformsAndSamples(exec_list* ir, _mesa_glsl_parse_state* State, bool bConvertUniforms, bool bConvertSamples);
	void BreakPrecisionChangesVisitor(exec_list* ir, _mesa_glsl_parse_state* State);
	void FixupMetalBaseOffsets(exec_list* ir, _mesa_glsl_parse_state* state, EHlslShaderFrequency Frequency);
	void InsertSamplerStates(exec_list* ir, _mesa_glsl_parse_state* State);
	void FixupTextureAtomics(exec_list* ir, _mesa_glsl_parse_state* state);

	TMap<ir_variable*, TSet<uint8>> IABVariableMask;
	TMap<ir_variable*, ir_variable*> IABVariablesMap;
    TMap<ir_variable*, uint32> ImageRW;
    FMetalTessellationOutputs& TessAttribs;
	TArray<uint8> TypedBufferFormats;
	uint32 InvariantBuffers;
	uint32 TypedBuffers;
    uint32 TypedUAVs;
	uint32 ConstantBuffers;
    
    uint8 Version;
	EMetalGPUSemantics bIsDesktop;
	EMetalTypeBufferMode TypedMode;
	uint32 MaxUnrollLoops;
	bool bZeroInitialise;
	bool bBoundsChecks;
	bool bAllowFastIntriniscs;
	bool bExplicitDepthWrites;
	bool bForceInvariance;

	bool bIsTessellationVSHS = false;
	unsigned int inputcontrolpoints = 0;
	unsigned int patchesPerThreadgroup = 0;
	uint32 PatchControlPointStructHash;
};

struct FShaderCompilerEnvironment;
bool IsRemoteBuildingConfigured(const FShaderCompilerEnvironment* InEnvironment = nullptr);
