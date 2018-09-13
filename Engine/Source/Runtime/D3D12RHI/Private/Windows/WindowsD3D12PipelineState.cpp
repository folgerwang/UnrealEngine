// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"
#include "Misc/ScopeRWLock.h"
#include "Stats/StatsMisc.h"

static TAutoConsoleVariable<int32> CVarPipelineStateDiskCache(
	TEXT("D3D12.PSO.DiskCache"),
	1,
	TEXT("Enables a disk cache for Pipeline State Objects (PSOs).\n")
	TEXT("PSO descs are cached to disk so subsequent runs can create PSOs at load-time instead of at run-time.\n")
	TEXT("This cache contains data that is independent of hardware, driver, or machine that it was created on. It can be distributed with shipping content.\n")
	TEXT("0 to disable the pipeline state disk cache\n")
	TEXT("1 to enable the pipeline state disk cache (default)\n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<int32> CVarDriverOptimizedPipelineStateDiskCache(
	TEXT("D3D12.PSO.DriverOptimizedDiskCache"),
	1,
	TEXT("Enables a disk cache for driver-optimized Pipeline State Objects (PSOs).\n")
	TEXT("PSO descs are cached to disk so subsequent runs can create PSOs at load-time instead of at run-time.\n")
	TEXT("This cache contains data specific to the hardware, driver, and machine that it was created on.\n")
	TEXT("0 to disable the driver-optimized pipeline state disk cache\n")
	TEXT("1 to enable the driver-optimized pipeline state disk cache\n"),
	ECVF_ReadOnly);

FD3D12_GRAPHICS_PIPELINE_STATE_STREAM FD3D12_GRAPHICS_PIPELINE_STATE_DESC::PipelineStateStream() const
{
	FD3D12_GRAPHICS_PIPELINE_STATE_STREAM Stream = {};
	check(this->Flags == D3D12_PIPELINE_STATE_FLAG_NONE);	//Stream.Flags = this->Flags;
	Stream.NodeMask = this->NodeMask;
	Stream.pRootSignature = this->pRootSignature;
	Stream.InputLayout = this->InputLayout;
	Stream.IBStripCutValue = this->IBStripCutValue;
	Stream.PrimitiveTopologyType = this->PrimitiveTopologyType;
	Stream.VS = this->VS;
	Stream.GS = this->GS;
	Stream.StreamOutput = this->StreamOutput;
	Stream.HS = this->HS;
	Stream.DS = this->DS;
	Stream.PS = this->PS;
	Stream.BlendState = CD3DX12_BLEND_DESC(this->BlendState);
	Stream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(this->DepthStencilState);
	Stream.DSVFormat = this->DSVFormat;
	Stream.RasterizerState = CD3DX12_RASTERIZER_DESC(this->RasterizerState);
	Stream.RTVFormats = this->RTFormatArray;
	Stream.SampleDesc = this->SampleDesc;
	Stream.SampleMask = this->SampleMask;
	Stream.CachedPSO = this->CachedPSO;
	return Stream;
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC FD3D12_GRAPHICS_PIPELINE_STATE_DESC::GraphicsDescV0() const
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC D;
	D.Flags = this->Flags;
	D.NodeMask = this->NodeMask;
	D.pRootSignature = this->pRootSignature;
	D.InputLayout = this->InputLayout;
	D.IBStripCutValue = this->IBStripCutValue;
	D.PrimitiveTopologyType = this->PrimitiveTopologyType;
	D.VS = this->VS;
	D.GS = this->GS;
	D.StreamOutput = this->StreamOutput;
	D.HS = this->HS;
	D.DS = this->DS;
	D.PS = this->PS;
	D.BlendState = this->BlendState;
	D.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(this->DepthStencilState);
	D.DSVFormat = this->DSVFormat;
	D.RasterizerState = this->RasterizerState;
	D.NumRenderTargets = this->RTFormatArray.NumRenderTargets;
	FMemory::Memcpy(D.RTVFormats, this->RTFormatArray.RTFormats, sizeof(D.RTVFormats));
	D.SampleDesc = this->SampleDesc;
	D.SampleMask = this->SampleMask;
	D.CachedPSO = this->CachedPSO;
	return D;
}

FD3D12_COMPUTE_PIPELINE_STATE_STREAM FD3D12_COMPUTE_PIPELINE_STATE_DESC::PipelineStateStream() const
{
	FD3D12_COMPUTE_PIPELINE_STATE_STREAM Stream = {};
	check(this->Flags == D3D12_PIPELINE_STATE_FLAG_NONE);	//Stream.Flags = this->Flags;
	Stream.NodeMask = this->NodeMask;
	Stream.pRootSignature = this->pRootSignature;
	Stream.CS = this->CS;
	Stream.CachedPSO = this->CachedPSO;
	return Stream;
}

D3D12_COMPUTE_PIPELINE_STATE_DESC FD3D12_COMPUTE_PIPELINE_STATE_DESC::ComputeDescV0() const
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC D;
	D.Flags = this->Flags;
	D.NodeMask = this->NodeMask;
	D.pRootSignature = this->pRootSignature;
	D.CS = this->CS;
	D.CachedPSO = this->CachedPSO;
	return D;
}

