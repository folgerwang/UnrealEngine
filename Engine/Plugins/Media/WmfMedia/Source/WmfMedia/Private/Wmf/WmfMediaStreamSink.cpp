// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WmfMediaStreamSink.h"

#if WMFMEDIA_SUPPORTED_PLATFORM

#include "Math/UnrealMathUtility.h"
#include "MediaSampleQueueDepths.h"
#include "Misc/ScopeLock.h"
#include "RenderingThread.h"
#include "timeapi.h"
#include "WmfMediaHardwareVideoDecodingTextureSample.h"
#include "WmfMediaSink.h"
#include "WmfMediaUtils.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d11.h>

/* FWmfMediaStreamSink static functions
 *****************************************************************************/

bool FWmfMediaStreamSink::Create(const GUID& MajorType, TComPtr<FWmfMediaStreamSink>& OutSink)
{
	TComPtr<FWmfMediaStreamSink> StreamSink = new FWmfMediaStreamSink(MajorType, 1);
	TComPtr<FWmfMediaSink> MediaSink = new FWmfMediaSink();

	if (!MediaSink->Initialize(StreamSink))
	{
		return false;
	}

	OutSink = StreamSink;

	return true;
}


/* FWmfMediaStreamSink structors
 *****************************************************************************/

FWmfMediaStreamSink::FWmfMediaStreamSink(const GUID& InMajorType, DWORD InStreamId)
	: Owner(nullptr)
	, Prerolling(false)
	, RefCount(0)
	, StreamId(InStreamId)
	, StreamType(InMajorType)
	, ClockRate(1.0f)
	, WaitTimer(nullptr)
	, VideoSamplePool(nullptr)
	, VideoSampleQueue(nullptr)
	, bShowSubTypeErrorMessage(true)
{
	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Created with stream type %s"), this, *WmfMedia::MajorTypeToString(StreamType));
}


FWmfMediaStreamSink::~FWmfMediaStreamSink()
{
	check(RefCount == 0);

	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Destroyed"), this);
}


/* FWmfMediaStreamSink interface
 *****************************************************************************/

bool FWmfMediaStreamSink::GetNextSample(TComPtr<IMFSample>& OutSample)
{
	FScopeLock Lock(&CriticalSection);

	while (SampleQueue.Num())
	{
		FQueuedSample QueuedSample = SampleQueue.Pop();

		if (QueuedSample.Sample.IsValid())
		{
			OutSample = QueuedSample.Sample;
			return true;
		}
		else
		{
			// process pending marker
			QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, QueuedSample.MarkerContext);
			PropVariantClear(QueuedSample.MarkerContext);
			delete QueuedSample.MarkerContext;
			UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Processed marker (%s)"), this, *WmfMedia::MarkerTypeToString(QueuedSample.MarkerType));
		}
	}

	return false;
}


bool FWmfMediaStreamSink::Initialize(FWmfMediaSink& InOwner)
{
	FScopeLock Lock(&CriticalSection);

	const HRESULT Result = ::MFCreateEventQueue(&EventQueue);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Failed to create event queue for stream sink: %s"), this, *WmfMedia::ResultToString(Result));
		return false;
	}

	Owner = &InOwner;

	return true;
}


HRESULT FWmfMediaStreamSink::Pause()
{
	FScopeLock Lock(&CriticalSection);

	return QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, NULL);
}


HRESULT FWmfMediaStreamSink::Preroll()
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	Prerolling = true;

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p:Preroll Request Sample"), this);

	return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
}


HRESULT FWmfMediaStreamSink::Restart()
{
	HRESULT Result = QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, NULL);

	if (FAILED(Result))
	{
		return Result;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p:Restart Request Sample"), this);

	return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
}


void FWmfMediaStreamSink::Shutdown()
{
	FScopeLock Lock(&CriticalSection);

	if (EventQueue.IsValid())
	{
		EventQueue->Shutdown();
		EventQueue.Reset();
	}

	CurrentMediaType.Reset();

	CloseTimer();
}


