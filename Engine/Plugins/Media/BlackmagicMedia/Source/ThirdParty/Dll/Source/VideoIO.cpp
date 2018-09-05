// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "stdafx.h"

namespace BlackmagicDevice
{
	static struct FFrameFormatInfo
	{
		FUInt Width;
		FUInt Height;
		FUInt RatioWidth;
		FUInt RatioHeight;
		const wchar_t* FormatName;
	} FrameFormatInfo[] = {
		{  720,  576,  4, 3, TEXT("PALI"),	},
		{  720,  480,  4, 3, TEXT("NTSCI"),	},
		{  720,  486,  4, 3, TEXT("NTSCP"),	},
		{ 1280,  720, 16, 9, TEXT("720p"),	},
		{ 1920, 1080, 16, 9, TEXT("1080i"),	},
		{ 1920, 1080, 16, 9, TEXT("1080p"),	},
		/** added for auto */
		{    0,    0,  1, 1, TEXT("Automatic"),	},
	};

	static struct FPixelFormatInfo 
	{
		EPixelFormat PixelFormat;
		const wchar_t* FormatName;
	} PixelFormatInfo[] = {
		{ EPixelFormat::PF_UYVY, TEXT("YUV"), },
		{ EPixelFormat::PF_ARGB, TEXT("RGBA"), },
	};

	static struct FFrameRateInfo
	{
		/** Is this a frame rate thats normal uses a drop timecode format */
		bool DropFrame;

		/** The actual framerate clock */
		float FrameRate;

		/** Framerate for encoding the drop timecode rate */
		float RootFrameRate;

		/** Clocks per Second */
		FUInt TimeScale;

		/** Clocks per Frame */
		FUInt TimeValue;

		/** Textual format */
		const wchar_t* FormatName;

	} FrameRateInfo[] = {
		{ true,  23.98f, 24.00f, 24000, 1001, TEXT("23.98fps"), },
		{ false, 24.00f, 24.00f, 24000, 1000, TEXT("24fps"), },
		{ false, 25.00f, 25.00f, 25000, 1000, TEXT("25fps"), },
		{ true,  29.97f, 30.00f, 30000, 1001, TEXT("29.97fps"), },
		{ false, 30.00f, 30.00f, 30000, 1000, TEXT("30fps"), },
		{ false, 50.00f, 50.00f, 50000, 1000, TEXT("50fps"), },
		{ true,  59.94f, 60.00f, 60000, 1001, TEXT("59.94fps"), },
		{ false, 60.00f, 60.00f, 60000, 1000, TEXT("60fps"), },
		/** added for Auto */
		{ false, 60.00f, 60.00f,    60,    1, TEXT(""), },
	};