void FD3D12PipelineStateCache::OnPSOCreated(FD3D12PipelineState* PipelineState, const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	// Actually create the PSO.
	PipelineState->Create(GraphicsPipelineCreationArgs(&Desc, PipelineLibrary.GetReference()));

	// Indicate we should add this PSO to necessary disk caches.
	PipelineState->MarkForDiskCacheAdd();
}

void FD3D12PipelineStateCache::OnPSOCreated(FD3D12PipelineState* PipelineState, const FD3D12ComputePipelineStateDesc& Desc)
{
	// Actually create the PSO.
	PipelineState->Create(ComputePipelineCreationArgs(&Desc, PipelineLibrary.GetReference()));

	// Indicate we should add this PSO to necessary disk caches.
	PipelineState->MarkForDiskCacheAdd();
}

void FD3D12PipelineStateCache::RebuildFromDiskCache(ID3D12RootSignature* GraphicsRootSignature, ID3D12RootSignature* ComputeRootSignature)
{
	FScopeLock Lock(&DiskCachesCS);

	UNREFERENCED_PARAMETER(GraphicsRootSignature);
	UNREFERENCED_PARAMETER(ComputeRootSignature);

	if (IsInErrorState())
	{
		// TODO: Make sure we clear the disk caches that are in error.
		return;
	}
	// The only time shader code is ever read back is on debug builds
	// when it checks for hash collisions in the PSO map. Therefore
	// there is no point backing the memory on release.
#if UE_BUILD_DEBUG
	static const bool bBackShadersWithSystemMemory = true;
#else
	static const bool bBackShadersWithSystemMemory = false;
#endif

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT); // Reset this one to the end as we always append

	FD3D12Adapter* Adapter = GetParentAdapter();

	const uint32 NumGraphicsPSOs = DiskCaches[PSO_CACHE_GRAPHICS].GetNumPSOs();
	UE_LOG(LogD3D12RHI, Log, TEXT("Reading %u Graphics PSO(s) from the disk cache."), NumGraphicsPSOs);
	for (uint32 i = 0; i < NumGraphicsPSOs; i++)
	{
		FD3D12LowLevelGraphicsPipelineStateDesc* Desc = nullptr;
		DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&Desc, sizeof(*Desc));
		FD3D12_GRAPHICS_PIPELINE_STATE_DESC* PSODesc = &Desc->Desc;

		Desc->pRootSignature = nullptr;
		SIZE_T* RSBlobLength = nullptr;
		DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&RSBlobLength, sizeof(*RSBlobLength));
		if (RSBlobLength && *RSBlobLength > 0)
		{
			FD3D12QuantizedBoundShaderState* QBSS = nullptr;
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&QBSS, sizeof(*QBSS));
			
			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			FD3D12RootSignature* const RootSignature = RootSignatureManager->GetRootSignature(*QBSS);
			PSODesc->pRootSignature = RootSignature->GetRootSignature();
			check(PSODesc->pRootSignature);
		}
		if (PSODesc->InputLayout.NumElements)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs, PSODesc->InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC), true);
			for (uint32 j = 0; j < PSODesc->InputLayout.NumElements; j++)
			{
				// Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->InputLayout.pInputElementDescs[j].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumEntries)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration, PSODesc->StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY), true);
			for (uint32 j = 0; j < PSODesc->StreamOutput.NumEntries; j++)
			{
				//Get the Sematic name string
				uint32* stringLength = nullptr;
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&stringLength, sizeof(uint32));
				DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pSODeclaration[j].SemanticName, *stringLength, true);
			}
		}
		if (PSODesc->StreamOutput.NumStrides)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->StreamOutput.pBufferStrides, PSODesc->StreamOutput.NumStrides * sizeof(uint32), true);
		}
		if (PSODesc->VS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->VS.pShaderBytecode, PSODesc->VS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->PS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->PS.pShaderBytecode, PSODesc->PS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->DS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->DS.pShaderBytecode, PSODesc->DS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->HS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->HS.pShaderBytecode, PSODesc->HS.BytecodeLength, bBackShadersWithSystemMemory);
		}
		if (PSODesc->GS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_GRAPHICS].SetPointerAndAdvanceFilePosition((void**)&PSODesc->GS.pShaderBytecode, PSODesc->GS.BytecodeLength, bBackShadersWithSystemMemory);
		}

		ReadBackShaderBlob(*PSODesc, PSO_CACHE_GRAPHICS);

		if (!DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState())
		{
			// Only reload PSOs that match the LDA mask, or else it will fail.
			if ((uint32)FRHIGPUMask::All() == Desc->Desc.NodeMask)
			{
				// Add PSO to low level cache.
				FD3D12PipelineState* PipelineState = nullptr;
				AddToLowLevelCache(*Desc, &PipelineState, [this](FD3D12PipelineState** PipelineState, const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
				{
					// Actually create the PSO.
					const GraphicsPipelineCreationArgs Args(&Desc, PipelineLibrary.GetReference());
					(*PipelineState)->CreateAsync(Args);

					// Sanity check that we won't add this back to the disk cache again.
					check(!(*PipelineState)->ShouldAddToDiskCache())
				});
			}
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}

	const uint32 NumComputePSOs = DiskCaches[PSO_CACHE_COMPUTE].GetNumPSOs();
	UE_LOG(LogD3D12RHI, Log, TEXT("Reading %u Compute PSO(s) from the disk cache."), NumComputePSOs);
	for (uint32 i = 0; i < NumComputePSOs; i++)
	{
		FD3D12ComputePipelineStateDesc* Desc = nullptr;
		DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&Desc, sizeof(*Desc));
		FD3D12_COMPUTE_PIPELINE_STATE_DESC* PSODesc = &Desc->Desc;

		Desc->pRootSignature = nullptr;
		SIZE_T* RSBlobLength = nullptr;
		DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&RSBlobLength, sizeof(*RSBlobLength));
		if (RSBlobLength && *RSBlobLength > 0)
		{
			FD3D12QuantizedBoundShaderState* QBSS = nullptr;
			DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&QBSS, sizeof(*QBSS));

			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			FD3D12RootSignature* const RootSignature = RootSignatureManager->GetRootSignature(*QBSS);
			PSODesc->pRootSignature = RootSignature->GetRootSignature();
			check(PSODesc->pRootSignature);
		}
		if (PSODesc->CS.BytecodeLength)
		{
			DiskCaches[PSO_CACHE_COMPUTE].SetPointerAndAdvanceFilePosition((void**)&PSODesc->CS.pShaderBytecode, PSODesc->CS.BytecodeLength, bBackShadersWithSystemMemory);
		}

		ReadBackShaderBlob(*PSODesc, PSO_CACHE_COMPUTE);

		if (!DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState())
		{
			// Only reload PSOs that match the LDA mask, or else it will fail.
			if ((uint32)FRHIGPUMask::All() == Desc->Desc.NodeMask)
			{
				Desc->CombinedHash = FD3D12PipelineStateCache::HashPSODesc(*Desc);

				// Add PSO to low level cache.
				FD3D12PipelineState* PipelineState = nullptr;
				AddToLowLevelCache(*Desc, &PipelineState, [&](FD3D12PipelineState* PipelineState, const FD3D12ComputePipelineStateDesc& Desc)
				{
					// Actually create the PSO.
					const ComputePipelineCreationArgs Args(&Desc, PipelineLibrary.GetReference());
					PipelineState->CreateAsync(Args);

					// Sanity check that we won't add this back to the disk cache again.
					check(!PipelineState->ShouldAddToDiskCache())
				});
			}
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("PSO Cache read error!"));
			break;
		}
	}
}

