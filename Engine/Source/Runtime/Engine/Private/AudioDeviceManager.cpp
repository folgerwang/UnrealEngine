// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AudioDeviceManager.h"
#include "AudioDevice.h"
#include "Sound/SoundWave.h"
#include "Sound/AudioSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "AudioEditorModule.h"
#endif

// Private consts for helping with index/generation determination in audio device manager
static const uint32 AUDIO_DEVICE_HANDLE_INDEX_BITS		= 24;
static const uint32 AUDIO_DEVICE_HANDLE_INDEX_MASK		= (1 << AUDIO_DEVICE_HANDLE_INDEX_BITS) - 1;
static const uint32 AUDIO_DEVICE_HANDLE_GENERATION_BITS = 8;
static const uint32 AUDIO_DEVICE_HANDLE_GENERATION_MASK = (1 << AUDIO_DEVICE_HANDLE_GENERATION_BITS) - 1;

static const uint16 AUDIO_DEVICE_MINIMUM_FREE_AUDIO_DEVICE_INDICES = 32;

// The number of multiple audio devices allowed by default
static const uint32 AUDIO_DEVICE_DEFAULT_ALLOWED_DEVICE_COUNT = 2;

// The max number of audio devices allowed
static const uint32 AUDIO_DEVICE_MAX_DEVICE_COUNT = 8;

static int32 GCVarEnableAudioThreadWait = 1;
TAutoConsoleVariable<int32> CVarEnableAudioThreadWait(
	TEXT("AudioThread.EnableAudioThreadWait"),
	GCVarEnableAudioThreadWait,
	TEXT("Enables waiting on the audio thread to finish its commands.\n")
	TEXT("0: Not Enabled, 1: Enabled"),
	ECVF_Default);

static int32 GCvarIsUsingAudioMixer = 0;
FAutoConsoleVariableRef CVarIsUsingAudioMixer(
	TEXT("au.IsUsingAudioMixer"),
	GCvarIsUsingAudioMixer,
	TEXT("Whether or not we're currently using the audio mixer. Change to dynamically toggle on/off. Note: sounds will stop. Looping sounds won't automatically resume. \n")
	TEXT("0: Not Using Audio Mixer, 1: Using Audio Mixer"),
	ECVF_Default);


FAudioDeviceManager::FCreateAudioDeviceResults::FCreateAudioDeviceResults()
	: Handle(INDEX_NONE)
	, bNewDevice(false)
	, AudioDevice(nullptr)
{
}

/*-----------------------------------------------------------------------------
FAudioDeviceManager implementation.
-----------------------------------------------------------------------------*/

FAudioDeviceManager::FAudioDeviceManager()
	: AudioDeviceModule(nullptr)
	, FreeIndicesSize(0)
	, NumActiveAudioDevices(0)
	, NumWorldsUsingMainAudioDevice(0)
	, NextResourceID(1)
	, SoloDeviceHandle(INDEX_NONE)
	, ActiveAudioDeviceHandle(INDEX_NONE)
	, bUsingAudioMixer(false)
	, bPlayAllDeviceAudio(false)
	, bVisualize3dDebug(false)
{
	// Check for a command line debug sound argument.
	FString DebugSound;
	if (FParse::Value(FCommandLine::Get(), TEXT("DebugSound="), DebugSound))
	{
		SetAudioDebugSound(*DebugSound);
	}
}

FAudioDeviceManager::~FAudioDeviceManager()
{
	// Confirm that we freed all the audio devices
	check(NumActiveAudioDevices == 0);

	// Release any loaded buffers - this calls stop on any sources that need it
	for (int32 Index = Buffers.Num() - 1; Index >= 0; Index--)
	{
		FreeBufferResource(Buffers[Index]);
	}
}

