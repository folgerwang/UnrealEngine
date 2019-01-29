// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidDeviceDetectionModule.cpp: Implements the FAndroidDeviceDetectionModule class.
=============================================================================*/

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "Containers/Map.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformProcess.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "ITcpMessagingModule.h"

#include "PIEPreviewDeviceSpecification.h"
#include "JsonObjectConverter.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FAndroidDeviceDetectionModule" 

DEFINE_LOG_CATEGORY_STATIC(AndroidDeviceDetectionLog, Log, All);

class FAndroidDeviceDetectionRunnable : public FRunnable
{
public:
	FAndroidDeviceDetectionRunnable(TMap<FString,FAndroidDeviceInfo>& InDeviceMap, FCriticalSection* InDeviceMapLock, FCriticalSection* InADBPathCheckLock) :
		StopTaskCounter(0),
		DeviceMap(InDeviceMap),
		DeviceMapLock(InDeviceMapLock),
		ADBPathCheckLock(InADBPathCheckLock),
		HasADBPath(false),
		ForceCheck(false)
	{
		TcpMessagingModule = FModuleManager::LoadModulePtr<ITcpMessagingModule>("TcpMessaging");
	}

public:

	// FRunnable interface.
	virtual bool Init(void) 
	{ 
		return true; 
	}

	virtual void Exit(void) 
	{
	}

	virtual void Stop(void)
	{
		StopTaskCounter.Increment();
	}

	virtual uint32 Run(void)
	{
		int LoopCount = 10;

		while (StopTaskCounter.GetValue() == 0)
		{
			// query every 10 seconds
			if (LoopCount++ >= 10 || ForceCheck)
			{
				// Make sure we have an ADB path before checking
				FScopeLock PathLock(ADBPathCheckLock);
				if (HasADBPath)
					QueryConnectedDevices();

				LoopCount = 0;
				ForceCheck = false;
			}

			FPlatformProcess::Sleep(1.0f);
		}

		return 0;
	}

	void UpdateADBPath(FString &InADBPath, FString& InGetPropCommand, bool InbGetExtensionsViaSurfaceFlinger, bool InbForLumin)
	{
		ADBPath = InADBPath;
		GetPropCommand = InGetPropCommand;
		bGetExtensionsViaSurfaceFlinger = InbGetExtensionsViaSurfaceFlinger;
		bForLumin = InbForLumin;

		HasADBPath = !ADBPath.IsEmpty();
		// Force a check next time we go around otherwise it can take over 10sec to find devices
		ForceCheck = HasADBPath;	

		// If we have no path then clean the existing devices out
		if (!HasADBPath && DeviceMap.Num() > 0)
		{
			DeviceMap.Reset();
		}
	}

private:

	bool ExecuteAdbCommand( const FString& CommandLine, FString* OutStdOut, FString* OutStdErr ) const
	{
		// execute the command
		int32 ReturnCode;
		FString DefaultError;

		// make sure there's a place for error output to go if the caller specified nullptr
		if (!OutStdErr)
		{
			OutStdErr = &DefaultError;
		}

		FPlatformProcess::ExecProcess(*ADBPath, *CommandLine, &ReturnCode, OutStdOut, OutStdErr);

		if (ReturnCode != 0)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The Android SDK command '%s' failed to run. Return code: %d, Error: %s\n"), *CommandLine, ReturnCode, **OutStdErr);

			return false;
		}

