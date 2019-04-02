//
// Copyright (C) Microsoft. All rights reserved.
//

#pragma once
#include "HolographicStreamer.h"
#include <Microsoft.Perception.Simulation.h>
#include <TraceLoggingProvider.h>
#include <winmeta.h>
#include <wrl.h>
#include <strsafe.h>
#include <collection.h>
#include <robuffer.h>
#include <ppltasks.h>
#include <atomic>
#include <set>
#include <vector>

namespace Microsoft {
	namespace Holographic {

		// Defines provider Microsoft.Holographic.HolographicStreamerHelpers, guid {a68ed9da-6452-5b46-1677-3ba16dcab03d}
		class HolographicStreamerHelpersTrace final
		{
		private:
			HolographicStreamerHelpersTrace()
			{
				TraceLoggingRegister(Provider());
			}

			static HolographicStreamerHelpersTrace* Instance()
			{
				static Microsoft::WRL::Wrappers::CriticalSection s_lock;
				static std::unique_ptr<HolographicStreamerHelpersTrace> s_spInstance;

				auto lock = s_lock.Lock();
				if (!s_spInstance)
				{
					s_spInstance = std::unique_ptr<HolographicStreamerHelpersTrace>(new HolographicStreamerHelpersTrace());
				}

				return s_spInstance.get();
			}

			bool IsEnabledInternal()
			{
				return (Provider() && TraceLoggingProviderEnabled(Provider(), 0, 0));
			}

		public:
			~HolographicStreamerHelpersTrace()
			{
				TraceLoggingUnregister(Provider());
			}

			static bool IsEnabled()
			{
				return Instance()->IsEnabledInternal();
			}

			const TraceLoggingHProvider Provider()
			{
				TRACELOGGING_DEFINE_PROVIDER_STORAGE(
					providerStorage,
					"Microsoft.Holographic.HolographicStreamerHelpers",
					(0xa68ed9da, 0x6452, 0x5b46, 0x16, 0x77, 0x3b, 0xa1, 0x6d, 0xca, 0xb0, 0x3d));
				return &providerStorage;
			}

			static void SignalSpatialMappingUpdate()
			{
				if (IsEnabled())
				{
					TraceLoggingWrite(
						Instance()->Provider(),
						"SignalSpatialMappingUpdate",
						TraceLoggingLevel(WINEVENT_LEVEL_INFO));
				}
			}

			static void SpatialMappingChannelNotCreated()
			{
				if (IsEnabled())
				{
					TraceLoggingWrite(
						Instance()->Provider(),
						"SpatialMappingChannelNotCreated",
						TraceLoggingLevel(WINEVENT_LEVEL_WARNING));
				}
			}

			static void ErrorThrown(HRESULT hr)
			{
				if (IsEnabled())
				{
					TraceLoggingWrite(
						Instance()->Provider(),
						"ErrorThrown",
						TraceLoggingValue(hr, "HRESULT"),
						TraceLoggingLevel(WINEVENT_LEVEL_WARNING));
				}
			}
		};

		static void ThrowIfFailed(HRESULT hr)
		{
			if (FAILED(hr))
			{
				HolographicStreamerHelpersTrace::ErrorThrown(hr);

				// Set a breakpoint on this line to catch Win32 API errors.
				throw Platform::Exception::CreateException(hr);
			}
		}

		struct FrameMetadata
		{
			INT64 PredictionTargetTime;
			FocusPoint FocusPointInLeftViewSpace;
		};

		private enum class RemotingSpeechRecognitionMode
		{
			Off = 0,
			On = 1,
		};

		ref class SpeechState sealed
		{
		public:
			SpeechState()
			{
				Dictionary = ref new Platform::Collections::Vector<Platform::String^>();
				Language = "";
				mode = RemotingSpeechRecognitionMode::Off;
			}

			property Windows::Foundation::Collections::IVector<Platform::String^>^ Dictionary;
			property Platform::String^ Language;
			property RemotingSpeechRecognitionMode mode;
			property Windows::Storage::StorageFile^ GrammarFile;
		};

		ref class RecognizedSpeech
		{
		public:
			property Platform::String^ RecognizedText;
			property Windows::Media::SpeechRecognition::SpeechRecognitionConfidence Confidence;
			property double RawConfidence;
		};

		enum class CustomChannels : uint8_t
		{
			// Reserve channels 0-63 for system channels driven by the PerceptionMonitor API
			SystemChannelStart = 0,
			SystemChannelEnd = 63,

			// Unallocated channels available for custom data
			UserChannelStart = 64,

			// Channels used by HolographicStreamerHelpers
			SavedSpatialAnchorsChannel = 253,
			SpatialSurfacesChannel = 254,
			SpeechDataChannel = 255,
		};

