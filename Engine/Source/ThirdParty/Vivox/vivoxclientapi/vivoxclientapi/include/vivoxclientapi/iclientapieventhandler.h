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

#include "vivoxclientapi/accountname.h"
#include "vivoxclientapi/util.h"
#include "vivoxclientapi/uri.h"
#include "vivoxclientapi/audiodeviceid.h"
#include "vivoxclientapi/audiodevicepolicy.h"
#include "vivoxclientapi/channeltransmissionpolicy.h"
#include "VxcEvents.h"
#include "VxcResponses.h"
#if defined(__APPLE__) || defined(__ANDROID__)
#include <stdlib.h>
#endif

namespace VivoxClientApi {
    ///
    /// The game application must have a class that implements this interface. This interface has two roles:
    ///
    /// 1. It provides an interface to get some basic system services that are likely to be particular to the game application environment.
    /// 2. It provides an interface that the ClientConnection class can use to notify the application of important state changes.
    ///
    class IClientApiEventHandler {
    public:
        ///
        /// This enumerator provides the supported logging levels used in Vivox client software.
        /// Retail builds should typically use a log level of LogLevelWarning.
        /// Non Retail builds may elect to use a more verbose logging level.
        ///
        typedef enum {
            LogLevelNone = -1,
            /// log only failures
            LogLevelError,   
            /// log things worthy of investigation if an issue is reported.
            LogLevelWarning, 
            /// log all SDK api requests, responses, and events, and all HTTP and SIP traffic
            LogLevelInfo,    
            LogLevelDebug,
            LogLevelTrace
        } LogLevel;

        ///
        /// When a participant leaves a channel, it could be for a number of reasons.
        ///
        typedef enum {
            /// normal conditions
            ReasonLeft = 0,    
            /// the participant left because they lost their network connection to the service
            ReasonNetwork = 1, 
            /// the participant was kicked from the channel
            ReasonKicked = 2,  
            /// the participant was banned from the channel
            ReasonBanned = 3,  
        } ParticipantLeftReason;

        /// Basic System Services

        ///
        /// This method runs the passed in function on the UI thread.
        /// It is essential that the application implement this correctly, otherwise, code that was not designed to
        /// run on multiple threads would run on multiple threads.
        ///
        /// This method should *not* be synchronous, and is called from multiple threads.
        ///
        /// There is a sample implementation of this in the provided WindowsMarshaller class that will invoke
        /// this method on a UI thread of a Windows application.
        ///
        /// @param pf_func - the function pointer to invoke
        /// @param arg0 - an argument to pass to the pf_func invocation
        ///
        virtual void InvokeOnUIThread(void (pf_func)(void *arg0), void *arg0) = 0;

        ///
        /// This method can be called from multiple threads when Vivox client software wants to write a log message.
        /// The game application can implement this method to integrate Vivox client logging into their own logging subsystem.
        /// This may be called from realtime threads, and it's critical that this does not block - otherwise, audio may be affected.
        ///
        /// @param level - the level associated with the message
        /// @param nativeMillisecondsSinceEpoch - the time when the log message was issued. Use FileTimeToSystemTime to convert to date/time elements.
        /// @param threadId - the threadId on which the message was issued.
        /// @param logMessage - the content to log.
        ///
        virtual void onLogStatementEmitted(LogLevel level, long long nativeMillisecondsSinceEpoch, long threadId, const char *logMessage) = 0;

        ///
        /// This method is called when there is an internal inconsistency in the Vivox client software.
        ///
        /// This method should log the message in a retail build, and somehow more obviously cue the user that the voice
        /// subsystem may be having issues in non-retail builds.
        ///
        /// @param filename - the filename where the consistency check failed
        /// @line - the line in the file where the consistency check failed
        /// @message - the check that failed
        ///
        virtual void onAssert(const char *filename, int line, const char *message) = 0;

        ///
        /// !!!!! Note: the remaining methods are always called on the user interface thread.
        ///

        /// Service Connection

        ///
        /// This method is called when the ClientConnection class has successfully connected to the Vivox provisioning server.
        ///
        /// @param server - the server that has been connected to
        ///
        virtual void onConnectCompleted(const Uri &server) = 0;

