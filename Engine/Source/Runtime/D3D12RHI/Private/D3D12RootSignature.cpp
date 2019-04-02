// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12RootSignature.cpp: D3D12 Root Signatures
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "D3D12RootSignatureDefinitions.h"
#include "RayTracingBuiltInResources.h"

namespace
{
	// Root parameter costs in DWORDs as described here: https://docs.microsoft.com/en-us/windows/desktop/direct3d12/root-signature-limits
	static const uint32 RootDescriptorTableCostGlobal = 1; // Descriptor tables cost 1 DWORD
	static const uint32 RootDescriptorTableCostLocal = 2; // Local root signature descriptor tables cost 2 DWORDs -- undocumented as of 2018-11-12
	static const uint32 RootConstantCost = 1; // Each root constant is 1 DWORD
	static const uint32 RootDescriptorCost = 2; // Root descriptor is 64-bit GPU virtual address, 2 DWORDs
}

FORCEINLINE D3D12_SHADER_VISIBILITY GetD3D12ShaderVisibility(EShaderVisibility Visibility)
{
	switch (Visibility)
	{
	case SV_Vertex:
		return D3D12_SHADER_VISIBILITY_VERTEX;
	case SV_Hull:
		return D3D12_SHADER_VISIBILITY_HULL;
	case SV_Domain:
		return D3D12_SHADER_VISIBILITY_DOMAIN;
	case SV_Geometry:
return D3D12_SHADER_VISIBILITY_GEOMETRY;
	case SV_Pixel:
		return D3D12_SHADER_VISIBILITY_PIXEL;
	case SV_All:
		return D3D12_SHADER_VISIBILITY_ALL;

	default:
		check(false);
		return static_cast<D3D12_SHADER_VISIBILITY>(-1);
	};
}

FORCEINLINE D3D12_ROOT_SIGNATURE_FLAGS GetD3D12RootSignatureDenyFlag(EShaderVisibility Visibility)
{
	switch (Visibility)
	{
	case SV_Vertex:
		return D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
	case SV_Hull:
		return D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
	case SV_Domain:
		return D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
	case SV_Geometry:
		return D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
	case SV_Pixel:
		return D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	case SV_All:
		return D3D12_ROOT_SIGNATURE_FLAG_NONE;

	default:
		check(false);
		return static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(-1);
	};
}