HRESULT FWmfMediaStreamSink::Start()
{
	FScopeLock Lock(&CriticalSection);

	// Set a high the timer resolution (ie, short timer period).
	timeBeginPeriod(1);

	// create the waitable timer
	WaitTimer = CreateWaitableTimer(NULL, FALSE, NULL);
	if (WaitTimer == nullptr)
	{
		HRESULT Result = HRESULT_FROM_WIN32(GetLastError());
		if (FAILED(Result))
		{
			return Result;
		}
	}

	HRESULT Result = QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, NULL);

	if (FAILED(Result))
	{
		return Result;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p:Start Request Sample"), this);

	return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
}


HRESULT FWmfMediaStreamSink::Stop()
{
	Flush();

	FScopeLock Lock(&CriticalSection);

	// Restore the timer resolution.
	timeEndPeriod(1);

	CloseTimer();

	return QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, NULL);
}

void FWmfMediaStreamSink::CloseTimer()
{
	if (WaitTimer != nullptr)
	{
		CloseHandle(WaitTimer);
		WaitTimer = nullptr;
	}
}

/* IMFGetService interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::GetService(__RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject)
{
	return Owner->GetService(guidService, riid, ppvObject);
}


/* IMFMediaEventGenerator interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return EventQueue->BeginGetEvent(pCallback, pState);
}


STDMETHODIMP FWmfMediaStreamSink::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return EventQueue->EndGetEvent(pResult, ppEvent);
}


STDMETHODIMP FWmfMediaStreamSink::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
	TComPtr<IMFMediaEventQueue> TempQueue;
	{
		FScopeLock Lock(&CriticalSection);

		if (!EventQueue.IsValid())
		{
			return MF_E_SHUTDOWN;
		}

		TempQueue = EventQueue;
	}

	return TempQueue->GetEvent(dwFlags, ppEvent);
}


STDMETHODIMP FWmfMediaStreamSink::QueueEvent(MediaEventType met, REFGUID extendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return EventQueue->QueueEventParamVar(met, extendedType, hrStatus, pvValue);
}


/* IMFMediaTypeHandler interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::GetCurrentMediaType(_Outptr_ IMFMediaType** ppMediaType)
{
	FScopeLock Lock(&CriticalSection);

	if (ppMediaType == NULL)
	{
		return E_POINTER;
	}

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!CurrentMediaType.IsValid())
	{
		return MF_E_NOT_INITIALIZED;
	}

	*ppMediaType = CurrentMediaType;
	(*ppMediaType)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMajorType(__RPC__out GUID* pguidMajorType)
{
	if (pguidMajorType == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!CurrentMediaType.IsValid())
	{
		return MF_E_NOT_INITIALIZED;
	}

	return CurrentMediaType->GetGUID(MF_MT_MAJOR_TYPE, pguidMajorType);
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaTypeByIndex(DWORD dwIndex, _Outptr_ IMFMediaType** ppType)
{
	if (ppType == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	// get supported media type
	TArray<TComPtr<IMFMediaType>> SupportedTypes = WmfMedia::GetSupportedMediaTypes(StreamType);

	if (!SupportedTypes.IsValidIndex(dwIndex))
	{
		return MF_E_NO_MORE_TYPES;
	}

	TComPtr<IMFMediaType> SupportedType = SupportedTypes[dwIndex];

	if (!SupportedType.IsValid())
	{
		return MF_E_INVALIDMEDIATYPE;
	}

	// create result type
	TComPtr<IMFMediaType> MediaType;
	{
		HRESULT Result = ::MFCreateMediaType(&MediaType);

		if (FAILED(Result))
		{
			return Result;
		}

		Result = SupportedType->CopyAllItems(MediaType);

		if (FAILED(Result))
		{
			return Result;
		}
	}

	*ppType = MediaType;
	(*ppType)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaTypeCount(__RPC__out DWORD* pdwTypeCount)
{
	if (pdwTypeCount == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pdwTypeCount = (DWORD)WmfMedia::GetSupportedMediaTypes(StreamType).Num();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::IsMediaTypeSupported(IMFMediaType* pMediaType, _Outptr_opt_result_maybenull_ IMFMediaType** ppMediaType)
{
	if (ppMediaType != NULL)
	{
		*ppMediaType = NULL;
	}

	if (pMediaType == NULL)
	{
		return E_POINTER;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Checking if media type is supported:\n%s"), this, *WmfMedia::DumpAttributes(*pMediaType));

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	// get requested major type
	GUID MajorType;
	{
		const HRESULT Result = pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &MajorType);

		if (FAILED(Result))
		{
			return Result;
		}
	}

	if (MajorType != StreamType)
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Media type doesn't match stream type %s"), this, *WmfMedia::MajorTypeToString(StreamType));
		return MF_E_INVALIDMEDIATYPE;
	}

	// compare media 
	const DWORD CompareFlagsData = MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_DATA;
	const DWORD CompareFlagsUserData = MF_MEDIATYPE_EQUAL_MAJOR_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_TYPES | MF_MEDIATYPE_EQUAL_FORMAT_USER_DATA;

	for (const TComPtr<IMFMediaType>& MediaType : WmfMedia::GetSupportedMediaTypes(StreamType))
	{
		if (!MediaType.IsValid())
		{
			continue;
		}

		DWORD OutFlags = 0;
		const HRESULT Result = MediaType->IsEqual(pMediaType, &OutFlags);

		if (SUCCEEDED(Result) && (((OutFlags & CompareFlagsData) == CompareFlagsData) || ((OutFlags & CompareFlagsUserData) == CompareFlagsUserData)))
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Media type is supported"), this, *WmfMedia::MajorTypeToString(MajorType));
			return S_OK;
		}
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Media type is not supported"), this);

	return MF_E_INVALIDMEDIATYPE;
}


STDMETHODIMP FWmfMediaStreamSink::SetCurrentMediaType(IMFMediaType* pMediaType)
{
	if (pMediaType == NULL)
	{
		return E_POINTER;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Setting current media type:\n%s"), this, *WmfMedia::DumpAttributes(*pMediaType));

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	const HRESULT Result = IsMediaTypeSupported(pMediaType, NULL);

	if (FAILED(Result))
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Tried to set unsupported media type"), this);
		return Result;
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Current media type set"), this);

	CurrentMediaType = pMediaType;

	return S_OK;
}


/* IMFStreamSink interface
 *****************************************************************************/

