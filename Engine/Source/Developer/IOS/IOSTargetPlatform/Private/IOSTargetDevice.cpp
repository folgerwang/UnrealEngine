// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "IOSTargetDevice.h"

#include "HAL/PlatformProcess.h"
#include "IOSMessageProtocol.h"
#include "Interfaces/ITargetPlatform.h"
#include "Async/Async.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "IOSTargetDeviceOutput.h"
#if PLATFORM_WINDOWS
// for windows mutex
#include "Windows/AllowWindowsPlatformTypes.h"
#endif // #if PLATFORM_WINDOWS

enum
{
    DEFAULT_DS_COMMANDER_PORT = 41000, // default port to use when issuing DeploymentServer commands
};

FTcpDSCommander::FTcpDSCommander(const uint8* Data, int32 Count, void* WPipe)
: bStopping(false)
, bStoped(true)
, bIsSuccess(false)
, bIsSystemError(false)
, DSSocket(nullptr)
, Thread(nullptr)
, WritePipe(WPipe)
, DSCommand(nullptr)
, DSCommandLen(Count + 1)
, LastActivity(0.0)
{
    if (Count > 0)
    {
        DSCommand = (uint8*)FMemory::Malloc(Count + 1);
        FPlatformMemory::Memcpy(DSCommand, Data, Count);
        DSCommand[Count] = '\n';
        Thread = FRunnableThread::Create(this, TEXT("FTcpDSCommander"), 128 * 1024, TPri_Normal);
    }
}

/** Virtual destructor. */
FTcpDSCommander::~FTcpDSCommander()
{
    if (Thread != nullptr)
    {
        Thread->Kill(true);
        delete Thread;
    }
    if (DSCommand)
    {
        FMemory::Free(DSCommand);
    }
}

void FTcpDSCommander::Exit()
{
    // do nothing
    bStoped = true;
}

