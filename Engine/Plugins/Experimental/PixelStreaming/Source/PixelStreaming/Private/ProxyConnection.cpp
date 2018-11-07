// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ProxyConnection.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Common/TcpSocketBuilder.h"
#include "Sockets.h"

#include "PixelStreamingCommon.h"
#include "Streamer.h"
#include "IPixelStreamingPlugin.h"
#include "PixelStreamingInputDevice.h"
#include "ProtocolDefs.h"

FProxyConnection::FProxyConnection(const FString& IP, uint16 Port, FStreamer& Streamer) :
	Streamer(Streamer),
	InputDevice(FModuleManager::Get().GetModuleChecked<IPixelStreamingPlugin>("PixelStreaming").GetInputDevice()),
	Thread(TEXT("WebRTC Proxy Connection"), [this, IP, Port]() { Run(IP, Port); }),
	Socket(nullptr),
	Listener(nullptr),
	ExitRequested(false)
{}

FProxyConnection::~FProxyConnection()
{
	ExitRequested = true;

	{
		FScopeLock Lock(&SocketMt);
		if (Socket)
		{
			Socket->Close();
		}
	}

	{
		FScopeLock Lock(&ListenerMt);
		if (Listener)
		{
			Listener->Close();
		}
	}

	Thread.Join();
}

void FProxyConnection::Run(const FString& IP, uint16 Port)
{
	InitReceiveHandlers();

	while (!ExitRequested)
	{
		if (!AcceptConnection(IP, Port))
		{
			continue;
		}

		Receive();
		DestroyConnection();
	}
	UE_LOG(PixelStreamingNet, Log, TEXT("WebRTC Proxy connection thread exited"));
}

bool FProxyConnection::Send(const uint8* Data, uint32 Size)
{
	FScopeLock Lock(&SocketMt);
	if (!Socket)
	{
		return false;
	}

	int32 bytesSent;
	return Socket->Send(Data, Size, bytesSent);
}

bool FProxyConnection::AcceptConnection(const FString& IP, uint16 Port)
{
	// listen to a single incoming connection from WebRTC Proxy
	FIPv4Address BindToAddr;
	bool bResult = FIPv4Address::Parse(IP, BindToAddr);
	checkf(bResult, TEXT("Failed to parse IPv4 address %s"), *IP);

	{
		FScopeLock Lock(&ListenerMt);
		Listener = FTcpSocketBuilder(TEXT("WebRTC Proxy Listener")).
			AsBlocking().
			AsReusable().
			Listening(1).
			BoundToAddress(BindToAddr).
			BoundToPort(Port).
			WithSendBufferSize(10 * 1024 * 1024).
			Build();
		check(Listener);
	}

	UE_LOG(PixelStreamingNet, Log, TEXT("Waiting for connection from WebRTC Proxy on %s:%d"), *IP, Port);
	FSocket* S = Listener->Accept(TEXT("WebRTC Proxy"));
	if (!S) // usually happens on exit because `Listener` was closed in destructor
	{
		return false;
	}

	// only one connection is expected, stop listening
	{
		FScopeLock Lock(&ListenerMt);
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Listener);
		Listener = nullptr;
	}

	{
		FScopeLock Lock(&SocketMt);
		Socket = S;
	}

	TSharedPtr<FInternetAddr> ProxyAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	Socket->GetPeerAddress(*ProxyAddr);

	UE_LOG(PixelStreamingNet, Log, TEXT("Accepted connection from WebRTC Proxy: %s"), *ProxyAddr->ToString(true));

	return true;
}