        ///
        /// onConnectFailed()
        ///
        /// This method is called when there is a failure to connect to the Vivox provisioning server.
        ///
        /// @param server - the server failed to be connected to
        /// @param status - the status code associated with the failure. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onConnectFailed(const Uri &server, const VCSStatus &status) = 0;

        ///
        /// This method is called when there is a disconnection from the Vivox provisioning server
        ///
        /// @param server - the server that was disconnected from
        /// @param status - the status code associated with the disconnection. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onDisconnected(const Uri &server, const VCSStatus &status) = 0;

        /// Logging/Logging out

        ///
        /// This method is called when a user successfully logs into the Vivox network.
        ///
        /// @param accountName - the account name of the logged in user
        ///
        virtual void onLoginCompleted(const AccountName &accountName) = 0;

        ///
        /// This method is called when a user fails to login because of invalid credentials.
        /// In a game environment, this should never be called, since the game server maintains the
        /// Vivox account name/password pair.
        ///
        /// @param accountName - the account name for that user
        ///
        virtual void onInvalidLoginCredentials(const AccountName &accountName) = 0;

        ///
        /// This method is called when there is failure to login for a reason other than
        /// invalid account credentials. In retail builds, this most often is due to transient network
        /// conditions or blocked UDP ports. In developer builds, this might indicated a defect
        /// in the game integration.
        ///
        /// @param accountName - the account name for that user
        /// @param status - the status code associated with the failure. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onLoginFailed(const AccountName &accountName, const VCSStatus &status) = 0;

        ///
        /// This method is called when a user has logged out. Game applications typically do not need to handle this event
        /// as logging out from Vivox will coincide with logging out from the game.
        ///
        /// @param accountName - the account name of the logged in user
        ///
        virtual void onLogoutCompleted(const AccountName &accountName) = 0;

        ///
        /// This method is called when a user fails to logout. This rarely occurs in the field
        /// and is most likely an indication of a programming error.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param status - the status code associated with the failure. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onLogoutFailed(const AccountName &accountName, const VCSStatus &status) = 0;

        /// Joining/Leaving Channels

        ///
        /// This function is called when the logged in user joins a channel.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that was joined
        ///
        virtual void onChannelJoined(const AccountName &accountName, const Uri &channelUri) = 0;

        ///
        /// This function is called when the logged in user tries to join a channel, but fails due to
        /// invalid channel password. This should not occur in a typical game integration, where the game
        /// server controls the channel passwords.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that was joined
        ///
        virtual void onInvalidChannelCredentials(const AccountName &accountName, const Uri &channelUri) = 0;

        ///
        /// This function is called when the channel join fails for some other reason besides invalid credentials.
        /// This could be a network issue or a non-existent channel URI, or a channel that has an ACL that prevents
        /// the user from accessing the channel.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that was attempted to be joined
        /// @param status - the status code associated with the failure. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onChannelJoinFailed(const AccountName &accountName, const Uri &channelUri, const VCSStatus &status) = 0;

        ///
        /// This function is called when a channel is exited unexpectedly. This most often
        /// is due to some sort of network error.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that was attempted to be joined
        /// @param status - the status code associated with the unexpected exit. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onChannelExited(const AccountName &accountName, const Uri &channelUri, const VCSStatus &reasonCode) = 0;

        ///
        /// This function is called when call statistics have been updated. This is for diagnostic purposes only.
        /// Game applications do not need to do anything specific with this call.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param stats - the call statistics
        /// @param isFinal - if true, the statistics are the final statistics for the call.
        ///
        virtual void onCallStatsUpdated(const AccountName &accountName, vx_call_stats_t &stats, bool isFinal) = 0;

        /// Roster list maintenance

        ///
        /// This function is called when a participant is added to a channel. This function is only called
        /// if the logged in user is in the channel.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel to which the participant was added
        /// @param participantUri - the participant that was added
        /// @param isLoggedInUser - whether or not the participant is this logged in user. This is useful if UI elements for the logged in user
        /// are separate from the roster list.
        ///
        virtual void onParticipantAdded(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser) = 0;