		static Windows::Storage::Streams::DataReader^ GetReaderFromBytes(_In_ byte* data, UINT32 length)
		{
			using namespace Windows::Storage::Streams;

			DataWriter^ writer = ref new DataWriter();
			writer->WriteBytes(Platform::ArrayReference<BYTE>(data, length)); // we end up copying the data, but packets are small enough that its OK
			auto buffer = writer->DetachBuffer();
			return DataReader::FromBuffer(buffer);
		}

		static void SendIBuffer(_In_ Windows::Storage::Streams::IBuffer^ buffer, _In_ IHolographicStreamerDataChannel* channel)
		{
			using namespace Windows::Storage::Streams;

			Microsoft::WRL::ComPtr<IBufferByteAccess> spBufferByteAccess;
			ThrowIfFailed(reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(spBufferByteAccess.ReleaseAndGetAddressOf())));

			byte* data;
			ThrowIfFailed(spBufferByteAccess->Buffer(&data));
			ThrowIfFailed(channel->SendData(buffer->Length, data, true));
		}

		enum SpeechMessageTypes : UINT8
		{
			SetState,
			SpeechResult,
			GrammarFile,
		};

		class SpeechChannelHelpers
		{
		public:
			static RecognizedSpeech^ UnpackRecognizedSpeech(_In_ byte* data, UINT32 length)
			{
				using namespace Windows::Storage::Streams;

				DataReader^ reader = GetReaderFromBytes(data, length);

				RecognizedSpeech^ ret = nullptr;

				SpeechMessageTypes messageType = static_cast<SpeechMessageTypes>(reader->ReadByte());
				if (messageType == SpeechMessageTypes::SpeechResult)
				{
					ret = ref new RecognizedSpeech();
					ret->Confidence = static_cast<Windows::Media::SpeechRecognition::SpeechRecognitionConfidence>(reader->ReadUInt32());
					ret->RawConfidence = reader->ReadDouble();
					UINT32 Length = reader->ReadUInt32();
					ret->RecognizedText = reader->ReadString(Length);
				}

				return ret;
			}

			static void SendRecognizedSpeech(_In_ RecognizedSpeech^ speech, _In_ IHolographicStreamerDataChannel* channel)
			{
				using namespace Windows::Storage::Streams;

				DataWriter^ writer = ref new DataWriter();
				writer->WriteByte(SpeechMessageTypes::SpeechResult);
				writer->WriteUInt32(static_cast<UINT32>(speech->Confidence));
				writer->WriteDouble(speech->RawConfidence);
				writer->WriteUInt32(speech->RecognizedText->Length()); // +1 for null terminator?
				writer->WriteString(speech->RecognizedText);

				SendIBuffer(writer->DetachBuffer(), channel);
			}

			static Concurrency::task<Windows::Storage::StorageFile^> UnpackGrammarFileAsync(_In_ Windows::Storage::Streams::DataReader^ reader)
			{
				Concurrency::task<Windows::Storage::StorageFile^> result = Concurrency::task_from_result<Windows::Storage::StorageFile^>(nullptr);
				if (reader->UnconsumedBufferLength > 0)
				{
					SpeechMessageTypes messageType = static_cast<SpeechMessageTypes>(reader->ReadByte());
					if (messageType == SpeechMessageTypes::GrammarFile)
					{
						std::vector<byte> fileContents(reader->UnconsumedBufferLength);
						reader->ReadBytes(Platform::ArrayReference<byte>(fileContents.data(), static_cast<unsigned int>(fileContents.size())));

						result = Concurrency::create_task(Windows::Storage::ApplicationData::Current->TemporaryFolder->CreateFileAsync("grammar.xml", Windows::Storage::CreationCollisionOption::ReplaceExisting))
							.then([capturedFileContents = std::move(fileContents)](Windows::Storage::StorageFile^ grammarFile) mutable
						{
							return Concurrency::create_task(Windows::Storage::FileIO::WriteBytesAsync(grammarFile, Platform::ArrayReference<byte>(capturedFileContents.data(), static_cast<unsigned int>(capturedFileContents.size()))))
								.then([grammarFile, recapturedFileContents = std::move(capturedFileContents)]{ return grammarFile; });
						});
					}
				}
				return result;
			}

			static Concurrency::task<void> SerializeGrammarFileAsync(Windows::Storage::StorageFile^ grammarFile, Windows::Storage::Streams::DataWriter^ writer)
			{
				return Concurrency::create_task(Windows::Storage::FileIO::ReadBufferAsync(grammarFile))
					.then([writer](Windows::Storage::Streams::IBuffer^ buffer)
				{
					writer->WriteByte(SpeechMessageTypes::GrammarFile);
					writer->WriteBuffer(buffer);
				});
			}