void FD3D12PipelineStateCache::AddToDiskCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc, FD3D12PipelineState* PipelineState)
{
	FScopeLock Lock(&DiskCachesCS);

	FDiskCacheInterface& DiskCache = DiskCaches[PSO_CACHE_GRAPHICS];
	const FD3D12_GRAPHICS_PIPELINE_STATE_DESC &PsoDesc = Desc.Desc;

	//TODO: Optimize by only storing unique pointers
	if (!DiskCache.IsInErrorState())
	{
		DiskCache.AppendData(&Desc, sizeof(Desc));

		ID3DBlob* const pRSBlob = Desc.pRootSignature ? Desc.pRootSignature->GetRootSignatureBlob() : nullptr;
		const SIZE_T RSBlobLength = pRSBlob ? pRSBlob->GetBufferSize() : 0;
		DiskCache.AppendData((void*)(&RSBlobLength), sizeof(RSBlobLength));
		if (RSBlobLength > 0)
		{
			// Save the quantized bound shader state so we can use the root signature manager to deduplicate and handle root signature creation.
			check(Desc.pRootSignature->GetRootSignature() == PsoDesc.pRootSignature);
			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			const FD3D12QuantizedBoundShaderState QBSS = RootSignatureManager->GetQuantizedBoundShaderState(Desc.pRootSignature);
			DiskCache.AppendData(&QBSS, sizeof(QBSS));
		}
		if (PsoDesc.InputLayout.NumElements)
		{
			//Save the layout structs
			DiskCache.AppendData((void*)PsoDesc.InputLayout.pInputElementDescs, PsoDesc.InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
			for (uint32 i = 0; i < PsoDesc.InputLayout.NumElements; i++)
			{
				//Save the Sematic name string
				uint32 stringLength = (uint32)strnlen_s(PsoDesc.InputLayout.pInputElementDescs[i].SemanticName, IL_MAX_SEMANTIC_NAME);
				stringLength++; // include the NULL char
				DiskCache.AppendData((void*)&stringLength, sizeof(stringLength));
				DiskCache.AppendData((void*)PsoDesc.InputLayout.pInputElementDescs[i].SemanticName, stringLength);
			}
		}
		if (PsoDesc.StreamOutput.NumEntries)
		{
			DiskCache.AppendData((void*)&PsoDesc.StreamOutput.pSODeclaration, PsoDesc.StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY));
			for (uint32 i = 0; i < PsoDesc.StreamOutput.NumEntries; i++)
			{
				//Save the Sematic name string
				uint32 stringLength = (uint32)strnlen_s(PsoDesc.StreamOutput.pSODeclaration[i].SemanticName, IL_MAX_SEMANTIC_NAME);
				stringLength++; // include the NULL char
				DiskCache.AppendData((void*)&stringLength, sizeof(stringLength));
				DiskCache.AppendData((void*)PsoDesc.StreamOutput.pSODeclaration[i].SemanticName, stringLength);
			}
		}
		if (PsoDesc.StreamOutput.NumStrides)
		{
			DiskCache.AppendData((void*)&PsoDesc.StreamOutput.pBufferStrides, PsoDesc.StreamOutput.NumStrides * sizeof(uint32));
		}
		if (PsoDesc.VS.BytecodeLength)
		{
			DiskCache.AppendData((void*)PsoDesc.VS.pShaderBytecode, PsoDesc.VS.BytecodeLength);
		}
		if (PsoDesc.PS.BytecodeLength)
		{
			DiskCache.AppendData((void*)PsoDesc.PS.pShaderBytecode, PsoDesc.PS.BytecodeLength);
		}
		if (PsoDesc.DS.BytecodeLength)
		{
			DiskCache.AppendData((void*)PsoDesc.DS.pShaderBytecode, PsoDesc.DS.BytecodeLength);
		}
		if (PsoDesc.HS.BytecodeLength)
		{
			DiskCache.AppendData((void*)PsoDesc.HS.pShaderBytecode, PsoDesc.HS.BytecodeLength);
		}
		if (PsoDesc.GS.BytecodeLength)
		{
			DiskCache.AppendData((void*)PsoDesc.GS.pShaderBytecode, PsoDesc.GS.BytecodeLength);
		}

		WriteOutShaderBlob(PSO_CACHE_GRAPHICS, PipelineState->GetPipelineState());

		DiskCache.Flush(DiskCache.GetNumPSOs() + 1);
	}
}

