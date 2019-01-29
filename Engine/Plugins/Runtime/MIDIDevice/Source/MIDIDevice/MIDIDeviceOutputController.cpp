// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MIDIDeviceOutputController.h"
#include "MIDIDeviceLog.h"
#include "portmidi.h"

UMIDIDeviceOutputController::~UMIDIDeviceOutputController()
{
	// Clean everything up before we're garbage collected
	ShutdownDevice();
}

void UMIDIDeviceOutputController::StartupDevice(const int32 InitDeviceID, bool& bOutWasSuccessful)
{
	bOutWasSuccessful = false;

	this->DeviceID = InitDeviceID;
	this->PMMIDIStream = nullptr;

	const PmDeviceID PMDeviceID = this->DeviceID;
	const PmDeviceInfo* PMDeviceInfo = Pm_GetDeviceInfo(PMDeviceID);
	if(PMDeviceInfo != nullptr)
	{
		// Is the device already in use?  If so, spit out a warning
		if(PMDeviceInfo->opened != 0)
		{
			UE_LOG(LogMIDIDevice, Error, TEXT("Warning while creating a MIDI device controller:  PortMidi reports that device ID %i (%s) is already in use." ), PMDeviceID, ANSI_TO_TCHAR(PMDeviceInfo->name));
			return;
		}

		if(PMDeviceInfo->output == 0)
		{
			UE_LOG(LogMIDIDevice, Error, TEXT("Warning while creating a MIDI device controller:  PortMidi reports that device ID %i (%S) does is not setup to transmit MIDI data."), PMDeviceID, ANSI_TO_TCHAR(PMDeviceInfo->name));
			return;
		}

		// @todo midi: Add options for timing/latency (see timeproc, and pm_Synchronize)

		PmError PMError = Pm_OpenOutput(&this->PMMIDIStream, PMDeviceID, NULL, 1, NULL, NULL, 0);
		if (PMError == pmNoError)
		{
			check(this->PMMIDIStream != nullptr);

			this->DeviceName = ANSI_TO_TCHAR(PMDeviceInfo->name);

			// Good to go!
			bOutWasSuccessful = true;
		}
		else
		{
			this->PMMIDIStream = nullptr;
			UE_LOG(LogMIDIDevice, Error, TEXT("Unable to open output connection to MIDI device ID %i (%s) (PortMidi error: %s)."), PMDeviceID, ANSI_TO_TCHAR(PMDeviceInfo->name), ANSI_TO_TCHAR(Pm_GetErrorText(PMError)));
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Error, TEXT("Unable to query information about MIDI device (PortMidi device ID: %i)."), PMDeviceID);
	}
}

void UMIDIDeviceOutputController::ShutdownDevice()
{
	if (this->PMMIDIStream != nullptr)
	{
		PmError PMError = Pm_Close(this->PMMIDIStream);

		if (PMError != pmNoError)
		{
			UE_LOG(LogMIDIDevice, Error, TEXT("Encounter an error when closing the output connection to MIDI device ID %i (%s) (PortMidi error: %s)."), this->DeviceID, *this->DeviceName, ANSI_TO_TCHAR(Pm_GetErrorText(PMError)));
		}

		this->PMMIDIStream = nullptr;
	}
}

void UMIDIDeviceOutputController::SendMIDIEvent(EMIDIEventType EventType, int32 Channel, int32 data1, int32 data2)
{
	if (this->PMMIDIStream != nullptr)
	{
		int32 status = ((int32)EventType << 4) | Channel;

		// timestamp is ignored because latency is set to 0
		Pm_WriteShort(this->PMMIDIStream, 0, Pm_Message(status, data1, data2));
	}
}

void UMIDIDeviceOutputController::SendMIDINoteOn(int32 Channel, int32 Note, int32 Velocity)
{
	SendMIDIEvent(EMIDIEventType::NoteOn, Channel, Note, Velocity);
}

void UMIDIDeviceOutputController::SendMIDINoteOff(int32 Channel, int32 Note, int32 Velocity)
{
	SendMIDIEvent(EMIDIEventType::NoteOff, Channel, Note, Velocity);
}

void UMIDIDeviceOutputController::SendMIDIPitchBend(int32 Channel, int32 Pitch)
{
	Pitch = FMath::Clamp<int32>(Pitch, 0, 16383);

	SendMIDIEvent(EMIDIEventType::PitchBend, Channel, Pitch & 0x7F, Pitch >> 7);
}

void UMIDIDeviceOutputController::SendMIDINoteAftertouch(int32 Channel, int32 Note, float Amount)
{
	SendMIDIEvent(EMIDIEventType::NoteAfterTouch, Channel, Note, Amount);
}

void UMIDIDeviceOutputController::SendMIDIControlChange(int32 Channel, int32 Type, int32 Value)
{
	SendMIDIEvent(EMIDIEventType::ControlChange, Channel, Type, Value);
}

void UMIDIDeviceOutputController::SendMIDIProgramChange(int32 Channel, int32 ProgramNumber)
{
	SendMIDIEvent(EMIDIEventType::ProgramChange, Channel, ProgramNumber, 0);
}

void UMIDIDeviceOutputController::SendMIDIChannelAftertouch(int32 Channel, float Amount)
{
	SendMIDIEvent(EMIDIEventType::ChannelAfterTouch, Channel, Amount, 0);
}