	static FSupportedDescription SupportedDescription[] = {
		{{ EFrameFormat::FF_AUTO, EPixelFormat::PF_UYVY, EFrameRate::FR_AUTO, },  bmdModeHD1080p6000, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, }, // Real NTSC

		{{ EFrameFormat::FF_NTSCI, EPixelFormat::PF_UYVY, EFrameRate::FR_2997, },        bmdModeNTSC, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, }, // Real NTSC
		{{  EFrameFormat::FF_PALI, EPixelFormat::PF_UYVY, EFrameRate::FR_2500, },         bmdModePAL, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, }, // Real PAL

		{{ EFrameFormat::FF_720P, EPixelFormat::PF_UYVY, EFrameRate::FR_5000, },    bmdModeHD720p50, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_720P, EPixelFormat::PF_UYVY, EFrameRate::FR_5994, },  bmdModeHD720p5994, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_720P, EPixelFormat::PF_UYVY, EFrameRate::FR_6000, },    bmdModeHD720p60, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },

		{{ EFrameFormat::FF_1080I, EPixelFormat::PF_UYVY, EFrameRate::FR_5000, },   bmdModeHD1080i50, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080I, EPixelFormat::PF_UYVY, EFrameRate::FR_5994, }, bmdModeHD1080i5994, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080I, EPixelFormat::PF_UYVY, EFrameRate::FR_6000, }, bmdModeHD1080i6000, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },

		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_2398, }, bmdModeHD1080p2398, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_2400, },   bmdModeHD1080p24, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_2500, },   bmdModeHD1080p25, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_2997, }, bmdModeHD1080p2997, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_3000, },   bmdModeHD1080p30, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_5000, },   bmdModeHD1080p50, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_5994, }, bmdModeHD1080p5994, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_UYVY, EFrameRate::FR_6000, }, bmdModeHD1080p6000, bmdFormat8BitYUV, EVIDEOIO_SD_INPUT, },

		{{ EFrameFormat::FF_NTSCI, EPixelFormat::PF_ARGB, EFrameRate::FR_2997, },        bmdModeNTSC, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, }, // Real NTSC
		{{ EFrameFormat::FF_PALI, EPixelFormat::PF_ARGB, EFrameRate::FR_2500, },         bmdModePAL, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, }, // Real PAL

		{{ EFrameFormat::FF_720P, EPixelFormat::PF_ARGB, EFrameRate::FR_5000, },    bmdModeHD720p50, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_720P, EPixelFormat::PF_ARGB, EFrameRate::FR_5994, },  bmdModeHD720p5994, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_720P, EPixelFormat::PF_ARGB, EFrameRate::FR_6000, },    bmdModeHD720p60, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },

		{{ EFrameFormat::FF_1080I, EPixelFormat::PF_ARGB, EFrameRate::FR_5000, },   bmdModeHD1080i50, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080I, EPixelFormat::PF_ARGB, EFrameRate::FR_5994, }, bmdModeHD1080i5994, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080I, EPixelFormat::PF_ARGB, EFrameRate::FR_6000, }, bmdModeHD1080i6000, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },

		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_2398, }, bmdModeHD1080p2398, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_2400, },   bmdModeHD1080p24, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_2500, },   bmdModeHD1080p25, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_2997, }, bmdModeHD1080p2997, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_3000, },   bmdModeHD1080p30, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_5000, },   bmdModeHD1080p50, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_5994, }, bmdModeHD1080p5994, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
		{{ EFrameFormat::FF_1080P, EPixelFormat::PF_ARGB, EFrameRate::FR_6000, }, bmdModeHD1080p6000, bmdFormat8BitARGB, EVIDEOIO_SD_OUTPUT, },
	};

	FSupportedDescription* GetSupportedDescription(BMDDisplayMode InDisplayMode)
	{
		for (int i = 0; i < (sizeof(SupportedDescription) / sizeof(FSupportedDescription)); i++)
		{
			if (SupportedDescription[i].DisplayMode == InDisplayMode)
			{
				return SupportedDescription + i;
			}
		}
		return nullptr;
	}

	FSupportedDescription* GetSupportedDescription(const FFrameDesc& InFrameDesc)
	{
		for (int i = 0; i < (sizeof(SupportedDescription) / sizeof(FSupportedDescription)); i++)
		{
			if (SupportedDescription[i].FrameDesc == InFrameDesc)
			{
				return SupportedDescription + i;
			}
		}
		return nullptr;
	}

	VIDEOIO_API bool VideoIOFrameDescSupported(const FFrameDesc& InFrameDesc)
	{
		return (GetSupportedDescription(InFrameDesc) != nullptr);
	}

	template<typename T>
	static T TClamp(T In, T InMin, T InMax)
	{
		if (In < InMin)
		{
			return InMin;
		}
		if (InMax < In)
		{
			return InMax;
		}
		return In;
	}

	static bool ClenseFrameDesc(const FFrameDesc& inFrameDesc, FFrameDesc& OutFrameDesc)
	{
		OutFrameDesc.FrameFormat = TClamp(inFrameDesc.FrameFormat, EFrameFormat::FF_PALI, EFrameFormat::FF_AUTO);
		OutFrameDesc.PixelFormat = TClamp(inFrameDesc.PixelFormat, EPixelFormat::PF_UYVY, EPixelFormat::PF_ARGB);
		OutFrameDesc.FrameRate = TClamp(inFrameDesc.FrameRate, EFrameRate::FR_2398, EFrameRate::FR_AUTO);
		return inFrameDesc == OutFrameDesc;
	}

	VIDEOIO_API bool VideoIOFrameDesc2Info(const FFrameDesc& InFrameDesc, FFrameInfo& OutFrameInfo)
	{
		FFrameDesc FrameDesc;
		ClenseFrameDesc(InFrameDesc, FrameDesc);

		OutFrameInfo.DropFrame = FrameRateInfo[static_cast<FUInt>(FrameDesc.FrameRate)].DropFrame;
		OutFrameInfo.FrameRate = FrameRateInfo[static_cast<FUInt>(FrameDesc.FrameRate)].FrameRate;
		OutFrameInfo.RootFrameRate = FrameRateInfo[static_cast<FUInt>(FrameDesc.FrameRate)].RootFrameRate;

		OutFrameInfo.TimeScale = FrameRateInfo[static_cast<FUInt>(FrameDesc.FrameRate)].TimeScale;
		OutFrameInfo.TimeValue = FrameRateInfo[static_cast<FUInt>(FrameDesc.FrameRate)].TimeValue;

		OutFrameInfo.Width = FrameFormatInfo[static_cast<FUInt>(FrameDesc.FrameFormat)].Width;
		OutFrameInfo.Height = FrameFormatInfo[static_cast<FUInt>(FrameDesc.FrameFormat)].Height;
		OutFrameInfo.RatioWidth = FrameFormatInfo[static_cast<FUInt>(FrameDesc.FrameFormat)].RatioWidth;
		OutFrameInfo.RatioHeight = FrameFormatInfo[static_cast<FUInt>(FrameDesc.FrameFormat)].RatioHeight;

		OutFrameInfo.FormatName = FrameFormatInfo[static_cast<FUInt>(FrameDesc.FrameFormat)].FormatName;

		OutFrameInfo.BytesPerPixel = (FrameDesc.PixelFormat == EPixelFormat::PF_UYVY) ? 2 : 4;
		return true;
	}

	VIDEOIO_API FUInt VideoIOModeCount()
	{
		return sizeof(SupportedDescription) / sizeof(FSupportedDescription);
	}

	static bool ModeNames(const FFrameDesc& InFrameDesc, TCHAR* OutModeName, FUInt InSize, bool InShort)
	{
		std::wstringstream StringStream;
		if (InShort)
		{
			StringStream << PixelFormatInfo[static_cast<FUInt>(InFrameDesc.PixelFormat)].FormatName << " "
				<< FrameFormatInfo[static_cast<FUInt>(InFrameDesc.FrameFormat)].FormatName;
		}
		else
		{
			StringStream << PixelFormatInfo[static_cast<FUInt>(InFrameDesc.PixelFormat)].FormatName << " "
				<< FrameFormatInfo[static_cast<FUInt>(InFrameDesc.FrameFormat)].FormatName << " ("
				<< FrameFormatInfo[static_cast<FUInt>(InFrameDesc.FrameFormat)].Width << "x"
				<< FrameFormatInfo[static_cast<FUInt>(InFrameDesc.FrameFormat)].Height << ") "
				<< FrameRateInfo[static_cast<FUInt>(InFrameDesc.FrameRate)].FormatName;
		}
		std::wstring String(StringStream.str());
		wcscpy_s(OutModeName, InSize, String.c_str());
		return true;
	}

	VIDEOIO_API bool VideoIOFrameDesc2Name(const FFrameDesc& InFrameDesc, TCHAR* OutModeName, FUInt InSize)
	{
		FFrameDesc FrameDesc;
		if (!OutModeName || !ClenseFrameDesc(InFrameDesc, FrameDesc))
		{
			if (OutModeName)
			{
				// return empty string;
				OutModeName[0] = 0;
			}
			return false;
		}

		return ModeNames(FrameDesc, OutModeName, InSize, true);
	}

	VIDEOIO_API bool VideoIOModeNames(FUInt InMode, EModeFilter InModeFilter, TCHAR* OutModeName, FUInt InSize)
	{
		if (InMode > sizeof(SupportedDescription) / sizeof(FSupportedDescription))
		{
			return false;
		}

		FSupportedDescription& SupportedDescriptionRef = SupportedDescription[InMode];

		if (!((InModeFilter == EModeFilter::MF_INPUT && SupportedDescriptionRef.SupportedDirection&EVIDEOIO_SD_INPUT)
			|| (InModeFilter == EModeFilter::MF_OUTPUT && SupportedDescriptionRef.SupportedDirection&EVIDEOIO_SD_OUTPUT)
			|| InModeFilter == EModeFilter::MF_BOTH))
		{
			return false;
		}

		return ModeNames(SupportedDescriptionRef.FrameDesc, OutModeName, InSize, false);
	}

	VIDEOIO_API bool VideoIOModeFrameDesc(FUInt InMode, FFrameDesc& OutFrameDesc)
	{
		if (InMode > sizeof(SupportedDescription) / sizeof(FSupportedDescription))
		{
			// out of range, so return the first mode.
			OutFrameDesc = SupportedDescription[0].FrameDesc;
			return false;
		}
		OutFrameDesc = SupportedDescription[InMode].FrameDesc;
		return true;
	}
	
	/*
	 * Logging Callbacks
	 */

	VIDEOIO_API void VideoIOSetLoggingCallbacks(LoggingCallbackPtr LogInfoFunc, LoggingCallbackPtr LogWarningFunc, LoggingCallbackPtr LogErrorFunc)
	{
		GLogInfo = LogInfoFunc;
		GLogWarning = LogWarningFunc;
		GLogError = LogErrorFunc;
	}

	/*
	 * IPortCallback Methods 
	 */

	IPortCallback::IPortCallback()
	{
	}

	IPortCallback::~IPortCallback()
	{
	}

	/*
	 * Device Scanner
	 */

	VIDEOIO_API FDeviceScanner VideoIOCreateDeviceScanner(void)
	{
		{	// Handle CoInitialize
			static bool IsInitialized = false;
			if (!IsInitialized)
			{
				::CoInitializeEx(NULL, COINIT_MULTITHREADED);
				IsInitialized = true;
			}
		}

		return new PrivateDeviceScanner();
	}

	VIDEOIO_API void VideoIOReleaseDeviceScanner(FDeviceScanner InDeviceScanner)
	{
		PrivateDeviceScanner* DeviceScanner = reinterpret_cast<PrivateDeviceScanner*>(InDeviceScanner);
		DeviceScanner->Release();
	}

	VIDEOIO_API FUInt VideoIODeviceScannerGetNumDevices(FDeviceScanner InDeviceScanner)
	{
		PrivateDeviceScanner* DeviceScanner = reinterpret_cast<PrivateDeviceScanner*>(InDeviceScanner);
		return DeviceScanner->GetDeviceCount();
	}

	VIDEOIO_API void VideoIODeviceScannerScanHardware(FDeviceScanner InDeviceScanner)
	{
	}

	VIDEOIO_API FDeviceInfo VideoIODeviceScannerGetDeviceInfo(FDeviceScanner InDeviceScanner, FUInt InDeviceId)
	{
		PrivateDeviceScanner* DeviceScanner = reinterpret_cast<PrivateDeviceScanner*>(InDeviceScanner);
		RefPointer<IDeckLink> DeckLink(DeviceScanner->GetDevice(InDeviceId));
		if (DeckLink)
		{
			return new PrivateDeviceScannerInfo(InDeviceId, DeckLink);
		}
		return nullptr;
	}

	/*
	 * Device Info
	 */

	VIDEOIO_API void VideoIOReleaseDeviceInfo(FDeviceInfo InDeviceInfo)
	{
		PrivateDeviceScannerInfo* DeviceScannerInfo = reinterpret_cast<PrivateDeviceScannerInfo*>(InDeviceInfo);
		DeviceScannerInfo->Release();
	}

	VIDEOIO_API bool VideoIODeviceInfoGetDeviceId(FDeviceInfo InDeviceInfo, TCHAR* OutDeviceId, FUInt InSize)
	{
		PrivateDeviceScannerInfo* DeviceScannerInfo = reinterpret_cast<PrivateDeviceScannerInfo*>(InDeviceInfo);
		
		BSTR DeviceName = 0;	
		ComCheck(DeviceScannerInfo->GetDevice()->GetDisplayName(&DeviceName));
		wcscpy_s(OutDeviceId, InSize, DeviceName);
		::SysFreeString(DeviceName);

		return false;
	}

	VIDEOIO_API FUInt VideoIODeviceInfoGetVidInputs(FDeviceInfo InDeviceInfo)
	{
		return 1;
	}

	VIDEOIO_API FUInt VideoIODeviceInfoGetVidOutputs(FDeviceInfo InDeviceInfo)
	{
		return 1;
	}

	/*
	 * Device Handling
	 */

	VIDEOIO_API FDevice VideoIOCreateDevice(FUInt InDeviceId)
	{
		return PrivateDeviceCache::GetCache().AquireDevice(InDeviceId);
	}

	VIDEOIO_API void VideoIOReleaseDevice(FDevice InDevice)
	{
		PrivateDevice* Device = reinterpret_cast<PrivateDevice*>(InDevice);
		Device->Release();
	}

	VIDEOIO_API bool VideoIODeviceIsDeviceReady(FDevice InDevice)
	{
		return true;
	}

	VIDEOIO_API bool VideoIODeviceCanDoCapture(FDevice InDevice)
	{
		return true;
	}

};