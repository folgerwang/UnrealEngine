// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NetworkAudioCapturer.h"
#include "Logging.h"

// These are copied from webrtc internals
#define CHECKinitialized_() \
	{                       \
		if (!bInitialized)  \
		{                   \
			return -1;      \
		};                  \
	}
#define CHECKinitialized__BOOL() \
	{                            \
		if (!bInitialized)       \
		{                        \
			return false;        \
		};                       \
	}

#define LOGFUNC() EG_LOG(LogDefault, Log, "%s", __FUNCTION__)

void FNetworkAudioCapturer::ProcessPacket(PixelStreamingProtocol::EToProxyMsg PkType, const void* Data, uint32_t Size)
{
	if (PkType != PixelStreamingProtocol::EToProxyMsg::AudioPCM)
	{
		return;
	}

	if (!(bInitialized && bRecordingInitialized))
	{
		return;
	}

	auto PkData = static_cast<const uint8_t*>(Data);

	RecordingBuffer.insert(RecordingBuffer.end(), PkData, PkData + Size);
	int BytesPer10Ms = (SampleRate * Channels * static_cast<int>(sizeof(uint16_t))) / 100;

	// Feed in 10ms chunks
	while (RecordingBuffer.size() >= BytesPer10Ms)
	{
		// If this check fails, then it means we tried to use it after it was
		// destroyed in "Terminate". If so, then we should use a mutex around it
		// so we are either destroying it, or using it.
		// The way the objects and threads interact, and the way shutdown is done,
		// it shouldn't happen, but nevertheless, having the check doesn't hurt.
		check(DeviceBuffer);
		if (DeviceBuffer)
		{
			DeviceBuffer->SetRecordedBuffer(RecordingBuffer.data(), BytesPer10Ms / (sizeof(uint16_t) * Channels));
			DeviceBuffer->DeliverRecordedData();
		}

		RecordingBuffer.erase(RecordingBuffer.begin(), RecordingBuffer.begin() + BytesPer10Ms);
	}
}

int32_t FNetworkAudioCapturer::ActiveAudioLayer(AudioLayer* audioLayer) const
{
	//LOGFUNC();
	*audioLayer = AudioDeviceModule::kDummyAudio;
	return 0;
}

int32_t FNetworkAudioCapturer::RegisterAudioCallback(webrtc::AudioTransport* audioCallback)
{
	//LOGFUNC();
	DeviceBuffer->RegisterAudioCallback(audioCallback);
	return 0;
}

int32_t FNetworkAudioCapturer::Init()
{
	//LOGFUNC();
	if (bInitialized)
		return 0;

	DeviceBuffer = std::make_unique<webrtc::AudioDeviceBuffer>();

	bInitialized = true;
	return 0;
}

int32_t FNetworkAudioCapturer::Terminate()
{
	//LOGFUNC();
	if (!bInitialized)
		return 0;

	DeviceBuffer.reset();

	bInitialized = false;
	return 0;
}

bool FNetworkAudioCapturer::Initialized() const
{
	//LOGFUNC();
	return bInitialized;
}

int16_t FNetworkAudioCapturer::PlayoutDevices()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int16_t FNetworkAudioCapturer::RecordingDevices()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::PlayoutDeviceName(
    uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize], char guid[webrtc::kAdmMaxGuidSize])
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::RecordingDeviceName(
    uint16_t index, char name[webrtc::kAdmMaxDeviceNameSize], char guid[webrtc::kAdmMaxGuidSize])
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::SetPlayoutDevice(uint16_t index)
{
	//LOGFUNC();
	CHECKinitialized_();
	return 0;
}

int32_t FNetworkAudioCapturer::SetPlayoutDevice(WindowsDeviceType device)
{
	//LOGFUNC();
	CHECKinitialized_();
	return 0;
}

int32_t FNetworkAudioCapturer::SetRecordingDevice(uint16_t index)
{
	//LOGFUNC();
	CHECKinitialized_();
	return 0;
}

int32_t FNetworkAudioCapturer::SetRecordingDevice(WindowsDeviceType device)
{
	//LOGFUNC();
	CHECKinitialized_();
	return 0;
}

int32_t FNetworkAudioCapturer::PlayoutIsAvailable(bool* available)
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::InitPlayout()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

bool FNetworkAudioCapturer::PlayoutIsInitialized() const
{
	//LOGFUNC();
	CHECKinitialized__BOOL();
	return false;
}

int32_t FNetworkAudioCapturer::RecordingIsAvailable(bool* available)
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::InitRecording()
{
	LOGFUNC();
	CHECKinitialized_();

	// #Audio : Allow dynamic values for samplerate and/or channels ,
	// or receive those from UE4 ?
	DeviceBuffer->SetRecordingSampleRate(SampleRate);
	DeviceBuffer->SetRecordingChannels(Channels);

	bRecordingInitialized = true;
	return 0;
}

bool FNetworkAudioCapturer::RecordingIsInitialized() const
{
	//LOGFUNC();
	CHECKinitialized__BOOL();
	return bRecordingInitialized == true;
}

int32_t FNetworkAudioCapturer::StartPlayout()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::StopPlayout()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

bool FNetworkAudioCapturer::Playing() const
{
	//LOGFUNC();
	CHECKinitialized__BOOL();
	return false;
}

int32_t FNetworkAudioCapturer::StartRecording()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::StopRecording()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

bool FNetworkAudioCapturer::Recording() const
{
	//LOGFUNC();
	CHECKinitialized__BOOL();
	return bRecordingInitialized;
}

int32_t FNetworkAudioCapturer::InitSpeaker()
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

bool FNetworkAudioCapturer::SpeakerIsInitialized() const
{
	//LOGFUNC();
	CHECKinitialized__BOOL();
	return false;
}

int32_t FNetworkAudioCapturer::InitMicrophone()
{
	//LOGFUNC();
	CHECKinitialized_();
	return 0;
}

bool FNetworkAudioCapturer::MicrophoneIsInitialized() const
{
	//LOGFUNC();
	CHECKinitialized__BOOL();
	return true;
}

int32_t FNetworkAudioCapturer::StereoPlayoutIsAvailable(bool* available) const
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::SetStereoPlayout(bool enable)
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::StereoPlayout(bool* enabled) const
{
	//LOGFUNC();
	CHECKinitialized_();
	return -1;
}

int32_t FNetworkAudioCapturer::StereoRecordingIsAvailable(bool* available) const
{
	//LOGFUNC();
	CHECKinitialized_();
	*available = true;
	return 0;
}

int32_t FNetworkAudioCapturer::SetStereoRecording(bool enable)
{
	//LOGFUNC();
	CHECKinitialized_();
	return 0;
}

int32_t FNetworkAudioCapturer::StereoRecording(bool* enabled) const
{
	//LOGFUNC();
	CHECKinitialized_();
	*enabled = true;
	return 0;
}
