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

#include "vivoxclientapi/util.h"
#include "vivoxclientapi/uri.h"
#include "vivoxclientapi/accountname.h"
#include "vivoxclientapi/iclientapieventhandler.h"
#include <set>
#include <vector>

///
/// All entities in the vivoxclientapi library are in the VivoxClientApi namespace.
///
namespace VivoxClientApi {
    class ClientConnectionImpl;

    ///
    /// The recommended minimum volume for all volume related calls
    ///
#define VIVOX_MIN_VOL 40
    ///
    /// The recommended maximum volume for all volume related calls
    ///
#define VIVOX_MAX_VOL 60

	struct Vector
	{
		double x, y, z;
	};
    
    ///
    /// The ClientConnection class is the main class that a game application will use when accessing Vivox services.
    ///
    /// Each game application should have one and only one instance of these classes. It is up the game to implement the singleton semantics in a way that
    /// is appropriate in the games environment. This instance should be created and accessed ONLY ON THE UI THREAD.
    ///
    /// All methods are non-blocking, except for Uninitialize(), which may take a few seconds as it cleans up network connections and threads.
    ///
    /// The game application will call methods on this class, and receive notifications about the asynchronous completion of these methods through
    /// an a callback interface of type IClientApiEventHandler, which is passed to the instance of this class in the Initialize() method.
    ///
    class ClientConnection
    {
    public:
        ///
        /// The game application should create one and only one instance of this class.
        /// This instance should be created and accessed only on the game application's user interface thread.
        ///
        ClientConnection();

        virtual ~ClientConnection();

        ///
        /// Before any other methods can be called, the application must call Initialize().
        ///
        /// @param app - a pointer to a callback interface. The game application implements this interface to receive notifications from the ClientConnection instance.
        /// @param logLevel - the verbosity of the logging that will occur. Requests to write messages to a log are forwarded to the application through the IClientApiEventHandler
        /// interface.
        /// @param multiChannel - if this is false, a request to join a channel will cause the user to leave all other channels. If true, the user's request to join
        /// a channel will not affect existing joined channels. Depending on game play, the game might want to set this to either true or false.
        /// @param multiLogin - if this is true, a request to login one user will force any other users that are logged in by this game application instance to be logged out.
        /// It would be highly unusual for a game application to set this to true - it is used mostly for internal testing.
		/// @param configHints - if set, must be initialized using vx_get_default_config3 before filling in any additional settings. Vivox sdk headers must match the one used to
		/// compile the vivoxclientapi
		/// @param configSize - size of the vx_sdk_config_t struct
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus Initialize(IClientApiEventHandler *app, IClientApiEventHandler::LogLevel logLevel, bool multiChannel = false, bool multiLogin = false, vx_sdk_config_t *configHints = NULL, size_t configSize = 0);

        ///
        /// Before exiting, the game application must call Uninitialize(). This will gracefully cleanup any resources that have been allocated by Vivox client software.
        ///
        void Uninitialize();

#ifdef _XBOX
        VCSStatus CreateCaptureDevice(vxa_apcd *capture_device, int *apcd_id);
        VCSStatus DestroyCaptureDevice(int apcd_id);
        VCSStatus CreateRenderDevice(vxa_aprd *render_device, int *aprd_id);
        VCSStatus DestroyRenderDevice(int aprd_id);
#endif
        ///
        /// The application must call this method before calling Login. This will connect the game application to a Vivox provisioning server
        /// that maintains the list of Vivox accounts and Vivox channels.
        ///
        /// If the game application is not already connected to that server, the application will be notified of the completion of this operation
        /// through one of the following callbacks:
        ///
        /// IClientApiEventHandler::onConnectCompleted()
        /// IClientApiEventHandler::onConnectFailed().
        ///
        /// It's not necessary to wait to call Login() until receiving a call to IClientApiEventHandler::onConnectCompleted().
        ///
        /// @param server - the Uri of the Vivox provisioning server e.g. "https://www.vd1.vivox.com/api2/"
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus Connect(const Uri &server);

        ///
        /// The application calls this method when they wish to disconnect from the Vivox service. This will log out any logged in user (and in the process
        /// remove them from any channels). If the application is not already disconnected from the specified provisioning server, the game application
        /// will receive the following IClientSdkEventHandler callback:
        ///
        /// onDisconnected()
        ///
        /// @param server - The uri of the server to disconnect from.
        ///
        void Disconnect(const Uri &server);

        ///
        /// This method begins the login process for a user, if that user is not already logged in.
        /// If the user is not already logged in, the application will receive one of the following callbacks:
        ///
        /// IClientApiEventHandler::onLoginCompleted() - this is the expected behavior
        /// IClientApiEventHandler::onLoginFailed() - this may happen in the presence of transient network issues, or blocked UDP ports
        /// IClientApiEventHandler::onInvalidLoginCredentials() - in a game integration, this should never happen because the game server should set the user's Vivox password.
        ///
        /// @param accountName - the Vivox account for the user
        /// @param password - the password associated with that account
        /// @captureDevice - currently unused
        /// @renderDevice - currently unused
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///           VX_E_FAILED will be returned if the a prior call to Connect() did not succeed.
        ///
        VCSStatus Login(const AccountName &accountName, const char *password, const char *captureDevice = NULL, const char *renderDevice = NULL);

        ///
        /// This method begins the logout process for the specific user. If that user is in the process of logging in, or that user is logged in, the
        /// game application will be notified of the completion of this operation through the following IClientApiEventHandler callback:
        ///
        /// onLogoutCompleted()
        /// onLogoutFailed()
        ///
        /// @param accountName - the Vivox account for the user
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus Logout(const AccountName &accountName);

        ///
        /// This method will put a user in a channel. This method may be called after Login() is called. The game application does not need to
        /// wait for an onLoginCompleted() callback before calling this method. This is an asynchronous operation - if the user is not already in
        /// the requested channel, the appication will be notified of its completion through one of the following callbacks:
        ///
        /// IClientApiEventHandler::onChannelJoined()
        /// IClientApiEventHandler::onInvalidChannelCredentials()
        /// IClientApiEventHandler::onChannelJoinFailed()
        ///
        /// @param accountName - the Vivox account for the user
        /// @param channelUri - the channelUri to join
        /// @param access_token - See the Vivox Access Token Developer's Guide for details regarding access tokens.  Initial development and integration 
		///     can be done using the vx_generate_debug_token() locally.
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus JoinChannel(const AccountName &accountName, const Uri &channelUri, const char *access_token = NULL);

        ///
        /// This method removes the specified user from the specified channel. If the user is in the channel already or is in the process
        /// of joining the channel, the application will be notified through the following callbacks:
        ///
        /// IClientApiEventHandler::onChannelExited()
        ///
        /// @param accountName - the Vivox account for the user
        /// @param channelUri - the channel to un-join
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus LeaveChannel(const AccountName &accountName, const Uri &channelUri);

        ///
        /// This method removes the specified user all channels. For each of the channels that the user has joined, or is in the process
        /// of joining, the application will be notified through the following callbacks:
        ///
        /// IClientApiEventHandler::onChannelExited()
        ///
        /// @param accountName - the Vivox account for the user
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus LeaveAll(const AccountName &accountName);

        ///
        /// This method will prevent a particular user from either hearing any voice from another set of users, or being heard by those set of users.
        /// This method is incremental - blocking users A, B and C in one method call, and blocking D, E and F in another method call will result
        /// in users A-F being blocked. This blocking information is not persisted across logins - the game application should call this method
        /// after Login() is called in order to restore the list of blocked users.
        ///
        /// @param accountName - the Vivox account for the user
        /// @param usersToBlock - a set of users to add to the block list
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus BlockUsers(const AccountName &accountName, const std::set<Uri> &usersToBlock);

        ///
        /// This method will allow a particular user from either hearing any voice from another set of users, or being heard by those set of users, if
        /// the any of those users had been blocked before.
        ///
        /// @param accountName - the Vivox account for the user
        /// @param usersToBlock - a set of users to remove from the block list
        ///
        /// @return - 0 on success, non-zero on error. Error Codes can be translated to string by the function VivoxClientApi::GetErrorString(), which is located in util.h
        ///
        VCSStatus UnblockUsers(const AccountName &accountName, const std::set<Uri> &usersToBlock);

        ///
        /// This function plays a file into any currently connected channels for the specified account.
        ///
        /// If no channels are currently connected, nothing is played
        /// If another file is currently playing, it is stopped and the new file is started
        /// If the file does not exist, an error is returned.
        /// If the account does not exist, an error is returned
        ///
        /// The application will be notified of this completing ansynchronously through on or more of the following
        /// callbacks:
        ///
        /// IClientApiEventHandler::onStartPlayFileIntoChannels()
        /// IClientApiEventHandler::onStartPlayFileIntoChannelsFailed()
        /// IClientApiEventHandler::onPlayFileIntoChannelsStopped()
        ///
        /// @param accountName - the user that is currently logged in
        /// @param filename - the file to play
        /// @return - 0 if success, non-zero if failure.
        ///
        VCSStatus StartPlayFileIntoChannels(const AccountName &accountName, const char *filename);

        ///
        /// This function stops any files that are playing into a channels at that moment.
        /// If nothing is playing nothing is stopped, and no error is returned.
        /// If the account does not exist an error is returned.
        ///
        /// The application will be notified of this completing ansynchronously through on or more of the following
        /// callbacks:
        ///
        /// IClientApiEventHandler::onPlayFileIntoChannelsStopped()
        ///
        /// @param - the user that is currently logged in
        /// @return - 0 if success, non-zero if failure.
        ///
        VCSStatus StopPlayFileIntoChannels(const AccountName &accountName);

        ///
        /// Using the identity associated with "accountName", kick the user "userToKick" out of channel "channelKickedFrom"
        /// The user "accountName" must be logged in.
        /// The user "accountName" must be an owner or moderator of the channel.
        ///
        /// The application will be notified of this completing ansynchronously through on or more of the following
        /// callbacks:
        ///
        /// IClientApiEventHandler::onParticipantKickedCompleted()
        /// IClientApiEventHandler::onParticipantKickFailed()
        ///
        /// @param accountName - the account name of the user that will do the kicking
        /// @param channelKickedFrom - the channel to kick the user from
        /// @param userToKick - the URI of the user to kick
        /// @return - 0 if success, non-zero if failure.
        ///
        VCSStatus KickUser(const AccountName &accountName, const Uri &channelKickedFrom, const Uri &userToKick);

        /// Audio Input Device Management

        ///
        /// There are types of audio devices that are used by the Vivox Client SDK - audio input devices (most often a microphone), and 
        /// audio output devices (most often headphones). 
        ///
        /// The user has two choices when selecting either of these devices. They can allow the operating system to determine 
        /// which device is used (through the control panel on Windows, or through the System Settings panel on OSX), or they 
        /// can pick a specific device that will always be used. If the user picks the former, the user will experience typical
        /// plug and play behavior for that operating system - as they plug devices in and out, the device used for voice will
        /// automatically change. If the user picks the former, the user selected device is the only device that will be used
        /// for voice.
        /// 

        ///
        /// Returns the full list of audio input devices
        ///
        const std::vector<AudioDeviceId> &GetAvailableAudioInputDevices() const;

        ///
        /// This function tells the voice subsystem to the use the operating system chose audio input device for 
        /// voice input.
        ///
        void UseOperatingSystemChosenAudioInputDevice();

        ///
        /// This function indicates to the application whether the voice subsystem will be using
        /// the operating system chose audio input device.
        ///
        /// @return true if the voice subsystem is using the operating system chosen audio input device.
        ///
        bool IsUsingOperatingSystemChosenAudioInputDevice() const;

        ///
        /// This function returns the operating system's current audio input device. It should be noted
        /// that if the application has called SetApplicationChosenAudioInputDevice, this value
        /// has no bearing on which device is actually being used.
        ///
        /// @return the operating systems current audio input device.
        ///
        const AudioDeviceId &GetOperatingSystemChosenAudioInputDevice() const;

        ///
        /// This function tells this voice subsystem to always use the audio input device chosen by the application.
        ///
        /// @param deviceId - the device id to always use for voice communication
        /// @return 0 on success, non-zero if failure.
        ///
        VCSStatus SetApplicationChosenAudioInputDevice(const AudioDeviceId &deviceId);

        ///
        /// This function returns the AudioDeviceId that the application has chosen using SetApplicationChosenAudioInputDevice.
        /// If the application has most recently called UseOperatingSystemChosenAudioInputDevice, then this function will return
        /// an invalid AudioDeviceId() (which can be tested for using AudioDeviceId.IsInvalid(); 
        ///
        AudioDeviceId GetApplicationChosenAudioInputDevice() const;


        /// Audio Output Device Management
        
        ///
        /// Returns the full list of audio output devices
        ///
        const std::vector<AudioDeviceId> &GetAvailableAudioOutputDevices() const;

        ///
        /// This function tells the voice subsystem to the use the operating system chose audio output device for 
        /// voice input.
        ///
        void UseOperatingSystemChosenAudioOutputDevice();

        ///
        /// This function indicates to the application whether the voice subsystem will be using
        /// the operating system chose audio output device.
        ///
        /// @return true if the voice subsystem is using the operating system chosen audio input device.
        ///
        bool IsUsingOperatingSystemChosenAudioOutputDevice() const;

        ///
        /// This function returns the operating system's current audio output device. It should be noted
        /// that if the application has called SetApplicationChosenAudioOutputDevice, this value
        /// has no bearing on which device is actually being used.
        ///
        /// @return the operating systems current audio output device.
        ///
        const AudioDeviceId &GetOperatingSystemChosenAudioOutputDevice() const;

        ///
        /// This function tells this voice subsystem to always use the audio output device chosen by the application.
        ///
        /// @param deviceId - the device id to always use for voice communication
        /// @return 0 on success, non-zero if failure.
        ///
        VCSStatus SetApplicationChosenAudioOutputDevice(const AudioDeviceId &deviceId);

        ///
        /// This function returns the AudioDeviceId that the application has chosen using SetApplicationChosenAudioOutputDevice.
        /// If the application has most recently called UseOperatingSystemChosenAudioOutputDevice, then this function will return
        /// an invalid AudioDeviceId() (which can be tested for using AudioDeviceId.IsInvalid(); 
        ///
        AudioDeviceId GetApplicationChosenAudioOutputDevice() const;


        /// Volume Controls

        /// 
        /// Get the master audio input device volume
        ///
        int GetMasterAudioInputDeviceVolume() const;

        /// 
        /// Set the master audio input device volume
        ///
        /// @param volume - the master audio input device volume (recommended range 30-70)
        ///
        VCSStatus SetMasterAudioInputDeviceVolume(int volume);

        /// 
        /// Get the master audio output device volume
        ///
        /// @return the current master output device volume
        ///
        int GetMasterAudioOutputDeviceVolume() const;

        /// 
        /// Set the master audio output device volume
        ///
        /// @param volume - the volume for the master output device. The recommended range for this is 30-70.
        /// @return 0 for success, non zero for failure
        ///
        VCSStatus SetMasterAudioOutputDeviceVolume(int volume);

		VCSStatus SetVoiceActivateDetectionSensitivity(int volume);

		VCSStatus SetVADAutomaticParameterSelection(bool enabled);

        ///
        /// Start playing a wav file through the currently selected audio output device.
        ///
        VCSStatus StartAudioOutputDeviceTest(const char *filename);

        ///
        /// Stop the output device test
        ///
        void StopAudioOutputDeviceTest();

        ///
        /// Indicates whether or not the output device test is running
        ///
        bool AudioOutputDeviceTestIsRunning() const;

        ///
        /// Start the Input Device Test Record Phase
        ///
        VCSStatus StartAudioInputDeviceTestRecord();

        ///
        /// Stop the Input Device Test Record Phase
        ///
        void StopAudioInputDeviceTestRecord();

        ///
        /// Start the Input Device Test Playback Phase
        ///
        VCSStatus StartAudioInputDeviceTestPlayback();

        ///
        /// Stop the Input Device Test Playback Phase
        ///
        void StopAudioInputDeviceTestPlayback();

        ///
        /// Indicates whether or not the input device test is recording
        ///
        bool AudioInputDeviceTestIsRecording() const;

        ///
        /// Indicates whether or not the input device test is playing back
        ///
        bool AudioInputDeviceTestIsPlayingBack() const;

        ///
        /// Indicates whether or not there is sufficient audio to playback
        ///
        bool AudioInputDeviceTestHasAudioToPlayback() const;

        ///
        /// Stops audio from playing out the audio output device
        /// 
        void SetAudioOutputDeviceMuted(bool value);

        ///
        /// Indicates whether or not audio can play through the output device
        ///
        bool GetAudioOutputDeviceMuted() const;

        ///
        /// Stops audio from being read from the audio input device
        /// 
        void SetAudioInputDeviceMuted(bool value);

        ///
        /// Indicates whether or not audio is being read from the audio input device
        ///
        bool GetAudioInputDeviceMuted() const;

        /// 
        /// Returns audio output volume for a particular accountName, channelUri pair. 
        ///
        /// @param accountName - the account name of the currently logged in user
        /// @param channelUri - the URI of the channel 
        /// @return - the volume
        ///
        int GetChannelAudioOutputDeviceVolume(const AccountName &accountName, const Uri &channelUri) const;

        /// 
        /// Sets audio output volume for a particular accountName, channelUri pair.
        ///
        /// @param accountName - the account name of the currently logged in user
        /// @param channelUri - the URI of the channel 
        /// @param volume - the custom volume for the channel. The recommended range for this is between 30 and 70
        /// @return 0 on success, non-zero on failure.
        ///
        VCSStatus SetChannelAudioOutputDeviceVolume(const AccountName &accountName, const Uri &channelUri, int volume);

        ///
		/// Sets the volume for the session of the channel URI specified.  No other sessions are impacted.
		///
		VCSStatus SetSessionVolume(const AccountName &accountName, const Uri &channelUri, int volume);

        ///
        /// Returns target participant's audio output volume, relative to accountName
        ///
        /// @param accountName - the account name of the currently logged in user
        /// @param targetUser - the uri of the user whose volume should be adjusted
        /// @param channelUri - the uri of the channel that targetUser is in.
        /// @return - the volume for that user.
        ///
        int GetParticipantAudioOutputDeviceVolumeForMe(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri) const;

        ///
        /// Set a target participant's audio output volume
        /// This can be used by a user to make one user louder than another, or to even out volumes between users.
        ///
        /// @param accountName - the account name of the currently logged in user
        /// @param targetUser - the uri of the user whose volume should be adjusted
        /// @param channelUri - the uri of the channel that targetUser is in.
        /// @param volume - the volume for that user in that channel
        /// @return 0 on success, non zero on failure
        /// 
        VCSStatus SetParticipantAudioOutputDeviceVolumeForMe(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, int volume);

        /// Muting Controls

        /// 
        /// Mute or unmute a user in a channel, for everyone in that channel
        /// 
        /// @param accountName - the account of the logged user. This user must have moderator privileges for the provided channel
        /// @param targetUser - the user to be muted.
        /// @param channelUri - the channel in which the user is to be muted.
        /// @param muted - true to mute the user, false to unmute the use
        /// @return 0 on success, non-zero on failure
        ///
        VCSStatus SetParticipantMutedForAll(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, bool muted);

        ///
        /// Indicates whether a participant is muted at the server, for all users, in a particular channel
        /// 
        /// @param accountName - the account of the logged in user
        /// @param targetUser - the user to mute or unmute
        /// @param channelUri - the channel that the target user is in
        /// @return true if player muted for all, false if not, or if not found. 
        ///
        bool GetParticipantMutedForAll(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri) const;

        /// 
        /// Mute or unmute a user in a channel, just for the logged in the account
        ///
        /// @param accountName - the account of the logged in user
        /// @param targetUser - the user to mute or unmute
        /// @param channelUri - the channel that the target user is in
        /// @param muted - true to mute the user, false to unmute the user
        ///
        VCSStatus SetParticipantMutedForMe(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, bool muted);

        /// Channel Transmission

        /// 
        /// Returns a participant's channel transmission policy object. Channel transmission policies control
        /// which channels receive audio from a user when that user is in multiple channels at once.
        ///
        /// @param accountName - the accountName of the currently logged in user.
        /// @return the channel transmission policy for the specific user
        /// 
        ChannelTransmissionPolicy GetChannelTransmissionPolicy(const AccountName &accountName) const;

        /// 
        /// Sets a participant's transmitting channel to channelUri
        ///
        /// @param accountName - the account of the currently logged in user
        /// @param channelUri - the URI of the channel 
        /// @return 0 on success non zero on failure
        /// 
        VCSStatus SetTransmissionToSpecificChannel(const AccountName &accountName, const Uri &channelUri);
		
        /// 
        /// Sets a participant's transmitting channel to all channels
        /// 
        /// @param accountName - the account of the currently logged in user
        /// @return 0 on success non zero on failure
        ///
        VCSStatus SetTransmissionToAll(const AccountName &accountName);

        /// 
        /// Sets a users transmitting channel to no channels
        /// 
        /// @param accountName - the account of the currently logged in user
        /// @return 0 on success non zero on failure
        ///
        VCSStatus SetTransmissionToNone(const AccountName &accountName);

		VCSStatus Set3DPosition(const AccountName &accountName, const Uri &channel, const Vector &speakerPosition, const Vector &listenerPosition, const Vector &listenerForward, const Vector &listenerUp);

        ///
        /// Called by the application when the application entered the background.
        ///
        /// Mobile platforms only.
        ///
        void EnteredBackground();
        
        ///
        /// Called by the application whether the application is about to enter the foreground
        ///
        /// Mobile platforms only.
        ///
        void WillEnterForeground();
        
        ///
        /// Called by the application periodically when the application is in the background
        ///
        /// Mobile platforms only.
        ///
        void OnBackgroundIdleTimeout();

 		///
		/// Indicates if the user has connected successfully to the voice plane
		/// 
		/// @param accountName - the local account name of the player
		/// @return zero for channel has at least one participant in the voice plane or non-zero for an error.
		///
		bool HasConnectedChannel(const AccountName &accountName);

		///
		/// Indicates if  anyone in the channel is in the voice plane
		/// 
		/// @param accountName - the local account name of the player
		/// @return zero for channel has at least one participant in the voice plane or non-zero for an error.
		///
		//VCSStatus IsChannelActive(const AccountName &accountName, const Uri &channelUri);
       
    private:
        ClientConnectionImpl *m_pImpl;
    };
}