bool FTcpDSCommander::Init()
{
	if (DSCommandLen < 1)
	{
		bIsSuccess = true;
		return true;
	}
	ISocketSubsystem* SSS = ISocketSubsystem::Get();
	DSSocket = SSS->CreateSocket(NAME_Stream, TEXT("DSCommander tcp"));
	if (DSSocket == nullptr)
	{
		return false;
	}
	TSharedRef<FInternetAddr> Addr = SSS->CreateInternetAddr(0, DEFAULT_DS_COMMANDER_PORT);
	bool bIsValid;
	Addr->SetIp(TEXT("127.0.0.1"), bIsValid);

#if PLATFORM_WINDOWS
	// using the mutex to detect if the DeploymentServer is running
	// only on windows
	if (!IsDSRunning())
	{
		StartDSProcess();

		int TimeoutSeconds = 5;
		while (!IsDSRunning() && TimeoutSeconds > 0)
		{
			TimeoutSeconds--;
			FPlatformProcess::Sleep(1.0f);
		}
		if (!IsDSRunning())
		{
			bIsSystemError = true;
			return false;
		}
	}
	if (DSSocket->Connect(*Addr) == false)
	{
		{ // extra bracked for cleaner ifdefs
#else
	// try to connect to the server
	// on mac we use the old way to try a TCP connection
	if (DSSocket->Connect(*Addr) == false)
	{
		StartDSProcess();
		if (DSSocket->Connect(*Addr) == false)
		{
#endif // PLATFORM_WINDOWS
        	// on failure, shut it all down
        	SSS->DestroySocket(DSSocket);
        	DSSocket = nullptr;
			ESocketErrors LastError = SSS->GetLastErrorCode();
			const TCHAR* SocketErr = SSS->GetSocketError(LastError);
        	//UE_LOG(LogTemp, Display, TEXT("Failed to connect to deployment server at %s (%s)."), *Addr->ToString(true), SocketErr);
        	return false;
		}
    }
    LastActivity = FPlatformTime::Seconds();
    
    return true;
}

uint32 FTcpDSCommander::Run()
{
    if (!DSSocket)
    {
        //UE_LOG(LogTemp, Log, TEXT("Socket not created."));
        return 1;
    }
    int32 NSent = 0;
    bool BSent = DSSocket->Send(DSCommand, DSCommandLen, NSent);
    if (NSent != DSCommandLen || !BSent)
    {
        Stop();
        //UE_LOG(LogTemp, Log, TEXT("Socket send error."));
        return 1;
    }
    bStoped = false;
    
    static const SIZE_T CommandSize = 1024;
    uint8 RecvBuffer[CommandSize];
    
    while (!bStopping)
    {
        uint32 Pending = 0;
        if (DSSocket->GetConnectionState() != ESocketConnectionState::SCS_Connected)
        {
            Stop();
            //UE_LOG(LogTemp, Log, TEXT("Socket connection error."));
            return 1;
        }
        if (DSSocket->HasPendingData(Pending))
        {
            NSent = 0;
            FMemory::Memset(RecvBuffer, 0, CommandSize);
            LastActivity = FPlatformTime::Seconds();
            if (DSSocket->Recv(RecvBuffer, CommandSize, NSent))
            {
                FString Result = UTF8_TO_TCHAR(RecvBuffer);
                TArray<FString> TagArray;
                Result.ParseIntoArray(TagArray, TEXT("\n"), false);
                int i;
                for (i = 0; i < TagArray.Num() - 1; i++)
                {
                    if (TagArray[i].EndsWith(TEXT("CMDOK\r")))
                    {
                        bIsSuccess = true;
                        //UE_LOG(LogTemp, Log, TEXT("Socket command completed."));
                        Stop();
                        return 0;
                    }
                    else if (TagArray[i].StartsWith(TEXT("[DSDIR]")))
                    {
                        // just ignore the folder check
                    }
                    else if (TagArray[i].EndsWith(TEXT("CMDFAIL\r")))
                    {
                        Stop();
                        //UE_LOG(LogTemp, Display, TEXT("Socket command failed."));
                        return 1;
                    }
                    else
                    {
                        FPlatformProcess::WritePipe(WritePipe, *TagArray[i]);
                    }
                }
            }
        }
        double CurrentTime = FPlatformTime::Seconds();
        if (CurrentTime - LastActivity > 120.0)
        {
			//UE_LOG(LogTemp, Display, TEXT("Socket command timeouted."));
            Stop();
            return 0;
        }
        FPlatformProcess::Sleep(0.01f);
    }
    
    return 0;
}

void FTcpDSCommander::Stop()
{
    if (DSSocket)
    {
		DSSocket->Shutdown(ESocketShutdownMode::ReadWrite);
        DSSocket->Close();
        ISocketSubsystem::Get()->DestroySocket(DSSocket);
    }
    DSSocket = NULL;
    
    bStopping = true;
}

bool FTcpDSCommander::IsDSRunning()
{
	// is there a mutex we can use to connect test DS is running, also available on mac?
	// there is also a failsafe mechanism for this, since the DeploymentServer will not start a new server if one is already running
#if PLATFORM_WINDOWS
	HANDLE mutex = CreateMutexA(NULL, TRUE, "Global\\DeploymentServer_Mutex_SERVERINSTANCE");
	if (mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// deployment server instance already runnning
		if (mutex)
		{
			CloseHandle(mutex);
		}
		return true;
	}
	CloseHandle(mutex);
#endif // PLATFORM_WINDOWS
	return false;
}

bool FTcpDSCommander::StartDSProcess()
{   
    FString DSFilename = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/IOS/DeploymentServerLauncher.exe"));
    FString WorkingFoder = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/DotNET/IOS/"));
    FString Params = "";
    
#if PLATFORM_MAC
    // On Mac we launch UBT with Mono
    FString ScriptPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Build/BatchFiles/Mac/RunMono.sh"));
    Params = FString::Printf(TEXT("\"%s\" \"%s\" %s"), *ScriptPath, *DSFilename, *Params);
    DSFilename = TEXT("/bin/sh");
#endif
    //UE_LOG(LogTemp, Log, TEXT("DeploymentServer not running, Starting it!"));
    FPlatformProcess::CreateProc(*DSFilename, *Params, true, true, true, NULL, 0, *WorkingFoder, (void*)nullptr);
    FPlatformProcess::Sleep(1.0f);
    
    return true;
}


//**********************************************************************************************************************************************************
//*
FIOSTargetDevice::FIOSTargetDevice(const ITargetPlatform& InTargetPlatform)
	: TargetPlatform(InTargetPlatform)
	, DeviceEndpoint()
	, AppId()
	, bCanReboot(false)
	, bCanPowerOn(false)
	, bCanPowerOff(false)
	, DeviceType(ETargetDeviceTypes::Indeterminate)
{
	DeviceId = FTargetDeviceId(TargetPlatform.PlatformName(), FPlatformProcess::ComputerName());
	DeviceName = FPlatformProcess::ComputerName();
	MessageEndpoint = FMessageEndpoint::Builder("FIOSTargetDevice").Build();
}


bool FIOSTargetDevice::Connect()
{
	// @todo zombie - Probably need to write a specific ConnectTo(IpAddr) function for setting up a RemoteEndpoint for talking to the Daemon
	// Returning true since, if this exists, a device exists.

	return true;
}

bool FIOSTargetDevice::Deploy(const FString& SourceFolder, FString& OutAppId)
{
	return false;
}

void FIOSTargetDevice::Disconnect()
{
}

int32 FIOSTargetDevice::GetProcessSnapshot(TArray<FTargetDeviceProcessInfo>& OutProcessInfos)
{
	return 0;
}

ETargetDeviceTypes FIOSTargetDevice::GetDeviceType() const
{
	return DeviceType;
}

FTargetDeviceId FIOSTargetDevice::GetId() const
{
	return DeviceId;
}

FString FIOSTargetDevice::GetName() const
{
	return DeviceName;
}

FString FIOSTargetDevice::GetOperatingSystemName()
{
	return TargetPlatform.PlatformName();
}

const class ITargetPlatform& FIOSTargetDevice::GetTargetPlatform() const
{
	return TargetPlatform;
}

bool FIOSTargetDevice::IsConnected()
{
	return true;
}

bool FIOSTargetDevice::IsDefault() const
{
	return true;
}
	
bool FIOSTargetDevice::Launch(const FString& InAppId, EBuildConfigurations::Type InBuildConfiguration, EBuildTargets::Type BuildTarget, const FString& Params, uint32* OutProcessId)
{
#if !PLATFORM_MAC
	MessageEndpoint->Send(new FIOSLaunchDaemonLaunchApp(InAppId, Params), DeviceEndpoint);
	return true;
#else
	//Set return to false on Mac, since we could not find a way to do remote deploy/launch.
	return false;
#endif // !PLATFORM_MAC
}

bool FIOSTargetDevice::PowerOff(bool Force)
{
	// @todo zombie - Supported by the Daemon?

	return false;
}

bool FIOSTargetDevice::PowerOn()
{
	// @todo zombie - Supported by the Daemon?

	return false;
}

bool FIOSTargetDevice::Reboot(bool bReconnect)
{
	// @todo zombie - Supported by the Daemon?

	return false;
}

bool FIOSTargetDevice::Run(const FString& ExecutablePath, const FString& Params, uint32* OutProcessId)
{
#if !PLATFORM_MAC
	// The executable path usually looks something like this: directory/<gamename>.stub
	// We just need <gamename>, so strip that out.
	int32 LastPeriodPos = ExecutablePath.Find( TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 LastBackslashPos = ExecutablePath.Find( TEXT("\\"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 LastSlashPos = ExecutablePath.Find( TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	int32 TrimPos = LastBackslashPos > LastSlashPos ? LastBackslashPos : LastSlashPos;
	if ( TrimPos > LastPeriodPos )
	{
		// Ignore any periods in the path before the last "/" or "\"
		LastPeriodPos = ExecutablePath.Len() - 1;
	}

	if ( TrimPos == INDEX_NONE )
	{
		TrimPos = 0; // take the whole string from the beginning
	}
	else
	{
		// increment to one character beyond the slash
		TrimPos++;
	}

	int32 Count = LastPeriodPos - TrimPos;
	FString NewAppId = ExecutablePath.Mid(TrimPos, Count); // remove the ".stub" and the proceeding directory to get the game name

	SetAppId(NewAppId);
	MessageEndpoint->Send(new FIOSLaunchDaemonLaunchApp(AppId, Params), DeviceEndpoint);
	return true;
#else
	//Set return to false on Mac, since we could not find a way to do remote deploy/launch.
	return false;
#endif // !PLATFORM_MAC
}

bool FIOSTargetDevice::SupportsFeature(ETargetDeviceFeatures Feature) const
{
	switch (Feature)
	{
	case ETargetDeviceFeatures::Reboot:
		return bCanReboot;

	case ETargetDeviceFeatures::PowerOn:
		return bCanPowerOn;

	case ETargetDeviceFeatures::PowerOff:
		return bCanPowerOff;

	case ETargetDeviceFeatures::ProcessSnapshot:
		return false;

	default:
		return false;
	}
}

bool FIOSTargetDevice::SupportsSdkVersion(const FString& VersionString) const
{
	return true;
}

bool FIOSTargetDevice::TerminateProcess(const int64 ProcessId)
{
	return false;
}

void FIOSTargetDevice::SetUserCredentials(const FString& UserName, const FString& UserPassword)
{
}

bool FIOSTargetDevice::GetUserCredentials(FString& OutUserName, FString& OutUserPassword)
{
	return false;
}

inline void FIOSTargetDevice::ExecuteConsoleCommand(const FString& ExecCommand) const
{
	FString Params = FString::Printf(TEXT("command -device %s -param \"%s\""), *DeviceId.GetDeviceName(), *ExecCommand);

	AsyncTask(
		ENamedThreads::AnyThread,
		[Params]()
		{
			FString StdOut;
			FIOSTargetDeviceOutput::ExecuteDSCommand(TCHAR_TO_ANSI(*Params), &StdOut);
		}
	);
}

inline ITargetDeviceOutputPtr FIOSTargetDevice::CreateDeviceOutputRouter(FOutputDevice* Output) const
{
	FIOSTargetDeviceOutputPtr DeviceOutputPtr = MakeShareable(new FIOSTargetDeviceOutput());
	if (DeviceOutputPtr->Init(*this, Output))
	{
		return DeviceOutputPtr;
	}

	return nullptr;
}

int FIOSTargetDeviceOutput::ExecuteDSCommand(const char *CommandLine, FString* OutStdOut)
{
	void* WritePipe;
	void* ReadPipe;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
	FTcpDSCommander DSCommander((uint8*)CommandLine, strlen(CommandLine), WritePipe);
	while (DSCommander.IsValid() && !DSCommander.IsStopped())
	{
		FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
		if (NewLine.Len() > 0)
		{
			// process the string to break it up in to lines
			*OutStdOut += NewLine;
		}

		FPlatformProcess::Sleep(0.25);
	}
	FString NewLine = FPlatformProcess::ReadPipe(ReadPipe);
	if (NewLine.Len() > 0)
	{
		// process the string to break it up in to lines
		*OutStdOut += NewLine;
	}

	FPlatformProcess::Sleep(0.25);
	FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

	if (DSCommander.IsSystemError())
	{
		return -1;
	}

	if (!DSCommander.WasSuccess())
	{
		//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("The DeploymentServer command '%s' failed to run.\n"), ANSI_TO_TCHAR(CommandLine));
		return 0;
	}

	return 1;
}