void FAudioDeviceManager::ToggleAudioMixer()
{
	// Only need to toggle if we have 2 device module names loaded at init
	if (AudioDeviceModule && AudioDeviceModuleName.Len() > 0 && AudioMixerModuleName.Len() > 0)
	{
		// Suspend the audio thread
		FAudioThread::SuspendAudioThread();

		// If using audio mixer, we need to toggle back to non-audio mixer
		FString ModuleToUnload;

		// If currently using the audio mixer, we need to toggle to the old audio engine module
		if (bUsingAudioMixer)
		{
			// Unload the previous module
			ModuleToUnload = AudioMixerModuleName;

			AudioDeviceModule = FModuleManager::LoadModulePtr<IAudioDeviceModule>(*AudioDeviceModuleName);

			bUsingAudioMixer = false;
		}
		// If we're currently using old audio engine module, we toggle to the audio mixer module
		else
		{
			// Unload the previous module
			ModuleToUnload = AudioDeviceModuleName;

			// Load the audio mixer engine module
			AudioDeviceModule = FModuleManager::LoadModulePtr<IAudioDeviceModule>(*AudioMixerModuleName);

			bUsingAudioMixer = true;
		}

		// If we succeeded in loading a new module, create a new main audio device.
		if (AudioDeviceModule)
		{
			// Shutdown and create new audio devices
			const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
			const int32 QualityLevel = GEngine->GetGameUserSettings()->GetAudioQualityLevel(); // -V595
			const int32 QualityLevelMaxChannels = AudioSettings->GetQualityLevelSettings(QualityLevel).MaxChannels; //-V595
	
			// We could have multiple audio devices, so loop through them and patch them up best we can to
			// get parity. E.g. we need to pass the handle from the odl to the new, set whether or not its active
			// and try and get the mix-states to be the same.
			for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
			{
				FAudioDevice* AudioDevice = Devices[DeviceIndex];

				if (AudioDevice)
				{
					// Get the audio device handle and whether it is active
					uint32 Handle = AudioDevice->DeviceHandle;
					bool bIsActive = (Handle == ActiveAudioDeviceHandle);

					// To transfer mix states, we need to re-base the absolute clocks on the mix states
					// so the target audio device timing won't result in the mixes suddenly stopping.
					TMap<USoundMix *, FSoundMixState> MixModifiers = AudioDevice->GetSoundMixModifiers();
					double AudioClock = AudioDevice->GetAudioClock();

					for (TPair<USoundMix*, FSoundMixState>& SoundMixPair : MixModifiers)
					{
						// Rebase so that a new clock starting from 0.0 won't cause mixes to stop.
						SoundMixPair.Value.StartTime -= AudioClock;
						SoundMixPair.Value.FadeInStartTime -= AudioClock;
						SoundMixPair.Value.FadeInEndTime -= AudioClock;

						if (SoundMixPair.Value.EndTime > 0.0f)
						{
							SoundMixPair.Value.EndTime -= AudioClock;
						}

						if (SoundMixPair.Value.FadeOutStartTime > 0.0f)
						{
							SoundMixPair.Value.FadeOutStartTime -= AudioClock;
						}
					}
					
					// Tear it down and delete the old audio device. This does a bunch of cleanup.
					AudioDevice->Teardown();
					delete AudioDevice;

					// Make a new audio device using the new audio device module
					AudioDevice = AudioDeviceModule->CreateAudioDevice();

					// Set the new audio device handle to the old audio device handle
					AudioDevice->DeviceHandle = Handle;

					// Re-init the new audio device using appropriate settings so it behaves the same
					if (AudioDevice->Init(AudioSettings->GetHighestMaxChannels()))
					{
						AudioDevice->SetMaxChannels(QualityLevelMaxChannels);
					}

					// Transfer the sound mix modifiers to the new audio engine
					AudioDevice->SetSoundMixModifiers(MixModifiers);

					// Setup the mute state of the audio device to be the same that it was
					if (bIsActive)
					{
						AudioDevice->SetDeviceMuted(false);
					}
					else
					{
						AudioDevice->SetDeviceMuted(true);
					}
					
					// Fade in the new audio device (used only in audio mixer to prevent pops on startup/shutdown)
					AudioDevice->FadeIn();

					// Set the new audio device into the slot of the old audio device in the manager
					Devices[DeviceIndex] = AudioDevice;
				}
			}

			// We now must free any resources that have been cached with the old audio engine
			// This will result in re-caching of sound waves, but we're forced to do this because FSoundBuffer pointers
			// are cached and each AudioDevice backend has a derived implementation of this so once we 
			// switch to a new audio engine the FSoundBuffer pointers are totally invalid.
			for (TObjectIterator<USoundWave> SoundWaveIt; SoundWaveIt; ++SoundWaveIt)
			{
				USoundWave* SoundWave = *SoundWaveIt;
				FreeResource(SoundWave);
				
				// Flag that the sound wave needs to do a full decompress again
				SoundWave->DecompressionType = DTYPE_Setup;
			}

			// Unload the previous audio device module
			FModuleManager::Get().UnloadModule(*ModuleToUnload);

			// Resume the audio thread
			FAudioThread::ResumeAudioThread();
		}
	}
}