void FD3D12PipelineStateCache::AddToDiskCache(const FD3D12ComputePipelineStateDesc& Desc, FD3D12PipelineState* PipelineState)
{
	FScopeLock Lock(&DiskCachesCS);

	FDiskCacheInterface& DiskCache = DiskCaches[PSO_CACHE_COMPUTE];
	const FD3D12_COMPUTE_PIPELINE_STATE_DESC &PsoDesc = Desc.Desc;

	if (!DiskCache.IsInErrorState())
	{
		DiskCache.AppendData(&Desc, sizeof(Desc));

		ID3DBlob* const pRSBlob = Desc.pRootSignature ? Desc.pRootSignature->GetRootSignatureBlob() : nullptr;
		const SIZE_T RSBlobLength = pRSBlob ? pRSBlob->GetBufferSize() : 0;
		DiskCache.AppendData((void*)(&RSBlobLength), sizeof(RSBlobLength));
		if (RSBlobLength > 0)
		{
			// Save the quantized bound shader state so we can use the root signature manager to deduplicate and handle root signature creation.
			CA_SUPPRESS(6011);
			check(Desc.pRootSignature->GetRootSignature() == PsoDesc.pRootSignature);
			FD3D12RootSignatureManager* const RootSignatureManager = GetParentAdapter()->GetRootSignatureManager();
			const FD3D12QuantizedBoundShaderState QBSS = RootSignatureManager->GetQuantizedBoundShaderState(Desc.pRootSignature);
			DiskCache.AppendData(&QBSS, sizeof(QBSS));	
		}
		if (PsoDesc.CS.BytecodeLength)
		{
			DiskCache.AppendData((void*)PsoDesc.CS.pShaderBytecode, PsoDesc.CS.BytecodeLength);
		}

		WriteOutShaderBlob(PSO_CACHE_COMPUTE, PipelineState->GetPipelineState());

		DiskCache.Flush(DiskCache.GetNumPSOs() + 1);
	}
}

