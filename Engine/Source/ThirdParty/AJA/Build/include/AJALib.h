// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <memory>

#ifdef AJA_EXPORTS
#define AJA_API __declspec(dllexport)
#else
#define AJA_API __declspec(dllimport)
#endif

namespace AJA
{

/*
* Types provided from the interface
*/

	typedef void* FDeviceScanner;
	typedef void* FDeviceInfo;
	typedef void* FAJADevice;

	using LoggingCallbackPtr =  void (*)(const TCHAR* Format, ...);

	/*
	* Pixel formats supported
	*/

	enum struct EPixelFormat
	{
		PF_UYVY,
		PF_ARGB,
	};

	/*
	* Frame formats supported
	* PSF - Progressive segmented frame - https://en.wikipedia.org/wiki/Progressive_segmented_frame
	* PSF is returned when interlace frame is passed to AJA, and the progressive input flag is set.
	*/

	enum struct EFrameFormat
	{
		FF_PALI,
		FF_PALPSF,
		FF_NTSCI,
		FF_NTSCPSF,
		FF_720P,
		FF_1080I,
		FF_1080PSF,
		FF_1080P,
		FF_AUTO,
		FF_UNKNOWN,
	};

	/*
	* Frame Rates
	*/

	enum struct EFrameRate
	{
		FR_2398,
		FR_2400,
		FR_2500,
		FR_2997,
		FR_3000,
		FR_5000,
		FR_5000A,
		FR_5000B,
		FR_5994,
		FR_5994A,
		FR_5994B,
		FR_6000,
		FR_6000A,
		FR_6000B,
		FR_AUTO,
		FR_UNKNOWN,
	};

	enum struct EDirectionFilter
	{
		DF_INPUT,
		DF_OUTPUT,
	};

	/*
	* Format of the required frame format
	*/

	struct AJA_API FFrameDesc
	{
		FFrameDesc()
			: FrameFormat(EFrameFormat::FF_AUTO)
			, PixelFormat(EPixelFormat::PF_ARGB)
			, FrameRate(EFrameRate::FR_AUTO)
		{
		}

		FFrameDesc(EFrameFormat InFrameFormat, EPixelFormat InPixelFormat, EFrameRate InFrameRate)
			: FrameFormat(InFrameFormat)
			, PixelFormat(InPixelFormat)
			, FrameRate(InFrameRate)
		{
		}
		bool operator== (const FFrameDesc& Other) const
		{
			return FrameFormat == Other.FrameFormat
				&& PixelFormat == Other.PixelFormat
				&& FrameRate == Other.FrameRate;
		}
		EFrameFormat FrameFormat;
		EPixelFormat PixelFormat;
		EFrameRate FrameRate;
	};

	/*
	* Information about a given frame desc
	*/

	struct AJA_API FFrameInfo
	{
		FFrameInfo()
			: DropFrame(false)
			, FrameRate(60.0f)
			, RootFrameRate(60.0f)
			, TimeScale(1)
			, TimeValue(60)
			, Width(0)
			, Height(0)
			, RatioWidth(1)
			, RatioHeight(1)
			, BytesPerPixel(4)
		{
		}

		/** Is Drop framerate */
		bool DropFrame;

		/** Actual framerate */
		float FrameRate;

		/** Root framerate to calculate timecode */
		float RootFrameRate;

		/** Clocks per Second */
		uint32_t TimeScale;

		/** Clocks per Frame */
		uint32_t TimeValue;

		/** Image Width in pixels */
		uint32_t Width;

		/** Image Height in pixels */
		uint32_t Height;

		/** Aspect Ratio Width */
		uint32_t RatioWidth;

		/** Aspect Ratio Height */
		uint32_t RatioHeight;

		/** number of effective bytes per pixel (YUYV is 2 bytes per pixel) */
		uint32_t BytesPerPixel;
	};

	/*
	* Timecode
	*/

	struct AJA_API FTimecode
	{
		FTimecode();
		bool operator== (const FTimecode& Other) const;

		uint32_t Hours;
		uint32_t Minutes;
		uint32_t Seconds;
		uint32_t Frames;
	};

	/*
	 * Format support
	 */

