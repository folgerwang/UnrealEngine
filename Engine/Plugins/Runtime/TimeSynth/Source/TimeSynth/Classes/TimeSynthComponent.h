// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Engine/EngineTypes.h"
#include "Sound/SoundWaveProcedural.h"
#include "Components/SynthComponent.h"
#include "Engine/Classes/Sound/SoundWave.h"
#include "DSP/SoundWaveDecoder.h"
#include "Engine/Public/AudioDevice.h"
#include "Math/RandomStream.h"
#include "DSP/EventQuantizer.h"
#include "DSP/SpectrumAnalyzer.h"
#include "DSP/Filter.h"
#include "DSP/EnvelopeFollower.h"
#include "DSP/DynamicsProcesser.h"
#include "TimeSynthComponent.generated.h"

class USoundWave;

UENUM(BlueprintType)
enum class ETimeSynthBeatDivision : uint8
{
	One			UMETA(DisplayName = "1"),
	Two			UMETA(DisplayName = "2"),
	Four		UMETA(DisplayName = "4"),
	Eight		UMETA(DisplayName = "8"),
	Sixteen		UMETA(DisplayName = "16"),

	Count		UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ETimeSynthFFTSize : uint8
{
	Min_64,
	Small_256,
	Medium_512,
	Large_1024,
};


// An enumeration for specifying quantization for time synth clips
UENUM(BlueprintType)
enum class ETimeSynthEventClipQuantization : uint8
{
	Global					UMETA(DisplayName = "Global Quantization"),
	None					UMETA(DisplayName = "No Quantization"),
	Bars8					UMETA(DisplayName = "8 Bars"),
	Bars4					UMETA(DisplayName = "4 Bars"),
	Bars2					UMETA(DisplayName = "2 Bars"),
	Bar						UMETA(DisplayName = "1 Bar"),
	HalfNote				UMETA(DisplayName = "1/2"),
	HalfNoteTriplet			UMETA(DisplayName = "1/2 T"),
	QuarterNote				UMETA(DisplayName = "1/4"),
	QuarterNoteTriplet		UMETA(DisplayName = "1/4 T"),
	EighthNote				UMETA(DisplayName = "1/8"),
	EighthNoteTriplet		UMETA(DisplayName = "1/8 T"),
	SixteenthNote			UMETA(DisplayName = "1/16"),
	SixteenthNoteTriplet	UMETA(DisplayName = "1/16 T"),
	ThirtySecondNote		UMETA(DisplayName = "1/32"),

	Count					UMETA(Hidden)
};

// An enumeration for specifying "global" quantization for all clips if clips choose global quantization enumeration.
UENUM(BlueprintType)
enum class ETimeSynthEventQuantization : uint8
{
	None					UMETA(DisplayName = "No Quantization"),
	Bars8					UMETA(DisplayName = "8 Bars"),
	Bars4					UMETA(DisplayName = "4 Bars"),
	Bars2					UMETA(DisplayName = "2 Bars"),
	Bar						UMETA(DisplayName = "1 Bar"),
	HalfNote				UMETA(DisplayName = "1/2"),
	HalfNoteTriplet			UMETA(DisplayName = "1/2 T"),
	QuarterNote				UMETA(DisplayName = "1/4"),
	QuarterNoteTriplet		UMETA(DisplayName = "1/4 T"),
	EighthNote				UMETA(DisplayName = "1/8"),
	EighthNoteTriplet		UMETA(DisplayName = "1/8 T"),
	SixteenthNote			UMETA(DisplayName = "1/16"),
	SixteenthNoteTriplet	UMETA(DisplayName = "1/16 T"),
	ThirtySecondNote		UMETA(DisplayName = "1/32"),

	Count					UMETA(Hidden)
};

// An enumeration specifying which filter to use
UENUM(BlueprintType)
enum class ETimeSynthFilter : uint8
{
	FilterA,
	FilterB,