        ///
        /// This function is called when a participant has left a channel. This function is only called
        /// if the logged in user is in the channel.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel from which the participant left
        /// @param participantUri - the participant that was added
        /// @param isLoggedInUser - whether or not the participant is this logged in user. This is useful if UI elements for the logged in user
        /// are separate from the roster list.
        ///
        virtual void onParticipantLeft(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, ParticipantLeftReason reason) = 0;

        ///
        /// This function is called when a participant is added to a channel. This function is only called
        /// if the logged in user is in the channel.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that the participant is in
        /// @param participantUri - the participant that was updated
        /// @param isLoggedInUser - whether or not the participant is this logged in user. This is useful if UI elements for the logged in user
        /// are separate from the roster list.
        /// @param speaking - an indication whether or not the user is speaking. This is used to give the user a visual cue about
        /// which game character is speaking.
        /// @param vuMeterEnergy - a value from 0-1 indicating the relative energy in the speech. This can be used to build a VU meter. Most game
        /// applications do not need to do this.
        /// @param isMutedForAll - true if this user has been muted at the server for all other users.
        ///
        virtual void onParticipantUpdated(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, bool speaking, double vuMeterEnergy, bool isMutedForAll) = 0;

        /// Moderation

        ///
        /// This function is called when the logged in user has completed kicking a participant from a channel successfully.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that the participant is in
        /// @param participantUri - the participant that was kicked
        ///
        virtual void onParticipantKickedCompleted(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri) = 0;

        ///
        /// This function is called when the logged in user has failed to kick a participant from a channel successfully.
        /// This most often occurs because the logged in user was not an owner or a moderator.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param channelUri - the channel that the participant is in
        /// @param participantUri - the participant that was kicked
        /// @param status - the status code associated with the failure. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onParticipantKickFailed(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, const VCSStatus &status) = 0;

        /// Audio Injection

        ///
        /// This function is called when an audio file begins to play into all the currently joined channels.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param filename - the filename that was requested to be played
        ///
        virtual void onStartPlayFileIntoChannels(const AccountName &accountName, const char *filename) = 0;

        ///
        /// This function is called a request to play a file into a channel fails.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param filename - the filename that was requested to be played
        /// @param status - the status code associated with the failure. This can be translated to a string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        virtual void onStartPlayFileIntoChannelsFailed(const AccountName &accountName, const char *filename, const VCSStatus &status) = 0;

        ///
        /// This function is called when an audio file that is playing into the currently joined channels
        /// either stops because it reached the end of the file, or because the game application has
        /// explicitly stopped it.
        ///
        /// @param accountName - the account name of the logged in user
        /// @param filename - the filename that was requested to be played
        ///
        virtual void onPlayFileIntoChannelsStopped(const AccountName &accountName, const char *filename) = 0;

        /// Audio Input Device Management

        ///
        /// This function is called when either a new audio input or new audio output device is available.
        /// The application should refresh the audio setup user interface when it receives this callback.
        ///
        virtual void onAvailableAudioDevicesChanged() = 0;

        /// 
        /// This function is called when the operating system selected audio input device changes.
        ///
        /// @param deviceId - the device Id that is now being used
        ///
        virtual void onOperatingSystemChosenAudioInputDeviceChanged(const AudioDeviceId &deviceId) = 0;
        
        ///
		/// This function is called when SetApplicationChosenAudioInputDevice succeeds.
		/// 
		/// @param deviceId - the deviceId that is now being used for audio input.
		///
		virtual void onSetApplicationChosenAudioInputDeviceCompleted(const AudioDeviceId &deviceId) = 0;
		

        ///
        /// This function is called when SetApplicationChosenAudioInputDevice() fails.
        ///
        /// This generally does not fail, with the exception that there is either an application error,
        /// or a race condition where a device was unplugged while SetApplicationChosenAudioInputDevice() was in the middle of 
        /// processing.
        ///
        /// @param deviceId - the device passed to SetApplicationChosenAudioInputDevice().
        /// @status - an error code indicating the failure.
        ///
		virtual void onSetApplicationChosenAudioInputDeviceFailed(const AudioDeviceId &deviceId, const VCSStatus &status) = 0;


        /// Audio Output Device Management

