//
// Copyright (C) Microsoft. All rights reserved.
//

#pragma once
#include <windows.h>
#include <d3d11.h>

namespace Microsoft {
namespace Holographic {

    enum class HolographicStreamerConnectionState : unsigned int
    {
        Disconnected,
        Connecting,
        Connected
    };

    enum class HolographicStreamerConnectionFailureReason : unsigned int
    {
        None,
        Unknown,
        Unreachable,
        HandshakeFailed,
        ProtocolVersionMismatch,
        ConnectionLost
    };

    //!\brief Video composition mode
    //! Defines how the input texture be composited for encoding.  Supported values are:
    //!     Mono - The input texture passed to SendFrame has an ArraySize of 1.  The ReceiveFrame callback will have 1 composition operation.
    //!     Stereo - The input texture passed to SendFrame has an ArraySize of 2 (one texture per eye).  The ReceiveFrame callback will have 2 composition operations (one per eye, in the same order).
    enum class HolographicStreamerVideoCompositionMode : unsigned int
    {
        Mono,
        Stereo
    };

    //!\brief Video composition operation
    //! Defines how images should be extracted from the composited texture passed to ReceiveFrame.
    struct HolographicStreamerVideoCompositionOperation
    {
        ULONG subresourceIndex;     //!< the subresource index
        RECT region;                //!< the the bounding region of the subresource to extract (in pixels)
    };

    //!\brief Application data channel callback interface
    DECLARE_INTERFACE_IID_(IHolographicStreamerDataChannelCallback, IUnknown, "9aef0b24-d232-4ce3-ab18-312d2d3827ce")
    {
        //!\brief Called when data is received from the remote endpoint
        STDMETHOD(DataReceived)(_In_ UINT32 dataLength, _In_reads_(dataLength) byte* data) = 0;
    };

    //!\brief Application data channel
    DECLARE_INTERFACE_IID_(IHolographicStreamerDataChannel, IUnknown, "66b1e9f7-5ece-47f5-b783-91dacaf06188")
    {
        //!\brief Set the data channel callback to receive data from the remote endpoint
        STDMETHOD(SetCallback)(
            _In_ IHolographicStreamerDataChannelCallback* callback) = 0;    //!< channel callbacks

        //!\brief Send data to the remote endpoint
        //!\param guaranteedDelivery - Whether the data must be delivered, in order, to the remote endpoint
        STDMETHOD(SendData)(
            _In_ UINT32 dataLength, 
            _In_reads_(dataLength) const byte* data,
            _In_ bool guaranteedDelivery) = 0;
    };

    //!\brief Holographic streamer client callback interface
    DECLARE_INTERFACE_IID_(IHolographicStreamerClientCallback, IUnknown, "e08ea2c2-1118-41c4-9654-6b97808e3e6e")
    {
        //!\brief Called when connection has been established
        STDMETHOD(Connected)() = 0;

        //!\brief Called when the connection has been terminated
        STDMETHOD(Disconnected)(_In_ HolographicStreamerConnectionFailureReason failureReason) = 0;

        //!\brief Called when a new application data channel has been established
        STDMETHOD(DataChannelCreated)(_In_ IHolographicStreamerDataChannel* channel, _In_ UINT8 channelId) = 0;
    };