	Count					UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthSpectralData
{
	GENERATED_USTRUCT_BODY()

	// The frequency hz of the spectrum value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpectralData")
	float FrequencyHz;

	// The magnitude of the spectrum at this frequency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpectralData")
	float Magnitude;
};


// Called to get playback time progress callbacks. Time is based off the synth time clock, not game thread time so time will be accurate relative to the synth (minus thread communication latency).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeSynthPlaybackTime, float, SynthPlaybackTimeSeconds);

// Called on the given quantization type. Supplies quantization event type, the number of bars, and the beat fraction that the event happened in that bar.
// Beat is a float between 0.0 and the quantization setting for BeatsPerBar. Fractional beats are sub-divisions of a beat.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnQuantizationEvent, ETimeSynthEventQuantization, QuantizationType, int32, NumBars, float, Beat);

DECLARE_DYNAMIC_DELEGATE_ThreeParams(FOnQuantizationEventBP, ETimeSynthEventQuantization, QuantizationType, int32, NumBars, float, Beat);


// Struct defining the time synth global quantization settings
USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthQuantizationSettings
{
	GENERATED_USTRUCT_BODY()

	// The beats per minute of the pulse. Musical convention gives this as BPM for "quarter notes" (BeatDivision = 4).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth|PlayClip", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float BeatsPerMinute;

	// Defines numerator when determining beat time in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth|PlayClip", meta = (ClampMin = "1", UIMin = "1"))
	int32 BeatsPerBar;

	// Amount of beats in a whole note. Defines number of beats in a measure.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth|PlayClip", meta = (ClampMin = "1", UIMin = "1"))
	ETimeSynthBeatDivision BeatDivision;

	// The amount of latency to add to time synth events to allow BP delegates to perform logic on game thread
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth|PlayClip", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EventDelaySeconds;

	// This is the rate at which FOnTimeSynthEvent callbacks are made.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth|PlayClip")
	ETimeSynthEventQuantization GlobalQuantization;

	FTimeSynthQuantizationSettings()
		: BeatsPerMinute(90.0f)
		, BeatsPerBar(4)
		, BeatDivision(ETimeSynthBeatDivision::Four)
		, EventDelaySeconds(0.1f)
		, GlobalQuantization(ETimeSynthEventQuantization::Bar)
	{
	}
};

// Struct using to define a time range for the time synth in quantized time units
USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthTimeDef
{
	GENERATED_USTRUCT_BODY()

	// The number of bars
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumBars;

	// The number of beats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|TimeSynth", meta = (ClampMin = "0", UIMin = "0"))
	int32 NumBeats;

	FTimeSynthTimeDef()
		: NumBars(1)
		, NumBeats(0)
	{}
};

// Struct used to define a handle to a clip
USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthClipHandle
{
	GENERATED_USTRUCT_BODY()

	// The number of bars
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synth|TimeSynth")
	FName ClipName;

	// The Id of the clip
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Synth|TimeSynth")
	int32 ClipId;

	FTimeSynthClipHandle()
		: ClipName(TEXT("Invalid"))
		, ClipId(INDEX_NONE)
	{}

	bool operator==(const FTimeSynthClipHandle& Other) const
	{
		return ClipName == Other.ClipName && ClipId == Other.ClipId;
	}
};

USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthClipSound
{
	GENERATED_USTRUCT_BODY()

	// The sound wave clip to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClipSound")
	USoundWave* SoundWave;

	// The sound wave clip to play
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClipSound")
	float RandomWeight;

	// The distance range of the clip. If zeroed, will play the clip at any range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClipSound")
	FVector2D DistanceRange;

	FTimeSynthClipSound()
		: SoundWave(nullptr)
		, RandomWeight(1.0f)
		, DistanceRange(0.0f, 0.0f)
	{}
};

UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class TIMESYNTH_API UTimeSynthVolumeGroup : public UObject
{
	GENERATED_BODY()

public:

	// The default volume of the volume group
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip")
	float DefaultVolume;
};

UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class TIMESYNTH_API UTimeSynthClip : public UObject
{
	GENERATED_BODY()

public:

	// Array of possible choices for the clip, allows randomization and distance picking
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clip")
	TArray<FTimeSynthClipSound> Sounds;

	// The volume scale range of the clip
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VolumeControl")
	FVector2D VolumeScaleDb;

	// The pitch scale range of the clip (in semi-tone range)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeControl")
	FVector2D PitchScaleSemitones;

	// The amount of time to fade in the clip from the start
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VolumeControl")
	FTimeSynthTimeDef FadeInTime;

	// If true, the clip will apply a fade when the clip duration expires. Otherwise, the clip plays out past the "duration".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VolumeControl")
	bool bApplyFadeOut;

	// The amount of time to fade out the clip when it reaches its set duration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VolumeControl", meta=(EditCondition="bApplyFadeOut"))
	FTimeSynthTimeDef FadeOutTime;

	// The clip duration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeControl")
	FTimeSynthTimeDef ClipDuration;

	// The clip duration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeControl")
	ETimeSynthEventClipQuantization ClipQuantization;

	UTimeSynthClip()
		: VolumeScaleDb(0.0f, 0.0f)
		, PitchScaleSemitones(0.0f, 0.0f)
		, bApplyFadeOut(true)
		, ClipQuantization(ETimeSynthEventClipQuantization::Global)
	{
	}
};

UENUM(BlueprintType)
enum class ETimeSynthFilterType : uint8
{
	LowPass = 0,
	HighPass,
	BandPass,
	BandStop,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthFilterSettings
{
	GENERATED_USTRUCT_BODY()

	// The type of filter to use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	ETimeSynthFilterType FilterType;

	// The filter cutoff frequency
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = "20.0", UIMin = "20.0", UIMax = "12000.0"))
	float CutoffFrequency;