STDMETHODIMP FWmfMediaStreamSink::Flush()
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Flushing samples & markers"), this);

	while (SampleQueue.Num())
	{
		FQueuedSample QueuedSample = SampleQueue.Pop();
		if (QueuedSample.Sample.IsValid())
		{
			continue;
		}

		// notify WMF that flushed markers haven't been processed
		QueueEvent(MEStreamSinkMarker, GUID_NULL, E_ABORT, QueuedSample.MarkerContext);
		PropVariantClear(QueuedSample.MarkerContext);
		delete QueuedSample.MarkerContext;
	}

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetIdentifier(__RPC__out DWORD* pdwIdentifier)
{
	if (pdwIdentifier == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*pdwIdentifier = StreamId;

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaSink(__RPC__deref_out_opt IMFMediaSink** ppMediaSink)
{
	if (ppMediaSink == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	*ppMediaSink = Owner;
	(*ppMediaSink)->AddRef();

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::GetMediaTypeHandler(__RPC__deref_out_opt IMFMediaTypeHandler** ppHandler)
{
	if (ppHandler == NULL)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	return QueryInterface(IID_IMFMediaTypeHandler, (void**)ppHandler);
}


STDMETHODIMP FWmfMediaStreamSink::PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, __RPC__in const PROPVARIANT* pvarMarkerValue, __RPC__in const PROPVARIANT* pvarContextValue)
{
	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Placing marker (%s)"), this, *WmfMedia::MarkerTypeToString(eMarkerType));

	PROPVARIANT* MarkerContext = new PROPVARIANT;

	if (pvarContextValue != NULL)
	{
		HRESULT Result = ::PropVariantCopy(MarkerContext, pvarContextValue);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, Verbose, TEXT("StreamSink %p: Failed to copy marker context: %s"), this, *WmfMedia::ResultToString(Result));
			delete MarkerContext;

			return Result;
		}
	}

	SampleQueue.Add({ eMarkerType, MarkerContext, nullptr });

	return S_OK;
}


STDMETHODIMP FWmfMediaStreamSink::ProcessSample(__RPC__in_opt IMFSample* pSample)
{
	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Process Sample"), this);

	if (pSample == nullptr)
	{
		return E_POINTER;
	}

	FScopeLock Lock(&CriticalSection);

	if (!EventQueue.IsValid())
	{
		return MF_E_SHUTDOWN;
	}

	if (!CurrentMediaType.IsValid())
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Stream received a sample while not having a valid media type set"), this);
		return MF_E_INVALIDMEDIATYPE;
	}

	// get sample time
	LONGLONG Time = 0;
	{
		const HRESULT Result = pSample->GetSampleTime(&Time);

		if (FAILED(Result))
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("Failed to get time from sink sample: %s"), *WmfMedia::ResultToString(Result));
			return Result;
		}
	}

	SampleQueue.Add({ MFSTREAMSINK_MARKER_DEFAULT, NULL, pSample });

	// finish pre-rolling
	if (Prerolling)
	{
		if (IsVideoSampleQueueFull())
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Preroll complete, %d samples queued"), this, VideoSampleQueue->Num());
			Prerolling = false;
			return QueueEvent(MEStreamSinkPrerolled, GUID_NULL, S_OK, NULL);
		}
		else
		{
			TComPtr<IMFSample> NextSample;
			if (GetNextSample(NextSample))
			{
				CopyTextureAndEnqueueSample(NextSample);
				return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
			}
			else
			{
				UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p: Preroll complete, %d samples queued"), this, VideoSampleQueue->Num());
				Prerolling = false;
				return QueueEvent(MEStreamSinkPrerolled, GUID_NULL, S_OK, NULL);
			}
		}
	}
	else
	{
		TComPtr<IMFSample> NextSample;
		if (GetNextSample(NextSample))
		{
			ScheduleWaitForNextSample(NextSample);
		}
		else
		{
			QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
		}
		return S_OK;
	}
}