		return true;
	}

	// searches for 'DPIString' and 
	int32 ExtractDPI(const FString& SurfaceFlingerOutput, const FString& DPIString)
	{
		int32 FoundDpi = INDEX_NONE;

		int32 DpiIndex = SurfaceFlingerOutput.Find(DPIString);
		if (DpiIndex != INDEX_NONE)
		{
			int32 StartIndex = INDEX_NONE;
			for (int32 i = DpiIndex; i < SurfaceFlingerOutput.Len(); ++i)
			{
				// if we somehow hit a line break character something went wrong and no digits were found on this line
				// we don't want to search the SurfaceFlinger feed so exit now
				if (FChar::IsLinebreak(SurfaceFlingerOutput[i]))
				{
					break;
				}

				// search for the first digit aka the beginning of the DPI value
				if (StartIndex == INDEX_NONE && FChar::IsDigit(SurfaceFlingerOutput[i]))
				{
					StartIndex = i;
				}
				// if we hit some non-numeric character extract the number and exit
				else if (StartIndex != INDEX_NONE && !FChar::IsDigit(SurfaceFlingerOutput[i]))
				{
					FString str = SurfaceFlingerOutput.Mid(StartIndex, i - StartIndex);
					FoundDpi = FCString::Atoi(*str);
					break;
				}
			}
		}

		return FoundDpi;
	}
	// retrieve the string between 'InOutStartIndex' and the start position of the next 'Token' substring
	// the white spaces of the resulting string are trimmed out at both ends 
	FString ExtractNextToken(int32& InOutStartIndex, const FString& SurfaceFlingerOutput, const FString& Token)
	{
		FString OutString;
		int32 StartIndex = InOutStartIndex;

		int32 EndIndex = SurfaceFlingerOutput.Find(Token, ESearchCase::IgnoreCase, ESearchDir::FromStart, StartIndex);

		if (EndIndex != INDEX_NONE)
		{
			InOutStartIndex = EndIndex + 1;
			// the index should point to the position before the token start
			--EndIndex;

			for (int32 i = StartIndex; i < EndIndex; ++i)
			{
				if (!FChar::IsWhitespace(SurfaceFlingerOutput[i]))
				{
					StartIndex = i;
					break;
				}
			}

			for (int32 i = EndIndex; i > StartIndex; --i)
			{
				if (!FChar::IsWhitespace(SurfaceFlingerOutput[i]))
				{
					EndIndex = i;
					break;
				}
			}

			OutString = SurfaceFlingerOutput.Mid(StartIndex, FMath::Max(0, EndIndex - StartIndex + 1));
		}

		return OutString;
	}

	void ExtractGPUInfo(FString& outGLVersion, FString& outGPUFamily, const FString& SurfaceFlingerOutput)
	{
		int32 FoundDpi = INDEX_NONE;

		int32 LineIndex = SurfaceFlingerOutput.Find(TEXT("GLES:"));
		if (LineIndex != INDEX_NONE)
		{
			int32 StartIndex = SurfaceFlingerOutput.Find(TEXT(":"), ESearchCase::IgnoreCase, ESearchDir::FromStart, LineIndex);
			if (StartIndex != INDEX_NONE)
			{
				++StartIndex;

				FString GPUVendorString = ExtractNextToken(StartIndex, SurfaceFlingerOutput, TEXT(","));
				outGPUFamily = ExtractNextToken(StartIndex, SurfaceFlingerOutput, TEXT(","));
				outGLVersion = ExtractNextToken(StartIndex, SurfaceFlingerOutput, TEXT("\n"));
			}
		}
	}

	void QueryConnectedDevices()
	{
		// grab the list of devices via adb
		FString StdOut;
		if (!ExecuteAdbCommand(TEXT("devices -l"), &StdOut, nullptr))
		{
			return;
		}

		// separate out each line
		TArray<FString> DeviceStrings;
		StdOut = StdOut.Replace(TEXT("\r"), TEXT("\n"));
		StdOut.ParseIntoArray(DeviceStrings, TEXT("\n"), true);

		// list of any existing port forwardings, filled in when we find a device we need to add.
		TArray<FString> PortForwardings;

		// a list containing all devices found this time, so we can remove anything not in this list
		TArray<FString> CurrentlyConnectedDevices;

		for (int32 StringIndex = 0; StringIndex < DeviceStrings.Num(); ++StringIndex)
		{
			const FString& DeviceString = DeviceStrings[StringIndex];

			// skip over non-device lines
			if (DeviceString.StartsWith("* ") || DeviceString.StartsWith("List "))
			{
				continue;
			}

			// grab the device serial number
			int32 TabIndex;

			// use either tab or space as separator
			if (!DeviceString.FindChar(TCHAR('\t'), TabIndex))
			{
				if (!DeviceString.FindChar(TCHAR(' '), TabIndex))
				{
					continue;
				}
			}

			FAndroidDeviceInfo NewDeviceInfo;

			NewDeviceInfo.SerialNumber = DeviceString.Left(TabIndex);
			const FString DeviceState = DeviceString.Mid(TabIndex + 1).TrimStart();

			NewDeviceInfo.bAuthorizedDevice = DeviceState != TEXT("unauthorized");
			if (bForLumin)
			{
				// 'mldb oobestaus' is deprecated. 'mldb ps' gives us similar functionality for checking device readiness to some extent.
				const FString OobeCommand = FString::Printf(TEXT("-s %s ps"), *NewDeviceInfo.SerialNumber);
				FString OobeStatus;
				NewDeviceInfo.bAuthorizedDevice = ExecuteAdbCommand(*OobeCommand, &OobeStatus, nullptr);
				if (DeviceMap.Contains(NewDeviceInfo.SerialNumber))
				{
					if (DeviceMap[NewDeviceInfo.SerialNumber].bAuthorizedDevice != NewDeviceInfo.bAuthorizedDevice)
					{
						// if this device is already in the connected list but authorization has changed, remove it.
						// it will be added in the next query which will allow UI to refresh properly.
						continue;
					}
				}
			}

			// add it to our list of currently connected devices
			CurrentlyConnectedDevices.Add(NewDeviceInfo.SerialNumber);

			// move on to next device if this one is already a known device that has either already been authorized or the authorization
			// status has not changed
			if (DeviceMap.Contains(NewDeviceInfo.SerialNumber) && 
				(DeviceMap[NewDeviceInfo.SerialNumber].bAuthorizedDevice == NewDeviceInfo.bAuthorizedDevice))
			{
				continue;
			}

			if (!NewDeviceInfo.bAuthorizedDevice && !bForLumin)
			{
				//note: AndroidTargetDevice::GetName() does not fetch this value, do not rely on this
				NewDeviceInfo.DeviceName = TEXT("Unauthorized - enable USB debugging");
			}
			else
			{
				// grab the Lumin/Android version
				const FString AndroidVersionCommand = bForLumin ? FString::Printf(TEXT("%s ro.build.id"), *GetPropCommand) :
					FString::Printf(TEXT("-s %s %s ro.build.version.release"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				if (!ExecuteAdbCommand(*AndroidVersionCommand, &NewDeviceInfo.HumanAndroidVersion, nullptr))
				{
					continue;
				}
				NewDeviceInfo.HumanAndroidVersion = NewDeviceInfo.HumanAndroidVersion.Replace(TEXT("\r"), TEXT("")).Replace(TEXT("\n"), TEXT(""));
				NewDeviceInfo.HumanAndroidVersion.TrimStartAndEndInline();

				// grab the Android SDK version
				const FString SDKVersionCommand = FString::Printf(TEXT("-s %s %s ro.build.version.sdk"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				FString SDKVersionString;
				if (!ExecuteAdbCommand(*SDKVersionCommand, &SDKVersionString, nullptr))
				{
					continue;
				}
				NewDeviceInfo.SDKVersion = FCString::Atoi(*SDKVersionString);
				if (NewDeviceInfo.SDKVersion <= 0)
				{
					NewDeviceInfo.SDKVersion = INDEX_NONE;
				}

				if (bGetExtensionsViaSurfaceFlinger)
				{
					// get the GL extensions string (and a bunch of other stuff)
					const FString ExtensionsCommand = FString::Printf(TEXT("-s %s shell dumpsys SurfaceFlinger"), *NewDeviceInfo.SerialNumber);
					if (!ExecuteAdbCommand(*ExtensionsCommand, &NewDeviceInfo.GLESExtensions, nullptr))
					{
						continue;
					}

					// extract DPI information
					int32 XDpi = ExtractDPI(NewDeviceInfo.GLESExtensions, TEXT("x-dpi"));
					int32 YDpi = ExtractDPI(NewDeviceInfo.GLESExtensions, TEXT("y-dpi"));

					if (XDpi != INDEX_NONE && YDpi != INDEX_NONE)
					{
						NewDeviceInfo.DeviceDPI = (XDpi + YDpi) / 2;
					}

					// extract OpenGL version and GPU family name
					ExtractGPUInfo(NewDeviceInfo.OpenGLVersionString, NewDeviceInfo.GPUFamilyString, NewDeviceInfo.GLESExtensions);
				}

				// grab device brand
				{
					FString ExecCommand = FString::Printf(TEXT("-s %s %s ro.product.brand"), *NewDeviceInfo.SerialNumber, *GetPropCommand);

					FString RoProductBrand;
					ExecuteAdbCommand(*ExecCommand, &RoProductBrand, nullptr);
					const TCHAR* Ptr = *RoProductBrand;
					FParse::Line(&Ptr, NewDeviceInfo.DeviceBrand);
				}

				// grab screen resolution
				{
					FString ResolutionString;
					const FString ExecCommand = FString::Printf(TEXT("-s %s shell wm size"), *NewDeviceInfo.SerialNumber);
					if (ExecuteAdbCommand(*ExecCommand, &ResolutionString, nullptr))
					{
						bool bFoundResX = false;
						int32 StartIndex = INDEX_NONE;
						for (int32 Index = 0; Index < ResolutionString.Len(); ++Index)
						{
							if (StartIndex == INDEX_NONE && FChar::IsDigit(ResolutionString[Index]))
							{
								StartIndex = Index;
							}
							else if (StartIndex != INDEX_NONE && !FChar::IsDigit(ResolutionString[Index]))
							{
								FString str = ResolutionString.Mid(StartIndex, Index - StartIndex);

								if (bFoundResX)
								{
									NewDeviceInfo.ResolutionY = FCString::Atoi(*str);
									break;
								}
								else
								{
									NewDeviceInfo.ResolutionX = FCString::Atoi(*str);
									bFoundResX = true;
									StartIndex = INDEX_NONE;
								}
							}
						}
					}
				}

				// grab the GL ES version
				FString GLESVersionString;
				const FString GLVersionCommand = FString::Printf(TEXT("-s %s %s ro.opengles.version"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
				if (!ExecuteAdbCommand(*GLVersionCommand, &GLESVersionString, nullptr))
				{
					continue;
				}
				NewDeviceInfo.GLESVersion = FCString::Atoi(*GLESVersionString);

				// parse the device model
				FParse::Value(*DeviceString, TEXT("model:"), NewDeviceInfo.Model);
				if (NewDeviceInfo.Model.IsEmpty())
				{
					FString ModelCommand = FString::Printf(TEXT("-s %s %s ro.product.model"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
					FString RoProductModel;
					ExecuteAdbCommand(*ModelCommand, &RoProductModel, nullptr);
					const TCHAR* Ptr = *RoProductModel;
					FParse::Line(&Ptr, NewDeviceInfo.Model);
				}

				// parse the device name
				FParse::Value(*DeviceString, TEXT("device:"), NewDeviceInfo.DeviceName);
				if (NewDeviceInfo.DeviceName.IsEmpty())
				{
					FString DeviceCommand = FString::Printf(TEXT("-s %s %s ro.product.device"), *NewDeviceInfo.SerialNumber, *GetPropCommand);
					FString RoProductDevice;
					ExecuteAdbCommand(*DeviceCommand, &RoProductDevice, nullptr);
					const TCHAR* Ptr = *RoProductDevice;
					FParse::Line(&Ptr, NewDeviceInfo.DeviceName);
				}

				// establish port forwarding if we're doing messaging
				if (TcpMessagingModule != nullptr)
				{
					// fill in the port forwarding array if needed
					if (PortForwardings.Num() == 0)
					{
						FString ForwardList;
						if (ExecuteAdbCommand(TEXT("forward --list"), &ForwardList, nullptr))
						{
							ForwardList = ForwardList.Replace(TEXT("\r"), TEXT("\n"));
							ForwardList.ParseIntoArray(PortForwardings, TEXT("\n"), true);
						}
					}

					// check if this device already has port forwarding enabled for message bus, eg from another editor session
					for (FString& FwdString : PortForwardings)
					{
						const TCHAR* Ptr = *FwdString;
						FString FwdSerialNumber, FwdHostPortString, FwdDevicePortString;
						uint16 FwdHostPort, FwdDevicePort;
						if (FParse::Token(Ptr, FwdSerialNumber, false) && FwdSerialNumber == NewDeviceInfo.SerialNumber &&
							FParse::Token(Ptr, FwdHostPortString, false) && FParse::Value(*FwdHostPortString, TEXT("tcp:"), FwdHostPort) &&
							FParse::Token(Ptr, FwdDevicePortString, false) && FParse::Value(*FwdDevicePortString, TEXT("tcp:"), FwdDevicePort) && FwdDevicePort == 6666)
						{
							NewDeviceInfo.HostMessageBusPort = FwdHostPort;
							break;
						}
					}

					// if not, setup TCP port forwarding for message bus on first available TCP port above 6666
					if (NewDeviceInfo.HostMessageBusPort == 0)
					{
						uint16 HostMessageBusPort = 6666;
						bool bFoundPort;
						do
						{
							bFoundPort = true;
							for (auto It = DeviceMap.CreateConstIterator(); It; ++It)
							{
								if (HostMessageBusPort == It.Value().HostMessageBusPort)
								{
									bFoundPort = false;
									HostMessageBusPort++;
									break;
								}
							}
						} while (!bFoundPort);

						FString DeviceCommand = FString::Printf(TEXT("-s %s forward tcp:%d tcp:6666"), *NewDeviceInfo.SerialNumber, HostMessageBusPort);
						ExecuteAdbCommand(*DeviceCommand, nullptr, nullptr);
						NewDeviceInfo.HostMessageBusPort = HostMessageBusPort;
					}

					TcpMessagingModule->AddOutgoingConnection(FString::Printf(TEXT("127.0.0.1:%d"), NewDeviceInfo.HostMessageBusPort));
				}
			}

			// add the device to the map
			{
				FScopeLock ScopeLock(DeviceMapLock);

				FAndroidDeviceInfo& SavedDeviceInfo = DeviceMap.Add(NewDeviceInfo.SerialNumber);
				SavedDeviceInfo = NewDeviceInfo;
			}
		}

		// loop through the previously connected devices list and remove any that aren't still connected from the updated DeviceMap
		TArray<FString> DevicesToRemove;

		for (auto It = DeviceMap.CreateConstIterator(); It; ++It)
		{
			if (!CurrentlyConnectedDevices.Contains(It.Key()))
			{
				if (TcpMessagingModule && It.Value().HostMessageBusPort != 0)
				{
					TcpMessagingModule->RemoveOutgoingConnection(FString::Printf(TEXT("127.0.0.1:%d"), It.Value().HostMessageBusPort));
				}

				DevicesToRemove.Add(It.Key());
			}
		}

		{
			// enter the critical section and remove the devices from the map
			FScopeLock ScopeLock(DeviceMapLock);

			for (auto It = DevicesToRemove.CreateConstIterator(); It; ++It)
			{
				DeviceMap.Remove(*It);
			}
		}
	}

private:

	// path to the adb command
	FString ADBPath;
	FString GetPropCommand;
	bool bGetExtensionsViaSurfaceFlinger;
	bool bForLumin;

	// > 0 if we've been asked to abort work in progress at the next opportunity
	FThreadSafeCounter StopTaskCounter;

	TMap<FString,FAndroidDeviceInfo>& DeviceMap;
	FCriticalSection* DeviceMapLock;

	FCriticalSection* ADBPathCheckLock;
	bool HasADBPath;
	bool ForceCheck;

	ITcpMessagingModule* TcpMessagingModule;
};

class FAndroidDeviceDetection : public IAndroidDeviceDetection
{
public:

	FAndroidDeviceDetection() 
		: DetectionThread(nullptr)
		, DetectionThreadRunnable(nullptr)
	{
		// create and fire off our device detection thread
		DetectionThreadRunnable = new FAndroidDeviceDetectionRunnable(DeviceMap, &DeviceMapLock, &ADBPathCheckLock);
		DetectionThread = FRunnableThread::Create(DetectionThreadRunnable, TEXT("FAndroidDeviceDetectionRunnable"));
	}

	virtual ~FAndroidDeviceDetection()
	{
		if (DetectionThreadRunnable && DetectionThread)
		{
			DetectionThreadRunnable->Stop();
			DetectionThread->WaitForCompletion();
		}
	}

	virtual void Initialize(const TCHAR* InSDKDirectoryEnvVar, const TCHAR* InSDKRelativeExePath, const TCHAR* InGetPropCommand, bool InbGetExtensionsViaSurfaceFlinger, bool InbForLumin = false) override
	{
		SDKDirEnvVar = InSDKDirectoryEnvVar;
		SDKRelativeExePath = InSDKRelativeExePath;
		GetPropCommand = InGetPropCommand;
		bGetExtensionsViaSurfaceFlinger = InbGetExtensionsViaSurfaceFlinger;
		bForLumin = InbForLumin;
		UpdateADBPath();
	}

	virtual const TMap<FString,FAndroidDeviceInfo>& GetDeviceMap() override
	{
		return DeviceMap;
	}

	virtual FCriticalSection* GetDeviceMapLock() override
	{
		return &DeviceMapLock;
	}

	virtual FString GetADBPath() override
	{
		FScopeLock PathUpdateLock(&ADBPathCheckLock);
		return ADBPath;
	}

	virtual void UpdateADBPath() override
	{
		FScopeLock PathUpdateLock(&ADBPathCheckLock);
		FString AndroidDirectory = FPlatformMisc::GetEnvironmentVariable(*SDKDirEnvVar);

		ADBPath.Empty();
		
#if PLATFORM_MAC || PLATFORM_LINUX
		if (AndroidDirectory.Len() == 0)
		{
#if PLATFORM_LINUX
			// didn't find ANDROID_HOME, so parse the .bashrc file on Linux
			FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString("~/.bashrc"));
#else
			// didn't find ANDROID_HOME, so parse the .bash_profile file on MAC
			FArchive* FileReader = IFileManager::Get().CreateFileReader(*FString([@"~/.bash_profile" stringByExpandingTildeInPath]));
#endif
			if (FileReader)
			{
				const int64 FileSize = FileReader->TotalSize();
				ANSICHAR* AnsiContents = (ANSICHAR*)FMemory::Malloc(FileSize + 1);
				FileReader->Serialize(AnsiContents, FileSize);
				FileReader->Close();
				delete FileReader;

				AnsiContents[FileSize] = 0;
				TArray<FString> Lines;
				FString(ANSI_TO_TCHAR(AnsiContents)).ParseIntoArrayLines(Lines);
				FMemory::Free(AnsiContents);

				for (int32 Index = Lines.Num()-1; Index >=0; Index--)
				{
					if (AndroidDirectory.Len() == 0 && Lines[Index].StartsWith(FString::Printf(TEXT("export %s="), *SDKDirEnvVar)))
					{
						FString Directory;
						Lines[Index].Split(TEXT("="), NULL, &Directory);
						Directory = Directory.Replace(TEXT("\""), TEXT(""));
						AndroidDirectory = Directory;
						setenv(TCHAR_TO_ANSI(*SDKDirEnvVar), TCHAR_TO_ANSI(*AndroidDirectory), 1);
					}
				}
			}
		}
#endif

		if (AndroidDirectory.Len() > 0)
		{
			ADBPath = FPaths::Combine(*AndroidDirectory, SDKRelativeExePath);

			// if it doesn't exist then just clear the path as we might set it later
			if (!FPaths::FileExists(*ADBPath))
			{
				ADBPath.Empty();
			}
		}
		DetectionThreadRunnable->UpdateADBPath(ADBPath, GetPropCommand, bGetExtensionsViaSurfaceFlinger, bForLumin);
	}

	virtual void ExportDeviceProfile(const FString& OutPath, const FString& DeviceName) override
	{
		// instantiate an FPIEPreviewDeviceSpecifications instance and its values
		FPIEPreviewDeviceSpecifications DeviceSpecs;

		bool bOpenGL3x = false;
		{
			FScopeLock ExportLock(GetDeviceMapLock());

			const FAndroidDeviceInfo* DeviceInfo = GetDeviceMap().Find(DeviceName);
			if (DeviceInfo == nullptr)
			{
				FText TitleMessage = LOCTEXT("loc_ExportError_Title", "File export error.");
				FMessageDialog::Open(EAppMsgType::Ok, EAppReturnType::Ok, LOCTEXT("loc_ExportError_Message", "Device disconnected!"), &TitleMessage);
				return;
			}

			// generic values
			DeviceSpecs.DevicePlatform = EPIEPreviewDeviceType::Android;
			DeviceSpecs.ResolutionX = DeviceInfo->ResolutionX;
			DeviceSpecs.ResolutionY = DeviceInfo->ResolutionY;
			DeviceSpecs.ResolutionYImmersiveMode = 0;
			DeviceSpecs.PPI = DeviceInfo->DeviceDPI;
			DeviceSpecs.ScaleFactors = { 0.25f, 0.5f, 0.75f, 1.0f };

			// Android specific values
			DeviceSpecs.AndroidProperties.AndroidVersion = DeviceInfo->HumanAndroidVersion;
			DeviceSpecs.AndroidProperties.DeviceModel = DeviceInfo->Model;
			DeviceSpecs.AndroidProperties.DeviceMake = DeviceInfo->DeviceBrand;
			DeviceSpecs.AndroidProperties.GLVersion = DeviceInfo->OpenGLVersionString;
			DeviceSpecs.AndroidProperties.GPUFamily = DeviceInfo->GPUFamilyString;
			DeviceSpecs.AndroidProperties.VulkanVersion = "0.0.0";
			DeviceSpecs.AndroidProperties.UsingHoudini = false;
			DeviceSpecs.AndroidProperties.VulkanAvailable = false;

			// OpenGL ES 3.x
			bOpenGL3x = DeviceInfo->OpenGLVersionString.Contains(TEXT("OpenGL ES 3"));
			if (bOpenGL3x)
			{
				DeviceSpecs.AndroidProperties.GLES31RHIState.SupportsInstancing = true;
				DeviceSpecs.AndroidProperties.GLES31RHIState.MaxTextureDimensions = 4096;
				DeviceSpecs.AndroidProperties.GLES31RHIState.MaxShadowDepthBufferSizeX = 2048;
				DeviceSpecs.AndroidProperties.GLES31RHIState.MaxShadowDepthBufferSizeY = 2048;
				DeviceSpecs.AndroidProperties.GLES31RHIState.MaxCubeTextureDimensions = 2048;
				DeviceSpecs.AndroidProperties.GLES31RHIState.SupportsRenderTargetFormat_PF_G8 = true;
				DeviceSpecs.AndroidProperties.GLES31RHIState.SupportsRenderTargetFormat_PF_FloatRGBA = DeviceInfo->GLESExtensions.Contains(TEXT("GL_EXT_color_buffer_half_float"));
				DeviceSpecs.AndroidProperties.GLES31RHIState.SupportsMultipleRenderTargets = true;
			}

			// OpenGL ES 2.0
			{
				DeviceSpecs.AndroidProperties.GLES2RHIState.SupportsInstancing = false;
				DeviceSpecs.AndroidProperties.GLES2RHIState.MaxTextureDimensions = 2048;
				DeviceSpecs.AndroidProperties.GLES2RHIState.MaxShadowDepthBufferSizeX = 1024;
				DeviceSpecs.AndroidProperties.GLES2RHIState.MaxShadowDepthBufferSizeY = 1024;
				DeviceSpecs.AndroidProperties.GLES2RHIState.MaxCubeTextureDimensions = 512;
				DeviceSpecs.AndroidProperties.GLES2RHIState.SupportsRenderTargetFormat_PF_G8 = true;
				DeviceSpecs.AndroidProperties.GLES2RHIState.SupportsRenderTargetFormat_PF_FloatRGBA = DeviceInfo->GLESExtensions.Contains(TEXT("GL_EXT_color_buffer_half_float"));
				DeviceSpecs.AndroidProperties.GLES2RHIState.SupportsMultipleRenderTargets = false;
			}
		} // FScopeLock ExportLock released

		// create a JSon object from the above structure
		TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject<FPIEPreviewDeviceSpecifications>(DeviceSpecs);

		// if device does not support OpenGL 3.x avoid exporting anything about it
		if (!bOpenGL3x)
		{
			JsonObject->RemoveField("GLES31RHIState");
		}

		// remove IOS fields
		JsonObject->RemoveField("IOSProperties");

		// serialize the JSon object to string
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

		// export file to disk
		FFileHelper::SaveStringToFile(OutputString, *OutPath);
	} // end of virtual void ExportDeviceProfile(...)

private:

	// path to the adb command (local)
	FString ADBPath;

	FString SDKDirEnvVar;
	FString SDKRelativeExePath;
	FString GetPropCommand;
	bool bGetExtensionsViaSurfaceFlinger;
	bool bForLumin;

	FRunnableThread* DetectionThread;
	FAndroidDeviceDetectionRunnable* DetectionThreadRunnable;

	TMap<FString,FAndroidDeviceInfo> DeviceMap;
	FCriticalSection DeviceMapLock;
	FCriticalSection ADBPathCheckLock;
};


/**
 * Holds the target platform singleton.
 */
static TMap<FString, FAndroidDeviceDetection*> AndroidDeviceDetectionSingletons;


/**
 * Module for detecting android devices.
 */
class FAndroidDeviceDetectionModule : public IAndroidDeviceDetectionModule
{
public:

	/**
	 * Destructor.
	 */
	~FAndroidDeviceDetectionModule( )
	{
		for (auto It : AndroidDeviceDetectionSingletons)
		{
			delete It.Value;
		}
		AndroidDeviceDetectionSingletons.Empty();
	}

	virtual IAndroidDeviceDetection* GetAndroidDeviceDetection(const TCHAR* OverridePlatformName) override
	{
		FString Key(OverridePlatformName);
		FAndroidDeviceDetection* Value = AndroidDeviceDetectionSingletons.FindRef(Key);
		if (Value == nullptr)
		{
			Value = AndroidDeviceDetectionSingletons.Add(Key, new FAndroidDeviceDetection());
		}
		return Value;
	}
};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE( FAndroidDeviceDetectionModule, AndroidDeviceDetection);