	// The filter resonance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = "0.5", ClampMax = "10.0", UIMin = "0.5", UIMax = "10.0"))
	float FilterQ;
};

UENUM(BlueprintType)
enum class ETimeSynthEnvelopeFollowerPeakMode : uint8
{
	MeanSquared = 0,
	RootMeanSquared,
	Peak,
	Count UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct TIMESYNTH_API FTimeSynthEnvelopeFollowerSettings
{
	GENERATED_USTRUCT_BODY()

	// The attack time of the envelope follower in milliseconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Envelope Follower", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AttackTime;

	// The release time of the envelope follower in milliseconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Envelope Follower", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReleaseTime;

	// The peak mode of the envelope follower
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Envelope Follower")
	ETimeSynthEnvelopeFollowerPeakMode PeakMode;

	// Whether or not the envelope follower is in analog mode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Envelope Follower")
	bool bIsAnalogMode;
};

class UTimeSynthComponent;

// Class which implements the IQuantizedEventListener
// Forwards to the owning TimeSynth.
class FTimeSynthEventListener : public Audio::IQuantizedEventListener
{
public:
	FTimeSynthEventListener()
		: TimeSynth(nullptr)
	{}

	FTimeSynthEventListener(UTimeSynthComponent* InTimeSynth)
		: TimeSynth(InTimeSynth)
	{}

	//~ Begin IEventListener Interface
	virtual void OnEvent(Audio::EEventQuantization EventQuantizationType, int32 Bars, float Beat) override;
	//~ End IEventListener Interface

private:
	UTimeSynthComponent* TimeSynth;
};

class ITimeSynthSpectrumAnalysisTaskData
{
public:
	virtual Audio::FSpectrumAnalyzer* GetAnalyzer() = 0;
	virtual void OnAnalysisDone() = 0;
};

class FTimeSynthSpectrumAnalysisTask : public FNonAbandonableTask
{
	friend class FAutoDeleteAsyncTask<FTimeSynthSpectrumAnalysisTask>;

	FTimeSynthSpectrumAnalysisTask(Audio::FSpectrumAnalyzer* InAnalyzer, FThreadSafeCounter* InTaskCounter)
		: Analyzer(InAnalyzer)
		, TaskCounter(InTaskCounter)
	{
		TaskCounter->Increment();
	}