			static Concurrency::task<SpeechState^> UnpackSpeechStateAsync(_In_ byte* data, UINT32 length)
			{
				using namespace Windows::Storage::Streams;
				Concurrency::task<SpeechState^> result = Concurrency::task_from_result<SpeechState^>(nullptr);

				DataReader^ reader = GetReaderFromBytes(data, length);

				SpeechState^ state = nullptr;

				SpeechMessageTypes messageType = static_cast<SpeechMessageTypes>(reader->ReadByte());
				if (messageType == SpeechMessageTypes::SetState)
				{
					state = ref new SpeechState();
					state->mode = static_cast<RemotingSpeechRecognitionMode>(reader->ReadByte());

					UINT32 length = reader->ReadUInt32();
					state->Language = reader->ReadString(length);

					bool valid = true;
					UINT32 numDictionaryEntries = reader->ReadUInt32();
					for (UINT32 i = 0; valid && i < numDictionaryEntries; ++i)
					{
						UINT32 length = reader->ReadUInt32();
						if (length <= reader->UnconsumedBufferLength)
						{
							state->Dictionary->Append(reader->ReadString(length));
						}
						else
						{
							// Bogus data!
							valid = false;
						}
					}

					if (valid)
					{
						result = UnpackGrammarFileAsync(reader)
							.then([state](Windows::Storage::StorageFile^ grammarFile)
						{
							state->GrammarFile = grammarFile;
							return state;
						});
					}
				}
				return result;
			}

			static Concurrency::task<Windows::Storage::Streams::IBuffer^> SerializeSpeechStateAsync(SpeechState^ state)
			{
				using namespace Windows::Storage::Streams;

				DataWriter^ writer = ref new DataWriter();
				writer->WriteByte(SpeechMessageTypes::SetState);
				writer->WriteByte(static_cast<BYTE>(state->mode));
				writer->WriteUInt32(state->Language->Length());
				writer->WriteString(state->Language);
				writer->WriteUInt32(state->Dictionary->Size);
				for (auto string : state->Dictionary)
				{
					writer->WriteUInt32(string->Length());
					writer->WriteString(string);
				}

				Concurrency::task<void> serializeGrammarFileTask =
					state->GrammarFile ?
					SerializeGrammarFileAsync(state->GrammarFile, writer) :
					Concurrency::task_from_result();

				return serializeGrammarFileTask.then([writer] { return writer->DetachBuffer(); });
			}

		};

		ref class RemoteSpeech
		{
		public:
			event Windows::Foundation::EventHandler<RecognizedSpeech^>^ OnRecognizedSpeech;

			// speech api:
			void SetSpeechState(SpeechState^ state)
			{
				{
					auto lock = m_lock.LockExclusive();
					m_cachedState = state;
				}

				SendSpeechState();
			}

			void SetGrammarFile(Windows::Storage::StorageFile^ grammarFile)
			{
				{
					auto lock = m_lock.LockExclusive();
					m_cachedState->GrammarFile = grammarFile;
				}

				SendSpeechState();
			}

		internal:
			void SetChannel(WRL::ComPtr<IHolographicStreamerDataChannel> spChannel)
			{
				{
					auto lock = m_lock.LockExclusive();
					m_SpeechChannel = spChannel;
				}

				SendSpeechState();
			}

			void DataReceived(byte* data, UINT32 length)
			{
				if (length > 0 && data[0] == static_cast<byte>(SpeechMessageTypes::SpeechResult))
				{
					auto result = SpeechChannelHelpers::UnpackRecognizedSpeech(data, length);
					if (result != nullptr)
					{
						OnRecognizedSpeech(this, result);
					}
				}
			}

		private:
			void SendSpeechState()
			{
				auto lock = m_lock.LockExclusive();
				if (m_SpeechChannel)
				{
					m_sendSpeechStateTask = m_sendSpeechStateTask.then([this]
					{
						SpeechChannelHelpers::SerializeSpeechStateAsync(m_cachedState)
							.then([this](Windows::Storage::Streams::IBuffer^ buffer)
						{
							auto lock = m_lock.LockExclusive();
							if (m_SpeechChannel)
							{
								SendIBuffer(buffer, m_SpeechChannel.Get());
							}
						});
					});
				}
			}

			WRL::Wrappers::SRWLock m_lock;
			Concurrency::task<void> m_sendSpeechStateTask = Concurrency::task_from_result();
			SpeechState^ m_cachedState = ref new SpeechState();
			WRL::ComPtr<IHolographicStreamerDataChannel> m_SpeechChannel;
		};

		class PeriodicCallbackWhileEventSignalled
		{
		public:
			PeriodicCallbackWhileEventSignalled() {}

