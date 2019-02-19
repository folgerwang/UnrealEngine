//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioEnvironment.h"
#include "PhononScene.h"
#include "PhononProbeVolume.h"
#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/IPluginManager.h"
#include "EngineUtils.h"
#include "Internationalization/Regex.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformProcess.h"
#include "Engine/StreamableManager.h"
#include "AudioDevice.h"
#include "PhononScene.h"
#include "SteamAudioSettings.h"
#include "PhononProbeVolume.h"
#include "HAL/PlatformFilemanager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

namespace SteamAudio
{

	FEnvironment::FEnvironment()
		: EnvironmentCriticalSection()
		, ComputeDevice(nullptr)
		, PhononScene(nullptr)
		, PhononEnvironment(nullptr)
		, EnvironmentalRenderer(nullptr)
		, ProbeManager(nullptr)
	{
	}

	FEnvironment::~FEnvironment()
	{
	}

	bool FEnvironment::Initialize(UWorld* World, FAudioDevice* InAudioDevice)
	{
		if (World == nullptr)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Phonon environment: null World."));
			return false;
		}

		if (InAudioDevice == nullptr)
		{
			UE_LOG(LogSteamAudio, Error, TEXT("Unable to create Phonon environment: null Audio Device."));
			return false;
		}

		IPLerror Error = IPL_STATUS_SUCCESS;

		SimulationSettings.numBounces = GetDefault<USteamAudioSettings>()->RealtimeBounces;
		SimulationSettings.numDiffuseSamples = GetDefault<USteamAudioSettings>()->RealtimeSecondaryRays;
		SimulationSettings.numRays = GetDefault<USteamAudioSettings>()->RealtimeRays;
		SimulationSettings.maxConvolutionSources = GetDefault<USteamAudioSettings>()->MaxSources;
		SimulationSettings.ambisonicsOrder = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseOrder;
		SimulationSettings.irDuration = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseDuration;
		SimulationSettings.sceneType = IPL_SCENETYPE_PHONON;

		RenderingSettings.frameSize = InAudioDevice->GetBufferLength();
		RenderingSettings.samplingRate = InAudioDevice->GetSampleRate();
		RenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;

		IPLComputeDeviceFilter DeviceFilter;
		DeviceFilter.minReservableCUs = GetDefault<USteamAudioSettings>()->MinComputeUnits;
		DeviceFilter.maxCUsToReserve = GetDefault<USteamAudioSettings>()->MaxComputeUnits;

		switch (GetDefault<USteamAudioSettings>()->ConvolutionType)
		{
		case EIplConvolutionType::TRUEAUDIONEXT:
			DeviceFilter.type = IPL_COMPUTEDEVICE_GPU;
			DeviceFilter.requiresTrueAudioNext = IPL_TRUE;
			
			SimulationSettings.maxConvolutionSources = GetDefault<USteamAudioSettings>()->TANMaxSources;
			SimulationSettings.ambisonicsOrder = GetDefault<USteamAudioSettings>()->TANIndirectImpulseResponseOrder;
			SimulationSettings.irDuration = GetDefault<USteamAudioSettings>()->TANIndirectImpulseResponseDuration;

			RenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_TRUEAUDIONEXT;

			Error = iplCreateComputeDevice(GlobalContext, DeviceFilter, &ComputeDevice);
			
			if (Error == IPL_STATUS_SUCCESS)
			{
				// If we successfully created a compute device for TAN, we're done.
				UE_LOG(LogSteamAudio, Log, TEXT("Successfully created TAN compute device."));
				break;
			}
			else
			{
				// Otherwise, fall through to using a null compute device.
				UE_LOG(LogSteamAudio, Warning, TEXT("Unable to create TAN compute device. Falling back to default."));
			}

		case EIplConvolutionType::PHONON:
			SimulationSettings.maxConvolutionSources = GetDefault<USteamAudioSettings>()->MaxSources;
			SimulationSettings.ambisonicsOrder = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseOrder;
			SimulationSettings.irDuration = GetDefault<USteamAudioSettings>()->IndirectImpulseResponseDuration;
			RenderingSettings.convolutionType = IPL_CONVOLUTIONTYPE_PHONON;
			break;
		}

		IPLAudioFormat EnvironmentalOutputAudioFormat;
		EnvironmentalOutputAudioFormat.channelLayout = IPL_CHANNELLAYOUT_STEREO;
		EnvironmentalOutputAudioFormat.channelLayoutType = IPL_CHANNELLAYOUTTYPE_AMBISONICS;
		EnvironmentalOutputAudioFormat.channelOrder = IPL_CHANNELORDER_DEINTERLEAVED;
		// Number of channels for an ambisonics stream is the square of the ambisonics streams' order plus one:
		EnvironmentalOutputAudioFormat.numSpeakers = (SimulationSettings.ambisonicsOrder + 1) * (SimulationSettings.ambisonicsOrder + 1);
		EnvironmentalOutputAudioFormat.speakerDirections = nullptr;
		EnvironmentalOutputAudioFormat.ambisonicsOrder = SimulationSettings.ambisonicsOrder;
		EnvironmentalOutputAudioFormat.ambisonicsNormalization = IPL_AMBISONICSNORMALIZATION_N3D;
		EnvironmentalOutputAudioFormat.ambisonicsOrdering = IPL_AMBISONICSORDERING_ACN;