/* IUnknown interface
 *****************************************************************************/

STDMETHODIMP_(ULONG) FWmfMediaStreamSink::AddRef()
{
	return FPlatformAtomics::InterlockedIncrement(&RefCount);
}


#if _MSC_VER == 1900
	#pragma warning(push)
	#pragma warning(disable:4838)
#endif

STDMETHODIMP FWmfMediaStreamSink::QueryInterface(REFIID RefID, void** Object)
{
	static const QITAB QITab[] =
	{
		QITABENT(FWmfMediaStreamSink, IMFGetService),
		QITABENT(FWmfMediaStreamSink, IMFMediaTypeHandler),
		QITABENT(FWmfMediaStreamSink, IMFStreamSink),
		{ 0 }
	};

	return QISearch(this, QITab, RefID, Object);
}

#if _MSC_VER == 1900
	#pragma warning(pop)
#endif


STDMETHODIMP_(ULONG) FWmfMediaStreamSink::Release()
{
	int32 CurrentRefCount = FPlatformAtomics::InterlockedDecrement(&RefCount);

	if (CurrentRefCount == 0)
	{
		delete this;
	}

	return CurrentRefCount;
}


bool FWmfMediaStreamSink::IsVideoSampleQueueFull() const
{
	const int32 NumberOfQueueFrames = 3;
	const int32 MinNumberOfQueueFrames = FMath::Min(NumberOfQueueFrames, FMediaPlayerQueueDepths::MaxVideoSinkDepth);
	return (VideoSampleQueue->Num() >= MinNumberOfQueueFrames);
}