bool FAudioDeviceManager::IsUsingAudioMixer() const
{
	return bUsingAudioMixer;
}

bool FAudioDeviceManager::Initialize()
{
	if (LoadDefaultAudioDeviceModule())
	{
		check(AudioDeviceModule);

		const bool bIsAudioMixerEnabled = AudioDeviceModule->IsAudioMixerModule();
		GetMutableDefault<UAudioSettings>()->SetAudioMixerEnabled(bIsAudioMixerEnabled);

#if WITH_EDITOR
		if (bIsAudioMixerEnabled)
		{
			IAudioEditorModule* AudioEditorModule = &FModuleManager::LoadModuleChecked<IAudioEditorModule>("AudioEditor");
			AudioEditorModule->RegisterAudioMixerAssetActions();
			AudioEditorModule->RegisterEffectPresetAssetActions();
		}
#endif

		return CreateMainAudioDevice();
	}

	// Failed to initialize
	return false;
}

bool FAudioDeviceManager::LoadDefaultAudioDeviceModule()
{
	check(!AudioDeviceModule);

	// Check if we're going to try to force loading the audio mixer from the command line
	bool bForceAudioMixer = FParse::Param(FCommandLine::Get(), TEXT("AudioMixer"));

	bool bForceNoAudioMixer = FParse::Param(FCommandLine::Get(), TEXT("NoAudioMixer"));

	// If not using command line switch to use audio mixer, check the game platform engine ini file (e.g. WindowsEngine.ini) which enables it for player
	bUsingAudioMixer = bForceAudioMixer;
	if (!bForceAudioMixer && !bForceNoAudioMixer)
	{
		GConfig->GetBool(TEXT("Audio"), TEXT("UseAudioMixer"), bUsingAudioMixer, GEngineIni);
	}
	else if (bForceNoAudioMixer)
	{
		// Allow no audio mixer override from command line
		bUsingAudioMixer = false;
	}

	// Get the audio mixer and non-audio mixer device module names
	GConfig->GetString(TEXT("Audio"), TEXT("AudioDeviceModuleName"), AudioDeviceModuleName, GEngineIni);
	GConfig->GetString(TEXT("Audio"), TEXT("AudioMixerModuleName"), AudioMixerModuleName, GEngineIni);

	if (bUsingAudioMixer && AudioMixerModuleName.Len() > 0)
	{
		AudioDeviceModule = FModuleManager::LoadModulePtr<IAudioDeviceModule>(*AudioMixerModuleName);
		if (AudioDeviceModule)
		{
			static IConsoleVariable* IsUsingAudioMixerCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.IsUsingAudioMixer"));
			check(IsUsingAudioMixerCvar);
			IsUsingAudioMixerCvar->Set(1, ECVF_SetByConstructor);
		}
		else
		{
			bUsingAudioMixer = false;
		}
	}
	
	if (!AudioDeviceModule && AudioDeviceModuleName.Len() > 0)
	{
		AudioDeviceModule = FModuleManager::LoadModulePtr<IAudioDeviceModule>(*AudioDeviceModuleName);

		static IConsoleVariable* IsUsingAudioMixerCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("au.IsUsingAudioMixer"));
		check(IsUsingAudioMixerCvar);
		IsUsingAudioMixerCvar->Set(0, ECVF_SetByConstructor);
	}

	return AudioDeviceModule != nullptr;
}

