#pragma once
/* Copyright (c) 2014-2018 by Mercer Road Corp
*
* Permission to use, copy, modify or distribute this software in binary or source form
* for any purpose is allowed only under explicit prior consent in writing from Mercer Road Corp
*
* THE SOFTWARE IS PROVIDED "AS IS" AND MERCER ROAD CORP DISCLAIMS
* ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL MERCER ROAD CORP
* BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
* DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
* PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
* ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
* SOFTWARE.
*/

#ifdef WIN32
#include <Windows.h>
#endif
#include "vivoxclientapi/iclientapieventhandler.h"
#include "vivoxclientapi/vivoxclientsdk.h"

namespace VivoxClientApi {
    ///
    /// This class will log all events to via the DebugClientApiEventHandler::WriteStatus() method.
    /// The default implementation of WriteStatus() will log using OutputDebugString().
    ///
    /// For developers that wish to get started quickly, and not implement every single IClientApiEventHandler method
    /// immediately, this can be a good class to use as a base class.
    ///
    class DebugClientApiEventHandler :
        public IClientApiEventHandler
    {
    public:
        DebugClientApiEventHandler(ClientConnection *clientConnection = NULL);
        virtual ~DebugClientApiEventHandler();

        static void SetAbortEnabled(bool value);

        void SetClientConnection(ClientConnection *clientConnection) {
            m_clientConnection = clientConnection;
        }
        /// Override to redirect output
        virtual void WriteStatus(const char *msg) const;

        /// IClientApiEventHandler overrides

        /// Utility
        virtual const char *ParticipantLeftString(ParticipantLeftReason reason);

        /// Basic System Services
        virtual void onLogStatementEmitted(LogLevel level, long long nativeMillisecondsSinceEpoch, long threadId, const char *logMessage);
        virtual void onAssert(const char *filename, int line, const char *message);

        /// Service Connection
        virtual void onConnectCompleted(const Uri &server);
        virtual void onConnectFailed(const Uri &server, const VCSStatus &status);
        virtual void onDisconnected(const Uri &server, const VCSStatus &status);

        /// Logging/Logging out
        virtual void onLoginCompleted(const AccountName &accountName);
        virtual void onInvalidLoginCredentials(const AccountName &accountName);
        virtual void onLoginFailed(const AccountName &accountName, const VCSStatus &status);
        virtual void onLogoutCompleted(const AccountName &accountName);
        virtual void onLogoutFailed(const AccountName &accountName, const VCSStatus &status);

        /// Getting into/out of channel
        virtual void onChannelJoined(const AccountName &accountName, const Uri &channelUri);
        virtual void onInvalidChannelCredentials(const AccountName &accountName, const Uri &channelUri);
        virtual void onChannelJoinFailed(const AccountName &accountName, const Uri &channelUri, const VCSStatus &status);
        virtual void onChannelExited(const AccountName &accountName, const Uri &channelUri, const VCSStatus& reasonCode);
        virtual void onCallStatsUpdated(const AccountName &accountName, vx_call_stats_t &stats, bool isFinal);

        /// Roster list maintenance
        virtual void onParticipantAdded(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser);
        virtual void onParticipantLeft(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, ParticipantLeftReason reason);
        virtual void onParticipantUpdated(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, bool speaking, double vuMeterEnergy, bool mutedForAll);

        /// Moderation
        virtual void onParticipantKickedCompleted(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri);
        virtual void onParticipantKickFailed(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, const VCSStatus &status);

        /// Audio Injection
        virtual void onStartPlayFileIntoChannels(const AccountName &accountName, const char *filename);
        virtual void onStartPlayFileIntoChannelsFailed(const AccountName &accountName, const char *filename, const VCSStatus &status);
        virtual void onPlayFileIntoChannelsStopped(const AccountName &accountName, const char *filename);

        virtual void onAvailableAudioDevicesChanged();

        virtual void onOperatingSystemChosenAudioInputDeviceChanged(const AudioDeviceId &deviceId);
        virtual void onSetApplicationChosenAudioInputDeviceCompleted(const AudioDeviceId &deviceId);
        virtual void onSetApplicationChosenAudioInputDeviceFailed(const AudioDeviceId &deviceId, const VCSStatus &status);

        virtual void onOperatingSystemChosenAudioOutputDeviceChanged(const AudioDeviceId &deviceId);
        virtual void onSetApplicationChosenAudioOutputDeviceCompleted(const AudioDeviceId &deviceId);
        virtual void onSetApplicationChosenAudioOutputDeviceFailed(const AudioDeviceId &deviceId, const VCSStatus &status);
        virtual void onAudioInputDeviceTestPlaybackCompleted();

        /// Volume Controls
        virtual void onSetChannelAudioOutputDeviceVolumeCompleted(const AccountName &accountName, const Uri &channelUri, int volume);
        virtual void onSetChannelAudioOutputDeviceVolumeFailed(const AccountName &accountName, const Uri &channelUri, int volume, const VCSStatus &status);
        virtual void onSetParticipantAudioOutputDeviceVolumeForMeCompleted(const AccountName &accountName, const Uri &target, const Uri &channelUri, int volume);
        virtual void onSetParticipantAudioOutputDeviceVolumeForMeFailed(const AccountName &accountName, const Uri &target, const Uri &channelUri, int volume, const VCSStatus &status);

        /// Muting Controls
        virtual void onSetParticipantMutedForAllCompleted(const AccountName &accountName, const Uri &targetUri, const Uri &channelUri, bool muted);
        virtual void onSetParticipantMutedForAllFailed(const AccountName &accountName, const Uri &targetUri, const Uri &channelUri, bool muted, const VCSStatus &status);
        virtual void onSetParticipantMutedForMeCompleted(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted);
        virtual void onSetParticipantMutedForMeFailed(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted, const VCSStatus &status);

        /// Channel Transmission
        virtual void onSetChannelTransmissionToSpecificChannelCompleted(const AccountName &accountName, const Uri &channelUri);
        virtual void onSetChannelTransmissionToSpecificChannelFailed(const AccountName &accountName, const Uri &channelUri, const VCSStatus &status);
        virtual void onSetChannelTransmissionToAllCompleted(const AccountName &accountName);
        virtual void onSetChannelTransmissionToAllFailed(const AccountName &accountName, const VCSStatus &status);
        virtual void onSetChannelTransmissionToNoneCompleted(const AccountName &accountName);
        virtual void onSetChannelTransmissionToNoneFailed(const AccountName &accountName, const VCSStatus &status);

		/// Audio capture
		virtual void onAudioUnitStarted(const Uri &initialTargetUri);
		virtual void onAudioUnitStopped(const Uri &initialTargetUri);
		virtual void onAudioUnitAfterCaptureAudioRead(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame);
		virtual void onAudioUnitBeforeCaptureAudioSent(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, bool speaking);
		virtual void onAudioUnitBeforeRecvAudioRendered(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, bool silence);
    private:
        ClientConnection *m_clientConnection;
    };
}
