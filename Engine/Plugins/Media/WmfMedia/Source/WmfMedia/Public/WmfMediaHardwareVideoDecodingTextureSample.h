// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Player/WmfMediaTextureSample.h"
#include "RHI.h"

#include "IMediaTextureSampleConverter.h"
#include "WmfMediaHardwareVideoDecodingRendering.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "d3d11.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "RenderUtils.h"

#include "Windows/COMPointer.h"

/**
 * Texture sample for hardware video decoding.
 */
class FWmfMediaHardwareVideoDecodingTextureSample :
	public FWmfMediaTextureSample, 
	public IMediaTextureSampleConverter
{
public:

	/** Default constructor. */
	FWmfMediaHardwareVideoDecodingTextureSample()
		: FWmfMediaTextureSample()
	{ }

public:

	/**
	 * Initialize shared texture from Wmf device
	 *
	 * @param InD3D11Device WMF device to create shared texture from
	 * @param InTime The sample time (in the player's local clock).
	 * @param InDuration The sample duration
	 * @param InDim texture dimension to create
	 * @param InFormat texture format to create
	 * @param InMediaTextureSampleFormat Media texture sample format
	 * @param InCreateFlags texture create flag
	 * @return The texture resource object that will hold the sample data.
	 */
	ID3D11Texture2D* InitializeSourceTexture(ID3D11Device* InD3D11Device, FTimespan InTime, FTimespan InDuration, const FIntPoint& InDim, uint8 InFormat, EMediaTextureSampleFormat InMediaTextureSampleFormat)
	{
		Time = InTime;
		Dim = InDim;
		OutputDim = InDim;
		Duration = InDuration;
		SampleFormat = InMediaTextureSampleFormat;

		if (SourceTexture.IsValid())
		{
			return SourceTexture;
		}

		D3D11_TEXTURE2D_DESC TextureDesc;
		TextureDesc.Width = Dim.X;
		TextureDesc.Height = Dim.Y;
		TextureDesc.MipLevels = 1;
		TextureDesc.ArraySize = 1;
		TextureDesc.Format = (DXGI_FORMAT)GPixelFormats[InFormat].PlatformFormat;
		TextureDesc.SampleDesc.Count = 1;
		TextureDesc.SampleDesc.Quality = 0;
		TextureDesc.Usage = D3D11_USAGE_DEFAULT;
		TextureDesc.BindFlags = 0;
		TextureDesc.CPUAccessFlags = 0;
		TextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

		InD3D11Device->CreateTexture2D(&TextureDesc, nullptr, &SourceTexture);

		return SourceTexture;
	}

	/**
	 * Get media texture sample converter if sample implements it
	 *
	 * @return texture sample converter
	 */
	virtual IMediaTextureSampleConverter* GetMediaTextureSampleConverter() override
	{
		return this;
	}

	/**
	 * Texture sample convert using hardware video decoding.
	 */
	virtual void Convert(FTexture2DRHIRef InDstTexture) override
	{
		FWmfMediaHardwareVideoDecodingParameters::ConvertTextureFormat_RenderThread(this, InDstTexture);
	}

	/**
	 * Get source texture from WMF device
	 *
	 * @return Source texture
	 */
	TComPtr<ID3D11Texture2D> GetSourceTexture() const
	{
		return SourceTexture;
	}

	/**
	 * Set Destination Texture of rendering device
	 *
	 * @param InTexture Destination texture
	 */
	void SetDestinationTexture(FTexture2DRHIRef InTexture)
	{
		DestinationTexture = InTexture;
	}

	/**
	 * Get Destination Texture of render thread device
	 *
	 * @return Destination texture 
	 */
	FTexture2DRHIRef GetDestinationTexture() const
	{
		return DestinationTexture;
	}

private:

	/** Source Texture resource (from Wmf device). */
	TComPtr<ID3D11Texture2D> SourceTexture;

	/** Destination Texture resource (from Rendering device) */
	FTexture2DRHIRef DestinationTexture;
};

/** Implements a pool for WMF texture samples. */
class FWmfMediaHardwareVideoDecodingTextureSamplePool : public TMediaObjectPool<FWmfMediaHardwareVideoDecodingTextureSample> { };