bool FAudioDeviceManager::CreateMainAudioDevice()
{
	FAudioDeviceManager::FCreateAudioDeviceResults NewDeviceResults;

	// Create a new audio device.
	if (CreateAudioDevice(true, NewDeviceResults))
	{
		MainAudioDeviceHandle = NewDeviceResults.Handle;
		SetActiveDevice(MainAudioDeviceHandle);
		FAudioThread::StartAudioThread();
		return true;
	}
	return false;
}

bool FAudioDeviceManager::CreateAudioDevice(bool bCreateNewDevice, FCreateAudioDeviceResults& OutResults)
{
	OutResults = FCreateAudioDeviceResults();

	// If we don't have an audio device module, then we can't create new audio devices.
	if (AudioDeviceModule == nullptr)
	{
		return false;
	}

	// If we are running without the editor, we only need one audio device.
	if (!GIsEditor)
	{
		if (NumActiveAudioDevices == 1)
		{
			FAudioDevice* MainAudioDevice = GEngine->GetMainAudioDevice();
			if (MainAudioDevice)
			{
				OutResults.Handle = MainAudioDevice->DeviceHandle;
				OutResults.AudioDevice = MainAudioDevice;
				OutResults.AudioDevice->FadeIn();
				return true;
			}
			return false;
		}
	}

	if (NumActiveAudioDevices < AUDIO_DEVICE_DEFAULT_ALLOWED_DEVICE_COUNT || (bCreateNewDevice && NumActiveAudioDevices < AUDIO_DEVICE_MAX_DEVICE_COUNT))
	{
		// Create the new audio device and make sure it succeeded
		OutResults.AudioDevice = AudioDeviceModule->CreateAudioDevice();
		if (OutResults.AudioDevice == nullptr)
		{
			return false;
		}

		// Now generation a new audio device handle for the device and store the
		// ptr to the new device in the array of audio devices.

		uint32 AudioDeviceIndex(INDEX_NONE);

		// First check to see if we should start recycling audio device indices, if not
		// then we add a new entry to the Generation array and generate a new index
		if (FreeIndicesSize > AUDIO_DEVICE_MINIMUM_FREE_AUDIO_DEVICE_INDICES)
		{
			FreeIndices.Dequeue(AudioDeviceIndex);
			--FreeIndicesSize;
			check(int32(AudioDeviceIndex) < Devices.Num());
			check(Devices[AudioDeviceIndex] == nullptr);
			Devices[AudioDeviceIndex] = OutResults.AudioDevice;
		}
		else
		{
			// Add a zeroth generation entry in the Generation array, get a brand new
			// index and append the created device to the end of the Devices array

			Generations.Add(0);
			AudioDeviceIndex = Generations.Num() - 1;
			check(AudioDeviceIndex < (1 << AUDIO_DEVICE_HANDLE_INDEX_BITS));
			Devices.Add(OutResults.AudioDevice);
		}

		OutResults.bNewDevice = true;
		OutResults.Handle = CreateHandle(AudioDeviceIndex, Generations[AudioDeviceIndex]);

		// Store the handle on the audio device itself
		OutResults.AudioDevice->DeviceHandle = OutResults.Handle;
	}
	else
	{
		++NumWorldsUsingMainAudioDevice;
		FAudioDevice* MainAudioDevice = GEngine->GetMainAudioDevice();
		if (MainAudioDevice)
		{
			OutResults.Handle = MainAudioDevice->DeviceHandle;
			OutResults.AudioDevice = MainAudioDevice;
		}
	}

	++NumActiveAudioDevices;

	const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
	if (OutResults.AudioDevice->Init(AudioSettings->GetHighestMaxChannels())) //-V595
	{
		OutResults.AudioDevice->SetMaxChannels(AudioSettings->GetQualityLevelSettings(GEngine->GetGameUserSettings()->GetAudioQualityLevel()).MaxChannels); //-V595
	}
	else
	{
		ShutdownAudioDevice(OutResults.Handle);
		OutResults = FCreateAudioDeviceResults();
	}

	// We need to call fade in, in case we're reusing audio devices
	if (OutResults.AudioDevice)
	{
		OutResults.AudioDevice->FadeIn();
	}

	return (OutResults.AudioDevice != nullptr);
}

