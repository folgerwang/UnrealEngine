// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/RemoteSessionFrameBufferChannel.h"
#include "RemoteSession.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "BackChannel/Protocol/OSC/BackChannelOSCMessage.h"
#include "HAL/IConsoleManager.h"
#include "FrameGrabber.h"
#include "Async/Async.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Engine/Texture2D.h"
#include "Modules/ModuleManager.h"
#include "../../Channels/RemoteSessionChannel.h"

DECLARE_CYCLE_STAT(TEXT("RSFrameBufferCap"), STAT_FrameBufferCapture, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("RSImageCompression"), STAT_ImageCompression, STATGROUP_Game);

DECLARE_CYCLE_STAT(TEXT("RSTextureUpdate"), STAT_TextureUpdate, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("RSImageDecompression"), STAT_ImageDecompression, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("RSNumTicks"), STAT_RSNumTicks, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("RSReadyFrameCount"), STAT_RSNumFrames, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("RSDecodingFrameCount"), STAT_RSDecodingFrames, STATGROUP_Game);

static int32 FramerateMasterSetting = 0;
static FAutoConsoleVariableRef CVarFramerateOverride(
	TEXT("remote.framerate"), FramerateMasterSetting,
	TEXT("Sets framerate"),
	ECVF_Default);

static int32 QualityMasterSetting = 0;
static FAutoConsoleVariableRef CVarQualityOverride(
	TEXT("remote.quality"), QualityMasterSetting,
	TEXT("Sets quality (1-100)"),
	ECVF_Default);


FRemoteSessionFrameBufferChannel::FRemoteSessionFrameBufferChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection)
	: IRemoteSessionChannel(InRole, InConnection)
{
	LastSentImageTime = 0.0;
	Connection = InConnection;
	DecodedTextures[0] = nullptr;
	DecodedTextures[1] = nullptr;
	DecodedTextureIndex = 0;
	NumSentImages = 0;
	KickedTaskCount = 0;
	Role = InRole;

	if (Role == ERemoteSessionChannelMode::Receive)
	{
		MessageCallbackHandle = InConnection->GetDispatchMap().GetAddressHandler(TEXT("/Screen")).AddRaw(this, &FRemoteSessionFrameBufferChannel::ReceiveHostImage);
		InConnection->SetMessageOptions(TEXT("/Screen"), 1);
	}
}

FRemoteSessionFrameBufferChannel::~FRemoteSessionFrameBufferChannel()
{
	if (Role == ERemoteSessionChannelMode::Receive)
	{
		TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> LocalConnection = Connection.Pin();
		if (LocalConnection.IsValid())
		{
			// Remove the callback so it doesn't call back on an invalid this
			LocalConnection->GetDispatchMap().GetAddressHandler(TEXT("/Screen")).Remove(MessageCallbackHandle);
		}
		MessageCallbackHandle.Reset();
	}

	while (NumDecodingTasks.GetValue() > 0)
	{
		FPlatformProcess::SleepNoStats(0);
	}

	if (FrameGrabber.IsValid())
	{
		FrameGrabber->StopCapturingFrames();
		FrameGrabber = nullptr;
	}

	for (int32 i = 0; i < 2; i++)
	{
		if (DecodedTextures[i])
		{
			DecodedTextures[i]->RemoveFromRoot();
			DecodedTextures[i] = nullptr;
		}
	}
}

FString FRemoteSessionFrameBufferChannel::StaticType()
{
	return TEXT("rs.framebuffer");
}

void FRemoteSessionFrameBufferChannel::SetCaptureQuality(int32 InQuality, int32 InFramerate)
{
	// Set our framerate and quality cvars, if the user hasn't modified them
	if (FramerateMasterSetting == 0)
	{
		CVarFramerateOverride->Set(InFramerate);
	}

	if (QualityMasterSetting == 0)
	{
		CVarQualityOverride->Set(InQuality);
	}
}

void FRemoteSessionFrameBufferChannel::SetCaptureViewport(TSharedRef<FSceneViewport> Viewport)
{
	FrameGrabber = MakeShareable(new FFrameGrabber(Viewport, Viewport->GetSize()));
	FrameGrabber->StartCapturingFrames();
}

UTexture2D* FRemoteSessionFrameBufferChannel::GetHostScreen() const
{
	return DecodedTextures[DecodedTextureIndex];
}

