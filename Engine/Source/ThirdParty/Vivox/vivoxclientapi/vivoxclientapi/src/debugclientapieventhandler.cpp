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
#include "vivoxclientapi/debugclientapieventhandler.h"

//#include <sstream>
#include <vector>
#include "vivoxclientapi/types.h"

#if defined(WIN32) || defined(_XBOX_ONE)
#include <Windows.h>
#else
#include <sys/time.h>
#define UNREFERENCED_PARAMETER(x) (void)x
#endif

using namespace std;

namespace VivoxClientApi {

    static const char *PREFIX = ">>> ";

    static bool s_abortEnabled = true;

    DebugClientApiEventHandler::DebugClientApiEventHandler(ClientConnection *clientConnection)
    {
        m_clientConnection = clientConnection;
    }


    DebugClientApiEventHandler::~DebugClientApiEventHandler()
    {
    }

    void DebugClientApiEventHandler::SetAbortEnabled(bool value)
    {
        s_abortEnabled = value;
    }

    void DebugClientApiEventHandler::WriteStatus(const char *message) const {
        debugPrint(message);
    }


    const char *DebugClientApiEventHandler::ParticipantLeftString(ParticipantLeftReason reason)
    {
        switch (reason)
        {
        case 0:  return "Left";
        case 1:  return "Network";
        case 2:  return "Kicked";
        case 3:  return "Banned";
        default: return "Unknown";
        }
    }
	/*
    static const char *LogLevelToString(IClientApiEventHandler::LogLevel level) {
        switch (level) {
        case IClientApiEventHandler::LogLevelError: return "Error ";
        case IClientApiEventHandler::LogLevelWarning: return "Warn  ";
        case IClientApiEventHandler::LogLevelInfo: return "Info  ";
        case IClientApiEventHandler::LogLevelDebug: return "Debug ";
        case IClientApiEventHandler::LogLevelTrace: return "Trace ";
        case IClientApiEventHandler::LogLevelNone: return "None  ";
        }
        return "Other ";
    }*/

    // Basic System Services
    void DebugClientApiEventHandler::onLogStatementEmitted(LogLevel level, long long nativeMillisecondsSinceEpoch, long threadId, const char *logMessage)
	{
		UNREFERENCED_PARAMETER(level);
		UNREFERENCED_PARAMETER(nativeMillisecondsSinceEpoch);
		UNREFERENCED_PARAMETER(threadId);

		debugPrint(logMessage);
		/*
#if defined(WIN32) || defined(_XBOX_ONE)
        char buf[80];
        SYSTEMTIME lt;
        FILETIME f;
        ULARGE_INTEGER i;

        i.QuadPart = (ULONGLONG)nativeMillisecondsSinceEpoch;
        f.dwHighDateTime = i.HighPart;
        f.dwLowDateTime = i.LowPart;
        FileTimeToSystemTime(&f, &lt);

        sprintf_s(buf, sizeof(buf), "%02d:%02d:%02d.%03d", lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
#else
        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec;
        struct tm *t = localtime(&now);
        char buf[80];

        sprintf_s(buf, sizeof(buf), "%02d:%02d:%02d.%03d",t->tm_hour, t->tm_min, t->tm_sec, tv.tv_usec/1000);
#endif
        std::stringstream ss;
        ss << buf << " " << LogLevelToString(level) << " " << threadId << " " << logMessage;
#ifdef WIN32
        ss << "\r\n";
#else
        ss << "\n";
#endif
        debugPrint(ss.str().c_str());
		*/
    }

    void DebugClientApiEventHandler::onAssert(const char *filename, int line, const char *message)
	{
		UNREFERENCED_PARAMETER(line);
		UNREFERENCED_PARAMETER(filename);

		/*
        std::stringstream ss;
        ss << filename << "(" << line << "): " << message << "\r\n";
		*/
        debugPrint(message);
        if(s_abortEnabled)
            abort();
    }

    // Service Connection
    void DebugClientApiEventHandler::onConnectCompleted(const Uri &server)
    {
        //stringstream ss;
        //ss << PREFIX << __FUNCTION__ << "(" << server.ToString() << ")\r\n";
        WriteStatus(server.ToString());
    }

    void DebugClientApiEventHandler::onConnectFailed(const Uri &server, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(status);

        //stringstream ss;
        //ss << PREFIX << __FUNCTION__ << "(" << server.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
		//WriteStatus(ss.str().c_str());
		WriteStatus(server.ToString());
    }