	void DoWork()
	{
		while (Analyzer->PerformAnalysisIfPossible());

		TaskCounter->Decrement();
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FTimeSynthSpectrumAnalysisTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	Audio::FSpectrumAnalyzer* Analyzer;
	FThreadSafeCounter* TaskCounter;
};

UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class TIMESYNTH_API UTimeSynthComponent :	public USynthComponent
{
	GENERATED_BODY()

	UTimeSynthComponent(const FObjectInitializer& ObjectInitializer);
	~UTimeSynthComponent();

	//~ Begin USynthComponent
	virtual bool Init(int32& SampleRate) override;
	virtual void OnEndGenerate() override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;
	//~ End USynthComponent

	//~ Begin UObject 
	virtual void PostInitProperties() override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	//~ End UObject 

	//~ Begin ActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End ActorComponent Interface

public:

	// Sets the quantization settings on the time synth
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void SetQuantizationSettings(const FTimeSynthQuantizationSettings& InQuantizationSettings);

	// Sets just the BPM of the time synth on the next bar event.
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void SetBPM(const float BeatsPerMinute);

	// Returns the current BPM of the time synth
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	int32 GetBPM() const;

	// Sets the seed of the internal random stream so choices can be repeated or controlled.
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void SetSeed(int32 InSeed);

	// Resets the internal seed to it's current seed (allows repeating same random choices)
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void ResetSeed();

	// Plays the given clip using the global quantization setting
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	FTimeSynthClipHandle PlayClip(UTimeSynthClip* InClip, UTimeSynthVolumeGroup* InVolumeGroup = nullptr);

	// Stops the clip on the desired quantization boundary with given fade time. Uses clip's fade time.
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void StopClip(FTimeSynthClipHandle InClipHandle, ETimeSynthEventClipQuantization EventQuantization);

	// Stops the clip on the desired quantization boundary with given fade time. Overrides the clip's fade time.
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void StopClipWithFadeOverride(FTimeSynthClipHandle InClipHandle, ETimeSynthEventClipQuantization EventQuantization, const FTimeSynthTimeDef& FadeTime);

	// Sets the volume (in dB) of the given volume group over the supplied FadeTime
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void SetVolumeGroup(UTimeSynthVolumeGroup* InVolumeGroup, float VolumeDb, float FadeTimeSec = 0.0f);

	// Stops clips playing on given volume group. Clips use their fade time.
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void StopSoundsOnVolumeGroup(UTimeSynthVolumeGroup* InVolumeGroup, ETimeSynthEventClipQuantization EventQuantization);

	// Stops clips playing on given volume group with the given fade time override.
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	void StopSoundsOnVolumeGroupWithFadeOverride(UTimeSynthVolumeGroup* InVolumeGroup, ETimeSynthEventClipQuantization EventQuantization, const FTimeSynthTimeDef& FadeTime);

	// Returns the spectral data if spectrum analysis is enabled. 
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	TArray<FTimeSynthSpectralData> GetSpectralData() const;

	// Returns the current envelope follower value. Call at whatever rate desired
	UFUNCTION(BlueprintCallable, Category = "TimeSynth")
	float GetEnvelopeFollowerValue() const { return CurrentEnvelopeValue; }

	// The default quantizations settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TimeSynth")
	FTimeSynthQuantizationSettings QuantizationSettings;

	// Whether or not we are enabling spectrum analysis on the synth component. Enabling will result in FFT analysis being run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectral Analysis")
	uint8 bEnableSpectralAnalysis:1;

	// What frequencies to report magnitudes for during spectrum analysis
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectral Analysis", meta = (EditCondition="bEnableSpectralAnalysis"))
	TArray<float> FrequenciesToAnalyze;

	// What FFT bin-size to use. Smaller makes it more time-reactive but less accurate in frequency space.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spectral Analysis", meta = (EditCondition = "bEnableSpectralAnalysis"))
	ETimeSynthFFTSize FFTSize;

	// Delegate to get continuous playback time in seconds
	UPROPERTY(BlueprintAssignable, Category = "TimeSynth")
	FOnTimeSynthPlaybackTime OnPlaybackTime;

	// Whether or not the filter A is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	uint8 bIsFilterAEnabled : 1;

	// Whether or not the filter B is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	uint8 bIsFilterBEnabled : 1;

	// The filter settings to use for filter A
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bIsFilterEnabled"))
	FTimeSynthFilterSettings FilterASettings;

	// The filter settings to use for filter B
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bIsFilterEnabled"))
	FTimeSynthFilterSettings FilterBSettings;

	// Whether or not the filter is enabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	uint8 bIsEnvelopeFollowerEnabled : 1;

	// The envelope follower settings to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (EditCondition = "bisEnvelopeFollowerEnabled"))
	FTimeSynthEnvelopeFollowerSettings EnvelopeFollowerSettings;

	// Register an event to respond to a specific quantization event
	UFUNCTION(BlueprintCallable, Category = "Quantization", meta = (WorldContext = "WorldContextObject"))
	void AddQuantizationEventDelegate(ETimeSynthEventQuantization QuantizationType, const FOnQuantizationEventBP& OnQuantizationEvent);

	// Set the filter settings for the filter at the particular index
	UFUNCTION(BlueprintCallable, Category = "Filter", meta = (WorldContext = "WorldContextObject"))
	void SetFilterSettings(ETimeSynthFilter Filter, const FTimeSynthFilterSettings& InSettings);

	// Set the envelope follower settings
	UFUNCTION(BlueprintCallable, Category = "Filter", meta = (WorldContext = "WorldContextObject"))
	void SetEnvelopeFollowerSettings(const FTimeSynthEnvelopeFollowerSettings& InSettings);

	// Enables or disables the filter
	UFUNCTION(BlueprintCallable, Category = "Filter", meta = (WorldContext = "WorldContextObject"))
	void SetFilterEnabled(ETimeSynthFilter Filter, bool bIsEnabled);

	// Enables or disables the envelope follower
	UFUNCTION(BlueprintCallable, Category = "Filter", meta = (WorldContext = "WorldContextObject"))
	void SetEnvelopeFollowerEnabled(bool bInIsEnabled);

	// Sets the desired FFT Size for the spectrum analyzer
	UFUNCTION(BlueprintCallable, Category = "Spectral Analysis", meta = (WorldContext = "WorldContextObject"))
	void SetFFTSize(ETimeSynthFFTSize InFFTSize);

private:
	// Called when a new event happens when registered
	void OnQuantizationEvent(Audio::EEventQuantization EventQuantizationType, int32 Bars, float Beat);

	// Method to execute commands on game thread, communicated from the audio render thread
	void GameCommand(TFunction<void()> Command);
	void PumpGameCommandQueue();
	void UpdateFilter(int32 FilterIndex);
	void UpdateEnvelopeFollower();
	void ShutdownPlayingClips();

	Audio::FSpectrumAnalyzerSettings::EFFTSize GetFFTSize(ETimeSynthFFTSize InSize) const;

	// Defines type for a volume group ID
	typedef uint32 VolumeGroupUniqueId;

	// Struct to hold playing clip info
	struct FPlayingClipInfo
	{
		// The clip quantization to use
		Audio::EEventQuantization ClipQuantization;

		// Clip volume scale
		float VolumeScale;

		// Clip pitch scale
		float PitchScale;

		// The handle to the decoding sound source for this clip
		Audio::FDecodingSoundSourceHandle DecodingSoundSourceHandle;

		// The frame when this clip starts within the audio buffer callback
		uint32 StartFrameOffset;

		// Frame count of the clip
		uint32 CurrentFrameCount;

		// Duration values in frames
		uint32 DurationFrames;
		uint32 FadeInDurationFrames;
		uint32 FadeOutDurationFrames;

		// Handle used by BP to control this playing clip
		FTimeSynthClipHandle Handle;

		// The id of the volume group this clip is in
		VolumeGroupUniqueId VolumeGroupId;

		UTimeSynthClip* SynthClip;

		bool bIsGloballyQuantized;

		FPlayingClipInfo()
			: ClipQuantization(Audio::EEventQuantization::Bar)
			, VolumeScale(1.0f)
			, PitchScale(1.0f)
			, StartFrameOffset(0)
			, CurrentFrameCount(0)
			, DurationFrames(0)
			, FadeInDurationFrames(0)
			, FadeOutDurationFrames(0)
			, VolumeGroupId(INDEX_NONE)
			, SynthClip(nullptr)
			, bIsGloballyQuantized(false)
		{}
	};

	// Delegates for event quantization on game thread
	FOnQuantizationEvent EventNotificationDelegates_GameThread[(int32)ETimeSynthEventQuantization::Count];

	FTimeSynthQuantizationSettings QuantizationSettings_RenderThread;

	// Pool of playing clip data structures
	TArray<FPlayingClipInfo> PlayingClipsPool_AudioRenderThread;

	int32 CurrentPoolSize;

	// Array of free indicies int he playing clip pool
	TArray<int32> FreePlayingClipIndices_AudioRenderThread;
	TArray<int32> ActivePlayingClipIndices_AudioRenderThread;
	
	// Map of clip id to map index so clip handles can interact with the right clip on the audio render thread
	TMap<int32, int32> ClipIdToClipIndexMap_AudioRenderThread;

	// Sample rate of the time synth
	int32 SampleRate;

	// Random stream to use for random number generation of the time synth component
	FRandomStream RandomStream;

	// Object which handles the complexities of source filed decoding
	Audio::FSoundSourceDecoder SoundWaveDecoder;

	// Object which handles event quantization logic and notifications.
	Audio::FEventQuantizer EventQuantizer;

	Audio::EEventQuantization GlobalQuantization;

	// Scratch buffer to mix in source audio to from decoder
	Audio::AlignedFloatBuffer AudioScratchBuffer;

	FTimeSynthEventListener TimeSynthEventListener;

	// Clips which are playing
	TArray<TSharedPtr<FPlayingClipInfo>> PlayingClips;

	// Handles to decoding sound sources 
	TArray<Audio::FDecodingSoundSourceHandle> DecodingSounds_AudioThread;
	TArray<Audio::FDecodingSoundSourceHandle> DecodingSounds_GameThread;
	TArray<Audio::FDecodingSoundSourceHandle> DecodingSounds_AudioRenderThread;

	// Audio render thread version of the filter settings
	FTimeSynthFilterSettings FilterSettings_AudioRenderThread[2];
	bool bIsFilterEnabled_AudioRenderThread[2];

	FTimeSynthEnvelopeFollowerSettings EnvelopeFollowerSettings_AudioRenderThread;
	uint8 bIsEnvelopeFollowerEnabled_AudioRenderThread : 1;
	float CurrentEnvelopeValue;

	// Used for sending commands from audio render thread to game thread
	TQueue<TFunction<void()>> GameCommandQueue;

	struct FVolumeGroupData
	{
		// The volume in decibels of the volume group
		float TargetVolumeDb;
		float StartVolumeDb;
		float CurrentVolumeDb;

		float CurrentTime;
		float TargetFadeTime;

		// Array of clips associated with this volume group
		TArray<FTimeSynthClipHandle> Clips;

		FVolumeGroupData()
			: TargetVolumeDb(0.0f)
			, StartVolumeDb(0.0f)
			, CurrentVolumeDb(0.0f)
			, CurrentTime(0.0f)
			, TargetFadeTime(0.0f)
		{}
	};
	void SetVolumeGroupInternal(FVolumeGroupData& InData, float VolumeDb, float FadeTimeSec);

	TMap<VolumeGroupUniqueId, FVolumeGroupData> VolumeGroupData;

	// Spectum analyzer to allow BP delegates to visualize music
	Audio::FSpectrumAnalyzer SpectrumAnalyzer;
	Audio::FSpectrumAnalyzerSettings SpectrumAnalyzerSettings;
	FThreadSafeCounter SpectrumAnalysisCounter;

	// Array of spectrum data, maps to FrequenciesToAnalyze UProperty
	TArray<FTimeSynthSpectralData> SpectralData;

	// Using a state variable filter
	Audio::FStateVariableFilter Filter[2];

	// Envelope follower DSP object
	Audio::FEnvelopeFollower EnvelopeFollower;

	// Need to limit output to prevent wrap around issues when converting to int16
	Audio::FDynamicsProcessor DynamicsProcessor;

	friend class FTimeSynthEventListener;
};