void FProxyConnection::DestroyConnection()
{
	if (!ExitRequested)
	{
		UE_LOG(PixelStreamingNet, Log, TEXT("Disconnected from WebRTC proxy"));
	}

	{
		FScopeLock Lock(&SocketMt);
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
// receiving Proxy messages

namespace ProxyConnectionImpl
{
	template<typename T>
	bool Read(FSocket& Socket, T& Value)
	{
		int32 BytesRead = 0;
		UE_LOG(PixelStreamingInput, VeryVerbose, TEXT("receiving %d bytes"), sizeof(T));
		return Socket.Recv(reinterpret_cast<uint8*>(&Value), sizeof(T), BytesRead, ESocketReceiveFlags::WaitAll);
	}
}

#define READFROMSOCKET(Type, Var)\
	Type Var;\
	if (!ProxyConnectionImpl::Read(*Socket, Var))\
	{\
		return false;\
	}

bool ReceiveString(FSocket* Socket, FString& OutString)
{
	READFROMSOCKET(uint16, StrLen);
	if (StrLen > 1024)
	{
		return false; // to avoid OOM by malicious browser scripts
	}

	OutString.GetCharArray().SetNumUninitialized(StrLen + 1);
	int32 BytesRead;
	if (!Socket->Recv(reinterpret_cast<uint8*>(OutString.GetCharArray().GetData()), StrLen * sizeof(TCHAR), BytesRead, ESocketReceiveFlags::WaitAll))
	{
		return false;
	}
	OutString.GetCharArray()[StrLen] = '\0';

	return true;
}

// XY positions are the ratio (0.0..1.0) along a viewport axis, quantized
// into an uint16 (0..65536). This allows the browser viewport and client
// viewport to have a different size.
void UnquantizeAndDenormalize(uint16& InOutX, uint16& InOutY)
{
	FIntPoint SizeXY = GEngine->GameViewport->Viewport->GetSizeXY();
	InOutX = InOutX / 65536.0f * SizeXY.X;
	InOutY = InOutY / 65536.0f * SizeXY.Y;
}

// XY deltas are the ratio (-1.0..1.0) along a viewport axis, quantized
// into an int16 (-32767..32767). This allows the browser viewport and
// client viewport to have a different size.
void UnquantizeAndDenormalize(int16& InOutX, int16& InOutY)
{
	FIntPoint SizeXY = GEngine->GameViewport->Viewport->GetSizeXY();
	InOutX = InOutX / 32767.0f * SizeXY.X;
	InOutY = InOutY / 32767.0f * SizeXY.Y;
}

/**
 * A touch is a specific finger placed on the canvas as a specific position.
 */
struct FTouch
{
	uint16 PosX;   // X position of finger.
	uint16 PosY;   // Y position of finger.
	uint8 TouchIndex;   // Index of finger for tracking multi-touch events.
	uint8 Force;   // Amount of pressure being applied by the finger.
};

using FKeyCodeType = uint8;
using FCharacterType = TCHAR;
using FRepeatType = uint8;
using FButtonType = uint8;
using FPosType = uint16;
using FDeltaType = int16;
using FTouchesType = TArray<FTouch>;

/**
* Get the array of touch positions and touch indices for a touch event,
* consumed from the receive buffer.
* @param Consumed - The number of bytes consumed from the receive buffer.
* @param OutTouches - The array of touches.
* @return False if there insufficient room in the receive buffer to read the entire event.
*/
bool ReceiveTouches(FSocket* Socket, FTouchesType& OutTouches)
{
	// Get the number of touches in the array.
	READFROMSOCKET(uint8, NumTouches);

	// Get the value of each touch position and then the touch index.
	for (int Touch = 0; Touch < NumTouches; Touch++)
	{
		READFROMSOCKET(FPosType, PosX);
		READFROMSOCKET(FPosType, PosY);
		UnquantizeAndDenormalize(PosX, PosY);
		READFROMSOCKET(uint8, TouchIndex);
		READFROMSOCKET(uint8, Force);
		OutTouches.Add({ PosX, PosY, TouchIndex, Force });
	}

	return true;
}

/**
* Convert the given array of touches to a friendly string for logging.
* @param InTouches - The array of touches.
* @return The string representation of the array.
*/
FString TouchesToString(const FTouchesType& InTouches)
{
	FString String;
	for (const FTouch& Touch : InTouches)
	{
		String += FString::Printf(TEXT("F[%d]=(%d, %d)(%.3f)"), Touch.TouchIndex, Touch.PosX, Touch.PosY, Touch.Force / 255.0f);
	}
	return String;
}

enum class KeyState { Alt = 1 << 0, Ctrl = 1 << 1, Shift = 1 << 2 };
enum class MouseButtonState { Left = 1 << 0, Right = 1 << 1, Middle = 1 << 2, Button4 = 1 << 3, Button5 = 1 << 4, Button6 = 1 << 5, Button7 = 1 << 6, Button8 = 1 << 7 };

void FProxyConnection::InitReceiveHandlers()
{
	using namespace PixelStreamingProtocol;

	ReceiveHandlers.SetNum(static_cast<int32>(EToUE4Msg::Count));

#define HANDLER(MsgType, Handler) ReceiveHandlers[static_cast<int32>(EToUE4Msg::MsgType)] = [this]() { {Handler} return true; }

	HANDLER(IFrameRequest,
	{
		UE_LOG(PixelStreamingInput, Log, TEXT("IFrameRequest"));
		Streamer.ForceIdrFrame();
	});

	HANDLER(UIInteraction,
	{
		FString Descriptor;
		if (ReceiveString(Socket, Descriptor))
		{
			UE_LOG(PixelStreamingInput, Verbose, TEXT("UIInteraction: %s"), *Descriptor);
			InputDevice.ProcessUIInteraction(Descriptor);
		}
	});

	HANDLER(Command,
	{
		FString Descriptor;
		if (ReceiveString(Socket, Descriptor))
		{
			UE_LOG(PixelStreamingInput, Verbose, TEXT("Command: %s"), *Descriptor);
			InputDevice.ProcessCommand(Descriptor);
		}
	});

	HANDLER(KeyDown,
	{
		READFROMSOCKET(FKeyCodeType, KeyCode);
		READFROMSOCKET(FRepeatType, Repeat);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("key down: %d, repeat: %d"), KeyCode, Repeat);

		FPixelStreamingInputDevice::FEvent KeyDownEvent(FPixelStreamingInputDevice::EventType::KEY_DOWN);
		KeyDownEvent.SetKeyDown(KeyCode, Repeat != 0);
		InputDevice.ProcessEvent(KeyDownEvent);
	});

	HANDLER(KeyUp,
	{
		READFROMSOCKET(FKeyCodeType, KeyCode);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("key up: %d"), KeyCode);

		FPixelStreamingInputDevice::FEvent KeyUpEvent(FPixelStreamingInputDevice::EventType::KEY_UP);
		KeyUpEvent.SetKeyUp(KeyCode);
		InputDevice.ProcessEvent(KeyUpEvent);
	});

	HANDLER(KeyPress,
	{
		READFROMSOCKET(FCharacterType, Character);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("key press: '%c'"), Character);

		FPixelStreamingInputDevice::FEvent KeyPressEvent(FPixelStreamingInputDevice::EventType::KEY_PRESS);
		KeyPressEvent.SetCharCode(Character);
		InputDevice.ProcessEvent(KeyPressEvent);
	});

	HANDLER(MouseEnter,
	{
		InputDevice.ProcessEvent(FPixelStreamingInputDevice::FEvent(FPixelStreamingInputDevice::EventType::MOUSE_ENTER));
		UE_LOG(PixelStreamingInput, Verbose, TEXT("mouseEnter"));
	});

	HANDLER(MouseLeave,
	{
		InputDevice.ProcessEvent(FPixelStreamingInputDevice::FEvent(FPixelStreamingInputDevice::EventType::MOUSE_LEAVE));
		UE_LOG(PixelStreamingInput, Verbose, TEXT("mouseLeave"));
	});

	HANDLER(MouseDown,
	{
		READFROMSOCKET(FButtonType, Button);
		READFROMSOCKET(FPosType, PosX);
		READFROMSOCKET(FPosType, PosY);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("mouseDown at (%d, %d), button %d"), PosX, PosY, Button);

		UnquantizeAndDenormalize(PosX, PosY);

		FPixelStreamingInputDevice::FEvent MouseDownEvent(FPixelStreamingInputDevice::EventType::MOUSE_DOWN);
		MouseDownEvent.SetMouseClick(Button, PosX, PosY);
		InputDevice.ProcessEvent(MouseDownEvent);
	});

	HANDLER(MouseUp,
	{
		READFROMSOCKET(FButtonType, Button);
		READFROMSOCKET(FPosType, PosX);
		READFROMSOCKET(FPosType, PosY);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("mouseUp at (%d, %d), button %d"), PosX, PosY, Button);

		UnquantizeAndDenormalize(PosX, PosY);

		FPixelStreamingInputDevice::FEvent MouseUpEvent(FPixelStreamingInputDevice::EventType::MOUSE_UP);
		MouseUpEvent.SetMouseClick(Button, PosX, PosY);
		InputDevice.ProcessEvent(MouseUpEvent);
	});

	HANDLER(MouseMove,
	{
		READFROMSOCKET(FPosType, PosX);
		READFROMSOCKET(FPosType, PosY);
		READFROMSOCKET(FDeltaType, DeltaX);
		READFROMSOCKET(FDeltaType, DeltaY);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("mouseMove to (%d, %d), delta (%d, %d)"), PosX, PosY, DeltaX, DeltaY);

		UnquantizeAndDenormalize(PosX, PosY);
		UnquantizeAndDenormalize(DeltaX, DeltaY);

		FPixelStreamingInputDevice::FEvent MouseMoveEvent(FPixelStreamingInputDevice::EventType::MOUSE_MOVE);
		MouseMoveEvent.SetMouseDelta(PosX, PosY, DeltaX, DeltaY);
		InputDevice.ProcessEvent(MouseMoveEvent);
	});

	HANDLER(MouseWheel,
	{
		READFROMSOCKET(FDeltaType, Delta);
		READFROMSOCKET(FPosType, PosX);
		READFROMSOCKET(FPosType, PosY);
		UE_LOG(PixelStreamingInput, Verbose, TEXT("mouseWheel, delta %d"), Delta);

		UnquantizeAndDenormalize(PosX, PosY);

		FPixelStreamingInputDevice::FEvent MouseWheelEvent(FPixelStreamingInputDevice::EventType::MOUSE_WHEEL);
		MouseWheelEvent.SetMouseWheel(Delta, PosX, PosY);
		InputDevice.ProcessEvent(MouseWheelEvent);
	});

	HANDLER(TouchStart,
	{
		FTouchesType Touches;
		if (!ReceiveTouches(Socket, Touches))
		{
			return false;
		}

		UE_LOG(PixelStreamingInput, Verbose, TEXT("TouchStart: %s"), *TouchesToString(Touches));

		for (const FTouch& Touch : Touches)
		{
			FPixelStreamingInputDevice::FEvent TouchStartEvent(FPixelStreamingInputDevice::EventType::TOUCH_START);
			TouchStartEvent.SetTouch(Touch.TouchIndex, Touch.PosX, Touch.PosY, Touch.Force);
			InputDevice.ProcessEvent(TouchStartEvent);
		}
	});

	HANDLER(TouchEnd,
	{
		FTouchesType Touches;
		if (!ReceiveTouches(Socket, Touches))
		{
			return false;
		}

		UE_LOG(PixelStreamingInput, Verbose, TEXT("TouchEnd: %s"), *TouchesToString(Touches));

		for (const FTouch& Touch : Touches)
		{
			FPixelStreamingInputDevice::FEvent TouchEndEvent(FPixelStreamingInputDevice::EventType::TOUCH_END);
			TouchEndEvent.SetTouch(Touch.TouchIndex, Touch.PosX, Touch.PosY, Touch.Force);
			InputDevice.ProcessEvent(TouchEndEvent);
		}
	});

	HANDLER(TouchMove,
	{
		FTouchesType Touches;
		if (!ReceiveTouches(Socket, Touches))
		{
			return false;
		}

		UE_LOG(PixelStreamingInput, Verbose, TEXT("TouchMove: %s"), *TouchesToString(Touches));

		for (const FTouch& Touch : Touches)
		{
			FPixelStreamingInputDevice::FEvent TouchMoveEvent(FPixelStreamingInputDevice::EventType::TOUCH_MOVE);
			TouchMoveEvent.SetTouch(Touch.TouchIndex, Touch.PosX, Touch.PosY, Touch.Force);
			InputDevice.ProcessEvent(TouchMoveEvent);
		}
	});

	HANDLER(MaxFpsRequest,
	{
		READFROMSOCKET(uint8, Fps);
		UE_LOG(PixelStreamingInput, Log, TEXT("%d WebRTC FPS"), Fps);
		//Streamer.SetFramerate(Fps);
	});

	HANDLER(AverageBitrateRequest,
	{
		READFROMSOCKET(uint16, Kbps);
		Streamer.SetBitrate(Kbps);
		UE_LOG(PixelStreamingInput, Log, TEXT("AverageBitrateRequest: %d"), Kbps);
	});

	HANDLER(StartStreaming,
	{
		Streamer.StartStreaming();
		UE_LOG(PixelStreamingInput, Log, TEXT("streaming started"));
	});

	HANDLER(StopStreaming,
	{
		Streamer.StopStreaming();
		UE_LOG(PixelStreamingInput, Log, TEXT("streaming stopped"));
	});

#undef HANDLER
}

#undef READFROMSOCKET

void FProxyConnection::Receive()
{
	while (!ExitRequested)
	{
		uint8 MsgType;
		int32 BytesRead = 0;
		if (!Socket->Recv(&MsgType, sizeof(MsgType), BytesRead, ESocketReceiveFlags::WaitAll))
		{
			break;
		}

		UE_LOG(PixelStreamingInput, Verbose, TEXT("receiving msg %d"), MsgType);

		if (ReceiveHandlers.IsValidIndex(MsgType))
		{
			if (ReceiveHandlers[MsgType] != nullptr)
			{
				if (!ReceiveHandlers[MsgType]())
				{
					break;
				}
			}
			else
			{
				UE_LOG(PixelStreamingInput, Warning, TEXT("unbound receive handler %d"), MsgType);
			}
		}
		else
		{
			UE_LOG(PixelStreamingInput, Warning, TEXT("out of range %d"), MsgType);
		}
	}

	if (!ExitRequested)
	{
		Streamer.StopStreaming();
		UE_LOG(PixelStreamingNet, Log, TEXT("WebRTC Proxy disconnected"));
	}
}