    //!\brief Holographic streamer client
    DECLARE_INTERFACE_IID_(IHolographicStreamerClient, IUnknown, "96a96992-1d43-4ced-a87e-687c848c9e29")
    {
        //!\brief Connect to client at the given address
        STDMETHOD(Connect)(_In_z_ PCWSTR hostname, _In_ UINT32 port) = 0;

        //!\brief Terminate the active client connection
        STDMETHOD(Disconnect)() = 0;

        //!\brief Get the current connection state
        STDMETHOD(GetConnectionState)(_Out_ HolographicStreamerConnectionState* state) = 0;

        //!\brief Get the current session id
        //! A new sesson id is generated when a new connection attempt is initiated
        STDMETHOD(GetSessionId)(_Out_ GUID* sessionId) = 0;

        //!\brief Send video data to the client endpoint
        //! Texture must remain valid for the duration of this call
        //! Returns failure if video is not enabled
        STDMETHOD(SendFrame)(
            _In_ ID3D11Texture2D* texture, 
            _In_ UINT32 metadataLength, 
            _In_reads_(metadataLength) const byte* metadata) = 0;

        //!\brief Create an specific data channel
        //!\param channelId - The channel id number.
        STDMETHOD(CreateDataChannel)(
            _In_ UINT8 channelId) = 0;                                          //!< opaque channel id

        //!\brief Set the maximum bitrate
        STDMETHOD(SetMaxVideoBitrate)(_In_ UINT32 maxBitrateKbps) = 0;

        //!\brief Set video frame dimensions
        //!\param width - input texture width in pixels
        //!\param height - input texture height in pixels
        //!\param mode - defines how the input texture will be composited
        STDMETHOD(SetVideoFrameAttributes)(_In_ UINT32 width, _In_ UINT32 height, _In_ HolographicStreamerVideoCompositionMode mode) = 0;

        //!\brief Enable/disable video channel
        STDMETHOD(SetEnableVideo)(_In_ BOOL enable) = 0;

        //!\brief Enable/disable audio channel
        STDMETHOD(SetEnableAudio)(_In_ BOOL enable) = 0;

        //!\brief Get the ideal video FPS
        //! Returns the ideal rate at which the app should call SendFrame for best stream quality
        STDMETHOD(GetIdealVideoFPS)(_Out_ UINT32* fps) = 0;
    };

    //!\brief Holographic streamer client callback interface
    DECLARE_INTERFACE_IID_(IHolographicStreamerServerCallback, IUnknown, "092f5d15-bb0f-424c-ab45-6aa2b8d59ae6")
    {
        //!\brief Called when connection has been established
        STDMETHOD(Connected)() = 0;

        //!\brief Called when the connection has been terminated
        STDMETHOD(Disconnected)(
            _In_ HolographicStreamerConnectionFailureReason failureReason) = 0;

        //!\brief Called when a new video frame has been received
        //!\param texture - output texture
        //!\param metadataLengt - length of frame metadata
        //!\param metadata - frame metadata
        //!\param sample - associated Media Foundation sample.  This object must remain valid while the output texture is in use.
        //!\param compositionOperationCount - number of composition operations
        //!\param compositionOperations - composition operations
        STDMETHOD(ReceiveFrame)(
            _In_ ID3D11Texture2D* texture,
            _In_ UINT32 metadataLength, 
            _In_reads_(metadataLength) const byte* metadata,
            _In_ IUnknown* sample,
            _In_ UINT32 compositionOperationCount,
            _In_reads_(compositionOperationCount) const HolographicStreamerVideoCompositionOperation* compositionOperations) = 0;

        //!\brief Called when a new application data channel has been established
        STDMETHOD(DataChannelCreated)(
            _In_ IHolographicStreamerDataChannel* channel,
            _In_ UINT8 channelId) = 0;
    };

    //!\brief Holographic streamer client
    DECLARE_INTERFACE_IID_(IHolographicStreamerServer, IUnknown, "93ba5f65-4001-4bf9-8a6e-40359807ed84")
    {
        //!\brief Listen for incoming connections
        STDMETHOD(Listen)(_In_ UINT32 port) = 0;

        //!\brief Get the current connection state
        STDMETHOD(GetConnectionState)(
            _Out_ HolographicStreamerConnectionState* state) = 0;

        //!\brief Get the current session id
        //! A new sesson id is generated when a new connection attempt is initiated
        STDMETHOD(GetSessionId)(_Out_ GUID* sessionId) = 0;

        //!\brief Create an specific data channel
        //!\param channelId - The channel id number.  The Perception Monitoring API reserves channels 0-63, 64-255 are available for user channels.
        STDMETHOD(CreateDataChannel)(
            _In_ UINT8 channelId) = 0;                                          //!< opaque channel id
    };

}}

//!\brief Create holographic streamer client
extern "C" HRESULT __stdcall CreateHolographicStreamerClient(
    _In_ Microsoft::Holographic::IHolographicStreamerClientCallback* callback,  //!< client callbacks
    _In_ ID3D11Device* device,                                                  //!< D3D device
    _In_ REFIID riid,
    _Outptr_ void** ppv);

//!\brief Create holographic streamer server
extern "C" HRESULT __stdcall CreateHolographicStreamerServer(
    _In_ Microsoft::Holographic::IHolographicStreamerServerCallback* callback,  //!< server callbacks
    _In_ ID3D11Device* device,                                                  //!< D3D device
    _In_ REFIID riid,
    _Outptr_ void** ppv);