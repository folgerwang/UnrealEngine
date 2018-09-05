// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef VIDEOIO_EXPORTS
#define VIDEOIO_API __declspec(dllexport)
#else
#define VIDEOIO_API __declspec(dllimport)
#endif

namespace BlackmagicDevice
{
	/*
	* Types provided from the interface
	*/

	struct IPortShared;

	typedef void* FDeviceScanner;
	typedef void* FDeviceInfo;
	typedef void* FDevice;
	typedef IPortShared* FPort;
	typedef void* FFrame;

	typedef unsigned int FUInt;
	typedef unsigned char FUByte;

	using LoggingCallbackPtr = void(*)(const TCHAR* Format, ...);

	enum struct EFrameFormat
	{
		FF_PALI,
		FF_NTSCI,
		FF_NTSCP,
		FF_720P,
		FF_1080I,
		FF_1080P,
		FF_AUTO,
	};

	enum struct EPixelFormat
	{
		PF_UYVY,
		PF_ARGB,
		PF_MAX,
	};

	enum struct EFrameRate
	{
		FR_2398,
		FR_2400,
		FR_2500,
		FR_2997,
		FR_3000,
		FR_5000,
		FR_5994,
		FR_6000,
		FR_AUTO,
	};

	enum struct EModeFilter
	{		
		MF_INPUT,
		MF_OUTPUT,
		MF_BOTH,
	};

	/*
	 * Format of the required frame format
	 */

	struct FFrameDesc
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

	struct FPortOptions
	{
		/** use timecode with the port */
		bool bUseTimecode;

		/** use sync only */
		bool bUseSync;

		/** enable video */
		bool bUseVideo;

		/** deliver frames from blackmagic callback */
		bool bUseCallback;

		/** enable audio */
		bool bUseAudio;

		/** port is for output */
		bool bOutput;

		/** number of allocated frame buffers */
		FUInt FrameBuffers;
		
		/** output should have a key channel */
		/** output port also sends key on port + 1 */
		bool bOutputKey;
		
		/** number of audio channels */		
		FUInt AudioChannels;	// number of audio channels to capture
	};

	/*
	 * Information about a given frame desc
	 */

	struct FFrameInfo
	{
		/** Is Drop framerate */
		bool DropFrame;

		/** Actual framerate */
		float FrameRate;

		/** Root framerate to calculate timecode */
		float RootFrameRate;

		/** Clocks per Second */
		FUInt TimeScale;

		/** Clocks per Frame */
		FUInt TimeValue;

		/** Image Width in pixels */
		FUInt Width;

		/** Image Height in pixels */
		FUInt Height;

		/** Aspect Ratio Width */
		FUInt RatioWidth;

		/** Aspect Ratio Height */
		FUInt RatioHeight;

		/** number of effective bytes per pixel (YUYV is 2 bytes per pixel) */
		FUInt BytesPerPixel;

		/** Name of the display mode */
		const wchar_t* FormatName;
	};

	/*
	 * Timecode
	 */

	struct FTimecode
	{
		FTimecode()
			: Hours(0)
			, Minutes(0)
			, Seconds(0)
			, Frames(0)
		{
		}
		bool operator== (const FTimecode& Other) const
		{
			return Other.Hours == Hours
				&& Other.Minutes == Minutes
				&& Other.Seconds == Seconds
				&& Other.Frames == Frames;
		}
		FUInt Hours;
		FUInt Minutes;
		FUInt Seconds;
		/** limited to 30fps */
		FUInt Frames;
		bool bField;
		bool bIsDropFrame;
	};

	struct VIDEOIO_API IPortCallback
	{
		IPortCallback();
		virtual ~IPortCallback();
		
		//* only called if Option.bUseSync is true */
		virtual void OnInitializationCompleted(bool bSucceed) = 0;
		//* only called if Option.bUseVideo is true */
		//* return true if you want to hold the frame */
		virtual bool OnFrameArrived(FFrame InFrame) = 0;
	};

	/*
	 * Configure Logging
	 */

	VIDEOIO_API void VideoIOSetLoggingCallbacks(LoggingCallbackPtr LogInfoFunc, LoggingCallbackPtr LogWarningFunc, LoggingCallbackPtr LogErrorFunc);