FD3D12RootSignatureDesc::FD3D12RootSignatureDesc(const FD3D12QuantizedBoundShaderState& QBSS, const D3D12_RESOURCE_BINDING_TIER ResourceBindingTier)
	: RootParametersSize(0)
{
	const EShaderVisibility ShaderVisibilityPriorityOrder[] = { SV_Pixel, SV_Vertex, SV_Geometry, SV_Hull, SV_Domain, SV_All };
	const D3D12_ROOT_PARAMETER_TYPE RootParameterTypePriorityOrder[] = { D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE, D3D12_ROOT_PARAMETER_TYPE_CBV };
	uint32 RootParameterCount = 0;

	// Determine if our descriptors or their data is static based on the resource binding tier.
	// We do this because sometimes (based on binding tier) our descriptor tables are bigger than the # of descriptors we copy. See FD3D12QuantizedBoundShaderState::InitShaderRegisterCounts().
	const D3D12_DESCRIPTOR_RANGE_FLAGS SRVDescriptorRangeFlags = (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_1) ?
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE :
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	const D3D12_DESCRIPTOR_RANGE_FLAGS CBVDescriptorRangeFlags = (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2) ?
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE :
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	const D3D12_DESCRIPTOR_RANGE_FLAGS UAVDescriptorRangeFlags = (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_2) ?
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE :
		D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	const D3D12_DESCRIPTOR_RANGE_FLAGS SamplerDescriptorRangeFlags = (ResourceBindingTier <= D3D12_RESOURCE_BINDING_TIER_1) ?
		D3D12_DESCRIPTOR_RANGE_FLAG_NONE :
		D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	const D3D12_ROOT_DESCRIPTOR_FLAGS CBVRootDescriptorFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC;	// We always set the data in an upload heap before calling Set*RootConstantBufferView.

	uint32 BindingSpace = 0; // Default binding space for D3D 11 & 12 shaders

#if D3D12_RHI_RAYTRACING
	if (QBSS.RootSignatureType == RS_RayTracingLocal)
	{
		BindingSpace = RAY_TRACING_REGISTER_SPACE_LOCAL;

		// Add standard root parameters for hit groups, as per FHitGroupSystemParameters declaration in D3D12RayTracing.cpp and RayTracingHitGroupCommon.ush:
		//          Resources:
		// 8 bytes: index buffer as root SRV (raw buffer)
		// 8 bytes: vertex buffer as root SRV (raw buffer)
		//          FHitGroupSystemRootConstants:
		// 4 bytes: index/vertex fetch configuration as root constant (bitfield defining index and vertex formats)
		// 4 bytes: index buffer offset in bytes
		// 4 bytes: hit group user data
		// 4 bytes: unused padding to ensure the next parameter is aligned to 8-byte boundary
		// -----------
		// 32 bytes

		// #dxr_todo: Root parameters for hit shaders should be added in the shader pipeline, so that regular root parameter generation code can be used without hard-coding anything.

		check(RootParameterCount == 0 && RootParametersSize == 0); // We expect system RT parameters to come first

		// Index buffer descriptor
		{
			check(RootParameterCount < MaxRootParameters);
			TableSlots[RootParameterCount].InitAsShaderResourceView(RAY_TRACING_SYSTEM_INDEXBUFFER_REGISTER, RAY_TRACING_REGISTER_SPACE_SYSTEM);
			RootParameterCount++;
			RootParametersSize += RootDescriptorCost;
		}

		// Vertex buffer descriptor
		{
			check(RootParameterCount < MaxRootParameters);
			TableSlots[RootParameterCount].InitAsShaderResourceView(RAY_TRACING_SYSTEM_VERTEXBUFFER_REGISTER, RAY_TRACING_REGISTER_SPACE_SYSTEM);
			RootParameterCount++;
			RootParametersSize += RootDescriptorCost;
		}

		// FHitGroupSystemRootConstants structure
		{
			check(RootParameterCount < MaxRootParameters);
			static_assert(sizeof(FHitGroupSystemRootConstants) % 8 == 0, "FHitGroupSystemRootConstants structure must be 8-byte aligned");
			const uint32 NumConstants = sizeof(FHitGroupSystemRootConstants) / sizeof(uint32);
			TableSlots[RootParameterCount].InitAsConstants(NumConstants, RAY_TRACING_SYSTEM_ROOTCONSTANT_REGISTER, RAY_TRACING_REGISTER_SPACE_SYSTEM);
			RootParameterCount++;
			RootParametersSize += NumConstants * RootConstantCost;
		}
	}
	else if (QBSS.RootSignatureType == RS_RayTracingGlobal)
	{
		BindingSpace = RAY_TRACING_REGISTER_SPACE_GLOBAL;
	}
#endif //D3D12_RHI_RAYTRACING

	const uint32 RootDescriptorTableCost = QBSS.RootSignatureType == RS_RayTracingLocal ? RootDescriptorTableCostLocal : RootDescriptorTableCostGlobal;

	// For each root parameter type...
	for (uint32 RootParameterTypeIndex = 0; RootParameterTypeIndex < ARRAY_COUNT(RootParameterTypePriorityOrder); RootParameterTypeIndex++)
	{
		const D3D12_ROOT_PARAMETER_TYPE& RootParameterType = RootParameterTypePriorityOrder[RootParameterTypeIndex];

		// ... and each shader stage visibility ...
		for (uint32 ShaderVisibilityIndex = 0; ShaderVisibilityIndex < ARRAY_COUNT(ShaderVisibilityPriorityOrder); ShaderVisibilityIndex++)
		{
			const EShaderVisibility& Visibility = ShaderVisibilityPriorityOrder[ShaderVisibilityIndex];
			const FShaderRegisterCounts& Shader = QBSS.RegisterCounts[Visibility];

			switch (RootParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			{
				if (Shader.ShaderResourceCount > 0)
				{
					check(RootParameterCount < MaxRootParameters);
					DescriptorRanges[RootParameterCount].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, Shader.ShaderResourceCount, 0u, BindingSpace, SRVDescriptorRangeFlags);
					TableSlots[RootParameterCount].InitAsDescriptorTable(1, &DescriptorRanges[RootParameterCount], GetD3D12ShaderVisibility(Visibility));
					RootParameterCount++;
					RootParametersSize += RootDescriptorTableCost;
				}

				if (Shader.ConstantBufferCount > MAX_ROOT_CBVS)
				{
					checkf(QBSS.RootSignatureType != RS_RayTracingLocal, TEXT("CBV descriptor tables are not implemented for local root signatures"));

					// Use a descriptor table for the 'excess' CBVs
					check(RootParameterCount < MaxRootParameters);
					DescriptorRanges[RootParameterCount].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, Shader.ConstantBufferCount - MAX_ROOT_CBVS, MAX_ROOT_CBVS, BindingSpace, CBVDescriptorRangeFlags);
					TableSlots[RootParameterCount].InitAsDescriptorTable(1, &DescriptorRanges[RootParameterCount], GetD3D12ShaderVisibility(Visibility));
					RootParameterCount++;
					RootParametersSize += RootDescriptorTableCost;
				}

				if (Shader.SamplerCount > 0)
				{
					check(RootParameterCount < MaxRootParameters);
					DescriptorRanges[RootParameterCount].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, Shader.SamplerCount, 0u, BindingSpace, SamplerDescriptorRangeFlags);
					TableSlots[RootParameterCount].InitAsDescriptorTable(1, &DescriptorRanges[RootParameterCount], GetD3D12ShaderVisibility(Visibility));
					RootParameterCount++;
					RootParametersSize += RootDescriptorTableCost;
				}

				if (Shader.UnorderedAccessCount > 0)
				{
					check(RootParameterCount < MaxRootParameters);
					DescriptorRanges[RootParameterCount].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, Shader.UnorderedAccessCount, 0u, BindingSpace, UAVDescriptorRangeFlags);
					TableSlots[RootParameterCount].InitAsDescriptorTable(1, &DescriptorRanges[RootParameterCount], GetD3D12ShaderVisibility(Visibility));
					RootParameterCount++;
					RootParametersSize += RootDescriptorTableCost;
				}
				break;
			}

			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			{
				for (uint32 ShaderRegister = 0; (ShaderRegister < Shader.ConstantBufferCount) && (ShaderRegister < MAX_ROOT_CBVS); ShaderRegister++)
				{
					check(RootParameterCount < MaxRootParameters);
					TableSlots[RootParameterCount].InitAsConstantBufferView(ShaderRegister, BindingSpace, CBVRootDescriptorFlags, GetD3D12ShaderVisibility(Visibility));
					RootParameterCount++;
					RootParametersSize += RootDescriptorCost;
				}
				break;
			}

			default:
				check(false);
				break;
			}
		}
	}

	D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