bool FAudioDeviceManager::IsValidAudioDeviceHandle(uint32 Handle) const
{
	if (AudioDeviceModule == nullptr || Handle == INDEX_NONE)
	{
		return false;
	}

	uint32 Index = GetIndex(Handle);
	if (int32(Index) >= Generations.Num())
	{
		return false;
	}

	uint8 Generation = GetGeneration(Handle);
	return Generations[Index] == Generation;
}

bool FAudioDeviceManager::ShutdownAudioDevice(uint32 Handle)
{
	if (!IsValidAudioDeviceHandle(Handle))
	{
		return false;
	}

	check(NumActiveAudioDevices > 0);
	--NumActiveAudioDevices;

	// If there are more than 1 device active, check to see if this handle is the main audio device handle
	if (NumActiveAudioDevices >= 1)
	{
		uint32 MainDeviceHandle = GEngine->GetAudioDeviceHandle();

		if (NumActiveAudioDevices == 1)
		{
			// If we only have one audio device left, then set the active
			// audio device to be the main audio device
			SetActiveDevice(MainDeviceHandle);
		}

		// If this is the main device handle and there's more than one reference to the main device, 
		// don't shut it down until it's the very last handle to get shut down
		// this is because it's possible for some PIE sessions to be using the main audio device as a fallback to 
		// preserve CPU performance on low-performance machines
		if (NumWorldsUsingMainAudioDevice > 0 && MainDeviceHandle == Handle)
		{
			--NumWorldsUsingMainAudioDevice;

			return true;
		}
	}

	uint32 Index = GetIndex(Handle);
	uint8 Generation = GetGeneration(Handle);

	check(int32(Index) < Generations.Num());

	// Bump up the generation at the given index. This will invalidate
	// the handle without needing to broadcast to everybody who might be using the handle
	Generations[Index] = ++Generation;

	// Make sure we have a non-null device ptr in the index slot, then delete it
	FAudioDevice* AudioDevice = Devices[Index];
	check(AudioDevice != nullptr);

    // Tear down the audio device
	AudioDevice->Teardown();

	delete AudioDevice;

	// Nullify the audio device slot for future audio device creations
	Devices[Index] = nullptr;

	// Add this index to the list of free indices
	++FreeIndicesSize;
	FreeIndices.Enqueue(Index);

	return true;
}

bool FAudioDeviceManager::ShutdownAllAudioDevices()
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			ShutdownAudioDevice(AudioDevice->DeviceHandle);
		}
	}

	check(NumActiveAudioDevices == 0);
	check(NumWorldsUsingMainAudioDevice == 0);

	return true;
}

FAudioDevice* FAudioDeviceManager::GetAudioDevice(uint32 Handle)
{
	if (!IsValidAudioDeviceHandle(Handle))
	{
		return nullptr;
	}

	uint32 Index = GetIndex(Handle);
	check(int32(Index) < Devices.Num());
	FAudioDevice* AudioDevice = Devices[Index];
	check(AudioDevice != nullptr);
	return AudioDevice;
}