void FRemoteSessionFrameBufferChannel::Tick(const float InDeltaTime)
{
	INC_DWORD_STAT(STAT_RSNumTicks);

	if (FrameGrabber.IsValid())
	{
		SCOPE_CYCLE_COUNTER(STAT_FrameBufferCapture);

		FrameGrabber->CaptureThisFrame(FFramePayloadPtr());

		TArray<FCapturedFrameData> Frames = FrameGrabber->GetCapturedFrames();

		if (Frames.Num())
		{
			const double ElapsedImageTimeMS = (FPlatformTime::Seconds() - LastSentImageTime) * 1000;
			const int32 DesiredFrameTimeMS = 1000 / FramerateMasterSetting;

			if (ElapsedImageTimeMS >= DesiredFrameTimeMS)
			{
				FCapturedFrameData& LastFrame = Frames.Last();

				TArray<FColor>* ColorData = new TArray<FColor>(MoveTemp(LastFrame.ColorBuffer));

				FIntPoint Size = LastFrame.BufferSize;

				NumDecodingTasks.Increment();

				AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [this, Size, ColorData]()
				{
					SCOPE_CYCLE_COUNTER(STAT_ImageCompression);

					for (FColor& Color : *ColorData)
					{
						Color.A = 255;
					}

					SendImageToClients(Size.X, Size.Y, *ColorData);

					delete ColorData;

					NumDecodingTasks.Decrement();
				});

				LastSentImageTime = FPlatformTime::Seconds();
			}
		}
	}
	
	if (Role == ERemoteSessionChannelMode::Receive)
	{
		SCOPE_CYCLE_COUNTER(STAT_TextureUpdate);

		TSharedPtr<FImageData> QueuedImage;

		{
			// Check to see if there are any queued images. We just care about the last
			FScopeLock ImageLock(&DecodedImageMutex);
			if (IncomingDecodedImages.Num())
			{
				INC_DWORD_STAT(STAT_RSNumFrames);
				QueuedImage = IncomingDecodedImages.Last();

				UE_LOG(LogRemoteSession, Verbose, TEXT("GT: Image %d is ready, discarding %d earlier images"),
					QueuedImage->ImageIndex, IncomingDecodedImages.Num()-1);

				IncomingDecodedImages.Empty();
			}
		}

		// If an image was waiting...
		if (QueuedImage.IsValid())
		{
			int32 NextImage = DecodedTextureIndex == 0 ? 1 : 0;

			// create a texture if we don't have a suitable one
			if (DecodedTextures[NextImage] == nullptr || QueuedImage->Width != DecodedTextures[NextImage]->GetSizeX() || QueuedImage->Height != DecodedTextures[NextImage]->GetSizeY())
			{
				CreateTexture(NextImage, QueuedImage->Width, QueuedImage->Height);
			}

			// Update it on the render thread. There shouldn't (...) be any harm in GT code using it from this point
			FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, QueuedImage->Width, QueuedImage->Height);
			TArray<uint8>* TextureData = new TArray<uint8>(MoveTemp(QueuedImage->ImageData));

			DecodedTextures[NextImage]->UpdateTextureRegions(0, 1, Region, 4 * QueuedImage->Width, 8, TextureData->GetData(), [this, NextImage](auto InTextureData, auto InRegions) {
				DecodedTextureIndex = NextImage;
				delete InTextureData;
				delete InRegions;
			});

			UE_LOG(LogRemoteSession, Verbose, TEXT("GT: Uploaded image %d"),
				QueuedImage->ImageIndex);
		} //-V773
	}
}

void FRemoteSessionFrameBufferChannel::SendImageToClients(int32 Width, int32 Height, const TArray<FColor>& ImageData)
{
	static bool SkipImages = FParse::Param(FCommandLine::Get(), TEXT("remote.noimage"));

	// Can be released on the main thread at anytime so hold onto it
	TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> LocalConnection = Connection.Pin();

	if (LocalConnection.IsValid() && SkipImages == false)
	{
		const double TimeNow = FPlatformTime::Seconds();

		// created on demand because there can be multiple SendImage requests in flight
		IImageWrapperModule* ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>(FName("ImageWrapper"));

		if (ImageWrapperModule != nullptr)
		{
			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);

			ImageWrapper->SetRaw(ImageData.GetData(), ImageData.GetAllocatedSize(), Width, Height, ERGBFormat::BGRA, 8);

			TArray<uint8> JPGData = ImageWrapper->GetCompressed(QualityMasterSetting);

			FBackChannelOSCMessage Msg(TEXT("/Screen"));
			Msg.Write(Width);
			Msg.Write(Height);
			Msg.Write(JPGData);
			Msg.Write(++NumSentImages);
			LocalConnection->SendPacket(Msg);

			UE_LOG(LogRemoteSession, Verbose, TEXT("Sent image %d in %.02f ms"),
				NumSentImages, (FPlatformTime::Seconds() - TimeNow) * 1000.0);
		}
	}
}