void FD3D12PipelineStateCache::WriteOutShaderBlob(PSO_CACHE_TYPE Cache, ID3D12PipelineState* APIPso)
{
	if (!DiskCaches[Cache].IsInErrorState() && !DiskBinaryCache.IsInErrorState())
	{
		if (UseCachedBlobs())
		{
			TRefCountPtr<ID3DBlob> cachedBlob;
			HRESULT result = APIPso->GetCachedBlob(cachedBlob.GetInitReference());
			VERIFYD3D12RESULT(result);
			if (SUCCEEDED(result))
			{
				SIZE_T bufferSize = cachedBlob->GetBufferSize();

				SIZE_T currentOffset = DiskBinaryCache.GetCurrentOffset();
				DiskBinaryCache.AppendData(cachedBlob->GetBufferPointer(), bufferSize);

				DiskCaches[Cache].AppendData(&currentOffset, sizeof(currentOffset));
				DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));

				DiskBinaryCache.Flush(DiskBinaryCache.GetNumPSOs() + 1);
			}
			else
			{
				check(false);
				SIZE_T bufferSize = 0;
				DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
				DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
			}
		}
		else
		{
			SIZE_T bufferSize = 0;
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
			DiskCaches[Cache].AppendData(&bufferSize, sizeof(bufferSize));
		}
	}
}

void FD3D12PipelineStateCache::Close()
{
	FScopeLock Lock(&DiskCachesCS);

	FDiskCacheInterface& DiskGraphicsCache = DiskCaches[PSO_CACHE_GRAPHICS];
	FDiskCacheInterface& DiskComputeCache = DiskCaches[PSO_CACHE_COMPUTE];

	DiskGraphicsCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);
	DiskComputeCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);

	// Write graphics PSOs to the disk cache(s).
	if (!DiskGraphicsCache.IsInErrorState())
	{
		const uint32 NumGraphicsPSOsBefore = DiskGraphicsCache.GetNumPSOs();
		for (auto Iter = LowLevelGraphicsPipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			FD3D12PipelineState* const PipelineState = Iter.Value();
			if (PipelineState->ShouldAddToDiskCache())
			{
				const FD3D12LowLevelGraphicsPipelineStateDesc& Desc = Iter.Key();
				AddToDiskCache(Desc, PipelineState);
			}
		}

		const uint32 NumGraphicsPSOsAfter = DiskGraphicsCache.GetNumPSOs();
		const uint32 NumNewGraphicsPSOs = NumGraphicsPSOsAfter - NumGraphicsPSOsBefore;
		UE_CLOG(NumNewGraphicsPSOs, LogD3D12RHI, Log, TEXT("Added %u new Graphics PSO(s) to the disk cache."), NumNewGraphicsPSOs);
		UE_LOG(LogD3D12RHI, Log, TEXT("Closing Graphics PSO disk cache. Cache contains %u PSO(s)."), NumGraphicsPSOsAfter);
		DiskGraphicsCache.Close(NumGraphicsPSOsAfter);
	}

	// Write compute PSOs to the disk cache(s).
	if (!DiskComputeCache.IsInErrorState())
	{
		const uint32 NumComputePSOsBefore = DiskComputeCache.GetNumPSOs();
		for (auto Iter = ComputePipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			FD3D12PipelineState* const PipelineState = Iter.Value();
			if (PipelineState->ShouldAddToDiskCache())
			{
				const FD3D12ComputePipelineStateDesc& Desc = Iter.Key();
				AddToDiskCache(Desc, PipelineState);
			}
		}

		const uint32 NumComputePSOsAfter = DiskComputeCache.GetNumPSOs();
		const uint32 NumNewComputePSOs = NumComputePSOsAfter - NumComputePSOsBefore;
		UE_CLOG(NumNewComputePSOs, LogD3D12RHI, Log, TEXT("Added %u new Compute PSO(s) to the disk cache."), NumNewComputePSOs);
		UE_LOG(LogD3D12RHI, Log, TEXT("Closing Compute PSO disk cache. Cache contains %u PSO(s)."), NumComputePSOsAfter);
		DiskComputeCache.Close(NumComputePSOsAfter);
	}

	// Write driver-optimized PSOs to the disk cache.
	TArray<BYTE> LibraryData;
	const bool bOverwriteExistingPipelineLibrary = true;
	if (UsePipelineLibrary() && bOverwriteExistingPipelineLibrary)
	{
		// Serialize the Library.
		const SIZE_T LibrarySize = PipelineLibrary->GetSerializedSize();
		if (LibrarySize)
		{
			LibraryData.AddUninitialized(LibrarySize);
			check(LibraryData.Num() == LibrarySize);

			UE_LOG(LogD3D12RHI, Log, TEXT("Serializing Pipeline Library to disk (%llu KiB)."), LibrarySize / 1024ll);
			VERIFYD3D12RESULT(PipelineLibrary->Serialize(LibraryData.GetData(), LibrarySize));

			// Write the Library to disk (overwrite existing data).
			DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
			const bool bSuccess = DiskBinaryCache.AppendData(LibraryData.GetData(), LibrarySize);
			UE_CLOG(!bSuccess, LogD3D12RHI, Warning, TEXT("Failed to write Pipeline Library to disk."));
		}
	}

	DiskBinaryCache.Close(0);

	CleanupPipelineStateCaches();
}