    void DebugClientApiEventHandler::onDisconnected(const Uri &server, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(status);
		UNREFERENCED_PARAMETER(server);
       // stringstream ss;
      //  ss << PREFIX << __FUNCTION__ << "(" << server.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
       // WriteStatus(ss.str().c_str());
    }

    // Logging/Logging out
    void DebugClientApiEventHandler::onLoginCompleted(const AccountName &accountName)
	{
		UNREFERENCED_PARAMETER(accountName);
       // stringstream ss;
       // ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << ")\r\n";
       // WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onInvalidLoginCredentials(const AccountName &accountName)
	{
		UNREFERENCED_PARAMETER(accountName);
       // stringstream ss;
       // ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << ")\r\n";
       // WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onLoginFailed(const AccountName &accountName, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(status);
       // stringstream ss;
       // ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
       // WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onLogoutCompleted(const AccountName &accountName)
	{
		UNREFERENCED_PARAMETER(accountName);
        //stringstream ss;
       // ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << ")\r\n";
       // WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onLogoutFailed(const AccountName &accountName, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(status);
       // stringstream ss;
       // ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
      //  WriteStatus(ss.str().c_str());
    }

    // Getting into/out of channel
    void DebugClientApiEventHandler::onChannelJoined(const AccountName &accountName, const Uri &channelUri)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
      //  stringstream ss;
      //  ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << ")\r\n";
      //  WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onInvalidChannelCredentials(const AccountName &accountName, const Uri &channelUri)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
       // stringstream ss;
      //  ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << ")\r\n";
       // WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onChannelJoinFailed(const AccountName &accountName, const Uri &channelUri, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(status);
      //  stringstream ss;
      //  ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
      //  WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onChannelExited(const AccountName &accountName, const Uri &channelUri, const VCSStatus& reasonCode)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(reasonCode);
     //   stringstream ss;
      //  ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << GetErrorString(reasonCode) << " (" << reasonCode << ")\r\n";
      //  WriteStatus(ss.str().c_str());
    }

    void DebugClientApiEventHandler::onCallStatsUpdated(const AccountName &accountName, vx_call_stats_t &stats, bool isFinal)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(stats);
		UNREFERENCED_PARAMETER(isFinal);
		/*
        if (isFinal) {
            stringstream ss;
            ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString()
                << ", expected " << stats.incoming_expected
                << ", received " << stats.incoming_received
                << ", lost " << stats.incoming_packetloss
                << ", discarded too late " << stats.incoming_out_of_time
                << ", discarded malformed " << stats.incoming_discarded
                << "\r\n";
            WriteStatus(ss.str().c_str());
        }
		*/
    }

