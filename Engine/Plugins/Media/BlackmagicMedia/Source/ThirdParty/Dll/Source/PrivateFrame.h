// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
namespace BlackmagicDevice
{

	/*
	 * Frame in memory
	 */
	class PrivatePort;

	class PrivateFrame : public Thread::FMessage
	{
	public:
		~PrivateFrame();

	public:
		RefPointer<PrivatePort> PrivatePort;
		RefPointer<IDeckLinkVideoInputFrame> DeckLinkVideoInputFrame;
		RefPointer<IDeckLinkMutableVideoFrame> DeckLinkMutableVideoFrame;

		FUInt AudioSamples;
		FUInt ActiveAudioSamples;
		void *AudioFrame;

		FTimecode Timecode;
		FFrameDesc FrameDesc;
	};

};