void FD3D12PipelineStateCache::Init(FString& GraphicsCacheFileName, FString& ComputeCacheFileName, FString& DriverBlobFileName)
{
	FScopeLock Lock(&DiskCachesCS);

	const bool bEnableGeneralPipelineStateDiskCaches = CVarPipelineStateDiskCache.GetValueOnAnyThread() != 0;
	UE_CLOG(!bEnableGeneralPipelineStateDiskCaches, LogD3D12RHI, Display, TEXT("Not using pipeline state disk cache per r.D3D12.PSO.DiskCache=0"));
	
	const bool bEnableDriverOptimizedPipelineStateDiskCaches = CVarDriverOptimizedPipelineStateDiskCache.GetValueOnAnyThread() != 0;
	UE_CLOG(!bEnableDriverOptimizedPipelineStateDiskCaches, LogD3D12RHI, Display, TEXT("Not using driver-optimized pipeline state disk cache per r.D3D12.PSO.DriverOptimizedDiskCache=0"));
	bUseAPILibaries = bEnableDriverOptimizedPipelineStateDiskCaches;

	DiskCaches[PSO_CACHE_GRAPHICS].Init(GraphicsCacheFileName, bEnableGeneralPipelineStateDiskCaches);
	DiskCaches[PSO_CACHE_COMPUTE].Init(ComputeCacheFileName, bEnableGeneralPipelineStateDiskCaches);
	DiskBinaryCache.Init(DriverBlobFileName, bEnableDriverOptimizedPipelineStateDiskCaches);

	DiskCaches[PSO_CACHE_GRAPHICS].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskCaches[PSO_CACHE_COMPUTE].Reset(FDiskCacheInterface::RESET_TO_FIRST_OBJECT);
	DiskBinaryCache.Reset(FDiskCacheInterface::RESET_TO_AFTER_LAST_OBJECT);
	
	if (bUseAPILibaries)
	{
		// Create a pipeline library if the system supports it.
		ID3D12Device1* pDevice1 = GetParentAdapter()->GetD3DDevice1();
		if (pDevice1)
		{
			const SIZE_T LibrarySize = DiskBinaryCache.GetSizeInBytes();
			void* pLibraryBlob = LibrarySize ? DiskBinaryCache.GetDataAtStart() : nullptr;

			if (pLibraryBlob)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Creating Pipeline Library from existing disk cache (%llu KiB)."), LibrarySize / 1024ll);
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Creating new Pipeline Library."));
			}

			const HRESULT HResult = pDevice1->CreatePipelineLibrary(pLibraryBlob, LibrarySize, IID_PPV_ARGS(PipelineLibrary.GetInitReference()));

			// E_INVALIDARG if the blob is corrupted or unrecognized. D3D12_ERROR_DRIVER_VERSION_MISMATCH if the provided data came from 
			// an old driver or runtime. D3D12_ERROR_ADAPTER_NOT_FOUND if the data came from different hardware.
			if (DXGI_ERROR_UNSUPPORTED == HResult)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("The driver doesn't support Pipeline Libraries."));
			}
			else if (FAILED(HResult))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Create Pipeline Library failed. Perhaps the Library has stale PSOs for the current HW or driver. Clearing the disk cache and trying again..."));

				// TODO: In the case of D3D12_ERROR_ADAPTER_NOT_FOUND, we don't really need to clear the cache, we just need to try another one. We should really have a cache per adapter.
				DiskBinaryCache.ClearAndReinitialize();
				check(DiskBinaryCache.GetSizeInBytes() == 0);

				VERIFYD3D12RESULT(pDevice1->CreatePipelineLibrary(nullptr, 0, IID_PPV_ARGS(PipelineLibrary.GetInitReference())));
			}

			SetName(PipelineLibrary, L"Pipeline Library");
		}
	}
}

bool FD3D12PipelineStateCache::IsInErrorState() const
{
	return (DiskCaches[PSO_CACHE_GRAPHICS].IsInErrorState() ||
		DiskCaches[PSO_CACHE_COMPUTE].IsInErrorState() ||
		(bUseAPILibaries && DiskBinaryCache.IsInErrorState()));
}