    // Roster list maintenance
    void DebugClientApiEventHandler::onParticipantAdded(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(participantUri);
		UNREFERENCED_PARAMETER(isLoggedInUser);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << participantUri.ToString() << (isLoggedInUser ? ",self" : "") << ")\r\n";
        WriteStatus(ss.str().c_str());*/
    }
    void DebugClientApiEventHandler::onParticipantLeft(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, ParticipantLeftReason reason)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(participantUri);
		UNREFERENCED_PARAMETER(isLoggedInUser);
		UNREFERENCED_PARAMETER(reason);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << participantUri.ToString() << (isLoggedInUser ? ",self" : "") << "," << ParticipantLeftString(reason) << ")\r\n";
        WriteStatus(ss.str().c_str());*/
    }
    void DebugClientApiEventHandler::onParticipantUpdated(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, bool isLoggedInUser, bool speaking, double vuMeterEnergy, bool mutedForAll)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(isLoggedInUser);
		UNREFERENCED_PARAMETER(participantUri);
		UNREFERENCED_PARAMETER(speaking);
		UNREFERENCED_PARAMETER(vuMeterEnergy);
		UNREFERENCED_PARAMETER(mutedForAll);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << participantUri.ToString() 
            << (isLoggedInUser ? ",self" : "") << "," << (speaking ? "speaking" : "silent") << "," << vuMeterEnergy <<
            "," << (mutedForAll ? "muted for all" : "not muted for all") << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    // Moderation
    void DebugClientApiEventHandler::onParticipantKickedCompleted(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(participantUri);/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << participantUri.ToString() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onParticipantKickFailed(const AccountName &accountName, const Uri &channelUri, const Uri &participantUri, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(participantUri);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << participantUri.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    // Audio Injection
    void DebugClientApiEventHandler::onStartPlayFileIntoChannels(const AccountName &accountName, const char *filename)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(filename);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << filename << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onStartPlayFileIntoChannelsFailed(const AccountName &accountName, const char *filename, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(filename);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << filename << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    void DebugClientApiEventHandler::onPlayFileIntoChannelsStopped(const AccountName &accountName, const char *filename)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(filename);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << filename << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    void DebugClientApiEventHandler::onAvailableAudioDevicesChanged()
    {/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "\r\n";
        if (m_clientConnection != NULL) {
            for (vector<AudioDeviceId>::const_iterator i = m_clientConnection->GetAvailableAudioInputDevices().begin();
                i != m_clientConnection->GetAvailableAudioInputDevices().end();
                ++i) {
                ss << "  Input: " << i->GetAudioDeviceDisplayName() << " ";
            }
            for (vector<AudioDeviceId>::const_iterator i = m_clientConnection->GetAvailableAudioOutputDevices().begin();
                i != m_clientConnection->GetAvailableAudioOutputDevices().end();
                ++i) {
                ss << "  Output: " << i->GetAudioDeviceDisplayName() << " ";
            }
        }
        WriteStatus(ss.str().c_str()); */
    }

    /// Audio Input Device Management
    void DebugClientApiEventHandler::onOperatingSystemChosenAudioInputDeviceChanged(const AudioDeviceId &deviceId)
	{
		UNREFERENCED_PARAMETER(deviceId);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << deviceId.GetAudioDeviceId() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetApplicationChosenAudioInputDeviceCompleted(const AudioDeviceId &deviceId)
	{
		UNREFERENCED_PARAMETER(deviceId);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << deviceId.GetAudioDeviceId() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetApplicationChosenAudioInputDeviceFailed(const AudioDeviceId &deviceId, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(deviceId);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << deviceId.GetAudioDeviceId() << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    void DebugClientApiEventHandler::onOperatingSystemChosenAudioOutputDeviceChanged(const AudioDeviceId &deviceId)
	{
		UNREFERENCED_PARAMETER(deviceId);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << deviceId.GetAudioDeviceId() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetApplicationChosenAudioOutputDeviceCompleted(const AudioDeviceId &deviceId)
	{
		UNREFERENCED_PARAMETER(deviceId);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << deviceId.GetAudioDeviceId() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetApplicationChosenAudioOutputDeviceFailed(const AudioDeviceId &deviceId, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(deviceId);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << deviceId.GetAudioDeviceId() << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    void DebugClientApiEventHandler::onAudioInputDeviceTestPlaybackCompleted()
    {/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    /// Volume Controls
    void DebugClientApiEventHandler::onSetChannelAudioOutputDeviceVolumeCompleted(const AccountName &accountName, const Uri &channelUri, int volume)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(volume);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << volume << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetChannelAudioOutputDeviceVolumeFailed(const AccountName &accountName, const Uri &channelUri, int volume, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(volume);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << volume << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetParticipantAudioOutputDeviceVolumeForMeCompleted(const AccountName &accountName, const Uri &target, const Uri &channelUri, int volume)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(target);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(volume);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << target.ToString() << "," << channelUri.ToString() << "," << volume << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetParticipantAudioOutputDeviceVolumeForMeFailed(const AccountName &accountName, const Uri &target, const Uri &channelUri, int volume, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(target);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(volume);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << target.ToString() << "," << channelUri.ToString() << "," << volume << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    /// Muting Controls
    void DebugClientApiEventHandler::onSetParticipantMutedForAllCompleted(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, bool muted)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(targetUser);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(muted);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << targetUser.ToString() << "," << channelUri.ToString() << "," << muted << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetParticipantMutedForAllFailed(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri, bool muted, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(targetUser);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(muted);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << targetUser.ToString() << "," << channelUri.ToString() << "," << muted << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetParticipantMutedForMeCompleted(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(target);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(muted);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << target.ToString() << "," << channelUri.ToString() << "," << muted << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetParticipantMutedForMeFailed(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(target);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(muted);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << target.ToString() << "," << channelUri.ToString() << "," << muted << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

    /// Channel Transmission
    void DebugClientApiEventHandler::onSetChannelTransmissionToSpecificChannelCompleted(const AccountName &accountName, const Uri &channelUri)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetChannelTransmissionToSpecificChannelFailed(const AccountName &accountName, const Uri &channelUri, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(channelUri);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << channelUri.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetChannelTransmissionToAllCompleted(const AccountName &accountName)
	{
		UNREFERENCED_PARAMETER(accountName);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetChannelTransmissionToAllFailed(const AccountName &accountName, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetChannelTransmissionToNoneCompleted(const AccountName &accountName)
	{
		UNREFERENCED_PARAMETER(accountName);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }
    void DebugClientApiEventHandler::onSetChannelTransmissionToNoneFailed(const AccountName &accountName, const VCSStatus &status)
	{
		UNREFERENCED_PARAMETER(accountName);
		UNREFERENCED_PARAMETER(status);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << accountName.ToString() << "," << GetErrorString(status) << " (" << status << ")\r\n";
        WriteStatus(ss.str().c_str()); */
    }

	void DebugClientApiEventHandler::onAudioUnitStarted(const Uri &initialTargetUri)
	{
		UNREFERENCED_PARAMETER(initialTargetUri);
		/*
        stringstream ss;
        ss << PREFIX << __FUNCTION__ << "(" << initialTargetUri.ToString() << ")\r\n";
        WriteStatus(ss.str().c_str()); */
	}
	void DebugClientApiEventHandler::onAudioUnitStopped(const Uri &initialTargetUri)
	{
		UNREFERENCED_PARAMETER(initialTargetUri);
		/*
		stringstream ss;
		ss << PREFIX << __FUNCTION__ << "(" << initialTargetUri.ToString() << ")\r\n";
		WriteStatus(ss.str().c_str()); */
	}
	void DebugClientApiEventHandler::onAudioUnitAfterCaptureAudioRead(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame)
	{
		UNREFERENCED_PARAMETER(initialTargetUri);
		UNREFERENCED_PARAMETER(pcmFrames);
		UNREFERENCED_PARAMETER(pcmFrameCount);
		UNREFERENCED_PARAMETER(audioFrameRate);
		UNREFERENCED_PARAMETER(channelsPerFrame);
		/*
		stringstream ss;
		ss << PREFIX << __FUNCTION__ << "(" << initialTargetUri.ToString() << "," << pcmFrames << "," << pcmFrameCount << "," << audioFrameRate << "," << channelsPerFrame << ")\r\n";
		WriteStatus(ss.str().c_str()); */
	}
	void DebugClientApiEventHandler::onAudioUnitBeforeCaptureAudioSent(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, bool speaking)
	{
		UNREFERENCED_PARAMETER(initialTargetUri);
		UNREFERENCED_PARAMETER(pcmFrames);
		UNREFERENCED_PARAMETER(pcmFrameCount);
		UNREFERENCED_PARAMETER(audioFrameRate);
		UNREFERENCED_PARAMETER(channelsPerFrame);
		UNREFERENCED_PARAMETER(speaking);
		/*
		stringstream ss;
		ss << PREFIX << __FUNCTION__ << "(" << initialTargetUri.ToString() << "," << pcmFrames << "," << pcmFrameCount << "," << audioFrameRate << "," << channelsPerFrame << "," << silence << ")\r\n";
		WriteStatus(ss.str().c_str()); */
	}
	void DebugClientApiEventHandler::onAudioUnitBeforeRecvAudioRendered(const Uri &initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, bool silence)
	{
		UNREFERENCED_PARAMETER(initialTargetUri);
		UNREFERENCED_PARAMETER(pcmFrames);
		UNREFERENCED_PARAMETER(pcmFrameCount);
		UNREFERENCED_PARAMETER(audioFrameRate);
		UNREFERENCED_PARAMETER(channelsPerFrame);
		UNREFERENCED_PARAMETER(silence);
		/*
		stringstream ss;
		ss << PREFIX << __FUNCTION__ << "(" << initialTargetUri.ToString() << "," << pcmFrames << "," << pcmFrameCount << "," << audioFrameRate << "," << channelsPerFrame << "," << silence << ")\r\n";
		WriteStatus(ss.str().c_str()); */
	}
}