FAudioDevice* FAudioDeviceManager::GetActiveAudioDevice()
{
	if (ActiveAudioDeviceHandle != INDEX_NONE)
	{
		return GetAudioDevice(ActiveAudioDeviceHandle);
	}
	return GEngine->GetMainAudioDevice();
}

void FAudioDeviceManager::UpdateActiveAudioDevices(bool bGameTicking)
{
	// Before we kick off the next update make sure that we've finished the previous frame's update (this should be extremely rare)
	if (GCVarEnableAudioThreadWait)
	{
		SyncFence.Wait();
	}

	if (bUsingAudioMixer && !GCvarIsUsingAudioMixer)
	{
		ToggleAudioMixer();
		bUsingAudioMixer = false;
	}
	else if (!bUsingAudioMixer && GCvarIsUsingAudioMixer)
	{
		ToggleAudioMixer();
		bUsingAudioMixer = true;
	}

	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->Update(bGameTicking);
		}
	}

	if (GCVarEnableAudioThreadWait)
	{
		SyncFence.BeginFence();
	}
}

void FAudioDeviceManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->AddReferencedObjects(Collector);
		}
	}
}

void FAudioDeviceManager::StopSoundsUsingResource(USoundWave* InSoundWave, TArray<UAudioComponent*>* StoppedComponents)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->StopSoundsUsingResource(InSoundWave, StoppedComponents);
		}
	}
}

void FAudioDeviceManager::RegisterSoundClass(USoundClass* SoundClass)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->RegisterSoundClass(SoundClass);
		}
	}
}

void FAudioDeviceManager::UnregisterSoundClass(USoundClass* SoundClass)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->UnregisterSoundClass(SoundClass);
		}
	}
}

void FAudioDeviceManager::InitSoundClasses()
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->InitSoundClasses();
		}
	}
}

void FAudioDeviceManager::RegisterSoundSubmix(USoundSubmix* SoundSubmix)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->RegisterSoundSubmix(SoundSubmix, true);
		}
	}
}

void FAudioDeviceManager::UnregisterSoundSubmix(USoundSubmix* SoundSubmix)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->UnregisterSoundSubmix(SoundSubmix);
		}
	}
}

void FAudioDeviceManager::InitSoundSubmixes()
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->InitSoundSubmixes();
		}
	}
}

void FAudioDeviceManager::InitSoundEffectPresets()
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->InitSoundEffectPresets();
		}
	}
}

void FAudioDeviceManager::UpdateSourceEffectChain(const uint32 SourceEffectChainId, const TArray<FSourceEffectChainEntry>& SourceEffectChain, const bool bPlayEffectChainTails)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->UpdateSourceEffectChain(SourceEffectChainId, SourceEffectChain, bPlayEffectChainTails);
		}
	}
}

void FAudioDeviceManager::SetActiveDevice(uint32 InAudioDeviceHandle)
{
	// Only change the active device if there are no solo'd audio devices
	if (SoloDeviceHandle == INDEX_NONE)
	{
		for (FAudioDevice* AudioDevice : Devices)
		{
			if (AudioDevice)
			{
				if (AudioDevice->DeviceHandle == InAudioDeviceHandle)
				{
					ActiveAudioDeviceHandle = InAudioDeviceHandle;
					AudioDevice->SetDeviceMuted(false);
				}
				else
				{
					AudioDevice->SetDeviceMuted(true);
				}
			}
		}
	}
}

void FAudioDeviceManager::SetSoloDevice(uint32 InAudioDeviceHandle)
{
	SoloDeviceHandle = InAudioDeviceHandle;
	if (SoloDeviceHandle != INDEX_NONE)
	{
		for (FAudioDevice* AudioDevice : Devices)
		{
			if (AudioDevice)
			{
				// Un-mute the active audio device and mute non-active device, as long as its not the main audio device (which is used to play UI sounds)
				if (AudioDevice->DeviceHandle == InAudioDeviceHandle)
				{
					ActiveAudioDeviceHandle = InAudioDeviceHandle;
					AudioDevice->SetDeviceMuted(false);
				}
				else
				{
					AudioDevice->SetDeviceMuted(true);
				}
			}
		}
	}
}