FD3D12PipelineStateCache::FD3D12PipelineStateCache(FD3D12Adapter* InParent)
	: FD3D12PipelineStateCacheBase(InParent)
	, bUseAPILibaries(true)
{
}

FD3D12PipelineStateCache::~FD3D12PipelineStateCache()
{
}

template ID3D12PipelineState* CreatePipelineStateWrapper<GraphicsPipelineCreationArgs_POD>(FD3D12Adapter* Adapter, const GraphicsPipelineCreationArgs_POD* CreationArgs);
template ID3D12PipelineState* CreatePipelineStateWrapper<ComputePipelineCreationArgs_POD>(FD3D12Adapter* Adapter, const ComputePipelineCreationArgs_POD* CreationArgs);

template <typename TDesc>
ID3D12PipelineState* CreatePipelineStateWrapper(FD3D12Adapter* Adapter, const TDesc* CreationArgs)
{
	// Get the pipeline state name, currently based on the hash.
	FString Name = FString::Printf(TEXT("%llu"), CreationArgs->Desc->CombinedHash);

	// Use pipeline streams if the system supports it.
	ID3D12Device2* const pDevice2 = Adapter->GetD3DDevice2();
	if (pDevice2)
	{
		typename TPSOStreamFunctionMap<TDesc>::D3D12PipelineStateStreamType Stream = CreationArgs->Desc->Desc.PipelineStateStream();
		const D3D12_PIPELINE_STATE_STREAM_DESC StreamDesc = { sizeof(Stream), &Stream };
		return CreatePipelineStateFromStream<TDesc>(pDevice2, &StreamDesc, static_cast<ID3D12PipelineLibrary1*>(CreationArgs->Library), *Name);	// Static cast to ID3D12PipelineLibrary1 since we already checked for ID3D12Device2.
	}
	else
	{
		// Use the older pipeline descs.
		const typename TPSOStreamFunctionMap<TDesc>::D3D12PipelineStateDescV0Type Desc = (CreationArgs->Desc->Desc.*TPSOStreamFunctionMap<TDesc>::GetPipelineStateDescV0())();
		return CreatePipelineState(Adapter->GetD3DDevice(), &Desc, CreationArgs->Library, *Name);
	}
}

void FD3D12PipelineState::Create(const ComputePipelineCreationArgs& InCreationArgs)
{
	check(PipelineState.GetReference() == nullptr);
	PipelineState = CreatePipelineStateWrapper(GetParentAdapter(), &InCreationArgs.Args);
}

void FD3D12PipelineState::CreateAsync(const ComputePipelineCreationArgs& InCreationArgs)
{
	check(PipelineState.GetReference() == nullptr && Worker == nullptr);
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentAdapter(), InCreationArgs);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

void FD3D12PipelineState::Create(const GraphicsPipelineCreationArgs& InCreationArgs)
{
	check(PipelineState.GetReference() == nullptr);
	PipelineState = CreatePipelineStateWrapper(GetParentAdapter(), &InCreationArgs.Args);
}

void FD3D12PipelineState::CreateAsync(const GraphicsPipelineCreationArgs& InCreationArgs)
{
	check(PipelineState.GetReference() == nullptr && Worker == nullptr);
	Worker = new FAsyncTask<FD3D12PipelineStateWorker>(GetParentAdapter(), InCreationArgs);
	if (Worker)
	{
		Worker->StartBackgroundTask();
	}
}

void FD3D12PipelineStateWorker::DoWork()
{
	if (bIsGraphics)
	{
		PSO = CreatePipelineStateWrapper(GetParentAdapter(), &CreationArgs.GraphicsArgs);
	}
	else
	{
		PSO = CreatePipelineStateWrapper(GetParentAdapter(), &CreationArgs.ComputeArgs);
	}
}

#if LOG_PSO_CREATES
	/** Accumulative time spent creating pipeline states. */
	FTotalTimeAndCount GD3D12CreatePSOTime;
#endif

DECLARE_CYCLE_STAT(TEXT("Create time"), STAT_PSOCreateTime, STATGROUP_D3D12PipelineState);

template ID3D12PipelineState* CreatePipelineState<D3D12_GRAPHICS_PIPELINE_STATE_DESC>(ID3D12Device* Device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* Desc, ID3D12PipelineLibrary* Library, const TCHAR* Name);
template ID3D12PipelineState* CreatePipelineState<D3D12_COMPUTE_PIPELINE_STATE_DESC>(ID3D12Device* Device, const D3D12_COMPUTE_PIPELINE_STATE_DESC* Desc, ID3D12PipelineLibrary* Library, const TCHAR* Name);