	/*
	 * VideoFormat
	 */

	VIDEOIO_API bool VideoIOFrameDescSupported(const FFrameDesc& InFrameDesc);
	VIDEOIO_API bool VideoIOFrameDesc2Info(const FFrameDesc& InFrameDesc, FFrameInfo& OutFrameInfo);
	VIDEOIO_API bool VideoIOFrameDesc2Name(const FFrameDesc& InFrameDesc, TCHAR* OutModeName, FUInt InSize);
	VIDEOIO_API FUInt VideoIOModeCount();
	VIDEOIO_API bool VideoIOModeNames(FUInt InMode, EModeFilter InModeFilter, TCHAR* OutModeName, FUInt InSize);
	VIDEOIO_API bool VideoIOModeFrameDesc(FUInt InMode, FFrameDesc& OutFrameDesc);

	/*
	 * DeviceScanner
	 */

	VIDEOIO_API FDeviceScanner VideoIOCreateDeviceScanner(void);
	VIDEOIO_API void VideoIOReleaseDeviceScanner(FDeviceScanner InDeviceScanner);

	VIDEOIO_API FUInt VideoIODeviceScannerGetNumDevices(FDeviceScanner InDeviceScanner);
	VIDEOIO_API void VideoIODeviceScannerScanHardware(FDeviceScanner InDeviceScanner);
	VIDEOIO_API FDeviceInfo VideoIODeviceScannerGetDeviceInfo(FDeviceScanner InDeviceScanner, FUInt InDeviceIndex);

	/*
	 * Device Info
	 */

	VIDEOIO_API void VideoIOReleaseDeviceInfo(FDeviceInfo InDeviceInfo);
	VIDEOIO_API bool VideoIODeviceInfoGetDeviceId(FDeviceInfo InDeviceInfo, TCHAR* OutDeviceId, FUInt InSize);

	VIDEOIO_API FUInt VideoIODeviceInfoGetVidInputs(FDeviceInfo InDeviceInfo);
	VIDEOIO_API FUInt VideoIODeviceInfoGetVidOutputs(FDeviceInfo InDeviceInfo);

	/*
	 * Device/Card
	 */

	VIDEOIO_API FDevice VideoIOCreateDevice(FUInt InDeviceIndex);
	VIDEOIO_API void VideoIOReleaseDevice(FDevice InDevice);

	/*
	 * Frame
	 */

	VIDEOIO_API void VideoIOReleaseFrame(FFrame InFrame);

	/* Returns Frame Stride */
	VIDEOIO_API FUInt VideoIOFrameDimensions(FFrame InFrame, FUInt& OutWidth, FUInt& OutHeight);

	VIDEOIO_API FUByte* VideoIOFrameVideoBuffer(FFrame InFrame, FUInt& OutSize);
	VIDEOIO_API int32_t* VideoIOFrameAudioBuffer(FFrame InFrame, FUInt& OutSize, FUInt& OutNumChannels, FUInt& OutAudioRate, FUInt& OutNumSamples);

	VIDEOIO_API FUByte* VideoIOFrameMetaBuffer(FFrame InFrame, FUInt& OutSize);
	VIDEOIO_API void VideoIOFrameTimecode(FFrame InFrame, FTimecode& outTimecode);
	VIDEOIO_API void VideoIOFrameDesc(FFrame InFrame, FFrameDesc& OutFrameDesc);

	struct VIDEOIO_API IPortShared
	{
		virtual ~IPortShared() {}
		virtual void Release() = 0;

		virtual bool PeekFrame() = 0;
		virtual FFrame WaitFrame() = 0;

		virtual bool WaitVSync() = 0;
		virtual bool GetTimecode(FTimecode& OutTimecode) = 0;
		virtual FUInt FrameDropCount() = 0;

		virtual bool SetCallback(IPortCallback* InCallback) = 0;
	};

	VIDEOIO_API IPortShared* VideoIODeviceOpenSharedPort(FDevice InDevice, FUInt InPortIndex, const FFrameDesc& InFrameDesc, const FPortOptions& InOptions);
};