        /// 
        /// This function is called when the operating system selected audio output device changes.
        ///
        /// @param deviceId - the device Id that is now being used
        ///
        virtual void onOperatingSystemChosenAudioOutputDeviceChanged(const AudioDeviceId &deviceId) = 0;

        ///
        /// This function is called when SetApplicationChosenAudioOutputDevice succeeds.
        /// 
        /// @param deviceId - the deviceId that is now being used for audio input.
        ///
        virtual void onSetApplicationChosenAudioOutputDeviceCompleted(const AudioDeviceId &deviceId) = 0;

        ///
        /// This function is called when SetApplicationChosenAudioOutputDevice() fails.
        ///
        /// This generally does not fail, with the exception that there is either an application error,
        /// or a race condition where a device was unplugged while SetApplicationChosenAudioOutputDevice() was in the middle of 
        /// processing.
        ///
        /// @param deviceId - the device passed to SetApplicationChosenAudioOutputDevice().
        /// @status - an error code indicating the failure.
        ///
        virtual void onSetApplicationChosenAudioOutputDeviceFailed(const AudioDeviceId &deviceId, const VCSStatus &status) = 0;


        /// Volume Controls

        /// 
        /// This function is called when a change to the output volume for a specific channel succeeds.
        /// Per Session/Channel output volumes should only be used for multi-channel implementations.
        ///
        /// @accountName - the name of the account
        /// @channelUri - the URI of the channel whose volume was adjusted
        /// @volume - the current volume for that channel
        ///
        virtual void onSetChannelAudioOutputDeviceVolumeCompleted(const AccountName &accountName, const Uri &channelUri, int volume) = 0;

        /// 
        /// This function is called when a change to the output volume for a specific channel fails.
        /// Per Session/Channel output volumes should only be used for multi-channel implementations.
        ///
        /// @accountName - the name of the account
        /// @channelUri - the URI of the channel whose volume was adjusted
        /// @volume - the current volume for that channel
        /// @status - the status code associated with the failure.
        ///
        virtual void onSetChannelAudioOutputDeviceVolumeFailed(const AccountName &accountName, const Uri &channelUri, int volume, const VCSStatus &status) = 0;

        /// 
        /// This function is called when a change to the output volume for a user, in a specific channel, succeeds.
        /// Per Session/Channel output volumes should only be used for multi-channel implementations.
        ///
        /// @accountName - the name of the account
        /// @targetUser - the URI of the user whose volume was adjusted
        /// @channelUri - the URI of the channel that the targetUser is in.
        /// @volume - the current volume for that user in that channel.
        ///
        virtual void onSetParticipantAudioOutputDeviceVolumeForMeCompleted(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, int volume) = 0;

        /// 
        /// This function is called when a change to the output volume for a user, in a specific channel, fails.
        /// Per Session/Channel output volumes should only be used for multi-channel implementations.
        ///
        /// @accountName - the name of the account
        /// @targetUser - the URI of the user whose volume was adjusted
        /// @channelUri - the URI of the channel that the targetUser is in.
        /// @volume - the current volume for that user in that channel.
        /// @status - the status code associated with the failure.
        ///
        virtual void onSetParticipantAudioOutputDeviceVolumeForMeFailed(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, int volume, const VCSStatus &status) = 0;

        /// Muting Controls

        ///
        /// This function is called when SetParticipantMutedForAll() completes successfully.
        /// 
        /// @param accountName - the currently logged in user
        /// @param target - the user that was muted or unmuted
        /// @param channelUri - the channel that the user was in
        /// @param muted - true to mute, false to unmute
        ///
        virtual void onSetParticipantMutedForAllCompleted(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted) = 0;

        ///
        /// This function is called when SetParticipantMutedForAll() fails
        /// 
        /// @param accountName - the currently logged in user
        /// @param target - the user that was muted or unmuted
        /// @param channelUri - the channel that the user was in
        /// @param muted - true to mute, false to unmute
        /// @param status - a status code describing the failure
        /// 
        virtual void onSetParticipantMutedForAllFailed(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted, const VCSStatus &status) = 0;