uint8 FAudioDeviceManager::GetNumActiveAudioDevices() const
{
	return NumActiveAudioDevices;
}

uint8 FAudioDeviceManager::GetNumMainAudioDeviceWorlds() const
{
	return NumWorldsUsingMainAudioDevice;
}

uint32 FAudioDeviceManager::GetIndex(uint32 Handle) const
{
	return Handle & AUDIO_DEVICE_HANDLE_INDEX_MASK;
}

uint32 FAudioDeviceManager::GetGeneration(uint32 Handle) const
{
	return (Handle >> AUDIO_DEVICE_HANDLE_INDEX_BITS) & AUDIO_DEVICE_HANDLE_GENERATION_MASK;
}

uint32 FAudioDeviceManager::CreateHandle(uint32 DeviceIndex, uint8 Generation)
{
	return (DeviceIndex | (Generation << AUDIO_DEVICE_HANDLE_INDEX_BITS));
}

void FAudioDeviceManager::StopSourcesUsingBuffer(FSoundBuffer* SoundBuffer)
{
	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->StopSourcesUsingBuffer(SoundBuffer);
		}
	}
}

void FAudioDeviceManager::TrackResource(USoundWave* SoundWave, FSoundBuffer* Buffer)
{
	// Allocate new resource ID and assign to USoundWave. A value of 0 (default) means not yet registered.
	int32 ResourceID = NextResourceID++;
	Buffer->ResourceID = ResourceID;
	SoundWave->ResourceID = ResourceID;

	Buffers.Add(Buffer);
	WaveBufferMap.Add(ResourceID, Buffer);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	// Keep track of associated resource name.
	Buffer->ResourceName = SoundWave->GetPathName();
#endif
}

void FAudioDeviceManager::FreeResource(USoundWave* SoundWave)
{
	if (SoundWave->ResourceID)
	{
		FSoundBuffer* SoundBuffer = WaveBufferMap.FindRef(SoundWave->ResourceID);
		FreeBufferResource(SoundBuffer);

		SoundWave->ResourceID = 0;
	}
}

void FAudioDeviceManager::FreeBufferResource(FSoundBuffer* SoundBuffer)
{
	if (SoundBuffer)
	{
		// Make sure any realtime tasks are finished that are using this buffer
		SoundBuffer->EnsureRealtimeTaskCompletion();

		Buffers.Remove(SoundBuffer);

		// Stop any sound sources on any audio device currently using this buffer before deleting
		StopSourcesUsingBuffer(SoundBuffer);

		delete SoundBuffer;
		SoundBuffer = nullptr;
	}
}

FSoundBuffer* FAudioDeviceManager::GetSoundBufferForResourceID(uint32 ResourceID)
{
	return WaveBufferMap.FindRef(ResourceID);
}

void FAudioDeviceManager::RemoveSoundBufferForResourceID(uint32 ResourceID)
{
	WaveBufferMap.Remove(ResourceID);
}

void FAudioDeviceManager::RemoveSoundMix(USoundMix* SoundMix)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.RemoveSoundMix"), STAT_AudioRemoveSoundMix, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager, SoundMix]()
		{
			AudioDeviceManager->RemoveSoundMix(SoundMix);

		}, GET_STATID(STAT_AudioRemoveSoundMix));

		return;
	}

	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->RemoveSoundMix(SoundMix);
		}
	}
}

void FAudioDeviceManager::TogglePlayAllDeviceAudio()
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.TogglePlayAllDeviceAudio"), STAT_TogglePlayAllDeviceAudio, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager]()
		{
			AudioDeviceManager->TogglePlayAllDeviceAudio();

		}, GET_STATID(STAT_TogglePlayAllDeviceAudio));

		return;
	}
	
	bPlayAllDeviceAudio = !bPlayAllDeviceAudio;
}

