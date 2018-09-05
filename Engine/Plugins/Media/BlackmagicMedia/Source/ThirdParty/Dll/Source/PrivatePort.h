// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

template <class T> class RefPointer;
struct IDeckLinkInput;

namespace BlackmagicDevice
{

	class PrivateDevice;

	/*
	 * Input/Output Video port
	 */
	class PrivatePort;

	/*
	 * Frame arrived callback
	 */
	class PrivateInputCallback : public IDeckLinkInputCallback
	{
	public:
		PrivateInputCallback(PrivatePort* InPort);

		ULONG AddRef() override;
		ULONG Release() override;
		HRESULT QueryInterface(REFIID InRIID, void **OutObject) override;

		HRESULT VideoInputFormatChanged(BMDVideoInputFormatChangedEvents InNotificationEvents, IDeckLinkDisplayMode *InNewDisplayMode, BMDDetectedVideoInputFormatFlags InDetectedSignalFlags) override;
		HRESULT VideoInputFrameArrived(IDeckLinkVideoInputFrame* InVideoFrame, IDeckLinkAudioInputPacket* InAudioPacket) override;
	protected:
		Thread::FAtomic Count;
		// weak pointer
		PrivatePort* Port;
	};

	class PrivateOutputCallback : public IDeckLinkVideoOutputCallback
	{
	public:
		PrivateOutputCallback(PrivatePort* InPort);

		ULONG AddRef() override;
		ULONG Release() override;
		HRESULT QueryInterface(REFIID InRIID, void **OutObject) override;

		HRESULT ScheduledFrameCompleted(IDeckLinkVideoFrame* completedFrame, BMDOutputFrameCompletionResult result) override;
		HRESULT ScheduledPlaybackHasStopped() override;
	
	protected:
		Thread::FAtomic Count;
		// weak pointer
		PrivatePort* Port;
	};

	struct FSupportedDescription;

	class PrivatePort : public RefCount
	{
	public:
		PrivatePort(PrivateDevice* InDevice, FUInt InPortIndex, bool InDirection);
		~PrivatePort();

		bool Init(const FFrameDesc& InFrameDesc, const FPortOptions& InOptions);
		bool Deinit(const FPortOptions& InOptions);

		FUInt FrameSize();
		FUInt FrameDimensions(FUInt& OutWidth, FUInt& OutHeight);

		bool Start(FUInt InFrames);
		bool Stop();

		bool WaitVSync();

		bool PeekFrame();
		FFrame WaitFrame();
		void ReleaseFrame(PrivateFrame* InFrame);

		FUInt DropCount() const;

		HRESULT VideoInputFormatChanged(BMDVideoInputFormatChangedEvents InNotificationEvents, IDeckLinkDisplayMode *InNewDisplayMode, BMDDetectedVideoInputFormatFlags InDetectedSignalFlags);
		HRESULT VideoInputFrameArrived(IDeckLinkVideoInputFrame* InVideoFrame, IDeckLinkAudioInputPacket* InAudioPacket);
		HRESULT ScheduledFrameCompleted();

		bool IsInput() const;
		bool IsOutput() const;
		void GetAudioFormat(FUInt& OutChannels, FUInt& OutSampleRate) const;
		bool IsPixelFormat(EPixelFormat InFormat) const;
		FUInt GetPortIndex() const;
		PrivateDevice* GetDevice();

		bool GetTimecode(FTimecode& outTimecode) const;

		bool AddCallback(IPortCallback* InCallback);
		bool RemCallback(IPortCallback* InCallback);

	protected:
		bool Share(const FPortOptions& InOptions);
		bool Unshare(const FPortOptions& InOptions);

	protected:
		bool bInitializationCompleted;
		bool InvokeInitializationCompleted(bool bSucceed);
		bool InvokeOnFrameArrived(FFrame InFrame);

	protected:

		Thread::FLock Lock;
		FUInt Started;

		FPortOptions Options;

		FUInt AudioChannels;
		FUInt AudioSampleRate;

		bool Input;
		bool Output;

		FTimecode Timecode;

		BMDTimeValue OutputTime;
		BMDTimeValue OutputFrameTime;
		BMDTimeScale OutputScale;

		RefPointer<PrivateDevice> Device;

		FUInt PortIndex;

		FSupportedDescription* SupportedDesc;
		FFrameDesc FrameDesc;
		FFrameInfo FrameInfo;

		BMDVideoInputFlags InputFlags;
		RefPointer<IDeckLinkInput> DeckLinkInput;
		RefPointer<PrivateInputCallback> InputHandler;

		RefPointer<IDeckLinkOutput> DeckLinkOutput;
		RefPointer<PrivateOutputCallback> OutputHandler;

		RefPointer<IDeckLinkKeyer> DeckLinkKeyer;

		std::unique_ptr<PrivateFrame[]> Frames;
		std::unique_ptr<int32_t[]> AudioFrames;

		Thread::FMailbox FreeFrames;
		Thread::FMailbox InFlightFrames;
		Thread::FMailbox FullFrames;

		volatile FUInt DroppedFrames;
		Thread::FLock VSyncLock;
		Thread::FEvent VSyncEvent;

		std::vector<IPortCallback*> PortCallbacks;
	};

};