#if D3D12_RHI_RAYTRACING
	if (QBSS.RootSignatureType == RS_RayTracingLocal)
	{
		Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	}
	else if (QBSS.RootSignatureType == RS_Raster)
#endif // D3D12_RHI_RAYTRACING
	{
		// Determine what shader stages need access in the root signature.

		if (QBSS.bAllowIAInputLayout)
		{
			Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		}

		for (uint32 ShaderVisibilityIndex = 0; ShaderVisibilityIndex < ARRAY_COUNT(ShaderVisibilityPriorityOrder); ShaderVisibilityIndex++)
		{
			const EShaderVisibility& Visibility = ShaderVisibilityPriorityOrder[ShaderVisibilityIndex];
			const FShaderRegisterCounts& Shader = QBSS.RegisterCounts[Visibility];
			if ((Shader.ShaderResourceCount == 0) &&
				(Shader.ConstantBufferCount == 0) &&
				(Shader.UnorderedAccessCount == 0) &&
				(Shader.SamplerCount == 0))
			{
				// This shader stage doesn't use any descriptors, deny access to the shader stage in the root signature.
				Flags = (Flags | GetD3D12RootSignatureDenyFlag(Visibility));
			}
		}
	}

	// Init the desc (warn about the size if necessary).
#if !NO_LOGGING
	const uint32 SizeWarningThreshold = 12;
	if (RootParametersSize > SizeWarningThreshold)
	{
		UE_LOG(LogD3D12RHI, Display, TEXT("Root signature created where the root parameters take up %u DWORDS. Using more than %u DWORDs can negatively impact performance depending on the hardware and root parameter usage."), RootParametersSize, SizeWarningThreshold);
	}