void FAudioDeviceManager::ToggleVisualize3dDebug()
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ToggleVisualize3dDebug"), STAT_ToggleVisualize3dDebug, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager]()
		{
			AudioDeviceManager->ToggleVisualize3dDebug();

		}, GET_STATID(STAT_ToggleVisualize3dDebug));

		return;
	}

	bVisualize3dDebug = !bVisualize3dDebug;
}

void FAudioDeviceManager::ToggleDebugStat(const uint8 StatBitMask)
{
#if !UE_BUILD_SHIPPING
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.ToggleDebugStat"), STAT_ToggleDebugStat, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager, StatBitMask]()
		{
			AudioDeviceManager->ToggleDebugStat(StatBitMask);
		}, GET_STATID(STAT_ToggleDebugStat));

		return;
	}

	for (FAudioDevice* AudioDevice : Devices)
	{
		if (AudioDevice)
		{
			AudioDevice->UpdateRequestedStat(StatBitMask);
		}
	}
#endif
}

void FAudioDeviceManager::SetDebugSoloSoundClass(const TCHAR* SoundClassName)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetDebugSoloSoundClass"), STAT_SetDebugSoloSoundClass, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager, SoundClassName]()
		{
			AudioDeviceManager->SetDebugSoloSoundClass(SoundClassName);

		}, GET_STATID(STAT_SetDebugSoloSoundClass));
		return;
	}

	DebugNames.DebugSoloSoundClass = SoundClassName;

}

const FString& FAudioDeviceManager::GetDebugSoloSoundClass() const
{
	return DebugNames.DebugSoloSoundClass;
}

void FAudioDeviceManager::SetDebugSoloSoundWave(const TCHAR* SoundWave)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetDebugSoloSoundWave"), STAT_SetDebugSoloSoundWave, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager, SoundWave]()
		{
			AudioDeviceManager->SetDebugSoloSoundWave(SoundWave);

		}, GET_STATID(STAT_SetDebugSoloSoundWave));
		return;
	}

	DebugNames.DebugSoloSoundWave = SoundWave;
}

const FString& FAudioDeviceManager::GetDebugSoloSoundWave() const
{
	return DebugNames.DebugSoloSoundWave;
}

void FAudioDeviceManager::SetDebugSoloSoundCue(const TCHAR* SoundCue)
{
	if (!IsInAudioThread())
	{
		DECLARE_CYCLE_STAT(TEXT("FAudioThreadTask.SetDebugSoloSoundCue"), STAT_SetDebugSoloSoundCue, STATGROUP_AudioThreadCommands);

		FAudioDeviceManager* AudioDeviceManager = this;
		FAudioThread::RunCommandOnAudioThread([AudioDeviceManager, SoundCue]()
		{
			AudioDeviceManager->SetDebugSoloSoundCue(SoundCue);

		}, GET_STATID(STAT_SetDebugSoloSoundCue));
		return;
	}

	DebugNames.DebugSoloSoundCue = SoundCue;
}

const FString& FAudioDeviceManager::GetDebugSoloSoundCue() const
{
	return DebugNames.DebugSoloSoundCue;
}

void FAudioDeviceManager::SetAudioMixerDebugSound(const TCHAR* SoundName)
{
	DebugNames.DebugAudioMixerSoundName = SoundName;
}

void FAudioDeviceManager::SetAudioDebugSound(const TCHAR* SoundName)
{
	DebugNames.DebugSoundName = SoundName;
	DebugNames.bDebugSoundName = DebugNames.DebugSoundName != TEXT("");
}

const FString& FAudioDeviceManager::GetAudioMixerDebugSoundName() const
{
	return DebugNames.DebugAudioMixerSoundName;
}

bool FAudioDeviceManager::GetAudioDebugSound(FString& OutDebugSound)
{
	if (DebugNames.bDebugSoundName)
	{
		OutDebugSound = DebugNames.DebugSoundName;
		return true;
	}
	return false;
}