			void Initialize(_Inout_ WRL::Wrappers::Event&& signal, _In_ const std::function<void()>& callback, UINT32 msTimeout)
			{
				ThrowIfFailed(signal.IsValid() ? S_OK : E_UNEXPECTED);

				m_event = std::move(signal);
				m_callback = callback;
				m_msTimeout = msTimeout;

				// create threadpool wait on cancel event+trigger event
				m_wait = CreateThreadpoolWait([](PTP_CALLBACK_INSTANCE, PVOID pvThis, PTP_WAIT wait, TP_WAIT_RESULT)
				{
					PeriodicCallbackWhileEventSignalled *pThis = reinterpret_cast<PeriodicCallbackWhileEventSignalled*>(pvThis);

					if (!pThis->m_cancelled.load())
					{
						// call callback
						pThis->m_callback();

						// schedule timer for the next callback
						LARGE_INTEGER dueTime;
						dueTime.QuadPart = pThis->m_msTimeout * -1 * 10000; // milliseconds -> 100 nanosecond units
						FILETIME fileTime = {};
						SetThreadpoolTimer(pThis->m_periodicTimer, &fileTime, pThis->m_msTimeout, 0);
					}
				}, this, nullptr);

				m_periodicTimer = CreateThreadpoolTimer([](PTP_CALLBACK_INSTANCE, PVOID pvThis, PTP_TIMER timer)
				{
					PeriodicCallbackWhileEventSignalled *pThis = reinterpret_cast<PeriodicCallbackWhileEventSignalled*>(pvThis);

					if (!pThis->m_cancelled.load())
					{
						if (WaitForSingleObject(pThis->m_event.Get(), 0) == WAIT_OBJECT_0) // event is set
						{
							// call callback
							pThis->m_callback();
						}
						else
						{
							// cancel threadpool timer
							SetThreadpoolTimer(timer, nullptr, 0, 0);

							// wait for event
							SetThreadpoolWait(pThis->m_wait, pThis->m_event.Get(), nullptr);
						}
					}
				}, this, nullptr);

				// trigger the event
				SetThreadpoolWait(m_wait, m_event.Get(), nullptr);
			}

			void SetDelayBetweenCallbacks(UINT32 msTimeout)
			{
				m_msTimeout = msTimeout;
				if (m_periodicTimer != nullptr)
				{
					FILETIME fileTimeNow = {};
					SetThreadpoolTimer(m_periodicTimer, &fileTimeNow, m_msTimeout, 0);
				}
			}

			~PeriodicCallbackWhileEventSignalled()
			{
				m_cancelled.store(true);

				// currently running timers/waits may schedule 1 more wait/timer after this
				// so we need to wait and cancel twice
				for (int i = 0; i < 2; ++i)
				{
					// don't schedule more events
					SetThreadpoolTimer(m_periodicTimer, nullptr, 0, 0);
					WaitForThreadpoolTimerCallbacks(m_periodicTimer, true);

					// wait for outstanding callbacks, cancelling new ones
					WaitForThreadpoolWaitCallbacks(m_wait, true);
				}

				// cleanup our wait and timer
				CloseThreadpoolWait(m_wait);
				CloseThreadpoolTimer(m_periodicTimer);
			}

		private:
			std::atomic_bool m_cancelled = false;
			UINT32 m_msTimeout = 0;
			PTP_WAIT m_wait = nullptr;
			PTP_TIMER m_periodicTimer = nullptr;
			WRL::Wrappers::Event m_event;
			std::function<void()> m_callback;
		};

		class CallbackWhenEventSignalled
		{
		public:
			CallbackWhenEventSignalled() {}

			void Initialize(_Inout_ WRL::Wrappers::Event&& signal, _In_ const std::function<void()>& callback)
			{
				m_event = std::move(signal);
				m_callback = callback;

				m_wait = CreateThreadpoolWait([](PTP_CALLBACK_INSTANCE, PVOID pvThis, PTP_WAIT wait, TP_WAIT_RESULT)
				{
					CallbackWhenEventSignalled *pThis = reinterpret_cast<CallbackWhenEventSignalled*>(pvThis);

					if (!pThis->m_quit)
					{
						SetThreadpoolWait(pThis->m_wait, pThis->m_event.Get(), nullptr); // run again next time the event is set
					}

					pThis->m_callback();
				}, this, nullptr);

				SetThreadpoolWait(m_wait, m_event.Get(), nullptr);
			}

			~CallbackWhenEventSignalled()
			{
				m_quit = true;

				// wait for outstanding callbacks, cancelling new ones
				// twice because the callback may call SetThreadpoolWait if it already checked m_quit
				SetThreadpoolWait(m_wait, nullptr, nullptr);
				WaitForThreadpoolWaitCallbacks(m_wait, true);
				SetThreadpoolWait(m_wait, nullptr, nullptr);
				WaitForThreadpoolWaitCallbacks(m_wait, true);

				CloseThreadpoolWait(m_wait);
			}

		private:
			std::atomic<bool> m_quit = false;
			PTP_WAIT m_wait = nullptr;
			WRL::Wrappers::Event m_event;
			std::function<void()> m_callback;
		};

		delegate void ConnectedEvent();
		delegate void DisconnectedEvent(_In_ HolographicStreamerConnectionFailureReason failureReason);
		delegate void SendFrameEvent(_In_ const WRL::ComPtr<ID3D11Texture2D>& spTexture, _In_ const FrameMetadata& metadata);