	AJA_API	uint32_t ModeCount();
	AJA_API bool ModeNames(uint32_t InMode, EDirectionFilter InDirectionFilter, TCHAR* OutModeName, uint32_t InSize);
	AJA_API bool Mode2FrameDesc(uint32_t InMode, EDirectionFilter InDirectionFilter, FFrameDesc& OutFrameDesc);
	AJA_API bool FrameDescSupported(const FFrameDesc& InFrameDesc, EDirectionFilter InDirectionFilter);
	AJA_API bool FrameDesc2Info(const FFrameDesc& InFrameDesc, FFrameInfo& OutFrameInfo);
	AJA_API bool FrameDesc2Name(const FFrameDesc& InFrameDesc, TCHAR* OutModeName, uint32_t InSize);

	/*
	 * Logging Callbacks
	 */

	AJA_API void SetLoggingCallbacks(LoggingCallbackPtr LogInfoFunc, LoggingCallbackPtr LogWarningFunc, LoggingCallbackPtr LogErrorFunc);

	/*
	 * DeviceScanner
	 */

	AJA_API FDeviceScanner CreateDeviceScanner(void);
	AJA_API void ReleaseDeviceScanner(FDeviceScanner InDeviceScanner);

	AJA_API uint32_t DeviceScannerGetNumDevices(FDeviceScanner InDeviceScanner);
	AJA_API void DeviceScannerScanHardware(FDeviceScanner InDeviceScanner);
	AJA_API FDeviceInfo DeviceScannerGetDeviceInfo(FDeviceScanner InDeviceScanner, uint32_t InDeviceIndex);

	/*
	 * Device Info
	 */

	AJA_API void ReleaseDeviceInfo(FDeviceInfo InDeviceInfo);
	AJA_API bool DeviceInfoGetDeviceId(FDeviceInfo InDeviceInfo, TCHAR* OutDeviceId, uint32_t InSize);

	AJA_API uint32_t DeviceInfoGetVidInputs(FDeviceInfo InDeviceInfo);
	AJA_API uint32_t DeviceInfoGetVidOutputs(FDeviceInfo InDeviceInfo);

	namespace Private
	{
		class SyncChannel;
		class InputChannel;
		class OutputChannel;
	}

	/* AJADeviceOptions definition
	*****************************************************************************/
	enum class EAJAReferenceType
	{
		EAJA_REFERENCETYPE_EXTERNAL,
		EAJA_REFERENCETYPE_FREERUN,
		EAJA_REFERENCETYPE_INPUT,
	};

	struct AJA_API AJADeviceOptions
	{
		AJADeviceOptions(uint32_t InChannelIndex)
			: DeviceIndex(InChannelIndex)
			, ReferenceType(EAJAReferenceType::EAJA_REFERENCETYPE_FREERUN)
			, ChannelIndexForReferenceInput(0)
		{}

		uint32_t DeviceIndex;
		EAJAReferenceType ReferenceType;
		uint32_t ChannelIndexForReferenceInput;
	};

	/* AJASyncChannel definition
	*****************************************************************************/
	struct AJA_API IAJASyncChannelCallbackInterface
	{
		IAJASyncChannelCallbackInterface();
		virtual ~IAJASyncChannelCallbackInterface();

		virtual void OnInitializationCompleted(bool bSucceed) = 0;
	};

	struct AJA_API AJASyncChannelOptions
	{
		AJASyncChannelOptions(const TCHAR* DebugName, uint32_t InChannelIndex);

		IAJASyncChannelCallbackInterface* CallbackInterface;

		uint32_t ChannelIndex; // [1...x]
		bool bOutput; // port is output
		bool bUseTimecode; // enable timecode
		bool bUseLTCTimecode; // enable LTC or VITC timecode
	};

	class AJA_API AJASyncChannel
	{
	public:
		AJASyncChannel(AJASyncChannel&) = delete;
		AJASyncChannel& operator=(AJASyncChannel&) = delete;

		AJASyncChannel();
		~AJASyncChannel();

	public:
		//@param PortIndex [1...x]
		//@param bOutput port is output
		bool Initialize(const AJADeviceOptions& InDevice, const AJASyncChannelOptions& InOption);
		void Uninitialize();

		// Only available if the initialization succeeded
		bool WaitForSync(FTimecode& OutTimecode) const;
		bool GetTimecode(FTimecode& OutTimecode) const;

	private:
		Private::SyncChannel* Channel;
	};


	/* IAJAInputOutputChannelCallbackInterface definition
	*****************************************************************************/
	struct AJA_API AJAInputFrameData
	{
		AJAInputFrameData();

		FTimecode Timecode;
		uint32_t FramesDropped; // frame dropped by the AJA
	};

	struct AJA_API AJAOutputFrameData : AJAInputFrameData
	{
		AJAOutputFrameData();