void FRemoteSessionFrameBufferChannel::ReceiveHostImage(FBackChannelOSCMessage& Message, FBackChannelOSCDispatch& Dispatch)
{
	int32 Width(0);
	int32 Height(0);

	TSharedPtr<FImageData, ESPMode::ThreadSafe> ReceivedImage = MakeShareable(new FImageData);

	Message << ReceivedImage->Width;
	Message << ReceivedImage->Height;
	Message << ReceivedImage->ImageData;
	Message << ReceivedImage->ImageIndex;

	FScopeLock Lock(&IncomingImageMutex);
	IncomingEncodedImages.Add(ReceivedImage);

	UE_LOG(LogRemoteSession, Verbose, TEXT("Received Image %d, %d pending"), 
		ReceivedImage->ImageIndex, IncomingEncodedImages.Num());

	if (NumDecodingTasks.GetValue() == 0)
	{
		NumDecodingTasks.Increment();
		KickedTaskCount++;

		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this]()
		{
			SCOPE_CYCLE_COUNTER(STAT_ImageDecompression);

			int ProcessedImageCount = 0;

			do
			{
				TSharedPtr<FImageData, ESPMode::ThreadSafe> Image;

				const double StartTime = FPlatformTime::Seconds();

				{
					// check if there's anything to do, if not this task is done
					FScopeLock TaskLock(&IncomingImageMutex);

					if (IncomingEncodedImages.Num() == 0)
					{
						NumDecodingTasks.Decrement();
						return;
					}

					// take the last image we don't care about the rest
					Image = IncomingEncodedImages.Last();

					UE_LOG(LogRemoteSession, Verbose, TEXT("Processing Image %d, discarding %d other pending images"),
						Image->ImageIndex, IncomingEncodedImages.Num()-1);

					IncomingEncodedImages.Empty();
				}

				IImageWrapperModule* ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>(FName("ImageWrapper"));

				if (ImageWrapperModule != nullptr)
				{
					TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule->CreateImageWrapper(EImageFormat::JPEG);

					ImageWrapper->SetCompressed(Image->ImageData.GetData(), Image->ImageData.Num());

					const TArray<uint8>* RawData = nullptr;

					if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
					{
						TSharedPtr<FImageData> QueuedImage = MakeShareable(new FImageData);
						QueuedImage->Width = Image->Width;
						QueuedImage->Height = Image->Height;
						QueuedImage->ImageData = MoveTemp(*((TArray<uint8>*)RawData));
						QueuedImage->ImageIndex = Image->ImageIndex;

						{
							FScopeLock ImageLock(&DecodedImageMutex);
							IncomingDecodedImages.Add(QueuedImage);

							UE_LOG(LogRemoteSession, Verbose, TEXT("finished decompressing image %d in %.02f ms (%d in queue)"),
								Image->ImageIndex,
								(FPlatformTime::Seconds() - StartTime) * 1000.0,
								IncomingEncodedImages.Num());
						}
					}
				}

			} while (true);

			UE_LOG(LogRemoteSession, Verbose, TEXT("No remaining images for task %d (%d processed). Exiting."),
				KickedTaskCount, ProcessedImageCount);
		});
	}
}

void FRemoteSessionFrameBufferChannel::CreateTexture(const int32 InSlot, const int32 InWidth, const int32 InHeight)
{
	if (DecodedTextures[InSlot])
	{
		DecodedTextures[InSlot]->RemoveFromRoot();
		DecodedTextures[InSlot] = nullptr;
	}

	DecodedTextures[InSlot] = UTexture2D::CreateTransient(InWidth, InHeight);

	DecodedTextures[InSlot]->AddToRoot();
	DecodedTextures[InSlot]->UpdateResource();

	UE_LOG(LogRemoteSession, Log, TEXT("Created texture in slot %d %dx%d for incoming image"), InSlot, InWidth, InHeight);
}