#endif
	RootDesc.Init_1_1(RootParameterCount, TableSlots, 0, nullptr, Flags);
}

D3D12_VERSIONED_ROOT_SIGNATURE_DESC& FD3D12RootSignatureDesc::GetStaticGraphicsRootSignatureDesc()
{
	static const uint32 DescriptorTableCount = 16;
	static struct
	{
		D3D12_SHADER_VISIBILITY Vis;
		D3D12_DESCRIPTOR_RANGE_TYPE Type;
		uint32 Count;
		uint32 BaseShaderReg;
		D3D12_DESCRIPTOR_RANGE_FLAGS Flags;
	} RangeDesc[DescriptorTableCount] =
	{
		{ D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SRVS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SRVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MAX_CBS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::CBVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, MAX_SAMPLERS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SamplerDescriptorRangeFlags },

		{ D3D12_SHADER_VISIBILITY_VERTEX, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SRVS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SRVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_VERTEX, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MAX_CBS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::CBVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_VERTEX, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, MAX_SAMPLERS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SamplerDescriptorRangeFlags },

		{ D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SRVS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SRVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MAX_CBS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::CBVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, MAX_SAMPLERS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SamplerDescriptorRangeFlags },

		{ D3D12_SHADER_VISIBILITY_HULL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SRVS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SRVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_HULL, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MAX_CBS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::CBVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_HULL, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, MAX_SAMPLERS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SamplerDescriptorRangeFlags },

		{ D3D12_SHADER_VISIBILITY_DOMAIN, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SRVS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SRVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_DOMAIN, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MAX_CBS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::CBVDescriptorRangeFlags },
		{ D3D12_SHADER_VISIBILITY_DOMAIN, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, MAX_SAMPLERS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SamplerDescriptorRangeFlags },

		{ D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, MAX_UAVS, 0, D3D12ShaderUtils::StaticRootSignatureConstants::UAVDescriptorRangeFlags },
	};

	static CD3DX12_ROOT_PARAMETER1 TableSlots[DescriptorTableCount];
	static CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[DescriptorTableCount];

	for (uint32 i = 0; i < DescriptorTableCount; i++)
	{
		DescriptorRanges[i].Init(
			RangeDesc[i].Type,
			RangeDesc[i].Count,
			RangeDesc[i].BaseShaderReg,
			0u,
			RangeDesc[i].Flags
		);

		TableSlots[i].InitAsDescriptorTable(1, &DescriptorRanges[i], RangeDesc[i].Vis);
	}

	static CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootDesc(DescriptorTableCount, TableSlots, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	return RootDesc;
}

D3D12_VERSIONED_ROOT_SIGNATURE_DESC& FD3D12RootSignatureDesc::GetStaticComputeRootSignatureDesc()
{
	static const uint32 DescriptorTableCount = 4;
	static CD3DX12_ROOT_PARAMETER1 TableSlots[DescriptorTableCount];
	static CD3DX12_DESCRIPTOR_RANGE1 DescriptorRanges[DescriptorTableCount];

	uint32 RangeIndex = 0;
	DescriptorRanges[RangeIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_SRVS, 0, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SRVDescriptorRangeFlags);
	TableSlots[RangeIndex].InitAsDescriptorTable(1, &DescriptorRanges[RangeIndex], D3D12_SHADER_VISIBILITY_ALL);
	++RangeIndex;
	DescriptorRanges[RangeIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, MAX_CBS, 0, 0, D3D12ShaderUtils::StaticRootSignatureConstants::CBVDescriptorRangeFlags);
	TableSlots[RangeIndex].InitAsDescriptorTable(1, &DescriptorRanges[RangeIndex], D3D12_SHADER_VISIBILITY_ALL);
	++RangeIndex;
	DescriptorRanges[RangeIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, MAX_SAMPLERS, 0, 0, D3D12ShaderUtils::StaticRootSignatureConstants::SamplerDescriptorRangeFlags);
	TableSlots[RangeIndex].InitAsDescriptorTable(1, &DescriptorRanges[RangeIndex], D3D12_SHADER_VISIBILITY_ALL);
	++RangeIndex;
	DescriptorRanges[RangeIndex].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, MAX_UAVS, 0, 0, D3D12ShaderUtils::StaticRootSignatureConstants::UAVDescriptorRangeFlags);
	TableSlots[RangeIndex].InitAsDescriptorTable(1, &DescriptorRanges[RangeIndex], D3D12_SHADER_VISIBILITY_ALL);
	++RangeIndex;

	static CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC RootDesc(RangeIndex, TableSlots, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
	return RootDesc;
}

void FD3D12RootSignature::Init(const FD3D12QuantizedBoundShaderState& InQBSS)
{
	// Create a root signature desc from the quantized bound shader state.
	const D3D12_RESOURCE_BINDING_TIER ResourceBindingTier = GetParentAdapter()->GetResourceBindingTier();
	FD3D12RootSignatureDesc Desc(InQBSS, ResourceBindingTier);

	uint32 BindingSpace = 0; // Default binding space for D3D 11 & 12 shaders

	if (InQBSS.RootSignatureType == RS_RayTracingGlobal)
	{
		BindingSpace = RAY_TRACING_REGISTER_SPACE_GLOBAL;
	}
	else if (InQBSS.RootSignatureType == RS_RayTracingLocal)
	{
		BindingSpace = RAY_TRACING_REGISTER_SPACE_LOCAL;
	}

	Init(Desc.GetDesc(), BindingSpace);
}

void FD3D12RootSignature::Init(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& InDesc, uint32 BindingSpace)
{
	ID3D12Device* Device = GetParentAdapter()->GetD3DDevice();
	
	// Serialize the desc.
	TRefCountPtr<ID3DBlob> Error;
	const D3D_ROOT_SIGNATURE_VERSION MaxRootSignatureVersion = GetParentAdapter()->GetRootSignatureVersion();
	const HRESULT SerializeHR = D3DX12SerializeVersionedRootSignature(&InDesc, MaxRootSignatureVersion, RootSignatureBlob.GetInitReference(), Error.GetInitReference());
	if (Error.GetReference())
	{
		UE_LOG(LogD3D12RHI, Fatal, TEXT("D3DX12SerializeVersionedRootSignature failed with error %s"), ANSI_TO_TCHAR(Error->GetBufferPointer()));
	}
	VERIFYD3D12RESULT(SerializeHR);

	// Create and analyze the root signature.
	VERIFYD3D12RESULT(Device->CreateRootSignature((uint32)FRHIGPUMask::All(),
		RootSignatureBlob->GetBufferPointer(),
		RootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(RootSignature.GetInitReference())));

	AnalyzeSignature(InDesc, BindingSpace);
}

void FD3D12RootSignature::Init(ID3DBlob* const InBlob, uint32 BindingSpace)
{
	ID3D12Device* Device = GetParentAdapter()->GetD3DDevice();

	// Save the blob
	RootSignatureBlob = InBlob;

	// Deserialize to get the desc.
	TRefCountPtr<ID3D12VersionedRootSignatureDeserializer> Deserializer;
	VERIFYD3D12RESULT(D3D12CreateVersionedRootSignatureDeserializer(RootSignatureBlob->GetBufferPointer(), RootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(Deserializer.GetInitReference())));

	// Create and analyze the root signature.
	VERIFYD3D12RESULT(Device->CreateRootSignature((uint32)FRHIGPUMask::All(),
		RootSignatureBlob->GetBufferPointer(),
		RootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(RootSignature.GetInitReference())));

	AnalyzeSignature(*Deserializer->GetUnconvertedRootSignatureDesc(), BindingSpace);
}

void FD3D12RootSignature::AnalyzeSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& Desc, uint32 BindingSpace)
{
	switch (Desc.Version)
	{
	case D3D_ROOT_SIGNATURE_VERSION_1_0:
		InternalAnalyzeSignature(Desc.Desc_1_0, BindingSpace);
		break;

	case D3D_ROOT_SIGNATURE_VERSION_1_1:
		InternalAnalyzeSignature(Desc.Desc_1_1, BindingSpace);
		break;

	default:
		ensureMsgf(false, TEXT("Invalid root signature version %u"), Desc.Version);
		break;
	}
}

