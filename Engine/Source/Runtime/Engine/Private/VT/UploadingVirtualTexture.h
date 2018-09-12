#pragma once

#include "VirtualTextureTypes.h"

class FChunkProvider;

// VT that is uploading from cpu to gpu
class FUploadingVirtualTexture : public IVirtualTexture
{
	friend struct FVirtualTextureChunkStreamingManager;
public:
	FUploadingVirtualTexture(uint32 SizeX, uint32 SizeY, uint32 SizeZ);
	virtual ~FUploadingVirtualTexture() {}

	virtual bool	LocatePageData(uint8 vLevel, uint64 vAddress, void* RESTRICT& Location) /*const*/ override;
	virtual IVirtualTextureProducer*	ProducePageData(FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint8 vLevel, uint64 vAddress, uint16 pAddress, void* RESTRICT Location) /*const*/ override;
	virtual void    DumpToConsole() override;

private:
	FChunkProvider * Provider;
};