        ///
        /// This function is called when SetParticipantMutedForMe() completes successfully.
        /// 
        /// @param accountName - the currently logged in user
        /// @param target - the user that was muted or unmuted
        /// @param channelUri - the channel that the user was in
        /// @param muted - true to mute, false to unmute
        ///
        virtual void onSetParticipantMutedForMeCompleted(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted) = 0;

        ///
        /// This function is called when SetParticipantMutedForMe() fails
        /// 
        /// @param accountName - the currently logged in user
        /// @param target - the user that was muted or unmuted
        /// @param channelUri - the channel that the user was in
        /// @param muted - true to mute, false to unmute
        /// @param status - a status code describing the failure
        /// 
        virtual void onSetParticipantMutedForMeFailed(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted, const VCSStatus &status) = 0;

        /// Channel Transmission

        ///
        /// This function is called when SetTransmissionToSpecificChannel succeeds.
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the channel URI to transmit into, solely
        ///
        virtual void onSetChannelTransmissionToSpecificChannelCompleted(const AccountName &accountName, const Uri &channelUri) = 0;

        ///
        /// This function is called when SetTransmissionToSpecificChannel fails.
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the channel URI to transmit into, solely
        /// @param status - a status code describing the failure
        ///
        virtual void onSetChannelTransmissionToSpecificChannelFailed(const AccountName &accountName, const Uri &channelUri, const VCSStatus &status) = 0;

        ///
        /// This function is called when SetTransmissionToAll succeeds.
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the channel URI to transmit into, solely
        ///
        virtual void onSetChannelTransmissionToAllCompleted(const AccountName &accountName) = 0;

        ///
        /// This function is called when SetTransmissionToAll fails.
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the channel URI to transmit into, solely
        /// @param status - a status code describing the failure
        ///
        virtual void onSetChannelTransmissionToAllFailed(const AccountName &accountName, const VCSStatus &status) = 0;

        ///
        /// This function is called when SetTransmissionToNone succeeds.
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the channel URI to transmit into, solely
        ///
        virtual void onSetChannelTransmissionToNoneCompleted(const AccountName &accountName) = 0;

        ///
        /// This function is called when SetTransmissionToNone fails.
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the channel URI to transmit into, solely
        /// @param status - a status code describing the failure
        ///
        virtual void onSetChannelTransmissionToNoneFailed(const AccountName &accountName, const VCSStatus &status) = 0;
        
        ///
        /// This function is called when the recorded audio for the input audio device test has completed playing.
        ///
        virtual void onAudioInputDeviceTestPlaybackCompleted() = 0;

		/// Audio capture. The following 5 functions are called from the audio processing threads. No blocking operations should occur in them

		///
		/// This function is called when an audio processing unit is started
		///
		virtual void onAudioUnitStarted(const Uri &initialTargetUri) = 0;

		///
		/// This function is called when an audio processing unit is stopped
		///
		virtual void onAudioUnitStopped(const Uri &initialTargetUri) = 0;

		///
		/// This function is called right after audio was read from the capture device
		///
		/// @param pcmFrames - pcm data, interleaved if channelsPerFrame is > 1
		/// @param pcmFrameCount - the number of frames available
		/// @param audioFrameRate - the sample rate
		/// @param channelsPerFrame - the number of channels per frame
		///
		virtual void onAudioUnitAfterCaptureAudioRead(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame) = 0;

		///
		/// This function is when an audio processing unit is about to send captured audio to the network
		///
		/// @param pcmFrames - pcm data, interleaved if channelsPerFrame is > 1
		/// @param pcmFrameCount - the number of frames available
		/// @param audioFrameRate - the sample rate
		/// @param channelsPerFrame - the number of channels per frame
		/// @param speaking - an indication whether or not the user is speaking
		///
		virtual void onAudioUnitBeforeCaptureAudioSent(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, bool speaking) = 0;

		///
		/// This function is called when an audio processing unit is about to write received audio to the render device
		///
		/// @param pcmFrames - pcm data, interleaved if channelsPerFrame is > 1
		/// @param pcmFrameCount - the number of frames available
		/// @param audioFrameRate - the sample rate
		/// @param channelsPerFrame - the number of channels per frame
		/// @param silence - an indication whether or not any users are speaking
		///
		virtual void onAudioUnitBeforeRecvAudioRendered(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, bool silence) = 0;
    };
}