		inline HRESULT MarshalAnchorToRemoteMachine(Windows::Perception::Spatial::SpatialAnchor^ anchor, Windows::Storage::Streams::DataWriter^ writer)
		{
			Microsoft::WRL::ComPtr<IInspectable> spInspectable = reinterpret_cast<IInspectable*>(anchor);

			Microsoft::WRL::ComPtr<IStream> spStream;
			HRESULT hr = CreateStreamOnHGlobal(nullptr, true, &spStream);
			if (SUCCEEDED(hr))
			{
				hr = CoMarshalInterface(spStream.Get(), IID_IInspectable, spInspectable.Get(), MSHCTX_DIFFERENTMACHINE, nullptr, MSHLFLAGS_TABLEWEAK);
				if (SUCCEEDED(hr))
				{
					LARGE_INTEGER start = {};
					hr = spStream->Seek(start, STREAM_SEEK_SET, nullptr);
					if (SUCCEEDED(hr))
					{
						STATSTG stats = {};
						hr = spStream->Stat(&stats, STATFLAG_NONAME);
						if (SUCCEEDED(hr))
						{
							writer->WriteUInt64(stats.cbSize.QuadPart);

							std::vector<byte> buffer(static_cast<size_t>(stats.cbSize.QuadPart));
							ULONG read;
							hr = spStream->Read(buffer.data(), static_cast<ULONG>(buffer.size()), &read);
							if (SUCCEEDED(hr) && (read == buffer.size()))
							{
								writer->WriteBytes(Platform::ArrayReference<byte>(buffer.data(), static_cast<unsigned int>(buffer.size())));
							}
						}
					}
				}
			}
			return hr;
		}