// Thread-safe create graphics/compute pipeline state. Conditionally load/store the PSO using a Pipeline Library.
template <typename TDesc>
ID3D12PipelineState* CreatePipelineState(ID3D12Device* Device, const TDesc* Desc, ID3D12PipelineLibrary* Library, const TCHAR* Name)
{
#if LOG_PSO_CREATES
	const FString CreatePipelineStateMessage = FString::Printf(TEXT("CreatePipelineState (%s, Hash = %s)"), *TPSOFunctionMap<TDesc>::GetString(), Name);
	SCOPE_LOG_TIME(*CreatePipelineStateMessage, &GD3D12CreatePSOTime);
#endif

	ID3D12PipelineState* PSO = nullptr;
	if (Library)
	{
		// Try to load the PSO from the library.
		check(Name);
		HRESULT HResult = (Library->*TPSOFunctionMap<TDesc>::GetLoadPipeline())(Name, Desc, IID_PPV_ARGS(&PSO));
		if (E_INVALIDARG == HResult)
		{
			// The name doesn't exist or the input desc doesn't match the data in the library, just create the PSO.
			{
				SCOPE_CYCLE_COUNTER(STAT_PSOCreateTime);
				VERIFYD3D12RESULT((Device->*TPSOFunctionMap<TDesc>::GetCreatePipelineState())(Desc, IID_PPV_ARGS(&PSO)));
			}

			// Try to save the PSO to the library for another time.
			check(PSO);
			HResult = Library->StorePipeline(Name, PSO);
			if (E_INVALIDARG != HResult)
			{
				// E_INVALIDARG means the name already exists in the library. Since the name is based on the hash, this is a hash collision.
				// We ignore E_INVALIDARG because we just create PSO's if they don't exist in the library.
				VERIFYD3D12RESULT(HResult);
			}
		}
		else
		{
			VERIFYD3D12RESULT(HResult);
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_PSOCreateTime);
		HRESULT r = (Device->*TPSOFunctionMap<TDesc>::GetCreatePipelineState())(Desc, IID_PPV_ARGS(&PSO));
		if (FAILED(r))
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("Failed to create PipelineState %s with hash %s"), *TPSOFunctionMap<TDesc>::GetString(), Name);
			return nullptr;
		}
	}

	check(PSO);
	return PSO;
}

template ID3D12PipelineState* CreatePipelineStateFromStream<GraphicsPipelineCreationArgs_POD>(ID3D12Device2* Device, const D3D12_PIPELINE_STATE_STREAM_DESC* Desc, ID3D12PipelineLibrary1* Library, const TCHAR* Name);
template ID3D12PipelineState* CreatePipelineStateFromStream<ComputePipelineCreationArgs_POD>(ID3D12Device2* Device, const D3D12_PIPELINE_STATE_STREAM_DESC* Desc, ID3D12PipelineLibrary1* Library, const TCHAR* Name);

// Thread-safe create graphics/compute pipeline state. Conditionally load/store the PSO using a Pipeline Library.
template <typename TDesc>
ID3D12PipelineState* CreatePipelineStateFromStream(ID3D12Device2* Device, const D3D12_PIPELINE_STATE_STREAM_DESC* Desc, ID3D12PipelineLibrary1* Library, const TCHAR* Name)
{
#if LOG_PSO_CREATES
	const FString CreatePipelineStateMessage = FString::Printf(TEXT("CreatePipelineState (%s, Hash = %s)"), *TPSOStreamFunctionMap<TDesc>::GetString(), Name);
	SCOPE_LOG_TIME(*CreatePipelineStateMessage, &GD3D12CreatePSOTime);
#endif

	ID3D12PipelineState* PSO = nullptr;
	if (Library)
	{
		// Try to load the PSO from the library.
		check(Name);
		HRESULT HResult = Library->LoadPipeline(Name, Desc, IID_PPV_ARGS(&PSO));
		if (E_INVALIDARG == HResult)
		{
			// The name doesn't exist or the input desc doesn't match the data in the library, just create the PSO.
			{
				SCOPE_CYCLE_COUNTER(STAT_PSOCreateTime);
				VERIFYD3D12RESULT(Device->CreatePipelineState(Desc, IID_PPV_ARGS(&PSO)));
			}

			// Try to save the PSO to the library for another time.
			check(PSO);
			HResult = Library->StorePipeline(Name, PSO);
			if (E_INVALIDARG != HResult)
			{
				// E_INVALIDARG means the name already exists in the library. Since the name is based on the hash, this is a hash collision.
				// We ignore E_INVALIDARG because we just create PSO's if they don't exist in the library.
				VERIFYD3D12RESULT(HResult);
			}
		}
		else
		{
			VERIFYD3D12RESULT(HResult);
		}
	}
	else
	{
		SCOPE_CYCLE_COUNTER(STAT_PSOCreateTime);
		HRESULT r = Device->CreatePipelineState(Desc, IID_PPV_ARGS(&PSO));

		if (FAILED(r))
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("Failed to create PipelineState"));
			return nullptr;
		}
	}

	check(PSO);
	return PSO;
}