void FWmfMediaStreamSink::CopyTextureAndEnqueueSample(IMFSample* pSample)
{
	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("Queue Size: %d"), VideoSampleQueue->Num());

	if (IsVideoSampleQueueFull())
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("Queue is full, dropping samples"));
		return;
	}

	DWORD cBuffers = 0;
	IMFMediaBuffer* pBuffer = nullptr;
	IMFDXGIBuffer* pDXGIBuffer = nullptr;
	UINT dwViewIndex = 0;
	ID3D11Texture2D* pTexture2D = nullptr;

	HRESULT Result = pSample->GetBufferCount(&cBuffers);
	if (FAILED(Result))
	{
		return;
	}
	if (1 == cBuffers)
	{
		Result = pSample->GetBufferByIndex(0, &pBuffer);
		if (FAILED(Result))
		{
			return;
		}
	}
	Result = pBuffer->QueryInterface(__uuidof(IMFDXGIBuffer), (LPVOID*)&pDXGIBuffer);
	if (FAILED(Result))
	{
		return;
	}

	Result = pDXGIBuffer->GetResource(__uuidof(ID3D11Texture2D), (LPVOID*)&pTexture2D);
	if (FAILED(Result))
	{
		return;
	}

	Result = pDXGIBuffer->GetSubresourceIndex(&dwViewIndex);
	if (FAILED(Result))
	{
		return;
	}

	UINT32 DimX;
	UINT32 DimY;
	MFGetAttributeSize(CurrentMediaType, MF_MT_FRAME_SIZE, &DimX, &DimY);

	LONGLONG SampleTime = 0;
	LONGLONG SampleDuration = 0;

	pSample->GetSampleTime(&SampleTime);
	pSample->GetSampleDuration(&SampleDuration);

	GUID Guid;
	if (SUCCEEDED(CurrentMediaType->GetGUID(MF_MT_SUBTYPE, &Guid)))
	{
		check(Guid == MFVideoFormat_NV12)
		{
			const TSharedRef<FWmfMediaHardwareVideoDecodingTextureSample, ESPMode::ThreadSafe> TextureSample = VideoSamplePool->AcquireShared();

			check(TextureSample->GetMediaTextureSampleConverter()!=nullptr);

			ID3D11Texture2D* SharedTexture = TextureSample->InitializeSourceTexture(
				Owner->GetDevice(),
				FTimespan::FromMicroseconds(SampleTime / 10),
				FTimespan::FromMicroseconds(SampleDuration / 10),
				FIntPoint(DimX, DimY),
				PF_NV12,
				EMediaTextureSampleFormat::CharNV12);


			D3D11_BOX SrcBox;
			SrcBox.left = 0;
			SrcBox.top = 0;
			SrcBox.front = 0;
			SrcBox.right = DimX;
			SrcBox.bottom = DimY;
			SrcBox.back = 1;

			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("CopySubresourceRegion() ViewIndex:%d"), dwViewIndex);

			TComPtr<IDXGIKeyedMutex> KeyedMutex;
			SharedTexture->QueryInterface(_uuidof(IDXGIKeyedMutex), (void**)&KeyedMutex);

			if (KeyedMutex)
			{
				// No wait on acquire since sample is new and key is 0.
				if (KeyedMutex->AcquireSync(0, 0) == S_OK)
				{
					Owner->GetImmediateContext()->CopySubresourceRegion(SharedTexture, 0, 0, 0, 0, pTexture2D, dwViewIndex, &SrcBox);

					// Mark texture as updated with key of 1
					// Sample will be read in FWmfMediaHardwareVideoDecodingParameters::ConvertTextureFormat_RenderThread
					KeyedMutex->ReleaseSync(1);
					VideoSampleQueue->Enqueue(TextureSample);
				}
			}
		}
	}
	else
	{
		if (bShowSubTypeErrorMessage)
		{
			UE_LOG(LogWmfMedia, Log, TEXT("StreamSink %p: Unable to query MF_MT_SUBTYPE GUID of current media type"), this);
			bShowSubTypeErrorMessage = false;
		}
	}

	if (pTexture2D)
	{
		pTexture2D->Release();
	}
	if (pDXGIBuffer)
	{
		pDXGIBuffer->Release();
	}
	if (pBuffer)
	{
		pBuffer->Release();
	}
}