		ref class HolographicStreamerHelpers
		{
		internal:
			HolographicStreamerHelpers()
			{
				m_speechRemote = ref new Microsoft::Holographic::RemoteSpeech();

				if (!IsPerceptionSimulationSupported())
				{
					throw ref new Platform::NotImplementedException();
				}

				ThrowIfFailed(InitializePerceptionSimulation(
					PerceptionSimulationControlFlags_WaitForCalibration,
					IID_PPV_ARGS(&m_spPerceptionSimulationControl)));

				WRL::ComPtr<IUnknown> spUnkHolographicSpace;
				ThrowIfFailed(m_spPerceptionSimulationControl->get_HolographicSpace(&spUnkHolographicSpace));
				m_holographicSpace = static_cast<Windows::Graphics::Holographic::HolographicSpace^>(reinterpret_cast<Platform::Object^>(spUnkHolographicSpace.Get()));

				ThrowIfFailed(m_spPerceptionSimulationControl->get_ControlStream(&m_spStreamSink));

				WRL::ComPtr<FrameGeneratedCallbackWrapper> spFrameGeneratedCallback;
				ThrowIfFailed(WRL::MakeAndInitialize<FrameGeneratedCallbackWrapper>(&spFrameGeneratedCallback, this));
				ThrowIfFailed(m_spPerceptionSimulationControl->SetFrameGeneratedCallback(spFrameGeneratedCallback.Get()));

				WRL::Wrappers::Event spatialMappingInterestEvent;
				spatialMappingInterestEvent.Attach(CreateSpatialSurfacesInterestEvent());

				Platform::WeakReference weakThis(this);

				m_SpatialMappingUpdateCallback.Initialize(std::move(spatialMappingInterestEvent), [weakThis]()
				{
					HolographicStreamerHelpers^ spThis = weakThis.Resolve<HolographicStreamerHelpers>();
					if (spThis)
					{
						byte data = 0;
						auto channel = spThis->m_SpatialMappingChannel;
						if (channel != nullptr)
						{
							HolographicStreamerHelpersTrace::SignalSpatialMappingUpdate();
							ThrowIfFailed(channel->SendData(1, &data, true)); // send a single byte to trigger updates
						}
						else
						{
							HolographicStreamerHelpersTrace::SpatialMappingChannelNotCreated();
						}
					}
				}, m_msTimeBetweenSurfaceUpdates);

				WRL::Wrappers::Event savedSpatialAnchorsChangedEvent;
				savedSpatialAnchorsChangedEvent.Attach(CreateSpatialAnchorsInUseChangedEvent());
				m_SavedSpatialAnchorsUpdateCallback.Initialize(std::move(savedSpatialAnchorsChangedEvent), [weakThis]()
				{
					HolographicStreamerHelpers^ spThis = weakThis.Resolve<HolographicStreamerHelpers>();
					if (spThis)
					{
						spThis->UpdateActiveSpatialAnchors();
					}
				});
			}

			static bool IsPerceptionSimulationSupported()
			{
				return Windows::Foundation::Metadata::ApiInformation::IsApiContractPresent(L"Windows.Perception.Automation.Core.PerceptionAutomationCoreContract", 1);
			}

			void CreateStreamer(
				_In_ ID3D11Device* device)
			{
				WRL::ComPtr<ClientCallbackWrapper> spClientCallback;
				ThrowIfFailed(WRL::MakeAndInitialize<ClientCallbackWrapper>(&spClientCallback, this));

				ThrowIfFailed(CreateHolographicStreamerClient(
					spClientCallback.Get(),
					device,
					IID_PPV_ARGS(&m_spHolographicStreamerClient)));
			}

			property Microsoft::Holographic::RemoteSpeech^ RemoteSpeech
			{
				Microsoft::Holographic::RemoteSpeech^ get()
				{
					return m_speechRemote;
				}
			}

			property Windows::Graphics::Holographic::HolographicSpace^ HolographicSpace
			{
				Windows::Graphics::Holographic::HolographicSpace^ get()
				{
					return m_holographicSpace;
				}
			}

			property unsigned int TimeBetweenSpatialSurfaceUpdates
			{
				unsigned int get()
				{
					return m_msTimeBetweenSurfaceUpdates;
				}

				void set(unsigned int timeout)
				{
					m_msTimeBetweenSurfaceUpdates = timeout;
					m_SpatialMappingUpdateCallback.SetDelayBetweenCallbacks(timeout);
				}
			}

			WRL::ComPtr<IPerceptionSimulationFrame> GetLatestSimulationFrame()
			{
				return m_spFrame;
			}

			//!\brief Connect to client at the given address
			void Connect(_In_z_ PCWSTR hostname, _In_ UINT32 port)
			{
				ValidateStreamerClientCreated();

				ThrowIfFailed(m_spHolographicStreamerClient->Connect(hostname, port));
			}

			//!\brief Terminate the active client connection
			void Disconnect()
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->Disconnect());
			}

			//!\brief Get the current connection state
			HolographicStreamerConnectionState GetConnectionState()
			{
				ValidateStreamerClientCreated();
				HolographicStreamerConnectionState state = HolographicStreamerConnectionState::Disconnected;
				ThrowIfFailed(m_spHolographicStreamerClient->GetConnectionState(&state));
				return state;
			}

			//!\brief Send video data to the client endpoint
			//! Texture must remain valid for the duration of this call
			//! Throws failure if video is not enabled
			void SendFrame(
				_In_ ID3D11Texture2D* texture,
				_In_ UINT32 metadataLength,
				_In_reads_(metadataLength) const byte* metadata)
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->SendFrame(texture, metadataLength, metadata));
			}

			//!\brief Create an specific data channel
			//!\param channelId - The channel id number.  Known channel ids are listed in the CustomChannels enum.
			void CreateDataChannel(
				_In_ UINT8 channelId) //!< opaque channel id
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->CreateDataChannel(channelId));
			}

			//!\brief Set the maximum bitrate
			void SetMaxBitrate(_In_ UINT32 maxBitrateKbps)
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->SetMaxVideoBitrate(maxBitrateKbps));
			}

			//!\brief Set video frame dimensions
			void SetVideoFrameSize(_In_ UINT32 width, _In_ UINT32 height)
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->SetVideoFrameAttributes(width, height, HolographicStreamerVideoCompositionMode::Stereo));
			}

			//!\brief Enable/disable video channel
			void SetEnableVideo(_In_ BOOL enable)
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->SetEnableVideo(enable));
			}

			//!\brief Enable/disable audio channel
			void SetEnableAudio(_In_ BOOL enable)
			{
				ValidateStreamerClientCreated();
				ThrowIfFailed(m_spHolographicStreamerClient->SetEnableAudio(enable));
			}

			event ConnectedEvent^ OnConnected;
			event DisconnectedEvent^ OnDisconnected;
			event SendFrameEvent^ OnSendFrame;

		private:

			void UpdateActiveSpatialAnchors()
			{
				using namespace Windows::Storage::Streams;
				auto lock = m_lock.LockExclusive();

				auto channel = m_SpatialMappingChannel;
				if (channel != nullptr)
				{
					// get the current spatial anchors
					Microsoft::WRL::ComPtr<IInspectable> spActiveSpatialAnchors;
					HRESULT hr = GetSpatialAnchorsInUse(spActiveSpatialAnchors.ReleaseAndGetAddressOf());
					if (SUCCEEDED(hr))
					{
						Platform::Object^ anchors = reinterpret_cast<Platform::Object^>(spActiveSpatialAnchors.Get());
						auto activeAnchors = static_cast<Windows::Foundation::Collections::IMapView<Platform::String^, Windows::Perception::Spatial::SpatialAnchor^>^>(anchors);
						auto iterableActiveAnchors = activeAnchors->First();
						DataWriter^ writer = ref new DataWriter();

						wchar_t filename[MAX_PATH] = {};
						auto filenameLength = GetModuleFileNameW(NULL, filename, _countof(filename)); // todo: consider just taking filename, not whole path
						Platform::String^ AppUniqueName = ref new Platform::String(filename, filenameLength);
						writer->WriteUInt32(AppUniqueName->Length());
						writer->WriteString(AppUniqueName);

						Platform::String^ MachineUniqueName = "";
						auto hostNames = Windows::Networking::Connectivity::NetworkInformation::GetHostNames();
						for (const auto &hostName : hostNames)
						{
							if (hostName->Type == Windows::Networking::HostNameType::DomainName)
							{
								MachineUniqueName = hostName->CanonicalName;
								break;
							}
						}

						writer->WriteUInt32(MachineUniqueName->Length());
						writer->WriteString(MachineUniqueName);

						writer->WriteUInt32(activeAnchors->Size);

						while (iterableActiveAnchors->HasCurrent && SUCCEEDED(hr))
						{
							auto kv = iterableActiveAnchors->Current;
							auto name = kv->Key;
							writer->WriteUInt32(name->Length());
							writer->WriteString(name);

							auto anchor = kv->Value;
							hr = MarshalAnchorToRemoteMachine(anchor, writer);

							iterableActiveAnchors->MoveNext();
						}

						if (SUCCEEDED(hr))
						{
							SendIBuffer(writer->DetachBuffer(), channel.Get());
						}
					}
				}
			}

			//
			// Event handlers.
			//

			void Connected()
			{
				OnConnected();
			}

			void Disconnected(_In_ HolographicStreamerConnectionFailureReason failureReason)
			{
				auto lock = m_lock.LockExclusive();
				m_channels.clear();
				m_SpeechChannel = nullptr;
				m_SpatialMappingChannel = nullptr;
				m_SavedSpatialAnchorsChannel = nullptr;

				OnDisconnected(failureReason);
			}

			void DataChannelCreated(_In_ IHolographicStreamerDataChannel* channel, _In_ UINT8 channelId)
			{
				WRL::ComPtr<ChannelCallbackWrapper> spChannelCallback;
				ThrowIfFailed(WRL::MakeAndInitialize<ChannelCallbackWrapper>(&spChannelCallback, this, channelId));
				ThrowIfFailed(channel->SetCallback(spChannelCallback.Get()));
				{
					auto lock = m_lock.LockExclusive();
					m_channels.push_back(channel);
				}

				if (channelId == static_cast<UINT8>(CustomChannels::SpeechDataChannel))
				{
					m_SpeechChannel = channel;
					m_speechRemote->SetChannel(m_SpeechChannel.Get());
				}
				else if (channelId == static_cast<UINT8>(CustomChannels::SpatialSurfacesChannel))
				{
					m_SpatialMappingChannel = channel;
				}
				else if (channelId == static_cast<UINT8>(CustomChannels::SavedSpatialAnchorsChannel))
				{
					m_SavedSpatialAnchorsChannel = channel;
					UpdateActiveSpatialAnchors();
				}
			}

			void FrameGenerated(_In_ IPerceptionSimulationFrame* frame)
			{
				INT64 timestamp = 0;
				ThrowIfFailed(frame->get_PredictionTargetTime(&timestamp));

				HolographicStreamerConnectionState state = HolographicStreamerConnectionState::Disconnected;
				ThrowIfFailed(m_spHolographicStreamerClient->GetConnectionState(&state));

				if (timestamp != m_lastFrameTimestamp && state == HolographicStreamerConnectionState::Connected)
				{
					m_lastFrameTimestamp = timestamp;

					WRL::ComPtr<ID3D11Texture2D> spTexture;
					ThrowIfFailed(frame->get_Frame(&spTexture));

					FrameMetadata frameMetadata;
					ThrowIfFailed(frame->get_PredictionTargetTime(&frameMetadata.PredictionTargetTime));
					ThrowIfFailed(frame->get_FocusPointInLeftViewSpace(&frameMetadata.FocusPointInLeftViewSpace));

					OnSendFrame(spTexture, frameMetadata);

					ThrowIfFailed(m_spHolographicStreamerClient->SendFrame(spTexture.Get(), sizeof(frameMetadata), reinterpret_cast<byte*>(&frameMetadata)));
				}

				m_spFrame = frame;
			}

			void DataReceived(_In_ UINT8 channelId, _In_ UINT32 dataLength, _In_reads_(dataLength) byte* data)
			{
				if (channelId == static_cast<UINT8>(CustomChannels::SpeechDataChannel))
				{
					HandleSpeechDataReceived(dataLength, data);
				}
				else
				{
					ThrowIfFailed(m_spStreamSink->OnPacketReceived(dataLength, data));
				}
			}

			void HandleSpeechDataReceived(UINT32 dataLength, byte* data)
			{
				// bubble up the recognized speech
				m_speechRemote->DataReceived(data, dataLength);
			}

			//
			// Helpers.
			//
			void DestroyHolographicRemoting()
			{
				m_holographicSpace = nullptr;
				m_spStreamSink.Reset();
				m_spPerceptionSimulationControl.Reset();
			}

			void ValidateStreamerClientCreated()
			{
				if (!m_spHolographicStreamerClient)
				{
					throw ref new Platform::InvalidArgumentException();
				}
			}

			class FrameGeneratedCallbackWrapper
				: public WRL::RuntimeClass<
				WRL::RuntimeClassFlags<WRL::ClassicCom>,
				IPerceptionSimulationFrameGeneratedCallback,
				WRL::FtmBase>
			{
			public:
				HRESULT RuntimeClassInitialize(
					HolographicStreamerHelpers^ outer)
				{
					m_outerWeak = outer;
					return S_OK;
				}

				STDMETHODIMP FrameGenerated(_In_ IPerceptionSimulationFrame* frame) override
				{
					HolographicStreamerHelpers^ outer = m_outerWeak.Resolve<HolographicStreamerHelpers>();
					if (outer)
					{
						outer->FrameGenerated(frame);
					}
					return S_OK;
				}

			private:
				Platform::WeakReference m_outerWeak;
			};

			class ChannelCallbackWrapper
				: public WRL::RuntimeClass<
				WRL::RuntimeClassFlags<WRL::ClassicCom>,
				IHolographicStreamerDataChannelCallback,
				WRL::FtmBase>
			{
			public:
				HRESULT RuntimeClassInitialize(
					HolographicStreamerHelpers^ outer,
					UINT8 channelId)
				{
					m_outerWeak = outer;
					m_channelId = channelId;
					return S_OK;
				}

				//!\brief Called when data is received from the remote endpoint
				STDMETHODIMP DataReceived(_In_ UINT32 dataLength, _In_reads_(dataLength) byte* data) override
				{
					HRESULT hr = S_OK;
					HolographicStreamerHelpers^ outer = m_outerWeak.Resolve<HolographicStreamerHelpers>();

					if (outer)
					{
						try
						{
							outer->DataReceived(m_channelId, dataLength, data);
						}
						catch (Platform::Exception^ ex)
						{
							hr = ex->HResult;
						}
					}

					return hr;
				}

			private:
				Platform::WeakReference m_outerWeak;
				UINT8 m_channelId;
			};

			class ClientCallbackWrapper
				: public WRL::RuntimeClass<
				WRL::RuntimeClassFlags<WRL::ClassicCom>,
				IHolographicStreamerClientCallback,
				WRL::FtmBase>
			{
			public:
				HRESULT RuntimeClassInitialize(
					HolographicStreamerHelpers^ outer)
				{
					m_outerWeak = outer;
					return S_OK;
				}

				//!\brief Called when connection has been established
				STDMETHODIMP Connected() override
				{
					HolographicStreamerHelpers^ outer = m_outerWeak.Resolve<HolographicStreamerHelpers>();
					if (outer)
					{
						outer->Connected();
					}
					return S_OK;
				}

				//!\brief Called when the connection has been terminated
				STDMETHODIMP Disconnected(_In_ HolographicStreamerConnectionFailureReason failureReason) override
				{
					HolographicStreamerHelpers^ outer = m_outerWeak.Resolve<HolographicStreamerHelpers>();
					if (outer)
					{
						outer->Disconnected(failureReason);
					}
					return S_OK;
				}

				//!\brief Called when a new application data channel has been established
				STDMETHODIMP DataChannelCreated(_In_ IHolographicStreamerDataChannel* channel, _In_ UINT8 channelId) override
				{
					HolographicStreamerHelpers^ outer = m_outerWeak.Resolve<HolographicStreamerHelpers>();
					if (outer)
					{
						outer->DataChannelCreated(channel, channelId);
					}
					return S_OK;
				}

			private:
				Platform::WeakReference m_outerWeak;
			};

			UINT32 m_msTimeBetweenSurfaceUpdates = 2000;
			Microsoft::Holographic::RemoteSpeech^ m_speechRemote;
			WRL::ComPtr<IHolographicStreamerClient> m_spHolographicStreamerClient;
			WRL::ComPtr<IPerceptionSimulationControl> m_spPerceptionSimulationControl;
			Windows::Graphics::Holographic::HolographicSpace^ m_holographicSpace;
			UINT64 m_lastFrameTimestamp = 0;

			WRL::ComPtr<ISimulationStreamSink> m_spStreamSink;

			WRL::Wrappers::SRWLock m_lock;
			std::vector<WRL::ComPtr<IHolographicStreamerDataChannel>> m_channels;
			WRL::ComPtr<IHolographicStreamerDataChannel> m_SpeechChannel;
			WRL::ComPtr<IHolographicStreamerDataChannel> m_SpatialMappingChannel;
			WRL::ComPtr<IHolographicStreamerDataChannel> m_SavedSpatialAnchorsChannel;

			WRL::ComPtr<IPerceptionSimulationFrame> m_spFrame;

			PeriodicCallbackWhileEventSignalled m_SpatialMappingUpdateCallback;
			CallbackWhenEventSignalled m_SavedSpatialAnchorsUpdateCallback;
		};

	}
}