		if (!LoadSceneFromDisk(World, ComputeDevice, SimulationSettings, &PhononScene, PhononSceneInfo))
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to create Phonon environment: failed to load scene from disk. Be sure to export the scene."));
			return false;
		}

		Error = iplCreateProbeManager(GlobalContext, &ProbeManager);
		LogSteamAudioStatus(Error);

		TArray<AActor*> PhononProbeVolumes;
		UGameplayStatics::GetAllActorsOfClass(World, APhononProbeVolume::StaticClass(), PhononProbeVolumes);

		for (AActor* PhononProbeVolumeActor : PhononProbeVolumes)
		{
			APhononProbeVolume* PhononProbeVolume = Cast<APhononProbeVolume>(PhononProbeVolumeActor);

			if (PhononProbeVolume->GetProbeBatchDataSize() == 0)
			{
				UE_LOG(LogSteamAudio, Warning, TEXT("No batch data found on probe volume. You may need to bake indirect sound."));
				continue;
			}

			IPLhandle ProbeBatch = nullptr;
			PhononProbeVolume->LoadProbeBatchFromDisk(&ProbeBatch);

			iplAddProbeBatch(ProbeManager, ProbeBatch);

			ProbeBatches.Add(ProbeBatch);
		}

		if (!LoadBakedIdentifierMapFromDisk(World, BakedIdentifierMap))
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load identifier map."));
		}

		Error = iplCreateEnvironment(GlobalContext, ComputeDevice, SimulationSettings, PhononScene, ProbeManager, &PhononEnvironment);
		LogSteamAudioStatus(Error);

		Error = iplCreateEnvironmentalRenderer(GlobalContext, PhononEnvironment, RenderingSettings, EnvironmentalOutputAudioFormat,
			nullptr, nullptr, &EnvironmentalRenderer);
		LogSteamAudioStatus(Error);

		return true;
	}

	void FEnvironment::Shutdown()
	{
		FScopeLock EnvironmentLock(&EnvironmentCriticalSection);
		if (ProbeManager)
		{
			for (IPLhandle ProbeBatch : ProbeBatches)
			{
				iplRemoveProbeBatch(ProbeManager, ProbeBatch);
				iplDestroyProbeBatch(&ProbeBatch);
			}

			ProbeBatches.Empty();

			iplDestroyProbeManager(&ProbeManager);
		}

		if (EnvironmentalRenderer)
		{
			iplDestroyEnvironmentalRenderer(&EnvironmentalRenderer);
		}

		if (PhononEnvironment)
		{
			iplDestroyEnvironment(&PhononEnvironment);
		}

		if (PhononScene)
		{
			iplDestroyScene(&PhononScene);
		}

		if (ComputeDevice)
		{
			iplDestroyComputeDevice(&ComputeDevice);
		}
	}

	//==============================================================================================================================================
	// FIdentifierMap
	//==============================================================================================================================================

	bool FIdentifierMap::ContainsKey(const FString& Key) const
	{
		for (const auto& Pair : IdentifierMapping)
		{
			if (Pair.Key == Key)
			{
				return true;
			}
		}

		return false;
	}

	bool FIdentifierMap::ContainsValue(const int32 Value) const
	{
		for (const auto& Pair : IdentifierMapping)
		{
			if (Pair.Value == Value)
			{
				return true;
			}
		}

		return false;
	}

	int32 FIdentifierMap::Add(const FString& Identifier)
	{
		int32 Value = FMath::Rand();

		while (ContainsValue(Value))
		{
			Value = FMath::Rand();
		}

		IdentifierMapping.Add(TPair<FString, int32>(Identifier, Value));

		return Value;
	}

	void FIdentifierMap::Add(const TPair<FString, int32>& Pair)
	{
		IdentifierMapping.Add(Pair);
	}

	int32 FIdentifierMap::Get(const FString& Identifier) const
	{
		for (const auto& Pair : IdentifierMapping)
		{
			if (Pair.Key == Identifier)
			{
				return Pair.Value;
			}
		}

		return -1;
	}

	FString FIdentifierMap::ToString() const
	{
		FString out;

		for (const auto& Pair : IdentifierMapping)
		{
			out += Pair.Key + ":" + FString::FromInt(Pair.Value) + ",";
		}

		out.RemoveFromEnd(",");

		return out;
	}

	void FIdentifierMap::FromString(const FString& MappingString)
	{
		IdentifierMapping.Empty();

		TArray<FString> MappingArr;
		MappingString.ParseIntoArray(MappingArr, *FString(","));

		for (const auto& PairStr : MappingArr)
		{
			FString Key, Value;
			PairStr.Split(":", &Key, &Value);
			TPair<FString, int32> Mapping(Key, FCString::Atoi(*Value));
			IdentifierMapping.Add(Mapping);
		}
	}

	void FIdentifierMap::Empty()
	{
		IdentifierMapping.Empty();
	}

	bool LoadBakedIdentifierMapFromDisk(UWorld* World, FIdentifierMap& BakedIdentifierMap)
	{
		FString MapName = StrippedMapName(World->GetMapName());
		FString BakedIdentifierMapFileName = RuntimePath + MapName + ".bakedsources";

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString BakedIdentifierMapString;

		if (PlatformFile.FileExists(*BakedIdentifierMapFileName))
		{
			FFileHelper::LoadFileToString(BakedIdentifierMapString, *BakedIdentifierMapFileName);
		}
		else
		{
			UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load baked identifier map: file doesn't exist."));
			return false;
		}

		BakedIdentifierMap.FromString(BakedIdentifierMapString);

		return true;
	}

	bool SaveBakedIdentifierMapToDisk(UWorld* World, const FIdentifierMap& BakedIdentifierMap)
	{
		FString MapName = StrippedMapName(World->GetMapName());
		FString BakedIdentifierMapFileName = RuntimePath + MapName + ".bakedsources";

		FFileHelper::SaveStringToFile(BakedIdentifierMap.ToString(), *BakedIdentifierMapFileName);

		return true;
	}

}