template<typename RootSignatureDescType>
void FD3D12RootSignature::InternalAnalyzeSignature(const RootSignatureDescType& Desc, uint32 BindingSpace)
{
	// Reset members to default values.
	{
		FMemory::Memset(BindSlotMap, 0xFF, sizeof(BindSlotMap));
		bHasUAVs = false;
		bHasSRVs = false;
		bHasCBVs = false;
		bHasRDTCBVs = false;
		bHasRDCBVs = false;
		bHasSamplers = false;

		FMemory::Memset(BindSlotOffsetsInDWORDs, 0, sizeof(BindSlotOffsetsInDWORDs));
		TotalRootSignatureSizeInDWORDs = 0;
	}

	const bool bDenyVS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyHS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyDS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyGS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS) != 0;
	const bool bDenyPS = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS) != 0;

#if D3D12_RHI_RAYTRACING
	const uint32 RootDescriptorTableCost = (Desc.Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE) ? RootDescriptorTableCostLocal : RootDescriptorTableCostGlobal;
#else
	const uint32 RootDescriptorTableCost = RootDescriptorTableCostGlobal;
#endif

	// Go through each root parameter.
	for (uint32 i = 0; i < Desc.NumParameters; i++)
	{
		const auto& CurrentParameter = Desc.pParameters[i];

		uint32 ParameterBindingSpace = ~0u;

		switch (CurrentParameter.ParameterType)
		{
		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
			check(CurrentParameter.DescriptorTable.NumDescriptorRanges == 1); // Code currently assumes a single descriptor range.
			ParameterBindingSpace = CurrentParameter.DescriptorTable.pDescriptorRanges[0].RegisterSpace;
			BindSlotOffsetsInDWORDs[i] = TotalRootSignatureSizeInDWORDs;
			TotalRootSignatureSizeInDWORDs += RootDescriptorTableCost;
			break;
		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			ParameterBindingSpace = CurrentParameter.Constants.RegisterSpace;
			BindSlotOffsetsInDWORDs[i] = TotalRootSignatureSizeInDWORDs;
			TotalRootSignatureSizeInDWORDs += RootConstantCost * CurrentParameter.Constants.Num32BitValues;
			break;
		case D3D12_ROOT_PARAMETER_TYPE_CBV:
		case D3D12_ROOT_PARAMETER_TYPE_SRV:
		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			ParameterBindingSpace = CurrentParameter.Descriptor.RegisterSpace;
			BindSlotOffsetsInDWORDs[i] = TotalRootSignatureSizeInDWORDs;
			TotalRootSignatureSizeInDWORDs += RootDescriptorCost;
			break;
		default:
			checkNoEntry();
			break;
		}

		if (ParameterBindingSpace != BindingSpace)
		{
			// Only consider parameters in the requested binding space.
			continue;
		}

		EShaderFrequency CurrentVisibleSF = SF_NumFrequencies;
		switch (CurrentParameter.ShaderVisibility)
		{
		case D3D12_SHADER_VISIBILITY_ALL:
			CurrentVisibleSF = SF_NumFrequencies;
			break;

		case D3D12_SHADER_VISIBILITY_VERTEX:
			CurrentVisibleSF = SF_Vertex;
			break;
		case D3D12_SHADER_VISIBILITY_HULL:
			CurrentVisibleSF = SF_Hull;
			break;
		case D3D12_SHADER_VISIBILITY_DOMAIN:
			CurrentVisibleSF = SF_Domain;
			break;
		case D3D12_SHADER_VISIBILITY_GEOMETRY:
			CurrentVisibleSF = SF_Geometry;
			break;
		case D3D12_SHADER_VISIBILITY_PIXEL:
			CurrentVisibleSF = SF_Pixel;
			break;

		default:
			check(false);
			break;
		}

		// Determine shader stage visibility.
		{
			Stage[SF_Vertex].bVisible = Stage[SF_Vertex].bVisible || (!bDenyVS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_VERTEX));
			Stage[SF_Hull].bVisible = Stage[SF_Hull].bVisible || (!bDenyHS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_HULL));
			Stage[SF_Domain].bVisible = Stage[SF_Domain].bVisible || (!bDenyDS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_DOMAIN));
			Stage[SF_Geometry].bVisible = Stage[SF_Geometry].bVisible || (!bDenyGS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_GEOMETRY));
			Stage[SF_Pixel].bVisible = Stage[SF_Pixel].bVisible || (!bDenyPS && HasVisibility(CurrentParameter.ShaderVisibility, D3D12_SHADER_VISIBILITY_PIXEL));

			// Compute is a special case, it must have visibility all.
			Stage[SF_Compute].bVisible = Stage[SF_Compute].bVisible || (CurrentParameter.ShaderVisibility == D3D12_SHADER_VISIBILITY_ALL);
		}

		// Determine shader resource counts.
		{
			switch (CurrentParameter.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				check(CurrentParameter.DescriptorTable.NumDescriptorRanges == 1);	// Code currently assumes a single descriptor range.
				{
					const auto& CurrentRange = CurrentParameter.DescriptorTable.pDescriptorRanges[0];
					check(CurrentRange.BaseShaderRegister == 0);	// Code currently assumes always starting at register 0.
					check(CurrentRange.RegisterSpace == BindingSpace); // Parameters in other binding spaces are expected to be filtered out at this point

					switch (CurrentRange.RangeType)
					{
					case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
						SetMaxSRVCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetSRVRDTBindSlot(CurrentVisibleSF, i);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
						SetMaxUAVCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetUAVRDTBindSlot(CurrentVisibleSF, i);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
						IncrementMaxCBVCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetCBVRDTBindSlot(CurrentVisibleSF, i);
						UpdateCBVRegisterMaskWithDescriptorRange(CurrentVisibleSF, CurrentRange);
						break;
					case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
						SetMaxSamplerCount(CurrentVisibleSF, CurrentRange.NumDescriptors);
						SetSamplersRDTBindSlot(CurrentVisibleSF, i);
						break;

					default: check(false); break;
					}
				}
				break;

			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			{
				check(CurrentParameter.Descriptor.RegisterSpace == BindingSpace); // Parameters in other binding spaces are expected to be filtered out at this point

				IncrementMaxCBVCount(CurrentVisibleSF, 1);
				if (CurrentParameter.Descriptor.ShaderRegister == 0)
				{
					// This is the first CBV for this stage, save it's root parameter index (other CBVs will be indexed using this base root parameter index).
					SetCBVRDBindSlot(CurrentVisibleSF, i);
				}

				UpdateCBVRegisterMaskWithDescriptor(CurrentVisibleSF, CurrentParameter.Descriptor);

				// The first CBV for this stage must come first in the root signature, and subsequent root CBVs for this stage must be contiguous.
				check(0xFF != CBVRDBindSlot(CurrentVisibleSF, 0));
				check(i == CBVRDBindSlot(CurrentVisibleSF, 0) + CurrentParameter.Descriptor.ShaderRegister);
			}
			break;

			default:
				// Need to update this for the other types. Currently we only use descriptor tables in the root signature.
				check(false);
				break;
			}
		}
	}
}