STDMETHODIMP FWmfMediaStreamSink::Invoke(IMFAsyncResult* pAsyncResult)
{
	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p:Invoke"), this);
	FScopeLock Lock(&CriticalSection);

	TComPtr<IMFSample> NextSample;
	if (GetNextSample(NextSample))
	{
		// process next samples
		ScheduleWaitForNextSample(NextSample);
		return S_OK;
	}
	else
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("StreamSink %p:Invoke Request Sample"), this);
		return QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, NULL);
	}
}


STDMETHODIMP FWmfMediaStreamSink::GetParameters(DWORD* pdwFlags, DWORD* pdwQueue)
{
	return E_NOTIMPL;
}


void FWmfMediaStreamSink::SetPresentationClock(IMFPresentationClock* InPresentationClock)
{
	FScopeLock Lock(&CriticalSection);
	PresentationClock = InPresentationClock;
}


void FWmfMediaStreamSink::SetClockRate(float InClockRate)
{
	ClockRate = InClockRate;
}


void FWmfMediaStreamSink::SetMediaSamplePoolAndQueue(
	FWmfMediaHardwareVideoDecodingTextureSamplePool* InVideoSamplePool,
	TMediaSampleQueue<IMediaTextureSample>* InVideoSampleQueue)
{
	FScopeLock Lock(&CriticalSection);
	VideoSamplePool = InVideoSamplePool;
	VideoSampleQueue = InVideoSampleQueue;
}


void FWmfMediaStreamSink::ScheduleWaitForNextSample(IMFSample* pSample)
{
	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("ScheduleWaitForNextSample Start"));

	if (IsVideoSampleQueueFull())
	{
		// Return sample to internal queue
		SampleQueue.Push({ MFSTREAMSINK_MARKER_DEFAULT, NULL, pSample });
	}
	else
	{
		CopyTextureAndEnqueueSample(pSample);
	}

	if (WaitTimer != nullptr)
	{
		// Re-schedule 
		const LONGLONG OneMilliSeconds = 10000;
		LARGE_INTEGER llDueTime;
		llDueTime.QuadPart = -4 * OneMilliSeconds;
		if (SetWaitableTimer(WaitTimer, &llDueTime, 0, NULL, NULL, FALSE) == 0)
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("SetWaitableTimer Error"));
			return;
		}

		TComPtr<IMFAsyncResult> pAsyncResult;
		HRESULT Result = MFCreateAsyncResult(nullptr, this, nullptr, &pAsyncResult);
		if (SUCCEEDED(Result))
		{
			MFPutWaitingWorkItem(WaitTimer, 0, pAsyncResult, nullptr);
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("MFPutWaitingWorkItem"));
		}
		else
		{
			UE_LOG(LogWmfMedia, VeryVerbose, TEXT("MFPutWaitingWorkItem Error"));
			return;
		}
	}
	else
	{
		UE_LOG(LogWmfMedia, VeryVerbose, TEXT("WaitTimer == 0"));
	}

	UE_LOG(LogWmfMedia, VeryVerbose, TEXT("ScheduleWaitForNextSample End"));

	return;
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif
