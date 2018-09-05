/// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "stdafx.h"
namespace BlackmagicDevice
{

	/*
	 * Video Frame methods
	 */
	VIDEOIO_API void VideoIOReleaseFrame(FFrame InFrame)
	{
		PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(InFrame);
		Frame->PrivatePort->ReleaseFrame(Frame);
	}

	VIDEOIO_API FUInt VideoIOFrameDimensions(FFrame InFrame, FUInt& OutWidth, FUInt& OutHeight)
	{
		PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(InFrame);

		if (Frame->PrivatePort->IsInput())
		{

			OutWidth = Frame->DeckLinkVideoInputFrame->GetWidth();
			if (Frame->PrivatePort->IsPixelFormat(EPixelFormat::PF_UYVY))
			{
				OutWidth /= 2;
			}
			OutHeight = Frame->DeckLinkVideoInputFrame->GetHeight();
			return Frame->DeckLinkVideoInputFrame->GetRowBytes();
		}
		else
		{
			// different pointer for input output
			OutWidth = Frame->DeckLinkMutableVideoFrame->GetWidth();
			if (Frame->PrivatePort->IsPixelFormat(EPixelFormat::PF_UYVY))
			{
				OutWidth /= 2;
			}
			OutHeight = Frame->DeckLinkMutableVideoFrame->GetHeight();
			return Frame->DeckLinkMutableVideoFrame->GetRowBytes();
		}
	}

	VIDEOIO_API FUByte* VideoIOFrameVideoBuffer(FFrame InFrame, FUInt& OutSize)
	{
		PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(InFrame);

		if (Frame->PrivatePort->IsInput())
		{
			FUInt Pitch = Frame->DeckLinkVideoInputFrame->GetRowBytes();
			FUInt Height = Frame->DeckLinkVideoInputFrame->GetHeight();
			OutSize = Pitch*Height;
			void* Buffer;
			ComCheck(Frame->DeckLinkVideoInputFrame->GetBytes(&Buffer));
			return reinterpret_cast<FUByte*>(Buffer);
		}
		// assume output
		FUInt Pitch = Frame->DeckLinkMutableVideoFrame->GetRowBytes();
		FUInt Height = Frame->DeckLinkMutableVideoFrame->GetHeight();
		OutSize = Pitch*Height;
		void* Buffer;
		ComCheck(Frame->DeckLinkMutableVideoFrame->GetBytes(&Buffer));
		return reinterpret_cast<FUByte*>(Buffer);
	}


	VIDEOIO_API int32_t* VideoIOFrameAudioBuffer(FFrame InFrame, FUInt& OutSize, FUInt& OutNumChannels, FUInt& OutAudioRate, FUInt& OutNumSamples)
	{
		PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(InFrame);
		// if no audio packet attached to this frame
		if (!Frame->ActiveAudioSamples)
		{
			OutSize = OutNumChannels = OutAudioRate = OutNumSamples = 0;
			return 0;
		}

		Frame->PrivatePort->GetAudioFormat(OutNumChannels, OutAudioRate);

		OutNumSamples = Frame->ActiveAudioSamples;
		OutSize = OutNumChannels * OutNumSamples;

		return reinterpret_cast<int32_t*>(Frame->AudioFrame);
	}

	VIDEOIO_API FUByte* VideoIOFrameMetaBuffer(FFrame InFrame, FUInt& OutSize)
	{
		OutSize = 0;
		return nullptr;
	}

	VIDEOIO_API void VideoIOFrameTimecode(FFrame InFrame, FTimecode& Timecode)
	{
		PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(InFrame);
		if (Frame->PrivatePort->IsInput())
		{
			Timecode = Frame->Timecode;
		}
		else
		{
			Frame->Timecode = Timecode;
		}
	}

	VIDEOIO_API void VideoIOFrameDesc(FFrame InFrame, FFrameDesc& OutFrameDesc)
	{
		PrivateFrame* Frame = reinterpret_cast<PrivateFrame*>(InFrame);
		OutFrameDesc = Frame->FrameDesc;
	}

	PrivateFrame::~PrivateFrame()
	{
	}

};