void FD3D12RootSignatureManager::Destroy()
{
	for (auto Iter = RootSignatureMap.CreateIterator(); Iter; ++Iter)
	{
		FD3D12RootSignature* pRootSignature = Iter.Value();
		delete pRootSignature;
	}
	RootSignatureMap.Reset();
}

FD3D12RootSignature* FD3D12RootSignatureManager::GetRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
{
	// Creating bound shader states happens in parallel, so this must be thread safe.
	FScopeLock Lock(&CS);

	FD3D12RootSignature** ppRootSignature = RootSignatureMap.Find(QBSS);
	if (ppRootSignature == nullptr)
	{
		// Create a new root signature and return it.
		return CreateRootSignature(QBSS);
	}

	check(*ppRootSignature);
	return *ppRootSignature;
}

FD3D12RootSignature* FD3D12RootSignatureManager::CreateRootSignature(const FD3D12QuantizedBoundShaderState& QBSS)
{
	// Create a desc and the root signature.
	FD3D12RootSignature* pNewRootSignature = new FD3D12RootSignature(GetParentAdapter(), QBSS);
	check(pNewRootSignature);

	// Add the index to the map.
	RootSignatureMap.Add(QBSS, pNewRootSignature);

	return pNewRootSignature;
}

FD3D12QuantizedBoundShaderState FD3D12RootSignatureManager::GetQuantizedBoundShaderState(const FD3D12RootSignature* const RootSignature)
{
	FScopeLock Lock(&CS);

	const FD3D12QuantizedBoundShaderState* QBSS = RootSignatureMap.FindKey(const_cast<FD3D12RootSignature*>(RootSignature));
	check(QBSS);

	return *QBSS;
}