		uint32_t FramesLost; // frame ready by the game but not sent to AJA
	};

	struct AJA_API AJAAncillaryFrameData
	{
		AJAAncillaryFrameData();

		uint8_t* AncBuffer;
		uint32_t AncBufferSize;
		uint8_t* AncF2Buffer;
		uint32_t AncF2BufferSize;
	};

	struct AJA_API AJAAudioFrameData
	{
		AJAAudioFrameData();

		uint8_t* AudioBuffer;
		uint32_t AudioBufferSize;
		uint32_t NumChannels;
		uint32_t AudioRate;
		uint32_t NumSamples;
	};

	struct AJA_API AJAVideoFrameData
	{
		AJAVideoFrameData();
		FFrameDesc FrameDesc;
		uint8_t* VideoBuffer;
		uint32_t VideoBufferSize;
		uint32_t Stride;
		uint32_t Width;
		uint32_t Height;
		bool bIsProgressivePicture;
	};

	struct AJA_API IAJAInputOutputChannelCallbackInterface : IAJASyncChannelCallbackInterface
	{
		IAJAInputOutputChannelCallbackInterface();

		virtual bool OnInputFrameReceived(const AJAInputFrameData& InFrameData, const AJAAncillaryFrameData& InAncillaryFrame, const AJAAudioFrameData& InAudioFrame, const AJAVideoFrameData& InVideoFrame) = 0;
		virtual bool OnOutputFrameCopied(const AJAOutputFrameData& InFrameData) = 0;
		virtual void OnCompletion(bool bSucceed) = 0;
	};

	/* AJAInputOutputChannelOptions definition
	*****************************************************************************/
	struct AJA_API AJAInputOutputChannelOptions
	{
		AJAInputOutputChannelOptions(const TCHAR* DebugName, uint32_t InChannelIndex);

		IAJAInputOutputChannelCallbackInterface* CallbackInterface;

		FFrameDesc FrameDesc;

		uint32_t NumberOfAudioChannel;
		uint32_t ChannelIndex; // [1...x]
		uint32_t SynchronizeChannelIndex; // [1...x]
		uint32_t OutputKeyChannelIndex; // [1...x] for output

		union
		{
			struct 
			{
				uint32_t bUseAutoCirculating : 1;
				uint32_t bOutput : 1; // port is output
				uint32_t bOutputKey : 1; // output will also sent the key on OutputKeyPortIndex
				uint32_t bOutputFreeRun : 1; // output as fast as the card & game can do
				uint32_t bUseTimecode : 1; // enable input/output timecode
				uint32_t bUseLTCTimecode : 1; // enable LTC or VITC timecode
				uint32_t bUseAncillary : 1; // enable ANC system
				uint32_t bUseAncillaryField2 : 1; // enable ANC Field 2 system
				uint32_t bUseAudio : 1; // enable audio input/output
				uint32_t bUseVideo : 1; // enable video input/output
				uint32_t bIsProgressivePicture : 1; // Specifies if the video format is expected to be progressive or not.
			};
			uint32_t Options;
		};
	};

	/* AJAInputChannel definition
	*****************************************************************************/
	class AJA_API AJAInputChannel
	{
	public:
		AJAInputChannel(AJAInputChannel&) = delete;
		AJAInputChannel& operator=(AJAInputChannel&) = delete;

		AJAInputChannel();
		~AJAInputChannel();

	public:
		bool Initialize(const AJADeviceOptions& InDevice, const AJAInputOutputChannelOptions& Options);
		void Uninitialize();

		// Only available if the initialization succeeded
		uint32_t GetFrameDropCount() const;

	private:
		Private::InputChannel* Channel;
	};

	/* AJAOutputChannel definition
	*****************************************************************************/
	class AJA_API AJAOutputChannel
	{
	public:
		AJAOutputChannel(AJAOutputChannel&) = delete;
		AJAOutputChannel& operator=(AJAOutputChannel&) = delete;

		AJAOutputChannel();
		~AJAOutputChannel();

	public:
		bool Initialize(const AJADeviceOptions& InDevice, const AJAInputOutputChannelOptions& Options);
		void Uninitialize();

		// Set a new video buffer that will be copied to the AJA.
		bool SetVideoBuffer(const FTimecode& InTimecode, const uint8_t* InVideoBuffer, uint32_t InVideoBufferSize);

		bool GetOutputDimension(uint32_t& OutWidth, uint32_t& OutHeight) const;

	private:
		Private::OutputChannel* Channel;
	};

}

