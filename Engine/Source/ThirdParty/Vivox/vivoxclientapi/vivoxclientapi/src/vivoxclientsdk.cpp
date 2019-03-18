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
#include "vivoxclientapi/vivoxclientsdk.h"
#include "VxcErrors.h"
#include "Vxc.h"
#include "VxcRequests.h"
//#include "VxcResponses.h"
#include "VxcEvents.h"
#include <stdint.h>
#include <sstream>
#include <set>
#if defined(_XBOX_ONE) || defined(WIN32)
#include <Windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include <string>
#include <map>
#include <vector>
#include "vivoxclientapi/types.h"
#ifdef __APPLE__
#include <pthread.h>
#endif

#ifdef __ANDROID__
#include <android/log.h>
#endif

static std::string CodePageToUTF8(const char *cpBuf, size_t cpBufLen)
{
    if (cpBuf == 0 || cpBufLen == 0 || cpBuf[0] == 0)
        return "";
#if defined(WIN32) && !defined(_XBOX_ONE) && !defined(_UAP)
    std::string uBuf;
    size_t wLen = cpBufLen * 2;
    WCHAR *wideBuf = new WCHAR[wLen];
    if (wideBuf != 0) {
        memset(wideBuf, 0, wLen);
        int wideCount = MultiByteToWideChar(GetACP(), 0, cpBuf, (int)cpBufLen, wideBuf, (int)wLen);
        if (wideCount >= 0) {
            uBuf.resize(wLen * 2);
            int uCount = WideCharToMultiByte(CP_UTF8, 0, wideBuf, wideCount, (char *)uBuf.data(), (int)uBuf.size(), NULL, NULL);
            if (uCount >= 0) {
                uBuf.resize((size_t)uCount);
            }
            else {
                uBuf = "";
            }
        }
        delete[]wideBuf;
    }
    return uBuf;
#else
    return std::string(cpBuf, cpBufLen);
#endif
}

static std::string UTF8ToCodePage(const char *uBuf, size_t uBufLen)
{
    if (uBuf == 0 || uBufLen == 0 || uBuf[0] == 0)
        return "";
#if defined(WIN32) && !defined(_XBOX_ONE) && !defined(_UAP)
    std::string cpBuf;
    size_t wLen = uBufLen * 2;
    WCHAR *wideBuf = new WCHAR[wLen];
    if (wideBuf != 0) {
        memset(wideBuf, 0, wLen);
        int wideCount = MultiByteToWideChar(CP_UTF8, 0, uBuf, (int)uBufLen, wideBuf, (int)wLen);
        if (wideCount >= 0) {
            cpBuf.resize(wLen * 2);
            int cpCount = WideCharToMultiByte(GetACP(), 0, wideBuf, (int)wideCount, (char *)cpBuf.data(), (int)cpBuf.size(), NULL, NULL);
            if (cpCount >= 0) {
                cpBuf.resize(cpCount);
            }
            else {
                cpBuf = "";
            }
        }
        delete[]wideBuf;
    }
    return cpBuf;
#else
    return std::string(uBuf, uBufLen);
#endif
}

#if !defined(WIN32) && !defined(_XBOX)
#define strcmpi(a,b) strcasecmp(a,b)
#define _strcmpi(a,b) strcasecmp(a,b)
#define _strdup(s) strdup(s)
#endif

#define CHECK_RET(x) if(!(x)) { m_app->onAssert(__FUNCTION__, __LINE__, #x); return; }
#define CHECK_RET1(x, y) if(!(x)) { m_app->onAssert(__FUNCTION__, __LINE__, #x); return y; }
#define CHECK(x) if(!(x)) { m_app->onAssert(__FUNCTION__, __LINE__, #x); }
#define CHECK_CONT(x) if(!(x)) { m_app->onAssert(__FUNCTION__, __LINE__, #x); continue; }
#define CHECK_BREAK(x) if(!(x)) { m_app->onAssert(__FUNCTION__, __LINE__, #x); break; }
#define ALWAYS_ASSERT(x) m_app->onAssert(__FUNCTION__, __LINE__, #x)

#define CHECK_STATUS_RET(x) if((x) != 0) { m_app->onAssert(__FUNCTION__, __LINE__, #x); return; }
#define CHECK_STATUS_RETVAL(x) { int RetVal = (x); if(RetVal != 0) { m_app->onAssert(__FUNCTION__, __LINE__, #x); return VCSStatus(RetVal); }}

namespace VivoxClientApi {

	const char * g_domain_with_at = "@vd2.vivox.com";   //Change this value to the domain name of the server you are developing against.

    static AudioDeviceId AudioDeviceIdFromCodePage(const char *device_id, const char *device_name)
    {
        return AudioDeviceId(CodePageToUTF8(device_id, strlen(device_id)), CodePageToUTF8(device_name, strlen(device_name)));
    }

    static std::string AudioDeviceIdToCodePage(const AudioDeviceId &id)
    {
        return UTF8ToCodePage(id.GetAudioDeviceId().c_str(), id.GetAudioDeviceId().size());
    }

    static VCSStatus issueRequest(vx_req_base_t *request)
    {
        int outstandingRequestCount = 0;
#ifdef _DEBUG
        char *xml = NULL;
        vx_request_to_xml(request, &xml);
        debugPrint(xml);
        debugPrint("\n");
        vx_free(xml);
#endif
        VCSStatusCode status = vx_issue_request3(request, &outstandingRequestCount);
        if(outstandingRequestCount > 10) {
            fprintf(stderr, "warning: outstandingRequestCount = %d\n", outstandingRequestCount);
        }
        return VCSStatus(status);
    }

#ifdef _DEBUG
    std::string NowString() {
        char buf[80];
    #ifdef WIN32
        SYSTEMTIME lt;
        GetLocalTime(&lt);
        _snprintf_c(buf, sizeof(buf), "%02d:%02d:%02d.%03d", lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
        return buf;
    #else
        time_t now = time(0L);
        struct tm *t = localtime(&now);
        sprintf(buf, "%02d:%02d:%02d.%03d", t->tm_hour, t->tm_min, t->tm_sec, 0);
        return buf;
    #endif
    }
#endif
    static void sleepMicroseconds(size_t us)
    {
    #if defined(WIN32) || defined(_XBOX)
        SleepEx((DWORD)(us/1000), TRUE);
    #else
    #ifdef SN_TARGET_PS3
        sys_timer_usleep(us);
    #else
        struct timespec ts;
        ts.tv_sec=0;
        ts.tv_nsec=us*1000LL;
        nanosleep(&ts,NULL);
    #endif
    #endif
    }

    std::vector<std::string> split(const char *s)
    {
        std::vector<std::string> ss;
        std::string t = s;
        size_t lastpos = 0;
        for(;;) {
            size_t pos = t.find("\n", lastpos);
            if (pos > lastpos && pos != 0)
            {
                if(pos == std::string::npos) {
                    if(lastpos < t.size()) {
                        ss.push_back(t.substr(lastpos, t.size() - lastpos));
                    }
                    break;
                } else {
                    ss.push_back(t.substr(lastpos, pos - lastpos));
                }
            }
            lastpos = pos + 1;
            if(lastpos >= t.size())
                break;
        }
        return ss;
    }

    static const char *safe_str(const char *s)
    {
        if(s == NULL)
            return "";
        return s;
    }

    static char *GetNextRequestId(const char *parent, const char *prefix)
    {
        static int lastRequestId = 0;
        std::stringstream ss;
        if(parent && parent[0]) {
            ss << parent << "." << prefix << lastRequestId++;
            return vx_strdup(ss.str().c_str());
        } else {
            ss << prefix << lastRequestId++;
            return vx_strdup(ss.str().c_str());
        }
    }

    class Participant {
    public:
        Participant(IClientApiEventHandler *app, const Uri &uri) : m_app(app), m_uri(uri)
        {
            m_isSpeaking = -1;
            m_energy = -1;
            m_currentVolume = 50;
            m_desiredVolume = 50;
            m_currentMutedForMe = false;
            m_desiredMutedForMe = false;
            m_volumeRequestInProgress = false;
            m_mutedForMeRequestInProgress = false;
            m_mutedForAll = false;
        }
        void NextState(const std::string &sessionHandle, const Uri &channelUri) {
            (void)channelUri;
            if (!m_volumeRequestInProgress && m_currentVolume != m_desiredVolume) {
                vx_req_session_set_participant_volume_for_me_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_session_set_participant_volume_for_me_create(&req));
                req->session_handle = vx_strdup(sessionHandle.c_str());
                req->participant_uri = vx_strdup(m_uri.ToString());
                req->volume = m_desiredVolume;
                issueRequest(&req->base);
                m_volumeRequestInProgress = true;
            }
            if (!m_mutedForMeRequestInProgress && m_currentMutedForMe != m_desiredMutedForMe) {
                vx_req_session_set_participant_mute_for_me_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_session_set_participant_mute_for_me_create(&req));
                req->session_handle = vx_strdup(sessionHandle.c_str());
                req->participant_uri = vx_strdup(m_uri.ToString());
                req->mute = m_desiredMutedForMe ? 1 : 0;
                issueRequest(&req->base);
                m_mutedForMeRequestInProgress = true;
            }
        }
        // returns true if updated
        bool SetIsSpeaking(bool value) {
            if(m_isSpeaking == -1 || value != (m_isSpeaking == 1 ? true : false)) {
                m_isSpeaking = value ? 1 : 0;
                return true;
            }
            return false;
        }
        bool SetEnergy(double value) {
            if(m_energy != value) {
                m_energy = value;
                return true;
            }
            return false;
        }
        bool SetMutedForAll(bool value) {
            if (m_mutedForAll != value) {
                m_mutedForAll = value;
                return true;
            }
            return false;
        }
        bool GetMutedForAll() const {
            return m_mutedForAll;
        }
        const Uri &GetUri() const { return m_uri; }
        bool GetIsSpeaking() const { return m_isSpeaking > 0; }
        double GetEnergy() const { return m_energy; }
        int GetCurrentVolume() const { return m_currentVolume; }
        bool GetCurrentMutedForMe() const { return m_currentMutedForMe; }
        int GetDesiredVolume() const { return m_desiredVolume; }
        bool GetDesiredMutedForMe() const { return m_desiredMutedForMe; }
        void SetCurrentVolume(int value) { m_currentVolume = value; }
        void SetCurrentMutedForMe(bool muted) { m_currentMutedForMe = muted; }
        void SetDesiredVolume(int value) { m_desiredVolume = value; }
        void SetDesiredMutedForMe(bool muted) { m_desiredMutedForMe = muted; }
        void SetVolumeRequestInProgress(bool value) { m_volumeRequestInProgress = value; }
        void SetMutedForMeRequestInProgress(bool value) { m_mutedForMeRequestInProgress = value; }
    private:
        IClientApiEventHandler *m_app;
        Uri m_uri;
        int m_isSpeaking;
        double m_energy;
        int m_currentVolume;
        bool m_currentMutedForMe;
        int m_desiredVolume;
        bool m_desiredMutedForMe;
        bool m_volumeRequestInProgress;
        bool m_mutedForMeRequestInProgress;
        bool m_mutedForAll;
    };

    
    class Channel {
    public:
        typedef enum {
            ChannelStateDisconnected,
            ChannelStateConnecting,
            ChannelStateConnected,
            ChannelStateDisconnecting
        } ChannelState;

        Channel(IClientApiEventHandler *app, const Uri &uri) : m_app(app)
        {
            CHECK(app != NULL);
            CHECK(uri.IsValid());
            m_channelUri = uri;
            m_currentState = ChannelStateDisconnected;
            m_desiredState = ChannelStateDisconnected;
            m_currentVolume = 50;
            m_desiredVolume = 50;
            m_volumeRequestInProgress = false;
			m_sessionMuted = false;
        }

		VCSStatus Join(const char *access_token)
        {
			m_access_token = safe_str(access_token);
            m_desiredState = ChannelStateConnected;
            return VCSStatus(0);
        }

        void Leave() {
            m_desiredState = ChannelStateDisconnected;
        }

        void NextState(const std::string &sessionGroupHandle, const AccountName &accountName)
        {
            CHECK_RET(!sessionGroupHandle.empty());
            m_accountName = accountName;
			m_self_sip_uri = (std::string)"sip:" + m_accountName.ToString() + g_domain_with_at;  // Useful when using access tokens
			
            if(m_currentState == ChannelStateDisconnected && m_desiredState == ChannelStateConnected) {
                CHECK_RET(m_channelUri.IsValid());
                {
                    vx_req_sessiongroup_add_session_t *req = nullptr;
                    CHECK_STATUS_RET(vx_req_sessiongroup_add_session_create(&req));
                    req->connect_audio = 1;
                    req->connect_text = 0;
                    req->uri = vx_strdup(m_channelUri.ToString());
                    req->sessiongroup_handle = vx_strdup(sessionGroupHandle.c_str());
                    req->base.cookie = GetNextRequestId(NULL, "S");
                    req->session_handle = vx_strdup(req->base.cookie);
					if (!m_access_token.empty()) {
						req->access_token = vx_strdup(m_access_token.c_str());
                    }
                    m_sessionHandle = req->session_handle;
                    m_currentState = ChannelStateConnecting;

                    issueRequest(&req->base);
                }
            } else if((m_currentState == ChannelStateConnecting || m_currentState == ChannelStateConnected) && m_desiredState == ChannelStateDisconnected) {
                vx_req_sessiongroup_remove_session_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_sessiongroup_remove_session_create(&req));
                req->session_handle = vx_strdup(m_sessionHandle.c_str());
                req->sessiongroup_handle = vx_strdup(sessionGroupHandle.c_str());
                m_currentState = ChannelStateDisconnecting;
                issueRequest(&req->base);
			}
			else if (m_currentState == ChannelStateConnected) {
				if (!m_volumeRequestInProgress && m_currentVolume != m_desiredVolume && !m_sessionMuted) {
                    vx_req_session_set_local_speaker_volume_t *req = nullptr;
                    CHECK_STATUS_RET(vx_req_session_set_local_speaker_volume_create(&req));
                    req->session_handle = vx_strdup(m_sessionHandle.c_str());
                    req->volume = m_desiredVolume;
                    issueRequest(&req->base);
                    m_volumeRequestInProgress = true;
                }
            }
        }

        ChannelState GetDesiredState() const { return m_desiredState; }
        ChannelState GetCurrentState() const { return m_currentState; }
        void SetCurrentState(ChannelState value) { 
            if (m_currentState != value) {
                m_currentState = value;
                if (m_currentState == ChannelStateDisconnected) {
                    ClearParticipants();
                }
            }
        }
        void SetDesiredState(ChannelState value) { m_desiredState = value; }

        int GetCurrentVolume() const { return m_currentVolume; }
        int GetDesiredVolume() const { return m_desiredVolume; }
        bool GetVolumeRequestInProgress() const { return m_volumeRequestInProgress; }
        void SetCurrentVolume(int value) { m_currentVolume = value; }
        void SetDesiredVolume(int value) { m_desiredVolume = value; }
        void SetSessionMuted(bool value) { m_sessionMuted = value; }

        void SetVolumeRequestInProgress(bool value) { m_volumeRequestInProgress = value; }

        const Uri &GetUri() const { return m_channelUri; }
        const std::string &GetSessionHandle() const { return m_sessionHandle; }

        int GetParticipantAudioOutputDeviceVolumeForMe(const Uri &target)
        {
            Participant *p = FindParticipantByUri(target, false);
            if (p == NULL){
                return 50; /// default value
            }
            return p->GetCurrentVolume();
        }

        VCSStatus SetParticipantAudioOutputDeviceVolumeForMe(const Uri &target, int volume)
        {
            Participant *p = FindParticipantByUri(target, false);
            if (p == NULL){
                return  VCSStatus(VX_E_NO_EXIST);
            }
            if (volume != p->GetDesiredVolume()) {
                p->SetDesiredVolume(volume);
                p->NextState(m_sessionHandle, m_channelUri);
            }
            return VCSStatus(0);
        }

        bool GetParticipantMutedForAll(const Uri &target)
        {
            Participant *p = FindParticipantByUri(target);
            if (p == NULL){
                return false;
            }
            return p->GetMutedForAll();
        }

        VCSStatus SetParticipantMutedForMe(const Uri &target, bool muted)
        {
            Participant *p = FindParticipantByUri(target);
            if (p == NULL){
                return VCSStatus(VX_E_NO_EXIST);
            }
            if (muted != p->GetDesiredMutedForMe()) {
                p->SetDesiredMutedForMe(muted);
                p->NextState(m_sessionHandle, m_channelUri);
            }
            return VCSStatus();
        }

        VCSStatus SetTransmissionToThisChannel(){
            vx_req_sessiongroup_set_tx_session_t *req = nullptr;
            CHECK_STATUS_RETVAL(vx_req_sessiongroup_set_tx_session_create(&req));
            req->session_handle = vx_strdup(m_sessionHandle.c_str());
            return issueRequest(&req->base);
        }

		VCSStatus Set3DPosition(const Vector &speakerPosition, const Vector &listenerPosition, const Vector &listenerForward, const Vector &listenerUp)
		{
			vx_req_session_set_3d_position_t *req = nullptr;
			CHECK_STATUS_RETVAL(vx_req_session_set_3d_position_create(&req));
			req->req_disposition_type = req_disposition_no_reply_required;
			req->session_handle = vx_strdup(m_sessionHandle.c_str());

			req->speaker_position[0] = speakerPosition.x;
			req->speaker_position[1] = speakerPosition.y;
			req->speaker_position[2] = speakerPosition.z;

			req->listener_position[0] = listenerPosition.x;
			req->listener_position[1] = listenerPosition.y;
			req->listener_position[2] = listenerPosition.z;

			req->listener_at_orientation[0] = listenerForward.x;
			req->listener_at_orientation[1] = listenerForward.y;
			req->listener_at_orientation[2] = listenerForward.z;
			
			req->listener_up_orientation[0] = listenerUp.x;
			req->listener_up_orientation[1] = listenerUp.y;
			req->listener_up_orientation[2] = listenerUp.z;

			return issueRequest(&req->base);
		}

        void HandleResponse(vx_resp_session_set_local_speaker_volume * resp){
            vx_req_session_set_local_speaker_volume_t *req = reinterpret_cast<vx_req_session_set_local_speaker_volume_t *>(resp->base.request);
            if (resp->base.return_code != 0) {
                if (m_desiredVolume == req->volume)
                    m_desiredVolume = m_currentVolume;
                m_app->onSetChannelAudioOutputDeviceVolumeFailed(m_accountName, m_channelUri, req->volume, VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_currentVolume = req->volume;
                m_app->onSetChannelAudioOutputDeviceVolumeCompleted(m_accountName, m_channelUri, req->volume);
            }
            m_volumeRequestInProgress = false;
        }

        void HandleResponse(vx_resp_session_set_participant_volume_for_me * resp) {
            vx_req_session_set_participant_volume_for_me_t *req = reinterpret_cast<vx_req_session_set_participant_volume_for_me_t *>(resp->base.request);
            CHECK_RET(req->participant_uri != NULL);
            Participant *p = FindParticipantByUri(Uri(req->participant_uri));
            CHECK_RET(p != NULL);
            if (resp->base.return_code != 0) {
                if (p->GetDesiredVolume() == req->volume)
                    p->SetDesiredVolume(p->GetCurrentVolume());
                m_app->onSetParticipantAudioOutputDeviceVolumeForMeFailed(m_accountName, Uri(req->participant_uri), m_channelUri, req->volume, VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                p->SetCurrentVolume(req->volume);
                m_app->onSetParticipantAudioOutputDeviceVolumeForMeCompleted(m_accountName, Uri(req->participant_uri), m_channelUri, req->volume);
            }
            p->SetVolumeRequestInProgress(false);
            p->NextState(m_sessionHandle, m_channelUri);
        }

        void HandleResponse(vx_resp_channel_mute_user * resp){
            vx_req_channel_mute_user_t *req = reinterpret_cast<vx_req_channel_mute_user_t *>(resp->base.request);
            Participant *p = FindParticipantByUri(Uri(req->participant_uri), false);
            CHECK_RET(p != NULL);
            bool req_muted = req->set_muted ? true : false;
            if (resp->base.return_code != 0) {
                m_app->onSetParticipantMutedForAllFailed(m_accountName, Uri(req->participant_uri), m_channelUri, req_muted, VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_app->onSetParticipantMutedForAllCompleted(m_accountName, Uri(req->participant_uri), m_channelUri, req_muted);
            }
            p->NextState(m_sessionHandle, m_channelUri);
        }

        void HandleResponse(vx_resp_session_set_participant_mute_for_me * resp) {
            vx_req_session_set_participant_mute_for_me_t *req = reinterpret_cast<vx_req_session_set_participant_mute_for_me_t *>(resp->base.request);
            Participant *p = FindParticipantByUri(Uri(req->participant_uri), false);
            CHECK_RET(p != NULL);
            bool req_muted = req->mute ? true : false;
            if (resp->base.return_code != 0) {
                if (p->GetDesiredMutedForMe() == req_muted)
                    p->SetDesiredMutedForMe(p->GetCurrentMutedForMe());
                m_app->onSetParticipantMutedForMeFailed(m_accountName, Uri(req->participant_uri), m_channelUri, req_muted, VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                p->SetCurrentMutedForMe(req_muted);
                m_app->onSetParticipantMutedForMeCompleted(m_accountName, Uri(req->participant_uri), m_channelUri, req_muted);
            }
            p->SetMutedForMeRequestInProgress(false);
            p->NextState(m_sessionHandle, m_channelUri);
        }

        void HandleEvent(vx_evt_participant_added *evt)
        {
            Participant *p = FindParticipantByUri(Uri(evt->participant_uri), false);
            CHECK_RET(p == NULL);
            p = FindParticipantByUri(Uri(evt->participant_uri), true);
            CHECK_RET(p != NULL);
            if(evt->is_current_user) {
                CHECK(GetCurrentState() == Channel::ChannelStateConnecting);
                if(GetCurrentState() == Channel::ChannelStateConnecting) {
                    SetCurrentState(Channel::ChannelStateConnected);
                    m_app->onChannelJoined(m_accountName, GetUri());
                }
            }

            m_app->onParticipantAdded(m_accountName, m_channelUri, p->GetUri(), evt->is_current_user != 0? true : false);
        }

        void HandleEvent(vx_evt_participant_updated *evt)
        {
            Participant *p = FindParticipantByUri(Uri(evt->participant_uri));
            //CHECK_RET(p != NULL);
			if (p == NULL) return;  // recieved a PU event after the user has left the channel.
            bool changed = p->SetIsSpeaking(evt->is_speaking ? true : false);
            changed |= p->SetEnergy(evt->energy);
            changed |= p->SetMutedForAll(evt->is_moderator_muted ? true : false);
            if (changed)
                m_app->onParticipantUpdated(m_accountName, m_channelUri, p->GetUri(), evt->is_current_user != 0 ? true : false, p->GetIsSpeaking(), p->GetEnergy(), p->GetMutedForAll());
        }

        void HandleEvent(vx_evt_participant_removed *evt)
        {
            Participant *p = FindParticipantByUri(Uri(evt->participant_uri), false);
			if (p != NULL) {
                m_app->onParticipantLeft(m_accountName, m_channelUri, p->GetUri(), evt->is_current_user != 0 ? true : false, (IClientApiEventHandler::ParticipantLeftReason)evt->reason);
                m_participants.erase(p->GetUri());
                delete p;
            }
		}

    private:
        void ClearParticipants()
        {
            for (std::map<Uri, Participant *>::const_iterator i = m_participants.begin(); i != m_participants.end(); ++i) {
                delete i->second;
            }
            m_participants.clear();
        }
        Participant *FindParticipantByUri(const Uri &uri, bool create = false) {
            std::map<Uri, Participant *>::const_iterator i = m_participants.find(uri);
            if(i == m_participants.end()) {
                if(create) {
                    Participant *p = new Participant(m_app, uri);
                    m_participants[uri] = p;
                    return p;
                } else {
                    return NULL;
                }
            } else {
                return i->second;
            }
        }
        std::map<Uri, Participant *> m_participants;

        ChannelState m_desiredState;
        ChannelState m_currentState;
        int m_currentVolume;
        int m_desiredVolume;
        bool m_volumeRequestInProgress;
		bool m_sessionMuted;

        Uri m_channelUri;
        std::string m_access_token;
        std::string m_sessionHandle;
        IClientApiEventHandler *m_app;
        AccountName m_accountName;
		std::string m_self_sip_uri;		// my sip uri in this channel
    };

    class MultiChannelSessionGroup {
    public:
        MultiChannelSessionGroup(IClientApiEventHandler *app) : m_app(app)
        {
            m_channelTransmissionPolicyRequestInProgress = false;
        }

        ~MultiChannelSessionGroup() {
            Clear();
        }

        void Clear()
        {
            m_sessionGroupHandle.clear();
            m_accountHandle.clear();
            for (std::map<Uri, Channel *>::const_iterator i = m_channels.begin(); i != m_channels.end(); ++i) {
                delete i->second;
            }
            m_channels.clear();
        }

        VCSStatus JoinChannel(const Uri &channelUri, const char *access_token, bool multiChannel)
        {
            if(!channelUri.IsValid())
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            Channel *c = FindChannel(channelUri);
            if(c == NULL) {
                c = new Channel(m_app, channelUri);
                m_channels[channelUri] = c;
            }
            if (!multiChannel) {
                for (std::map<Uri, Channel *>::const_iterator i = m_channels.begin(); i != m_channels.end(); ++i) {
                    if (i->second != c) {
                        i->second->Leave();
                    }
                }
            }
            return c->Join(access_token);
        }

        VCSStatus LeaveChannel(const Uri &channelUri)
        {
            if(!channelUri.IsValid())
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            Channel *s = FindChannel(channelUri);
            if(s == NULL)
                return VCSStatus(VX_E_NO_EXIST);
            s->Leave();
            return VCSStatus(0);
        }

        VCSStatus LeaveAll()
        {
            for(std::map<Uri, Channel *>::const_iterator i = m_channels.begin();i!=m_channels.end();++i) {
                i->second->SetDesiredState(Channel::ChannelStateDisconnected);
            }
            return VCSStatus(0);
        }

        VCSStatus StartPlayFileIntoChannels(const char *filename)
        {
            if (!filename || !filename[0])
                return VCSStatus(VX_E_INVALID_ARGUMENT);

#ifdef WIN32
            FILE *fp = NULL;
            fopen_s(&fp, filename, "r");
            if(!fp) {
                return VCSStatus(VX_E_FILE_OPEN_FAILED);
            }
#else
            FILE *fp = fopen(filename, "r");
            if(!fp) {
                return VCSStatus(VX_E_FILE_OPEN_FAILED);
            }
#endif
            fclose(fp);

            if(HasConnectedChannel()) {
                vx_req_sessiongroup_control_audio_injection_t *req = nullptr;
                CHECK_STATUS_RETVAL(vx_req_sessiongroup_control_audio_injection_create(&req));
                req->audio_injection_control_type = VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_RESTART;
                req->sessiongroup_handle = vx_strdup(m_sessionGroupHandle.c_str());
                req->filename = vx_strdup(filename);
                issueRequest(&req->base);
            }
            return VCSStatus(0);
        }

        void StopPlayFileIntoChannels()
        {
            vx_req_sessiongroup_control_audio_injection_t *req = nullptr;
            CHECK_STATUS_RET(vx_req_sessiongroup_control_audio_injection_create(&req));
            req->audio_injection_control_type = VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_STOP;
            req->sessiongroup_handle = vx_strdup(m_sessionGroupHandle.c_str());
            issueRequest(&req->base);
        }

        int GetChannelAudioOutputDeviceVolume(const Uri &channel) const
        {
            if (!channel.IsValid())
                return 50; /// default value
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return 50; /// default value
            return s->GetCurrentVolume();
        }

        VCSStatus SetChannelAudioOutputDeviceVolume(const Uri &channel, int volume)
        {
            if (!channel.IsValid())
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return VCSStatus(VX_E_NO_EXIST);
            if (volume != s->GetDesiredVolume()) {
                s->SetDesiredVolume(volume);
                s->NextState(m_sessionGroupHandle, m_accountName);
            }
            return VCSStatus(0);
        }

		// Set the output session volume for one particular session independently of 
		// any other session that might be active.  Setting the volume to zero effectly 
		// mutes the session.  Audio traffic is still being recieved, but not rendered.
		VCSStatus SetSessionVolume(const Uri &channel, int volume)
		{
			if (!channel.IsValid())
				return VCSStatus(VX_E_INVALID_ARGUMENT);
			if (volume < 0 || volume > 100)  // 
				return VCSStatus(VX_E_INVALID_ARGUMENT);
			Channel *s = FindChannel(channel);
			if (s == NULL)
				return VCSStatus(VX_E_NO_EXIST);

			if (volume == 0) { s->SetSessionMuted(true);}  // prevent the m_desiredVolume from overriding the mute
			if (volume == 100) { s->SetSessionMuted(false); volume=s->GetDesiredVolume(); }

			// issue the request,  there is no state information to worry about if it fails.
			vx_req_session_set_local_speaker_volume_t *req = nullptr;
			CHECK_STATUS_RETVAL(vx_req_session_set_local_speaker_volume_create(&req));
			req->session_handle = vx_strdup(s->GetSessionHandle().c_str());
			req->volume = volume;
			
			return issueRequest(&req->base);
		}

        int GetParticipantAudioOutputDeviceVolumeForMe(const Uri &target, const Uri &channel)
        {
            if (!channel.IsValid())
                return 50; /// default value
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return 50; /// default value
            return s->GetParticipantAudioOutputDeviceVolumeForMe(target);
        }

        VCSStatus SetParticipantAudioOutputDeviceVolumeForMe(const Uri &target, const Uri &channel, int volume)
        {
            if (!channel.IsValid())
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return VCSStatus(VX_E_NO_EXIST);
            return s->SetParticipantAudioOutputDeviceVolumeForMe(target, volume);
        }

        VCSStatus SetParticipantMutedForAll(const Uri &target, const Uri &channel, bool muted)
        {
            if (!channel.IsValid())
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return VCSStatus(VX_E_NO_EXIST);
            vx_req_channel_mute_user_t *req = nullptr;
            CHECK_STATUS_RETVAL(vx_req_channel_mute_user_create(&req));
            req->account_handle = vx_strdup(m_accountHandle.c_str());
            req->channel_uri = vx_strdup(channel.ToString());
            req->participant_uri = vx_strdup(target.ToString());
			req->set_muted = muted ? 1 : 0;
            issueRequest(&req->base);
            return VCSStatus(0);
        }
		
        bool GetParticipantMutedForAll(const Uri &target, const Uri &channel) const
        {
            CHECK_RET1(channel.IsValid(), false);
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return false;
            return s->GetParticipantMutedForAll(target);
        }

        VCSStatus SetParticipantMutedForMe(const Uri &target, const Uri &channel, bool muted)
        {
            if (!channel.IsValid())
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            Channel *s = FindChannel(channel);
            if (s == NULL)
                return VCSStatus(VX_E_NO_EXIST);
            return s->SetParticipantMutedForMe(target, muted);
        }

        ChannelTransmissionPolicy GetCurrentChannelTransmissionPolicy() const { return m_currentChannelTransmissionPolicy; }
        ChannelTransmissionPolicy GetDesiredChannelTransmissionPolicy() const { return m_desiredChannelTransmissionPolicy; }
		
		VCSStatus Set3DPosition(const Uri &channel, const Vector &speakerPosition, const Vector &listenerPosition, const Vector &listenerForward, const Vector &listenerUp)
		{
			if (!channel.IsValid())
				return VCSStatus(VX_E_INVALID_ARGUMENT);
			Channel *s = FindChannel(channel);
			if (s == NULL)
				return VCSStatus(VX_E_NO_EXIST);

			return s->Set3DPosition(speakerPosition, listenerPosition, listenerForward, listenerUp);
		}

        VCSStatus SetTransmissionToSpecificChannel(const Uri &channel)
        {
            if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() != ChannelTransmissionPolicy::vx_channel_transmission_policy_specific_channel ||
                m_desiredChannelTransmissionPolicy.GetSpecificTransmissionChannel() != channel)
            {
                if (!channel.IsValid())
                    return VCSStatus(VX_E_INVALID_ARGUMENT);
                Channel *s = FindChannel(channel);
                if (s == NULL)
                    return VCSStatus(VX_E_NO_EXIST);

                m_desiredChannelTransmissionPolicy.SetTransmissionToSpecificChannel(channel);
            }
            return VCSStatus(0);
        }

        VCSStatus SetTransmissionToAll()
        {
            if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() != ChannelTransmissionPolicy::vx_channel_transmission_policy_all) {
                m_desiredChannelTransmissionPolicy.SetTransmissionToAll();
            }
            return VCSStatus(0);
        }

        VCSStatus SetTransmissionToNone()
        {
            if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() != ChannelTransmissionPolicy::vx_channel_transmission_policy_none) {
                m_desiredChannelTransmissionPolicy.SetTransmissionToNone();
            }
            return VCSStatus(0);
        }

        void NextState(const AccountName &accountName, const std::string &accountHandle)
        {
            std::set<Channel *> channelsToDisconnect;
            std::set<Channel *> channelsToConnect;
            std::set<Channel *> connectedChannels;
			std::set<Channel *> channelsDisconnecting;
            bool currentlyConnectingChannel = false;

            SetSessionGroupHandle(accountName, accountHandle);

            for(std::map<Uri, Channel *>::const_iterator i = m_channels.begin();i!=m_channels.end();++i) {
                if(i->second->GetDesiredState() == Channel::ChannelStateDisconnected && (i->second->GetCurrentState() == Channel::ChannelStateConnected)) {
                    channelsToDisconnect.insert(i->second);
					channelsDisconnecting.insert(i->second); // this channel will be moving to the disconnecting state before the check below.
                }
                if(i->second->GetDesiredState() == Channel::ChannelStateConnected && (i->second->GetCurrentState() == Channel::ChannelStateDisconnected)) {
                    channelsToConnect.insert(i->second);
                }
                if(i->second->GetDesiredState() == Channel::ChannelStateConnected && i->second->GetCurrentState() == Channel::ChannelStateConnected) {
                    connectedChannels.insert(i->second);
                }
				if (i->second->GetCurrentState() == Channel::ChannelStateDisconnecting) {
					channelsDisconnecting.insert(i->second);
				}
                currentlyConnectingChannel |= i->second->GetCurrentState() == Channel::ChannelStateConnecting;
            }

            // This is tricky.
            // If we have zero channels, only add one
			// (Don't begin connecting a channel if another is already connecting or disconnecting)
			if (!currentlyConnectingChannel && !channelsToConnect.empty() && channelsDisconnecting.empty()) {
                (*channelsToConnect.begin())->NextState(m_sessionGroupHandle, m_accountName);
                return;
            }

			//Disconnect from channels before joining any new channels.
			for (std::set<Channel *>::const_iterator i = channelsToDisconnect.begin(); i != channelsToDisconnect.end(); ++i) {
				(*i)->NextState(m_sessionGroupHandle, m_accountName);
			}
			// Wait for disconnecting channel to completely disconnect before adding more channels to a session group
			if (channelsDisconnecting.empty() && !connectedChannels.empty()) {
                for(std::set<Channel *>::const_iterator i = channelsToConnect.begin();i!=channelsToConnect.end();++i) {
                    (*i)->NextState(m_sessionGroupHandle, m_accountName);
                }
            }

            if (!m_channelTransmissionPolicyRequestInProgress) {
                if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() != m_currentChannelTransmissionPolicy.GetChannelTransmissionPolicy()) {
					switch (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy()) {
                    case ChannelTransmissionPolicy::vx_channel_transmission_policy_specific_channel:
                        Channel *s;
                        s= FindChannel(m_desiredChannelTransmissionPolicy.GetSpecificTransmissionChannel());
                        if (s != NULL) {
                            s->SetTransmissionToThisChannel();
                        }
                        break;
                    case ChannelTransmissionPolicy::vx_channel_transmission_policy_all:
                    {
                        m_channelTransmissionPolicyRequestInProgress = true;
                        vx_req_sessiongroup_set_tx_all_sessions_t *req_all = nullptr;
                        CHECK_STATUS_RET(vx_req_sessiongroup_set_tx_all_sessions_create(&req_all));
                        req_all->sessiongroup_handle = vx_strdup(m_sessionGroupHandle.c_str());
                        issueRequest(&req_all->base);
                    }
                        break;
                    case ChannelTransmissionPolicy::vx_channel_transmission_policy_none:
                    {
                        m_channelTransmissionPolicyRequestInProgress = true;
                        vx_req_sessiongroup_set_tx_no_session_t *req_none = nullptr;
                        CHECK_STATUS_RET(vx_req_sessiongroup_set_tx_no_session_create(&req_none));
                        req_none->sessiongroup_handle = vx_strdup(m_sessionGroupHandle.c_str());
                        issueRequest(&req_none->base);
                    }
                        break;
                    default:
                        break;
                    }

				}
				else if (m_desiredChannelTransmissionPolicy.GetSpecificTransmissionChannel() !=
					m_currentChannelTransmissionPolicy.GetSpecificTransmissionChannel()) {

					if (Channel *s = FindChannel(m_desiredChannelTransmissionPolicy.GetSpecificTransmissionChannel())) {
                    m_channelTransmissionPolicyRequestInProgress = true;
                        s->SetTransmissionToThisChannel();
                    }
                }
            }

			// now step through each of the channels connected for any media state changes.
			for (std::set<Channel *>::const_iterator i = connectedChannels.begin(); i != connectedChannels.end(); ++i) {
				(*i)->NextState(m_sessionGroupHandle, m_accountName);
			}
        }

        const std::string &GetSessionGroupHandle() const { return m_sessionGroupHandle; }

        void HandleResponse(vx_resp_sessiongroup_add_session *resp)
        {
            vx_req_sessiongroup_add_session *req = reinterpret_cast<vx_req_sessiongroup_add_session *>(resp->base.request);
            CHECK_RET(!m_accountHandle.empty());
            Channel *c = FindChannelBySessionHandle(req->session_handle);
            CHECK_RET(c != NULL);
            if(resp->base.return_code == 1) {
                if(c->GetDesiredState() == Channel::ChannelStateConnected) {
                    m_app->onChannelJoinFailed(m_accountName, c->GetUri(), VCSStatus(resp->base.status_code, resp->base.status_string));
                    m_channels.erase(c->GetUri());
                    delete c;
                }
            }
        }

        void HandleResponse(vx_resp_sessiongroup_remove_session *resp)
        {
            vx_req_sessiongroup_remove_session *req = reinterpret_cast<vx_req_sessiongroup_remove_session *>(resp->base.request);
            CHECK_RET(!m_accountHandle.empty());
            Channel *c = FindChannelBySessionHandle(req->session_handle);
            CHECK_RET(c != NULL);
            if(resp->base.return_code == 1) {
                if(c->GetDesiredState() == Channel::ChannelStateConnected) {
                    m_app->onChannelJoinFailed(m_accountName, c->GetUri(), VCSStatus(resp->base.status_code, resp->base.status_string));
                    m_channels.erase(c->GetUri());
                    delete c;
                }
            }
        }

        void HandleResponse(vx_resp_session_set_local_speaker_volume *resp)
        {
            vx_req_session_set_local_speaker_volume_t *req = reinterpret_cast<vx_req_session_set_local_speaker_volume_t *>(resp->base.request);
            CHECK_RET(!m_accountHandle.empty());
            Channel *c = FindChannelBySessionHandle(req->session_handle);
            CHECK_RET(c != NULL);
            c->HandleResponse(resp);
            c->NextState(m_sessionGroupHandle, m_accountName);
        }

        void HandleResponse(vx_resp_session_set_participant_volume_for_me *resp)
        {
            vx_req_session_set_participant_volume_for_me_t *req = reinterpret_cast<vx_req_session_set_participant_volume_for_me_t *>(resp->base.request);
            CHECK_RET(!m_accountHandle.empty());
            Channel *c = FindChannelBySessionHandle(req->session_handle);
            CHECK_RET(c != NULL);
            c->HandleResponse(resp);
            c->NextState(m_sessionGroupHandle, m_accountName);
        }

        void HandleResponse(vx_resp_channel_mute_user *resp)
        {
            vx_req_channel_mute_user_t *req = reinterpret_cast<vx_req_channel_mute_user_t *>(resp->base.request);
            CHECK_RET(!m_accountHandle.empty());
            Channel *c = FindChannel(Uri(req->channel_uri));
            CHECK_RET(c != NULL);
			if (c == NULL) return;
            c->HandleResponse(resp);
            c->NextState(m_sessionGroupHandle, m_accountName);
        }

        void HandleResponse(vx_resp_session_set_participant_mute_for_me *resp)
        {
            vx_req_session_set_participant_mute_for_me_t *req = reinterpret_cast<vx_req_session_set_participant_mute_for_me_t *>(resp->base.request);
            CHECK_RET(!m_accountHandle.empty());
            Channel *c = FindChannelBySessionHandle(req->session_handle);
            CHECK_RET(c != NULL);
            c->HandleResponse(resp);
            c->NextState(m_sessionGroupHandle, m_accountName);
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_session *resp)
        {
            vx_req_sessiongroup_set_tx_session_t *req = reinterpret_cast<vx_req_sessiongroup_set_tx_session_t *>(resp->base.request);
            Channel *c = FindChannelBySessionHandle(req->session_handle);
            CHECK_RET(c != NULL);
            if (resp->base.return_code != 0) {
                if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() == ChannelTransmissionPolicy::vx_channel_transmission_policy_specific_channel)
                    m_desiredChannelTransmissionPolicy.SetChannelTransmissionPolicy(m_currentChannelTransmissionPolicy.GetChannelTransmissionPolicy());
                if (m_desiredChannelTransmissionPolicy.GetSpecificTransmissionChannel() == c->GetUri())
                    m_desiredChannelTransmissionPolicy.SetSpecificTransmissionChannel(m_currentChannelTransmissionPolicy.GetSpecificTransmissionChannel());
                m_app->onSetChannelTransmissionToSpecificChannelFailed(m_accountName, c->GetUri(), VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_currentChannelTransmissionPolicy.SetTransmissionToSpecificChannel(c->GetUri());
                m_app->onSetChannelTransmissionToSpecificChannelCompleted(m_accountName, c->GetUri());
            }
            m_channelTransmissionPolicyRequestInProgress = false;
            NextState(m_accountName, m_accountHandle);
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_all_sessions *resp)
        {
            if (resp->base.return_code != 0) {
                if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() == ChannelTransmissionPolicy::vx_channel_transmission_policy_all)
                    m_desiredChannelTransmissionPolicy.SetChannelTransmissionPolicy(m_currentChannelTransmissionPolicy.GetChannelTransmissionPolicy());
                m_app->onSetChannelTransmissionToAllFailed(m_accountName, VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_currentChannelTransmissionPolicy.SetTransmissionToAll();
                m_app->onSetChannelTransmissionToAllCompleted(m_accountName);
            }
            m_channelTransmissionPolicyRequestInProgress = false;
            NextState(m_accountName, m_accountHandle);
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_no_session *resp)
        {
            if (resp->base.return_code != 0) {
                if (m_desiredChannelTransmissionPolicy.GetChannelTransmissionPolicy() == ChannelTransmissionPolicy::vx_channel_transmission_policy_none)
                    m_desiredChannelTransmissionPolicy.SetChannelTransmissionPolicy(m_currentChannelTransmissionPolicy.GetChannelTransmissionPolicy());
                m_app->onSetChannelTransmissionToNoneFailed(m_accountName, VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_currentChannelTransmissionPolicy.SetTransmissionToNone();
                m_app->onSetChannelTransmissionToNoneCompleted(m_accountName);
            }
            m_channelTransmissionPolicyRequestInProgress = false;
            NextState(m_accountName, m_accountHandle);
        }

        void HandleEvent(vx_evt_media_stream_updated *evt)
        {
			Channel *c = FindChannelBySessionHandle(evt->session_handle);
			CHECK_RET(c != NULL);

			// evt states are
			// Connecting,  nothing to do,  just a progress msg
			// connected,  user has completed the signalling,  next msg will be a participant added for himself.
			// disconnecting, call is being torn down, progress msg
			// disconnected, call is terminated either normally of because of an error

			if (evt->state == session_media_disconnected && evt->status_code != 0) {
				// hit an error connecting to or while in a channel,  do not retry to join the channel.
				c->SetDesiredState(Channel::ChannelStateDisconnected);
				c->SetCurrentState(Channel::ChannelStateDisconnected);
				m_app->onChannelExited(m_accountName, c->GetUri(), VCSStatus(evt->status_code, evt->status_string));
				// delete the channel from the map of channels.
				m_channels.erase(c->GetUri());
				delete c;
			}
			else if (evt->state == session_media_disconnecting) {
				// Not much of anything to do,  might consider moving the DesiredState to Disconnected
				if (evt->call_stats != NULL) {
					m_app->onCallStatsUpdated(m_accountName, *evt->call_stats, true);
				}
			}
			else if (evt->state == session_media_disconnected) {
				// no status_code, leaving the channel,  therefore the player must of initiated the request to leave. 
				// the the channelstate should be set on the session_removed event, the player could still be connected to the text plane
				c->SetCurrentState(Channel::ChannelStateDisconnected);
				m_app->onChannelExited(m_accountName, c->GetUri(), VCSStatus(evt->status_code, evt->status_string));

				if (evt->call_stats != NULL) {
					m_app->onCallStatsUpdated(m_accountName, *evt->call_stats, true);
				}

				// delete the channel from the map of channels.
				m_channels.erase(c->GetUri());
				delete c;
			}

			// no else for 'connected' event,  DesiredState changed to connected with the ParticipantAdded event arrives 

        }

        void HandleEvent(vx_evt_participant_added *evt)
        {
            Channel *c = FindChannelBySessionHandle(evt->session_handle);
            CHECK_RET(c != NULL);
			if (c != NULL) c->HandleEvent(evt);
        }

        void HandleEvent(vx_evt_participant_updated *evt)
        {
            Channel *c = FindChannelBySessionHandle(evt->session_handle);
            //CHECK_RET(c != NULL);
			if (c != NULL) c->HandleEvent(evt);
        }

        void HandleEvent(vx_evt_participant_removed *evt)
        {
            Channel *c = FindChannelBySessionHandle(evt->session_handle);
            CHECK_RET(c != NULL);
			if (c != NULL) c->HandleEvent(evt);
        }

        VCSStatus IssueGetStats(bool reset)
        {
            vx_req_sessiongroup_get_stats_t *req = nullptr;
            CHECK_STATUS_RETVAL(vx_req_sessiongroup_get_stats_create(&req));
            req->sessiongroup_handle = vx_strdup(GetSessionGroupHandle().c_str());
            req->reset_stats = reset ? 1 : 0;
            return issueRequest(&req->base);
        }

        bool IsUsingSessionHandle(const char *handle) const
        {
			if (FindChannelBySessionHandle(handle) != NULL) {
				return true;
			}
			return false;
        }

		bool HasConnectedChannel() const
		{
			for (std::map<Uri, Channel *>::const_iterator i = m_channels.begin(); i != m_channels.end(); ++i) {
				if (i->second->GetDesiredState() == Channel::ChannelStateConnected && i->second->GetCurrentState() == Channel::ChannelStateConnected) {
					return true;
				}
			}
			return false;
		}
    private:
        Channel * FindChannelBySessionHandle(const char *handle) const
        {
            CHECK_RET1(handle != NULL, NULL);
            CHECK_RET1(handle[0] != 0, NULL);
            for(std::map<Uri, Channel *>::const_iterator i = m_channels.begin();i!=m_channels.end();++i) {
                if(i->second->GetSessionHandle() == handle)
                    return i->second;
            }
            return NULL;
        }

		Channel * FindActiveSession() const
		{
			for (std::map<Uri, Channel *>::const_iterator i = m_channels.begin(); i != m_channels.end(); ++i) {
				if (i->second->GetCurrentState() == Channel::ChannelStateConnected)
					return i->second;
			}
			return NULL;
		}
        void SetSessionGroupHandle(const AccountName &accountName, const std::string &accountHandle)
        {
            CHECK_RET(!accountHandle.empty());
            if(m_sessionGroupHandle.empty()) {
                CHECK(m_accountHandle.empty());
                CHECK(!m_accountName.IsValid() || m_accountName == accountName);
                m_accountHandle = accountHandle;
                m_accountName = accountName;
                vx_req_sessiongroup_create_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_sessiongroup_create_create(&req));
                req->account_handle = vx_strdup(accountHandle.c_str());
                req->base.cookie = GetNextRequestId(NULL, "G");
                req->sessiongroup_handle = vx_strdup(req->base.cookie);
                m_sessionGroupHandle = req->sessiongroup_handle;
                issueRequest(&req->base);
            }
        }


        Channel *FindChannel(const Uri &channelUri) const
        {
            std::map<Uri, Channel *>::const_iterator i = m_channels.find(channelUri);
            if(i == m_channels.end())
                return NULL;
            return i->second;
        }

        std::string m_sessionGroupHandle;
        std::string m_accountHandle;
        AccountName m_accountName;
        ChannelTransmissionPolicy m_currentChannelTransmissionPolicy;
        ChannelTransmissionPolicy m_desiredChannelTransmissionPolicy;
        bool m_channelTransmissionPolicyRequestInProgress;

        std::map<Uri, Channel *> m_channels;
        IClientApiEventHandler *m_app;
    };

    class UserBlockPolicy {
    public:
        UserBlockPolicy() {
            m_currentBlocked = false;
            m_desiredBlocked = false;
        }
        UserBlockPolicy(const Uri &uri) {
            m_uri = uri;
            m_currentBlocked = false;
            m_desiredBlocked = false;
        }
        bool GetCurrentBlock() const {
            return m_currentBlocked;
        }
        void SetCurrentBlock(bool value) {
            m_currentBlocked = value;
        }
        bool GetDesiredBlock() const {
            return m_desiredBlocked;
        };
        void SetDesiredBlock(bool value) {
            m_desiredBlocked = value;
        }

    private:
        Uri m_uri;
        bool m_currentBlocked;
        bool m_desiredBlocked;
    };

    class SingleLoginMultiChannelManager {
    public:
        typedef enum {
            LoginStateLoggedOut,
            LoginStateLoggingIn,
            LoginStateLoggedIn,
            LoginStateLoggingOut
        } LoginState;

        SingleLoginMultiChannelManager(IClientApiEventHandler *app,
            const std::string &connectorHandle,
            const AccountName &name,
            const char *captureDevice,
            const char *renderDevice,
            bool multichannel) : m_app(app), m_sg(app), m_serial(0)
        {
            CHECK(!connectorHandle.empty());
            CHECK(name.IsValid());
            m_name = name;
            m_connectorHandle = connectorHandle;
            m_currentLoginState = LoginStateLoggedOut;
            m_desiredLoginState = LoginStateLoggedOut;
            m_captureDevice = _strdup(captureDevice ? captureDevice : "");
            m_renderDevice = _strdup(renderDevice ? renderDevice : "");
            m_multichannel = multichannel;
        }

        ~SingleLoginMultiChannelManager()
        {
            for(std::map<Uri, UserBlockPolicy *>::const_iterator i = m_userBlockPolicy.begin();i!=m_userBlockPolicy.end();++i) {
                delete i->second;
            }
            if(m_captureDevice) free(m_captureDevice);
            if(m_renderDevice) free(m_renderDevice);
        }

        VCSStatus Login(const char *password)
        {
            m_desiredLoginState = LoginStateLoggedIn;
            m_desiredPassword = safe_str(password);
            m_desiredLoginState = LoginStateLoggedIn;
            return VCSStatus(0);
        }

        VCSStatus NextState(VCSStatus status)
        {
            NextState();
            return status;
        }

        void NextState()
        {
			
            if(m_currentLoginState == LoginStateLoggedOut && m_desiredLoginState == LoginStateLoggedIn) {
                vx_req_account_anonymous_login_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_account_anonymous_login_create(&req));
                req->connector_handle = vx_strdup(m_connectorHandle.c_str());
                req->base.cookie = GetNextRequestId(NULL, "A");
                req->account_handle = vx_strdup(req->base.cookie);
                req->participant_property_frequency = 100;
                req->enable_buddies_and_presence = 0;
                req->enable_presence_persistence = 0;
                req->participant_property_frequency = 100;
                m_accountHandle = req->account_handle;
                req->displayname = vx_strdup(m_name.ToString());
                req->acct_name = vx_strdup(m_name.ToString());

#ifdef USE_ACCESS_TOKENS
				m_sip_uri = std::string("sip:") + m_name.ToString() + g_domain_with_at;
				req->access_token = vx_debug_generate_token("demo-iss", time(0L)+180L, "login", m_serial++, NULL, m_sip_uri.c_str(), NULL, (unsigned char *)("demo-key"), sizeof("demo-key"));
#endif
				// PLK use password to store the access token
				req->access_token = vx_strdup(m_desiredPassword.c_str());

                m_currentLoginState = LoginStateLoggingIn;
                //m_currentPassword = m_desiredPassword;
                issueRequest(&req->base);
            } else if((m_currentLoginState == LoginStateLoggedIn || m_currentLoginState == LoginStateLoggingIn) && m_desiredLoginState == LoginStateLoggedOut) {
                vx_req_account_logout_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_account_logout_create(&req));
                req->account_handle = vx_strdup(m_accountHandle.c_str());
                m_currentLoginState = LoginStateLoggingOut;
                issueRequest(&req->base);
            }
            if(m_desiredLoginState == LoginStateLoggedIn && m_currentLoginState == LoginStateLoggedIn) {
                std::stringstream blocked;
                std::stringstream unblocked;
                const char *blockSep = "";
                const char *unblockSep = "";
                for(std::map<Uri, UserBlockPolicy *>::const_iterator i = m_userBlockPolicy.begin();i!=m_userBlockPolicy.end();++i) {
                    if(i->second->GetCurrentBlock() && !i->second->GetDesiredBlock()) {
                        unblocked << unblockSep << i->first.ToString();
                        unblockSep = "\n";
                    } else if(!i->second->GetCurrentBlock() && i->second->GetDesiredBlock()) {
                        blocked << blockSep << i->first.ToString();
                        blockSep = "\n";
                    }
                    i->second->SetCurrentBlock(i->second->GetDesiredBlock());
                }
                std::string blockedStr = blocked.str();
                std::string unblockedStr = unblocked.str();
                if(!blockedStr.empty()) {
                    vx_req_account_control_communications_t *req = nullptr;
                    CHECK_STATUS_RET(vx_req_account_control_communications_create(&req));
                    req->account_handle = vx_strdup(m_accountHandle.c_str());
                    req->user_uris = vx_strdup(blockedStr.c_str());
                    req->operation = vx_control_communications_operation_block;
                    issueRequest(&req->base);
                }

                if(!unblockedStr.empty()) {
                    vx_req_account_control_communications_t *req = nullptr;
                    CHECK_STATUS_RET(vx_req_account_control_communications_create(&req));
                    req->account_handle = vx_strdup(m_accountHandle.c_str());
                    req->user_uris = vx_strdup(unblockedStr.c_str());
                    req->operation = vx_control_communications_operation_unblock;
                    issueRequest(&req->base);
                }
                m_sg.NextState(m_name, m_accountHandle);
            }
        }

        void Logout()
        {
            if(m_desiredLoginState != LoginStateLoggedOut) {
                m_desiredLoginState = LoginStateLoggedOut;
            }
        }

        VCSStatus JoinChannel(const Uri &channelUri, const char *access_token)
        {
            return m_sg.JoinChannel(channelUri, access_token, m_multichannel);
        }

        VCSStatus LeaveChannel(const Uri &channelUri)
        {
            return m_sg.LeaveChannel(channelUri);
        }

        VCSStatus LeaveAll()
        {
            return m_sg.LeaveAll();
        }

        VCSStatus BlockUsers(const std::set<Uri> &usersToBlock)
        {
            for(std::set<Uri>::const_iterator i = usersToBlock.begin();i!=usersToBlock.end();++i) {
                std::map<Uri, UserBlockPolicy *>::const_iterator k = m_userBlockPolicy.find(*i);
                UserBlockPolicy *ubp;
                if(k == m_userBlockPolicy.end()) {
                    ubp = new UserBlockPolicy(*i);
                    m_userBlockPolicy[*i] = ubp;
                } else {
                    ubp = k->second;
                }
                ubp->SetDesiredBlock(true);
            }
            return VCSStatus(0);
        }

        VCSStatus UnblockUsers(const std::set<Uri> &usersToUnblock)
        {
            for(std::set<Uri>::const_iterator i = usersToUnblock.begin();i!=usersToUnblock.end();++i) {
                std::map<Uri, UserBlockPolicy *>::const_iterator k = m_userBlockPolicy.find(*i);
                if(k == m_userBlockPolicy.end()) {
                    continue;
                }
                k->second->SetDesiredBlock(false);
            }
            return VCSStatus(0);
        }

        bool CheckBlockedUser(const Uri &user)
        {
            return (m_actualBlockedPolicy.find(user) == m_actualBlockedPolicy.end());
        }

        VCSStatus IssueGetStats(bool reset)
        {
            return m_sg.IssueGetStats(reset);
        }

        VCSStatus StartPlayFileIntoChannels(const char *filename)
        {
            return m_sg.StartPlayFileIntoChannels(filename);
        }

        void StopPlayFileIntoChannels()
        {
            m_sg.StopPlayFileIntoChannels();
        }

        VCSStatus KickUser(const Uri &channel, const Uri &userUri)
        {
            vx_req_channel_kick_user_t *req;
            CHECK_STATUS_RETVAL(vx_req_channel_kick_user_create(&req));
            req->account_handle = vx_strdup(m_accountHandle.c_str());
            req->channel_uri = vx_strdup(channel.ToString());
            req->participant_uri = vx_strdup(userUri.ToString());
            return issueRequest(&req->base);
        }

        int GetChannelAudioOutputDeviceVolume(const Uri &channel) const
        {
            return m_sg.GetChannelAudioOutputDeviceVolume(channel);
        }

        VCSStatus SetChannelAudioOutputDeviceVolume(const Uri &channel, int volume)
        {
            return m_sg.SetChannelAudioOutputDeviceVolume(channel, volume);
        }

		VCSStatus SetSessionVolume(const Uri &channel, int volume)
		{
			return m_sg.SetSessionVolume(channel, volume);
		}

        int GetParticipantAudioOutputDeviceVolumeForMe(const Uri &target, const Uri &channel)
        {
            return m_sg.GetParticipantAudioOutputDeviceVolumeForMe(target, channel);
        }

        VCSStatus SetParticipantAudioOutputDeviceVolumeForMe(const Uri &target, const Uri &channel, int volume)
        {
            return m_sg.SetParticipantAudioOutputDeviceVolumeForMe(target, channel, volume);
        }

        VCSStatus SetParticipantMutedForAll(const Uri &target, const Uri &channel, bool muted) {
            return m_sg.SetParticipantMutedForAll(target, channel, muted);
        }


        bool GetParticipantMutedForAll(const Uri &target, const Uri &channel) const {
            return m_sg.GetParticipantMutedForAll(target, channel);
        }

        VCSStatus SetParticipantMutedForMe(const Uri &target, const Uri &channel, bool muted) {
            return m_sg.SetParticipantMutedForMe(target, channel, muted);
        }

        ChannelTransmissionPolicy GetChannelTransmissionPolicy() const {
            return m_sg.GetCurrentChannelTransmissionPolicy();
        }

		VCSStatus Set3DPosition(const Uri &channel, const Vector &speakerPosition, const Vector &listenerPosition, const Vector &listenerForward, const Vector &listenerUp) {
			return NextState(m_sg.Set3DPosition(channel, speakerPosition, listenerPosition, listenerForward, listenerUp));
		}

        VCSStatus SetTransmissionToSpecificChannel(const Uri &channel) {
            return NextState(m_sg.SetTransmissionToSpecificChannel(channel));
        }

        VCSStatus SetTransmissionToAll() {
            return NextState(m_sg.SetTransmissionToAll());
        }

        VCSStatus SetTransmissionToNone() {
            return NextState(m_sg.SetTransmissionToNone());
        }

        void HandleResponse(vx_resp_sessiongroup_add_session *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_sessiongroup_remove_session *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_sessiongroup_control_audio_injection_t *resp) {
            vx_req_sessiongroup_control_audio_injection_t *req = (vx_req_sessiongroup_control_audio_injection_t *)resp->base.request;
            if (resp->base.return_code != 0) {
                m_app->onStartPlayFileIntoChannelsFailed(m_name, req->filename, VCSStatus(resp->base.status_code, resp->base.status_string));
            } else {
                m_app->onStartPlayFileIntoChannels(m_name, req->filename);
            }
        }


        void HandleResponse(vx_resp_account_control_communications_t *resp)
        {
            if(resp->base.return_code == 0) {
                vx_req_account_control_communications_t *req = reinterpret_cast<vx_req_account_control_communications_t *>(resp->base.request);
                if(req->operation == vx_control_communications_operation_block) {
                    std::vector<std::string> blocked = split(req->user_uris);
                    for(std::vector<std::string>::const_iterator i = blocked.begin();i!=blocked.end();++i) {
                        m_actualBlockedPolicy.insert(Uri(i->c_str()));
                    }
                } else if(req->operation == vx_control_communications_operation_unblock) {
                    std::vector<std::string> blocked = split(req->user_uris);
                    for(std::vector<std::string>::const_iterator i = blocked.begin();i!=blocked.end();++i) {
                        m_actualBlockedPolicy.erase(Uri(i->c_str()));
                    }
                } else if(req->operation == vx_control_communications_operation_clear) {
                    m_actualBlockedPolicy.clear();
                }
            }
            NextState();
        }

        void HandleResponse(vx_resp_account_anonymous_login_t *resp)
        {
            vx_req_account_anonymous_login *req = reinterpret_cast<vx_req_account_anonymous_login *>(resp->base.request);
            CHECK_RET(req->account_handle == m_accountHandle);
            CHECK_RET(m_currentLoginState == LoginStateLoggingIn);
            if(m_desiredLoginState == LoginStateLoggedIn) {
                if(resp->base.return_code == 1) {
                    m_currentLoginState = LoginStateLoggedOut;
                    m_desiredLoginState = LoginStateLoggedOut;
                    m_app->onLoginFailed(m_name, VCSStatus(resp->base.status_code, resp->base.status_string));
                } else {
                    m_currentLoginState = m_desiredLoginState;
                    m_app->onLoginCompleted(m_name);
                }
            }
            NextState();
        }

        void HandleResponse(vx_resp_account_logout_t *resp)
        {
            vx_req_account_logout *req = reinterpret_cast<vx_req_account_logout *>(resp->base.request);
            CHECK_RET(req->account_handle == m_accountHandle);
            CHECK_RET(m_currentLoginState == LoginStateLoggingOut);
            if(m_desiredLoginState == LoginStateLoggedOut) {
                if(resp->base.return_code == 1) {
                    m_currentLoginState = LoginStateLoggedIn;
                    m_desiredLoginState = LoginStateLoggedIn;
                    m_app->onLogoutFailed(m_name, VCSStatus(resp->base.status_code, resp->base.status_string));
                } else {
                    m_currentLoginState = m_desiredLoginState;
                    m_sg.Clear();
                    m_app->onLogoutCompleted(m_name);
                }
            }
            NextState();
        }

        void HandleResponse(vx_resp_channel_kick_user_t *resp)
        {
            /// Check the return_code of the response to check for errors
            vx_req_channel_kick_user_t *req = (vx_req_channel_kick_user_t *)resp->base.request;
            if(resp->base.return_code != 0) {
                m_app->onParticipantKickFailed(m_name, Uri(req->channel_uri), Uri(req->participant_uri), VCSStatus(resp->base.status_code, resp->base.status_string));
            } else {
                m_app->onParticipantKickedCompleted(m_name, Uri(req->channel_uri), Uri(req->participant_uri));
            }
        }

        void HandleResponse(vx_resp_sessiongroup_create *resp)
        {
            CHECK_RET(resp->base.return_code == 0);
            CHECK_RET(m_sg.GetSessionGroupHandle() == resp->sessiongroup_handle);
        }

        void HandleResponse(vx_resp_session_set_local_speaker_volume *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_session_set_participant_volume_for_me *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_channel_mute_user *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_session_set_participant_mute_for_me *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_session *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_all_sessions *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_no_session *resp)
        {
            return m_sg.HandleResponse(resp);
        }

        void HandleEvent(vx_evt_account_login_state_change_t *evt)
        {
            (void)evt;
            /// This never seems to happen, and is redundant with
            /// HandleResponse(vx_resp_account_logout_t *resp)
            /*
            CHECK_RET(evt->account_handle == m_accountHandle);
            if(evt->state == login_state_logged_out) {
                if(m_currentLoginState == LoginStateLoggedIn) {
                    m_currentLoginState = LoginStateLoggedOut;
                    m_desiredLoginState = LoginStateLoggedOut;
                    m_app->onLogoutCompleted(m_name);
                }
            }
            NextState();
            */
        }

        void HandleEvent(vx_evt_media_stream_updated *evt)
        {
            m_sg.HandleEvent(evt);
            NextState();
        }

        void HandleEvent(vx_evt_participant_added *evt)
        {
            m_sg.HandleEvent(evt);
            NextState();
        }
        void HandleEvent(vx_evt_participant_updated *evt)
        {
            m_sg.HandleEvent(evt);
            NextState();
        }
        void HandleEvent(vx_evt_participant_removed *evt)
        {
            m_sg.HandleEvent(evt);
            NextState();
        }
        void HandleEvent(vx_evt_media_completion *evt)
        {
            if (evt->completion_type == sessiongroup_audio_injection) {
                m_app->onPlayFileIntoChannelsStopped(m_name, m_playingFile.c_str());
                NextState();
            }
        }

        const std::string &GetAccountHandle() const { return m_accountHandle; }
        const std::string &GetSessionGroupHandle() const { return m_sg.GetSessionGroupHandle(); }
        bool IsUsingSessionHandle(const char *handle) const { return m_sg.IsUsingSessionHandle(handle); }
		bool HasConnectedChannel() const { return m_sg.HasConnectedChannel(); }
    private:

        AccountName m_name;
        std::string m_sip_uri;  // Access Token uses this field,  set at login time
        int m_serial;           // Used by access token generator
        std::string m_accountHandle;
        std::string m_connectorHandle;

        LoginState m_desiredLoginState;
        std::string m_desiredPassword;

        LoginState m_currentLoginState;
        std::string m_currentPassword;
        std::string m_playingFile;
        IClientApiEventHandler *m_app;

        MultiChannelSessionGroup m_sg;

        std::map<Uri, UserBlockPolicy *> m_userBlockPolicy;
        std::set<Uri> m_actualBlockedPolicy;

        char *m_captureDevice;
        char *m_renderDevice;
        bool m_multichannel;
    };

    class ClientConnectionImpl
    {
    public:
        typedef enum {
            ConnectorStateUninitialized,
            ConnectorStateInitializing,
            ConnectorStateInitialized,
            ConnectorStateUninitializing
        } ConnectorState;

        ClientConnectionImpl()
        {
            Init();
        }

        ~ClientConnectionImpl()
        {
            Uninitialize();
        }

        VCSStatus Initialize(IClientApiEventHandler *app, IClientApiEventHandler::LogLevel level, bool multiChannel, bool multiLogin, vx_sdk_config_t *configHints, size_t configSize)
        {
            if(app == NULL) {
                return VCSStatus(VX_E_INVALID_ARGUMENT);
            }
            if(m_app != NULL) {
                return VCSStatus(VX_E_ALREADY_INITIALIZED);
            }
			if (configHints && configSize != sizeof(vx_sdk_config_t))
			{
				return VCSStatus(VX_E_INVALID_ARGUMENT);
			}

            m_multiChannel = multiChannel;
            m_multiLogin = multiLogin;

            vx_sdk_config_t config;
			if (configHints)
			{
				memcpy(&config, configHints, configSize);
			}
			else
			{
				int retval = vx_get_default_config3(&config, sizeof(config));
				if (retval != 0) {
					return VCSStatus(retval);
				}
			}

            m_loglevel = level;
            config.callback_handle = this;
            config.pf_sdk_message_callback = &sOnResponseOrEventFromSdk;
            config.pf_logging_callback = &sOnLogMessageFromSdk;
            config.initial_log_level = (vx_log_level)m_loglevel;
            config.allow_shared_capture_devices = 1;
#ifdef USE_ACCESS_TOKENS
			config.use_access_tokens = 1;  //Access Token setting
#endif
#ifdef VIVOX_SDK_HAS_ADVANCED_AUDIO_LEVELS
            config.enable_advanced_auto_levels = 1;
#endif
			config.use_os_proxy_settings = 1;

			config.pf_on_audio_unit_started = &sOnAudioUnitStarted;
			config.pf_on_audio_unit_stopped = &sOnAudioUnitStopped;
			config.pf_on_audio_unit_after_capture_audio_read = &sOnAudioUnitAfterCaptureAudioRead;
			config.pf_on_audio_unit_before_capture_audio_sent = &sOnAudioUnitBeforeCaptureAudioSent;
			config.pf_on_audio_unit_before_recv_audio_rendered = &sOnAudioUnitBeforeRecvAudioRendered;

            int retval = vx_initialize3(&config, sizeof(config));
            if(retval != 0) {
                return VCSStatus(retval);
            }
            m_app = app;
            
            /// Load local cache of audio input and output device member variables
            RequestAudioInputDevices();
            RequestAudioOutputDevices();

            while ( !m_audioInputDeviceListPopulated ||
                    !m_audioOutputDeviceListPopulated ){
                OnResponseOrEventFromSdkUiThread();
                sleepMicroseconds(100000);
            }

            return VCSStatus(0);
        }

        void Uninitialize()
        {
            if(m_app != NULL) {
                if(m_currentState == ConnectorStateInitialized || m_currentState == ConnectorStateInitializing) {
                    Disconnect(m_currentServer);
                }
                while(m_currentState == ConnectorStateUninitializing) {
                    // wait for the the response
                    WaitForShutdownResponse();
                    sleepMicroseconds(30000);
                }
                vx_uninitialize();
                m_app = NULL;
            }
            Init();
        }

        VCSStatus StartAudioOutputDeviceTest(const char *filename)
        {
            CHECK_RET1(filename && filename[0], VCSStatus(VX_E_INVALID_ARGUMENT));
#ifdef WIN32
            FILE *fp = NULL;
            fopen_s(&fp, filename, "r");
            CHECK_RET1(fp, VCSStatus(VX_E_FILE_OPEN_FAILED));
#else
            FILE *fp = fopen(filename, "r");
            CHECK_RET1(fp, VCSStatus(VX_E_FILE_OPEN_FAILED));
#endif
            fclose(fp);
            vx_req_aux_render_audio_start_t *req = nullptr;
            CHECK_STATUS_RETVAL(vx_req_aux_render_audio_start_create(&req));
            req->sound_file_path = vx_strdup(filename);
            req->loop = 1;
            issueRequest(&req->base);
            m_audioOutputDeviceTestIsRunning = true;
            return VCSStatus(0);
        }

        void StopAudioOutputDeviceTest()
        {
            if (m_audioOutputDeviceTestIsRunning) {
                vx_req_aux_render_audio_stop_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_aux_render_audio_stop_create(&req));
                issueRequest(&req->base);
                m_audioOutputDeviceTestIsRunning = false;
            }
        }

        bool AudioOutputDeviceTestIsRunning() const
        {
            return m_audioOutputDeviceTestIsRunning;
        }

        VCSStatus StartAudioInputDeviceTestRecord()
        {
            CHECK_RET1(m_audioOutputDeviceTestIsRunning == false, VCSStatus(VX_E_FAILED));
            CHECK_RET1(m_audioInputDeviceTestIsPlayingBack == false, VCSStatus(VX_E_FAILED));
            CHECK_RET1(m_audioInputDeviceTestIsRecording == false, VCSStatus(VX_E_FAILED));
            vx_req_aux_start_buffer_capture_t *req = nullptr;
            CHECK_STATUS_RETVAL(vx_req_aux_start_buffer_capture_create(&req));
            issueRequest(&req->base);
            m_audioInputDeviceTestIsRecording = true;
            return VCSStatus(0);
        }

        void StopAudioInputDeviceTestRecord()
        {
            if (m_audioInputDeviceTestIsRecording) {
                vx_req_aux_capture_audio_stop_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_aux_capture_audio_stop_create(&req));
                issueRequest(&req->base);
                m_audioInputDeviceTestIsRecording = false;
                m_audioInputDeviceTestHasAudioToPlayback = true;
            }
        }

        VCSStatus StartAudioInputDeviceTestPlayback()
        {
            CHECK_RET1(m_audioOutputDeviceTestIsRunning == false, VCSStatus(VX_E_FAILED));
            CHECK_RET1(m_audioInputDeviceTestIsPlayingBack == false, VCSStatus(VX_E_FAILED));
            CHECK_RET1(m_audioInputDeviceTestIsRecording == false, VCSStatus(VX_E_FAILED));
            vx_req_aux_play_audio_buffer_t *req = nullptr;
            CHECK_STATUS_RETVAL(vx_req_aux_play_audio_buffer_create(&req));
            issueRequest(&req->base);
            m_audioInputDeviceTestIsPlayingBack = true;
            return VCSStatus(0);
        }

        void StopAudioInputDeviceTestPlayback()
        {
            if (m_audioInputDeviceTestIsPlayingBack) {
                vx_req_aux_render_audio_stop_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_aux_render_audio_stop_create(&req));
                issueRequest(&req->base);
                m_audioInputDeviceTestIsPlayingBack = false;
            }
        }

        bool AudioInputDeviceTestIsRecording() const {
            return m_audioInputDeviceTestIsRecording;
        }
        bool AudioInputDeviceTestIsPlayingBack() const {
            return m_audioInputDeviceTestIsPlayingBack;
        }
        bool AudioInputDeviceTestHasAudioToPlayback() const {
            return m_audioInputDeviceTestHasAudioToPlayback;
        }
        VCSStatus Connect(const Uri &server)
        {
            CHECK_RET1(server.IsValid(), VCSStatus(VX_E_INVALID_ARGUMENT));

            m_desiredServer = server;
            m_desiredState = ConnectorStateInitialized;
            NextState();
            return VCSStatus(0);
        }

        void Disconnect(const Uri &server)
        {
            if(m_desiredState != ConnectorStateUninitialized) {
                CHECK_RET(m_desiredServer == server);
                // the act of disconnecting should clear all login information
                m_logins.clear();
                m_desiredServer.Clear();
                m_desiredState = ConnectorStateUninitialized;
                NextState();
            }
        }

        VCSStatus Login(const AccountName &accountName, const char *password, const char *captureDevice, const char *renderDevice)
        {
            CHECK_RET1(accountName.IsValid(), VCSStatus(VX_E_INVALID_ARGUMENT));
            CHECK_RET1(m_desiredServer.IsValid(), VCSStatus(VX_E_FAILED));

            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s == NULL) {
                s = new SingleLoginMultiChannelManager(m_app, m_connectorHandle, accountName, captureDevice, renderDevice, m_multiChannel);
                m_logins[accountName] = s;
            }
            if (m_multiLogin == false) {
                // logout everyone else
                for (std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.begin(); i != m_logins.end(); ++i) {
                    if (i->second != s) {
                        i->second->Logout();
                    }
                }
            }

            s->Login(password);
            NextState();
            return VCSStatus(0);
        }

        VCSStatus Logout(const AccountName &accountName)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s == NULL)
                return VCSStatus(VX_E_NO_EXIST);
            s->Logout();
            NextState();
            return VCSStatus(0);
        }

        VCSStatus JoinChannel(const AccountName &accountName, const Uri &channelUri, const char *access_token)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->JoinChannel(channelUri, access_token));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus LeaveChannel(const AccountName &accountName, const Uri &channelUri)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->LeaveChannel(channelUri));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus LeaveAll(const AccountName &accountName)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->LeaveAll());
            }
			return VCSStatus(VX_E_NO_EXIST);
		}

        VCSStatus BlockUsers(const AccountName &accountName, const std::set<Uri> &usersToBlock) {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->BlockUsers(usersToBlock));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus UnblockUsers(const AccountName &accountName, const std::set<Uri> &usersToUnblock) {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->UnblockUsers(usersToUnblock));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        bool CheckBlockedUser(const AccountName &accountName, const Uri &user) {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return s->CheckBlockedUser(user);
            }
            return false;
        }

        VCSStatus IssueGetStats(const AccountName &accountName, bool reset){
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return s->IssueGetStats(reset);
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus StartPlayFileIntoChannels(const AccountName &accountName, const char *filename) {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->StartPlayFileIntoChannels(filename));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus StopPlayFileIntoChannels(const AccountName &accountName) {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                s->StopPlayFileIntoChannels();
                NextState();
                return VCSStatus(0);
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus KickUser(const AccountName &accountName, const Uri &channelUri, const Uri &userUri) {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if(s) {
                return NextState(s->KickUser(channelUri, userUri));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        void RequestAudioInputDevices()
        {
            vx_req_aux_get_capture_devices_t *capture_req = nullptr;
            CHECK_STATUS_RET(vx_req_aux_get_capture_devices_create(&capture_req));
            issueRequest(&capture_req->base);
        }

        void RequestAudioOutputDevices()
        {
            vx_req_aux_get_render_devices_t *render_req = nullptr;
            CHECK_STATUS_RET(vx_req_aux_get_render_devices_create(&render_req));
            issueRequest(&render_req->base);
        }

        const std::vector<AudioDeviceId> &GetAudioInputDevices() const
        {
            return m_audioInputDeviceList;
        }

        AudioDeviceId GetApplicationChosenAudioInputDevice() const
        {
            if (AudioDevicePolicy::vx_audio_device_policy_default_system == m_currentAudioInputDevicePolicy.GetAudioDevicePolicy()) {
                return AudioDeviceId();
            } else {
                return m_currentAudioInputDevicePolicy.GetSpecificAudioDevice();
            }
        }

        const AudioDeviceId &GetOperatingSystemChosenAudioInputDevice() const
        {
            return m_operatingSystemChosenAudioInputDevice;
        }

        const AudioDevicePolicy &GetAudioInputDevicePolicy() const
        {
            return m_currentAudioInputDevicePolicy;
        }

        VCSStatus SetApplicationChosenAudioInputDevice(const AudioDeviceId &deviceName)
        {
#if !(_XBOX_ONE)
            CHECK_RET1(deviceName.IsValid(), VCSStatus(VX_E_INVALID_ARGUMENT));
            /// find in vector or return device not found
            AudioDeviceId validDevice;
            for(std::vector<AudioDeviceId>::const_iterator i =  m_audioInputDeviceList.begin();i!=m_audioInputDeviceList.end();++i) {
                if (*i == deviceName) {
                    validDevice = *i;
                    break;
                }
            }
            CHECK_RET1(validDevice.IsValid(), VCSStatus(VX_E_NO_EXIST));
#endif
            AudioDevicePolicy newPolicy(deviceName);
            if (!(m_desiredAudioInputDevicePolicy == newPolicy))
            {
                m_desiredAudioInputDevicePolicy.SetSpecificAudioDevice(deviceName);
                NextState();
            }
            return VCSStatus(0);
        }

        void UseOperatingSystemChosenAudioInputDevice()
        {
            if (m_desiredAudioInputDevicePolicy.GetAudioDevicePolicy() != AudioDevicePolicy::vx_audio_device_policy_default_system) {
                m_desiredAudioInputDevicePolicy.SetUseDefaultAudioDevice();
                NextState();
            }
        }

        bool IsUsingOperatingSystemChosenAudioInputDevice() const
        {
            return m_desiredAudioInputDevicePolicy.GetAudioDevicePolicy() == AudioDevicePolicy::vx_audio_device_policy_default_system;
        }

        const std::vector<AudioDeviceId> &GetAudioOutputDevices() const
        {
            return m_audioOutputDeviceList;
        }

        AudioDeviceId GetApplicationChosenAudioOutputDevice() const
        {
            if (AudioDevicePolicy::vx_audio_device_policy_default_system == m_currentAudioOutputDevicePolicy.GetAudioDevicePolicy()) {
                return AudioDeviceId();
            }
            else {
                return m_currentAudioOutputDevicePolicy.GetSpecificAudioDevice();
            }
        }

        const AudioDeviceId &GetOperatingSystemChosenAudioOutputDevice() const
        {
            return m_operatingSystemChosenAudioOutputDevice;
        }

        bool IsUsingOperatingSystemChosenAudioOutputDevice() const {
            return m_currentAudioOutputDevicePolicy.GetAudioDevicePolicy() == AudioDevicePolicy::vx_audio_device_policy_default_system;
        }

        VCSStatus SetApplicationChosenAudioOutputDevice(const AudioDeviceId &deviceName)
        {
#if !(_XBOX_ONE)
            CHECK_RET1(deviceName.IsValid(), VCSStatus(VX_E_INVALID_ARGUMENT));
            /// find in vector or return device not found
            AudioDeviceId validDevice;
            for (std::vector<AudioDeviceId>::const_iterator i = m_audioOutputDeviceList.begin();i!=m_audioOutputDeviceList.end();++i) {
                if (*i == deviceName) {
                    validDevice = *i;
                    break;
                }
            }
            CHECK_RET1(validDevice.IsValid(), VCSStatus(VX_E_NO_EXIST));
#endif
            if (m_desiredAudioOutputDevicePolicy.GetAudioDevicePolicy() != AudioDevicePolicy::vx_audio_device_policy_specific_device
                || m_desiredAudioOutputDevicePolicy.GetSpecificAudioDevice() != deviceName)
            {
                m_desiredAudioOutputDevicePolicy.SetSpecificAudioDevice(deviceName);
                NextState();
            }
            return VCSStatus(0);
        }

        void UseOperatingSystemChosenAudioOutputDevice()
        {
            if (m_desiredAudioOutputDevicePolicy.GetAudioDevicePolicy() != AudioDevicePolicy::vx_audio_device_policy_default_system) {
                m_desiredAudioOutputDevicePolicy.SetUseDefaultAudioDevice();
                NextState();
            }
        }

        const int GetMasterAudioInputDeviceVolume() const
        {
            return m_masterAudioInputDeviceVolume;
        }

        VCSStatus SetMasterAudioInputDeviceVolume(int volume)
        {
            if (volume == m_desiredAudioInputDeviceVolume) {
                return VCSStatus(0);
            }
            CHECK_RET1(volume >= VIVOX_MIN_VOL && volume <= VIVOX_MAX_VOL, VCSStatus(VX_E_INVALID_ARGUMENT));
            m_desiredAudioInputDeviceVolume = volume;
            NextState();
            return VCSStatus(0);
        }

        int GetMasterAudioOutputDeviceVolume() const
        {
            return m_masterAudioOutputDeviceVolume;
        }

        VCSStatus SetMasterAudioOutputDeviceVolume(int volume)
        {
            CHECK_RET1(volume >= VIVOX_MIN_VOL && volume <= VIVOX_MAX_VOL, VCSStatus(VX_E_INVALID_ARGUMENT));
            if (volume == m_desiredAudioOutputDeviceVolume) {
                return VCSStatus(0);
            }
            m_desiredAudioOutputDeviceVolume = volume;
            NextState();
            return VCSStatus(0);
        }

		VCSStatus SetVoiceActivateDetectionSensitivity(int sensitivity)
		{
			m_desiredVadSensitivity = sensitivity;
			NextState();
			return VCSStatus(0);
		}

		VCSStatus SetVADAutomaticParameterSelection(bool enabled)
		{
			m_desiredAutoVad = enabled;
			NextState();
			return VCSStatus(0);
		}

        int GetChannelAudioOutputDeviceVolume(const AccountName &accountName, const Uri &channelUri)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return s->GetChannelAudioOutputDeviceVolume(channelUri);
            }
            return 50; /// default value
        }

        VCSStatus SetChannelAudioOutputDeviceVolume(const AccountName &accountName, const Uri &channelUri, int volume)
        {
            CHECK_RET1(volume >= VIVOX_MIN_VOL && volume <= VIVOX_MAX_VOL, VCSStatus(VX_E_INVALID_ARGUMENT));
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetChannelAudioOutputDeviceVolume(channelUri, volume));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

		VCSStatus SetSessionVolume(const AccountName &accountName, const Uri &channelUri, int volume)
		{
			
			SingleLoginMultiChannelManager *s = FindLogin(accountName);
			if (s) {
				return NextState(s->SetSessionVolume(channelUri, volume));
			}
			return VCSStatus(VX_E_NO_EXIST);
		}

        int GetParticipantAudioOutputDeviceVolumeForMe(const AccountName &accountName, const Uri &target, const Uri &channelUri)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return s->GetParticipantAudioOutputDeviceVolumeForMe(target, channelUri);
            }
            return 50; /// default value
        }

        VCSStatus SetParticipantAudioOutputDeviceVolumeForMe(const AccountName &accountName, const Uri &target, const Uri &channelUri, int volume)
        {
            CHECK_RET1(volume >= VIVOX_MIN_VOL && volume <= VIVOX_MAX_VOL, VCSStatus(VX_E_INVALID_ARGUMENT));
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetParticipantAudioOutputDeviceVolumeForMe(target, channelUri, volume));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus SetParticipantMutedForAll(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetParticipantMutedForAll(target, channelUri, muted));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        bool GetParticipantMutedForAll(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri) const
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return s->GetParticipantMutedForAll(targetUser, channelUri);
            }
            return false;
        }


        VCSStatus SetParticipantMutedForMe(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetParticipantMutedForMe(target, channelUri, muted));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        ChannelTransmissionPolicy GetChannelTransmissionPolicy(const AccountName &accountName)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return s->GetChannelTransmissionPolicy();
            }
            return ChannelTransmissionPolicy(); /// default value
        }

        VCSStatus SetTransmissionToSpecificChannel(const AccountName &accountName, const Uri &channelUri)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetTransmissionToSpecificChannel(channelUri));
            }
            return VCSStatus(VX_E_NO_EXIST);
        }
		//VCSStatus IsChannelActive( const Uri &channelUri)
		//{
		//	SingleLoginMultiChannelManager *s = ;
		//	if (s) {
		//		//found channel,  check for active participants
		//	}
		//	return VX_E_NO_EXIST;  // not subscribed to this channel,  return an error
		//}

		VCSStatus Set3DPosition(const AccountName &accountName, const Uri &channelUri, const Vector &speakerPosition, const Vector &listenerPosition, const Vector &listenerForward, const Vector &listenerUp)
		{
			SingleLoginMultiChannelManager *s = FindLogin(accountName);
			if (s) {
				return NextState(s->Set3DPosition(channelUri, speakerPosition, listenerPosition, listenerForward, listenerUp));
			}
			return VCSStatus(VX_E_NO_EXIST);
		}

        VCSStatus SetTransmissionToAll(const AccountName &accountName)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetTransmissionToAll());
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

        VCSStatus SetTransmissionToNone(const AccountName &accountName)
        {
            SingleLoginMultiChannelManager *s = FindLogin(accountName);
            if (s) {
                return NextState(s->SetTransmissionToNone());
            }
            return VCSStatus(VX_E_NO_EXIST);
        }

		bool HasConnectedChannel(const AccountName &accountName)
		{
			SingleLoginMultiChannelManager *s = FindLogin(accountName);
			if (!s ) {
				// user is not logged in,  cannot be in a channel.
				return false;
			}
			return s->HasConnectedChannel();
		}

    private:
        VCSStatus NextState(VCSStatus status)
        {
            NextState();
            return status;
        }

        void NextState()
        {
            // if we are where we want to be don't do anything
            if(m_desiredServer == m_currentServer && m_desiredState == m_currentState) {
            } else{
                if(m_desiredState == ConnectorStateInitialized) {
                    CHECK_RET(m_desiredServer.IsValid());
                    if(m_currentState == ConnectorStateUninitialized) {
                        CHECK_RET(m_connectorHandle.empty());
                        CHECK_RET(!m_currentServer.IsValid());
                        vx_req_connector_create_t *req = nullptr;
                        CHECK_STATUS_RET(vx_req_connector_create_create(&req));
                        req->acct_mgmt_server = vx_strdup(m_desiredServer.ToString());
                        req->application = vx_strdup(m_application.c_str());
                        req->base.cookie = GetNextRequestId(NULL, "C");
                        req->connector_handle = vx_strdup(req->base.cookie);
                        req->log_level = (vx_log_level)m_loglevel;
                        m_connectorHandle = req->connector_handle;
                        m_currentState = ConnectorStateInitializing;
                        m_currentServer = m_desiredServer;
                        issueRequest(&req->base);
                    }
                } else if(m_desiredState == ConnectorStateUninitialized) {
                    CHECK_RET(!m_desiredServer.IsValid());
                    if(m_currentState == ConnectorStateInitialized) {
                        CHECK_RET(m_currentServer.IsValid());
                        CHECK_RET(!m_connectorHandle.empty());
                        vx_req_connector_initiate_shutdown *req = nullptr;
                        CHECK_STATUS_RET(vx_req_connector_initiate_shutdown_create(&req));
                        req->connector_handle = vx_strdup(m_connectorHandle.c_str());
                        m_currentState = ConnectorStateUninitializing;
                        issueRequest(&req->base);
                    }
                }
            }
            // if we are connected to the right backend...
            if(m_desiredState == ConnectorStateInitialized && m_currentState == ConnectorStateInitialized && m_desiredServer == m_currentServer) {
                for(std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.begin();i!=m_logins.end();++i) {
                    i->second->NextState();
                }
            }
            // audio device and master volume states
            if (!(m_currentAudioInputDevicePolicy == m_desiredAudioInputDevicePolicy)) {
                /// This only puts in a change request if the effective device would change
                vx_req_aux_set_capture_device_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_aux_set_capture_device_create(&req));

                req->base.vcookie = new AudioDevicePolicy(m_desiredAudioInputDevicePolicy);

                req->capture_device_specifier = vx_strdup(AudioDeviceIdToCodePage(m_desiredAudioInputDevicePolicy.GetSpecificAudioDevice()).c_str());
                issueRequest(&req->base);
                m_currentAudioInputDevicePolicy = m_desiredAudioInputDevicePolicy;
            }
            if (!(m_currentAudioOutputDevicePolicy == m_desiredAudioOutputDevicePolicy)) {
                /// This only puts in a change request if the effective device would change
                vx_req_aux_set_render_device_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_aux_set_render_device_create(&req));

                req->base.vcookie = new AudioDevicePolicy(m_desiredAudioOutputDevicePolicy);

                req->render_device_specifier = vx_strdup(AudioDeviceIdToCodePage(m_desiredAudioOutputDevicePolicy.GetSpecificAudioDevice()).c_str());
                issueRequest(&req->base);
                m_currentAudioOutputDevicePolicy = m_desiredAudioOutputDevicePolicy;
            }
            if (m_masterAudioInputDeviceVolume != m_desiredAudioInputDeviceVolume) {
                if (!m_masterAudioInputDeviceVolumeRequestInProgress) {
                    vx_req_connector_set_local_mic_volume_t *req = nullptr;
                    CHECK_STATUS_RET(vx_req_connector_set_local_mic_volume_create(&req));
                    req->volume = m_desiredAudioInputDeviceVolume;
                    issueRequest(&req->base);
                    m_masterAudioInputDeviceVolumeRequestInProgress = true;
                    m_masterAudioInputDeviceVolume = req->volume;
                }
			}
            if (m_masterAudioOutputDeviceVolume != m_desiredAudioOutputDeviceVolume) {
                if (!m_masterAudioOutputDeviceVolumeRequestInProgress) {
                    vx_req_connector_set_local_speaker_volume_t *req = nullptr;
                    CHECK_STATUS_RET(vx_req_connector_set_local_speaker_volume_create(&req));
                    req->volume = m_desiredAudioOutputDeviceVolume;
                    issueRequest(&req->base);
                    m_masterAudioOutputDeviceVolumeRequestInProgress = true;
                    m_masterAudioOutputDeviceVolume = req->volume;
                }
			}
			if (m_autoVad != m_desiredAutoVad || (!m_autoVad && m_masterVadSensitivity != m_desiredVadSensitivity)) {
				if (!m_masterVoiceActivateDetectionRequestInProgress) {
					vx_req_aux_set_vad_properties_t *req = nullptr;
					CHECK_STATUS_RET(vx_req_aux_set_vad_properties_create(&req));
					req->vad_sensitivity = m_desiredVadSensitivity;
					req->vad_noise_floor = 576;
					req->vad_hangover = 2000;
					req->vad_auto = m_desiredAutoVad;

					issueRequest(&req->base);
					m_masterVoiceActivateDetectionRequestInProgress = true;
					m_masterVadSensitivity = req->vad_sensitivity;
					m_autoVad = m_desiredAutoVad;
				}
			}
        }

        SingleLoginMultiChannelManager *FindLogin(const AccountName &name) const
        {
            std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.find(name);
            if (i == m_logins.end()) {
                return NULL;
            }
            return i->second;
        }

        SingleLoginMultiChannelManager *FindLogin(const AccountName &name, const char *access_token)
        {
            std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.find(name);
            if(i == m_logins.end()) {
                if(access_token) {  
                    SingleLoginMultiChannelManager *s = new SingleLoginMultiChannelManager(m_app, m_connectorHandle, name, NULL, NULL, m_multiChannel);
                    m_logins[name] = s;
                    return s;
                } else {
                    return NULL;
                }
            }
            return i->second;
        }

        SingleLoginMultiChannelManager *FindLoginBySessionHandle(const char *sessionHandle)
        {
            for (std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.begin(); i != m_logins.end(); ++i) {
                if (i->second->IsUsingSessionHandle(sessionHandle)) {
                    return i->second;
                }
            }
            return NULL;
        }

        SingleLoginMultiChannelManager *FindLoginBySessionGroupHandle(const char *sessionGroupHandle)
        {
            for(std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.begin(); i!=m_logins.end();++i) {
                if(i->second->GetSessionGroupHandle() == sessionGroupHandle) {
                    return i->second;
                }
            }
            return NULL;
        }

        SingleLoginMultiChannelManager *FindLogin(const char *accountHandle) const
        {
            for(std::map<AccountName, SingleLoginMultiChannelManager *>::const_iterator i = m_logins.begin(); i!=m_logins.end();++i) {
                if(i->second->GetAccountHandle() == accountHandle) {
                    return i->second;
                }
            }
            return NULL;
        }

        static void sOnLogMessageFromSdk(void *callbackHandle, vx_log_level level, const char *source, const char* message)
        {
            ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
            pThis->OnLogMessage(level, source, message);
        }

        static void sOnResponseOrEventFromSdk(void *callbackHandle)
        {
            ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
            pThis->OnResponseOrEventFromSdk();
        }

        void OnLogMessage(vx_log_level level, const char *source, const char* message)
        {
            std::stringstream ss;
            ss << source << " - " << message;
#ifdef WIN32
            FILETIME ft;
            GetSystemTimeAsFileTime(&ft);
            ULARGE_INTEGER ul;
            ul.HighPart = ft.dwHighDateTime;
            ul.LowPart = ft.dwLowDateTime;
            m_app->onLogStatementEmitted((IClientApiEventHandler::LogLevel)level, ul.QuadPart, GetCurrentThreadId(), ss.str().c_str());
#else
            struct timeval tv;
            gettimeofday(&tv, NULL);
            long long tmp = tv.tv_sec;
            tmp *= 1000000;
            tmp += tv.tv_usec;
            m_app->onLogStatementEmitted((IClientApiEventHandler::LogLevel)level, tmp, 0, ss.str().c_str());
#endif
        }

        void OnResponseOrEventFromSdk()
        {
            if(m_app != NULL) {
                m_app->InvokeOnUIThread(&sOnResponseOrEventFromSdkUiThread, this);
            }
        }

        static void sOnResponseOrEventFromSdkUiThread(void *callbackHandle)
        {
            ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
            pThis->OnResponseOrEventFromSdkUiThread();
        }

		static void sOnAudioUnitStarted(void *callbackHandle, const char *sessionGroupHandle, const char *initialTargetUri)
		{
			ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
			pThis->OnAudioUnitStarted(sessionGroupHandle, initialTargetUri);
		}

		static void sOnAudioUnitStopped(void *callbackHandle, const char *sessionGroupHandle, const char *initialTargetUri)
		{
			ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
			pThis->OnAudioUnitStopped(sessionGroupHandle, initialTargetUri);
		}

		static void sOnAudioUnitAfterCaptureAudioRead(void *callbackHandle, const char *sessionGroupHandle, const char *initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame)
		{
			ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
			pThis->OnAudioUnitAfterCaptureAudioRead(sessionGroupHandle, initialTargetUri, pcmFrames, pcmFrameCount, audioFrameRate, channelsPerFrame);
		}

		static void sOnAudioUnitBeforeCaptureAudioSent(void *callbackHandle, const char *sessionGroupHandle, const char *initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, int speaking)
		{
			ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
			pThis->OnAudioUnitBeforeCaptureAudioSent(sessionGroupHandle, initialTargetUri, pcmFrames, pcmFrameCount, audioFrameRate, channelsPerFrame, speaking);
		}

		static void sOnAudioUnitBeforeRecvAudioRendered(void *callbackHandle, const char *sessionGroupHandle, const char *initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, int silence)
		{
			ClientConnectionImpl *pThis = reinterpret_cast<ClientConnectionImpl *>(callbackHandle);
			pThis->OnAudioUnitBeforeRecvAudioRendered(sessionGroupHandle, initialTargetUri, pcmFrames, pcmFrameCount, audioFrameRate, channelsPerFrame, silence);
		}

		void OnAudioUnitStarted(const char *sessionGroupHandle, const char *initialTargetUri)
		{
			m_app->onAudioUnitStarted(Uri(initialTargetUri));
		}

		void OnAudioUnitStopped(const char *sessionGroupHandle, const char *initialTargetUri)
		{
			m_app->onAudioUnitStopped(Uri(initialTargetUri));
		}

		void OnAudioUnitAfterCaptureAudioRead(const char *sessionGroupHandle, const char *initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame)
		{
			m_app->onAudioUnitAfterCaptureAudioRead(Uri(initialTargetUri), pcmFrames, pcmFrameCount, audioFrameRate, channelsPerFrame);
		}

		void OnAudioUnitBeforeCaptureAudioSent(const char *sessionGroupHandle, const char *initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, int speaking)
		{
			m_app->onAudioUnitBeforeCaptureAudioSent(Uri(initialTargetUri), pcmFrames, pcmFrameCount, audioFrameRate, channelsPerFrame, speaking != 0);
		}

		void OnAudioUnitBeforeRecvAudioRendered(const char *sessionGroupHandle, const char *initialTargetUri, short *pcmFrames, int pcmFrameCount, int audioFrameRate, int channelsPerFrame, int silence)
		{
			m_app->onAudioUnitBeforeRecvAudioRendered(Uri(initialTargetUri), pcmFrames, pcmFrameCount, audioFrameRate, channelsPerFrame, silence != 0);
		}

        void HandleResponse(vx_resp_connector_create *resp)
        {
            vx_req_connector_create_t *req = reinterpret_cast<vx_req_connector_create_t *>(resp->base.request);
            Uri server(req->acct_mgmt_server);
            if(server == m_currentServer) {
                if(resp->base.return_code == 0) {
                    m_currentState = ConnectorStateInitialized;
                }
            }
            if(m_desiredState == ConnectorStateInitialized && m_desiredServer == m_currentServer) {
                // case 1 - app is still waiting to connect
                if(resp->base.return_code == 1) {
                    m_desiredState = ConnectorStateUninitialized;
                    m_currentState = ConnectorStateUninitialized;
                    m_connectorHandle.clear();
                    m_desiredServer.Clear();
                    m_currentServer.Clear();
                    m_connectorHandle.clear(); // DOOMan: to allow another Connect() call after onConnectFailed()
                    m_app->onConnectFailed(server, VCSStatus(resp->base.status_code, resp->base.status_string));
                } else {
                    m_app->onConnectCompleted(server);
                }
            }
            NextState();
        }

        void HandleResponse(vx_resp_connector_initiate_shutdown *resp)
        {
            CHECK_RET(resp->base.return_code == 0);
            if(m_desiredState == ConnectorStateUninitialized) {
                 m_currentState = m_desiredState;
                 m_connectorHandle.clear(); // DOOMan: to allow another Connect() call after onDisconnect()
                 m_desiredServer.Clear();
                 Uri currentServer = m_currentServer;
                 m_currentServer.Clear();
                 m_app->onDisconnected(currentServer, VCSStatus(0));
            }
            NextState();
        }

        void HandleResponse(vx_resp_account_anonymous_login *resp)
        {
            vx_req_account_anonymous_login *req = reinterpret_cast<vx_req_account_anonymous_login *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLogin(req->account_handle);
            if(login != NULL) {
                login->HandleResponse(resp);
            }

            NextState();
        }

        void HandleResponse(vx_resp_account_logout *resp)
        {
            vx_req_account_logout *req = reinterpret_cast<vx_req_account_logout *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLogin(req->account_handle);
            if(login != NULL) {
                login->HandleResponse(resp);
            }

            NextState();
        }

        void HandleResponse(vx_resp_channel_kick_user *resp)
        {
            vx_req_channel_kick_user *req = reinterpret_cast<vx_req_channel_kick_user *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLogin(req->account_handle);
            if(login != NULL) {
                login->HandleResponse(resp);
            }

            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_create *resp)
        {
            vx_req_sessiongroup_create *req = reinterpret_cast<vx_req_sessiongroup_create *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLogin(req->account_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_get_stats *resp)
        {
            (void)resp;
            CHECK_RET(resp->base.return_code == 0);
            // m_app->onGetStats(resp);
        }

        void HandleResponse(vx_resp_sessiongroup_add_session *resp)
        {
            vx_req_sessiongroup_add_session *req = reinterpret_cast<vx_req_sessiongroup_add_session *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(req->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_remove_session *resp)
        {
            if (resp->base.return_code != 0) {
                //printf("Cannot Process vx_resp_sessiongroup_remove_session due to error: (%d) %s", resp->base.status_code, resp->base.status_string);
                return;
            }
            vx_req_sessiongroup_remove_session *req = reinterpret_cast<vx_req_sessiongroup_remove_session *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(req->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);
            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_control_audio_injection *resp)
        {
            vx_req_sessiongroup_control_audio_injection_t *req = (vx_req_sessiongroup_control_audio_injection_t *)resp->base.request;

            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(req->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_account_control_communications *resp)
        {
            vx_req_account_control_communications_t *req = reinterpret_cast<vx_req_account_control_communications_t *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLogin(req->account_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);
            NextState();
        }

        void HandleResponse(vx_resp_aux_get_capture_devices *resp)
        {
            if (resp->base.status_code == 0)
            {
                std::vector<AudioDeviceId> oldDevices = m_audioInputDeviceList;
                bool osChosenDeviceChanged = false;
                bool deviceListChanged = false;
                m_audioInputDeviceList.clear();

                for (int i = 0; i < resp->count; ++i) {
                    if (resp->capture_devices[i]->device_type == vx_device_type_specific_device)
                        m_audioInputDeviceList.push_back(AudioDeviceIdFromCodePage(resp->capture_devices[i]->device, resp->capture_devices[i]->display_name));
                }
                if (oldDevices != m_audioInputDeviceList) {
                    deviceListChanged = true;
                }

                if (m_operatingSystemChosenAudioInputDevice != AudioDeviceIdFromCodePage(resp->default_capture_device->device, resp->default_capture_device->display_name)) {
                    osChosenDeviceChanged = true;
                    m_operatingSystemChosenAudioInputDevice = AudioDeviceIdFromCodePage(resp->default_capture_device->device, resp->default_capture_device->display_name);
                }

                if (deviceListChanged)
                    m_app->onAvailableAudioDevicesChanged();

                if (osChosenDeviceChanged)
                    m_app->onOperatingSystemChosenAudioInputDeviceChanged(m_operatingSystemChosenAudioInputDevice);

                m_audioInputDeviceListPopulated = true;
            }
        }

        void HandleResponse(vx_resp_aux_get_render_devices *resp)
        {
            if (resp->base.status_code == 0)
            {
                std::vector<AudioDeviceId> oldDevices = m_audioOutputDeviceList;
                bool osChosenDeviceChanged = false;
                bool deviceListChanged = false;
                m_audioOutputDeviceList.clear();

                for (int i = 0; i < resp->count; ++i) {
                    if (resp->render_devices[i]->device_type == vx_device_type_specific_device)
                        m_audioOutputDeviceList.push_back(AudioDeviceIdFromCodePage(resp->render_devices[i]->device, resp->render_devices[i]->display_name));
                }
                if (oldDevices != m_audioOutputDeviceList) {
                    deviceListChanged = true;
                }

                if (m_operatingSystemChosenAudioOutputDevice != AudioDeviceIdFromCodePage(resp->default_render_device->device, resp->default_render_device->display_name)) {
                    osChosenDeviceChanged = true;
                    m_operatingSystemChosenAudioOutputDevice = AudioDeviceIdFromCodePage(resp->default_render_device->device, resp->default_render_device->display_name);
                }

                if (deviceListChanged)
                    m_app->onAvailableAudioDevicesChanged();

                if (osChosenDeviceChanged)
                    m_app->onOperatingSystemChosenAudioOutputDeviceChanged(m_operatingSystemChosenAudioOutputDevice);

                m_audioOutputDeviceListPopulated = true;
            }
        }

        void HandleResponse(vx_resp_aux_set_capture_device *resp)
        {
            vx_req_aux_set_capture_device_t *req = reinterpret_cast<vx_req_aux_set_capture_device_t *>(resp->base.request);
            AudioDevicePolicy *requestedDevicePolicy = (AudioDevicePolicy *)req->base.vcookie;
            if (resp->base.return_code != 0) {
                // setting "use system device" should never fail
                CHECK_RET(requestedDevicePolicy->GetAudioDevicePolicy() != AudioDevicePolicy::vx_audio_device_policy_default_system);
                // if we do fail, set the desired to the current
                m_desiredAudioInputDevicePolicy = m_currentAudioInputDevicePolicy;
                m_app->onSetApplicationChosenAudioOutputDeviceFailed(requestedDevicePolicy->GetSpecificAudioDevice(), VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_currentAudioInputDevicePolicy = *requestedDevicePolicy;
                if (requestedDevicePolicy->GetAudioDevicePolicy() == AudioDevicePolicy::vx_audio_device_policy_default_system) {
                } else {
                    m_app->onSetApplicationChosenAudioInputDeviceCompleted(requestedDevicePolicy->GetSpecificAudioDevice());
                }
            }
            NextState();
            delete requestedDevicePolicy;
        }

        void HandleResponse(vx_resp_aux_set_render_device *resp)
        {
            vx_req_aux_set_render_device_t *req = reinterpret_cast<vx_req_aux_set_render_device_t *>(resp->base.request);
            AudioDevicePolicy *requestedDevicePolicy = (AudioDevicePolicy *)req->base.vcookie;
            if (resp->base.return_code != 0) {
                // setting "use system device" should never fail
                CHECK_RET(requestedDevicePolicy->GetAudioDevicePolicy() != AudioDevicePolicy::vx_audio_device_policy_default_system);
                // if we do fail, set the desired to the current
                m_desiredAudioOutputDevicePolicy = m_currentAudioOutputDevicePolicy;
                m_app->onSetApplicationChosenAudioOutputDeviceFailed(requestedDevicePolicy->GetSpecificAudioDevice(), VCSStatus(resp->base.status_code, resp->base.status_string));
            }
            else {
                m_currentAudioOutputDevicePolicy = *requestedDevicePolicy;
                if (requestedDevicePolicy->GetAudioDevicePolicy() == AudioDevicePolicy::vx_audio_device_policy_default_system) {
                } else {
                    m_app->onSetApplicationChosenAudioOutputDeviceCompleted(m_currentAudioOutputDevicePolicy.GetSpecificAudioDevice());
                }
            }
            NextState();
            delete requestedDevicePolicy;
        }

		void HandleResponse(vx_resp_aux_set_vad_properties *resp)
		{
			CHECK(resp->base.return_code == 0);
			m_masterVoiceActivateDetectionRequestInProgress = false;
			NextState();
		}

        void HandleResponse(vx_resp_connector_set_local_mic_volume *resp)
        {
            CHECK(resp->base.return_code == 0);
            m_masterAudioInputDeviceVolumeRequestInProgress = false;
            NextState();
        }

        void HandleResponse(vx_resp_connector_set_local_speaker_volume *resp)
        {
            CHECK(resp->base.return_code == 0);
            m_masterAudioOutputDeviceVolumeRequestInProgress = false;
            NextState();
        }

        void HandleResponse(vx_resp_session_set_local_speaker_volume *resp)
        {
            vx_req_session_set_local_speaker_volume *req = reinterpret_cast<vx_req_session_set_local_speaker_volume *>(resp->base.request);
			if (resp->base.return_code != 0 && resp->base.status_code == 1001) return;  // an error, the session no longer exists
            SingleLoginMultiChannelManager *login = FindLoginBySessionHandle(req->session_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_session_set_participant_volume_for_me *resp)
        {
            vx_req_session_set_participant_volume_for_me *req = reinterpret_cast<vx_req_session_set_participant_volume_for_me *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLoginBySessionHandle(req->session_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_channel_mute_user *resp)
        {
            vx_req_channel_mute_user *req = reinterpret_cast<vx_req_channel_mute_user *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLogin(req->account_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_session_set_participant_mute_for_me *resp)
        {
            vx_req_session_set_participant_mute_for_me *req = reinterpret_cast<vx_req_session_set_participant_mute_for_me *>(resp->base.request);
            SingleLoginMultiChannelManager *login = FindLoginBySessionHandle(req->session_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_session *resp)
        {
            vx_req_sessiongroup_set_tx_session_t *req = (vx_req_sessiongroup_set_tx_session_t *)resp->base.request;

            SingleLoginMultiChannelManager *login = FindLoginBySessionHandle(req->session_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_all_sessions *resp)
        {
            vx_req_sessiongroup_set_tx_all_sessions_t *req = (vx_req_sessiongroup_set_tx_all_sessions_t *)resp->base.request;

            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(req->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_sessiongroup_set_tx_no_session *resp)
        {
            vx_req_sessiongroup_set_tx_no_session_t *req = (vx_req_sessiongroup_set_tx_no_session_t *)resp->base.request;

            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(req->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleResponse(resp);

            NextState();
        }

        void HandleResponse(vx_resp_aux_render_audio_start_t *resp)
        {
            CHECK_RET(resp->base.return_code != 1);
        }

        void HandleResponse(vx_resp_aux_render_audio_stop_t *resp)
        {
            CHECK_RET(resp->base.return_code != 1);
        }

        void DispatchResponse(vx_resp_base_t *resp)
        {
            switch(resp->type) {
            case resp_connector_create: return HandleResponse(reinterpret_cast<vx_resp_connector_create *>(resp));
            case resp_connector_initiate_shutdown: return HandleResponse(reinterpret_cast<vx_resp_connector_initiate_shutdown *>(resp));
            case resp_account_anonymous_login: return HandleResponse(reinterpret_cast<vx_resp_account_anonymous_login *>(resp));
            case resp_account_logout: return HandleResponse(reinterpret_cast<vx_resp_account_logout *>(resp));
            case resp_channel_kick_user: return HandleResponse(reinterpret_cast<vx_resp_channel_kick_user *>(resp));
            case resp_sessiongroup_create: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_create *>(resp));
            case resp_sessiongroup_get_stats: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_get_stats *>(resp));
            case resp_sessiongroup_add_session: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_add_session *>(resp));
            case resp_sessiongroup_remove_session: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_remove_session *>(resp));
            case resp_sessiongroup_control_audio_injection: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_control_audio_injection *>(resp));
            case resp_account_control_communications: return HandleResponse(reinterpret_cast<vx_resp_account_control_communications *>(resp));
            case resp_aux_get_capture_devices: return HandleResponse(reinterpret_cast<vx_resp_aux_get_capture_devices *>(resp));
            case resp_aux_get_render_devices: return HandleResponse(reinterpret_cast<vx_resp_aux_get_render_devices *>(resp));
            case resp_aux_set_capture_device: return HandleResponse(reinterpret_cast<vx_resp_aux_set_capture_device *>(resp));
            case resp_aux_set_render_device: return HandleResponse(reinterpret_cast<vx_resp_aux_set_render_device *>(resp));
            case resp_connector_set_local_mic_volume: return HandleResponse(reinterpret_cast<vx_resp_connector_set_local_mic_volume *>(resp));
            case resp_connector_set_local_speaker_volume: return HandleResponse(reinterpret_cast<vx_resp_connector_set_local_speaker_volume *>(resp));
            case resp_session_set_local_speaker_volume: return HandleResponse(reinterpret_cast<vx_resp_session_set_local_speaker_volume *>(resp));
            case resp_session_set_participant_volume_for_me: return HandleResponse(reinterpret_cast<vx_resp_session_set_participant_volume_for_me *>(resp));
            case resp_channel_mute_user: return HandleResponse(reinterpret_cast<vx_resp_channel_mute_user *>(resp));
            case resp_session_set_participant_mute_for_me: return HandleResponse(reinterpret_cast<vx_resp_session_set_participant_mute_for_me *>(resp));
            case resp_sessiongroup_set_tx_session: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_set_tx_session *>(resp));
            case resp_sessiongroup_set_tx_all_sessions: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_set_tx_all_sessions *>(resp));
            case resp_sessiongroup_set_tx_no_session: return HandleResponse(reinterpret_cast<vx_resp_sessiongroup_set_tx_no_session *>(resp));
            case resp_aux_render_audio_start: return HandleResponse(reinterpret_cast<vx_resp_aux_render_audio_start_t *>(resp));
            case resp_aux_render_audio_stop: return HandleResponse(reinterpret_cast<vx_resp_aux_render_audio_stop_t *>(resp));
			case resp_aux_set_vad_properties: return HandleResponse(reinterpret_cast<vx_resp_aux_set_vad_properties_t *>(resp));
			case resp_session_set_3d_position: return;
            case resp_aux_start_buffer_capture: return;
            case resp_aux_capture_audio_stop: return;
            case resp_aux_play_audio_buffer: return;
            case resp_connector_mute_local_mic: return;
            case resp_connector_mute_local_speaker: return;
            case resp_aux_notify_application_state_change: return;
            default:
                CHECK_RET(resp == NULL);
            }
        }

        void DispatchEvent(vx_evt_account_login_state_change *evt)
        {
            SingleLoginMultiChannelManager *login = FindLogin(evt->account_handle);
            if(login != NULL) {
                login->HandleEvent(evt);
            }
        }

        void DispatchEvent(vx_evt_media_stream_updated *evt)
        {
            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(evt->sessiongroup_handle);
            if (login == NULL) {
                CHECK_RET(evt->state != session_media_connected);
                return;
            }
            login->HandleEvent(evt);
        }

        void DispatchEvent(vx_evt_participant_added *evt)
        {
            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(evt->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleEvent(evt);
        }

        void DispatchEvent(vx_evt_participant_updated *evt)
        {
            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(evt->sessiongroup_handle);
            CHECK_RET(login != NULL);
            login->HandleEvent(evt);
        }

        void DispatchEvent(vx_evt_participant_removed *evt)
        {
            SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(evt->sessiongroup_handle);
            if (login == NULL)
                return;
            login->HandleEvent(evt);
        }

        void DispatchEvent(vx_evt_media_completion *evt)
        {
            // aux* requests will have no sessiongroup handle
            if (evt->sessiongroup_handle && evt->sessiongroup_handle[0]) {
                SingleLoginMultiChannelManager *login = FindLoginBySessionGroupHandle(evt->sessiongroup_handle);
                CHECK_RET(login != NULL);
                login->HandleEvent(evt);
            }
            else {
                if (evt->completion_type == aux_buffer_audio_render) {
                    if (m_audioInputDeviceTestIsPlayingBack) {
                        m_audioInputDeviceTestIsPlayingBack = false;
                        m_app->onAudioInputDeviceTestPlaybackCompleted();
                    }
                }
            }
        }

        void DispatchEvent(vx_evt_audio_device_hot_swap *evt)
        {
            switch (evt->event_type)
            {
            case vx_audio_device_hot_swap_event_type_disabled_due_to_platform_constraints:
                RequestAudioInputDevices();
                RequestAudioOutputDevices();
                break;
            case vx_audio_device_hot_swap_event_type_active_render_device_changed:
                RequestAudioOutputDevices();
                break;
            case vx_audio_device_hot_swap_event_type_active_capture_device_changed:
                RequestAudioInputDevices();
                break;
#ifdef VIVOX_SDK_HAS_DEVICE_ADDED_REMOVED
            case vx_audio_device_hot_swap_event_type_audio_device_added:
            case vx_audio_device_hot_swap_event_type_audio_device_removed:
                RequestAudioInputDevices();
                RequestAudioOutputDevices();
                break;
#endif
            default:
                break;
            }
        }

        void DispatchEvent(vx_evt_base_t *evt)
        {
            switch(evt->type) {
            case evt_account_login_state_change: return DispatchEvent(reinterpret_cast<vx_evt_account_login_state_change *>(evt));
            case evt_sessiongroup_added: return;
            case evt_sessiongroup_updated: return;
            case evt_sessiongroup_removed: return;
            case evt_session_added: return;
            case evt_session_updated: return;
            case evt_session_removed: return;
            case evt_media_stream_updated: return DispatchEvent(reinterpret_cast<vx_evt_media_stream_updated *>(evt));
            case evt_participant_added: return DispatchEvent(reinterpret_cast<vx_evt_participant_added *>(evt));
            case evt_participant_updated: return DispatchEvent(reinterpret_cast<vx_evt_participant_updated *>(evt));
            case evt_participant_removed: return DispatchEvent(reinterpret_cast<vx_evt_participant_removed *>(evt));
            case evt_media_completion: return DispatchEvent(reinterpret_cast<vx_evt_media_completion *>(evt));
            case evt_audio_device_hot_swap: return DispatchEvent(reinterpret_cast<vx_evt_audio_device_hot_swap *>(evt));
            case evt_aux_audio_properties: return;
            default:
                CHECK_RET(evt == NULL);
            }
        }

        void OnResponseOrEventFromSdkUiThread()
        {
            for(;;) {
                vx_message_base_t *m = NULL;
                vx_get_message(&m);
                if(m == 0)
                    break;
                if(m->type == msg_response) {
                    DispatchResponse(reinterpret_cast<vx_resp_base_t *>(m));
                } else {
                    DispatchEvent(reinterpret_cast<vx_evt_base_t *>(m));
                }
                vx_destroy_message(m);
            }
        }

    public:
        VCSStatus CreateCaptureDevice(vxa_apcd *capture_device, int *apcd_id)
        {
            return VCSStatus(vxa_apcd_create(capture_device, apcd_id));
        }

        VCSStatus DestroyCaptureDevice(int apcd_id)
        {
            return VCSStatus(vxa_apcd_destroy(apcd_id));
        }

        VCSStatus CreateRenderDevice(vxa_aprd *render_device, int *aprd_id)
        {
            return VCSStatus(vxa_aprd_create(render_device, aprd_id));
        }

        VCSStatus DestroyRenderDevice(int aprd_id)
        {
            return VCSStatus(vxa_aprd_destroy(aprd_id));
        }

        void SetAudioOutputDeviceMuted(bool value)
        {
            if (value != m_audioOutputDeviceMuted) {
                m_audioOutputDeviceMuted = value;
                vx_req_connector_mute_local_speaker_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_connector_mute_local_speaker_create(&req));
                req->mute_level = value ? 1 : 0;
                vx_issue_request(&req->base);
            }
        }

        bool GetAudioOutputDeviceMuted() const
        {
            return m_audioOutputDeviceMuted;
        }

        void SetAudioInputDeviceMuted(bool value)
        {
            if (value != m_audioInputDeviceMuted) {
                m_audioInputDeviceMuted = value;
                vx_req_connector_mute_local_mic_t *req = nullptr;
                CHECK_STATUS_RET(vx_req_connector_mute_local_mic_create(&req));
                req->mute_level = value ? 1 : 0;
                vx_issue_request(&req->base);
            }
        }

        bool GetAudioInputDeviceMuted() const
        {
            return m_audioInputDeviceMuted;
        }
        
        ///
        /// Called by the application when the application entered the background.
        ///
        /// Mobile platforms only.
        ///
        void EnteredBackground()
        {
            vx_req_aux_notify_application_state_change_t *req = nullptr;
            CHECK_STATUS_RET(vx_req_aux_notify_application_state_change_create(&req));
            req->notification_type = vx_application_state_notification_type_before_background;
            issueRequest(&req->base);
        }
        
        ///
        /// Called by the application whether the application is about to enter the foreground
        ///
        /// Mobile platforms only.
        ///
        void WillEnterForeground()
        {
            vx_req_aux_notify_application_state_change_t *req = nullptr;
            CHECK_STATUS_RET(vx_req_aux_notify_application_state_change_create(&req));
            req->notification_type = vx_application_state_notification_type_after_foreground;
            issueRequest(&req->base);
        }
        
        ///
        /// Called by the application periodically when the application is in the background
        ///
        /// Mobile platforms only.
        ///
        void OnBackgroundIdleTimeout()
        {
            vx_req_aux_notify_application_state_change_t *req = nullptr;
            CHECK_STATUS_RET(vx_req_aux_notify_application_state_change_create(&req));
            req->notification_type = vx_application_state_notification_type_periodic_background_idle;
            issueRequest(&req->base);
        }
    private:

        void WaitForShutdownResponse()
        {
            for(;;) {
                vx_message_base_t *m = NULL;
                vx_get_message(&m);
                if(m == 0)
                    break;
                if(m->type == msg_response) {
                    vx_resp_base_t *r = reinterpret_cast<vx_resp_base_t *>(m);
                    if(r->type == resp_connector_initiate_shutdown) {
                        DispatchResponse(r);
                    }
                }
                vx_destroy_message(m);
            }
        }

        IClientApiEventHandler *m_app;

        std::string m_application;

        Uri m_desiredServer;
        ConnectorState m_desiredState;

        Uri m_currentServer;
        ConnectorState m_currentState;

        std::string m_connectorHandle;

        std::map<AccountName, SingleLoginMultiChannelManager *> m_logins;

        bool m_multiChannel;
        bool m_multiLogin;
        IClientApiEventHandler::LogLevel m_loglevel;

        std::vector<AudioDeviceId> m_audioOutputDeviceList;
        std::vector<AudioDeviceId> m_audioInputDeviceList;

        bool m_audioInputDeviceListPopulated;
        bool m_audioOutputDeviceListPopulated;

        AudioDeviceId m_operatingSystemChosenAudioInputDevice;
        AudioDeviceId m_operatingSystemChosenAudioOutputDevice;

        AudioDevicePolicy m_currentAudioInputDevicePolicy;
        AudioDevicePolicy m_currentAudioOutputDevicePolicy;
        AudioDevicePolicy m_desiredAudioInputDevicePolicy;
        AudioDevicePolicy m_desiredAudioOutputDevicePolicy;

        int m_masterAudioInputDeviceVolume;
        int m_masterAudioOutputDeviceVolume;
        int m_desiredAudioInputDeviceVolume;
        int m_desiredAudioOutputDeviceVolume;

		bool m_autoVad;
		bool m_desiredAutoVad;
		int m_masterVadSensitivity;
		int m_desiredVadSensitivity;

        bool m_masterAudioInputDeviceVolumeRequestInProgress;
		bool m_masterAudioOutputDeviceVolumeRequestInProgress;
		bool m_masterVoiceActivateDetectionRequestInProgress;

        bool m_audioOutputDeviceTestIsRunning;
        bool m_audioInputDeviceTestIsRecording;
        bool m_audioInputDeviceTestIsPlayingBack;
        bool m_audioInputDeviceTestHasAudioToPlayback;

        bool m_audioInputDeviceMuted;
        bool m_audioOutputDeviceMuted;
    private:
        void Init()
        {
            //TODO: Clean up more variables.

            m_app = NULL;
            m_desiredState = ConnectorStateUninitialized;
            m_currentState = ConnectorStateUninitialized;
            m_connectorHandle.clear();
            m_logins.clear();
            m_multiChannel = false;
            m_multiLogin = false;
            m_audioOutputDeviceList.clear();
            m_audioInputDeviceList.clear();
            m_audioInputDeviceListPopulated = false;
            m_audioOutputDeviceListPopulated = false;
            m_masterAudioInputDeviceVolume = 50;
			m_masterAudioOutputDeviceVolume = 50;
			m_masterVadSensitivity = 43;
			m_autoVad = true;
            m_desiredAudioInputDeviceVolume = 50;
			m_desiredAudioOutputDeviceVolume = 50;
			m_desiredVadSensitivity = 43;
			m_desiredAutoVad = false;
            m_masterAudioInputDeviceVolumeRequestInProgress = false;
            m_masterAudioOutputDeviceVolumeRequestInProgress = false;
			m_masterVoiceActivateDetectionRequestInProgress = false;
            m_audioOutputDeviceTestIsRunning = false;
            m_audioInputDeviceTestIsRecording = false;
            m_audioInputDeviceTestIsPlayingBack = false;
            m_audioInputDeviceTestHasAudioToPlayback = false;
            m_audioInputDeviceMuted = false;
            m_audioOutputDeviceMuted = false;
			m_application = "SApi";   //TODO  this value could be a changed to a short value to identify your application.
        }
    };


    ClientConnection::ClientConnection()
    {
        m_pImpl = new ClientConnectionImpl();
    }

    ClientConnection::~ClientConnection()
    {
        delete m_pImpl;
    }

    VCSStatus ClientConnection::Initialize(IClientApiEventHandler *app, IClientApiEventHandler::LogLevel logLevel, bool multiChannel, bool multiLogin, vx_sdk_config_t *configHints, size_t configSize)
    {
        return m_pImpl->Initialize(app, logLevel, multiChannel, multiLogin, configHints, configSize);
    }

    void ClientConnection::Uninitialize()
    {
        return m_pImpl->Uninitialize();
    }

#ifdef _XBOX
    VCSStatus ClientConnection::CreateCaptureDevice(vxa_apcd *capture_device, int *apcd_id)
    {
        return m_pImpl->CreateCaptureDevice(capture_device, apcd_id);
    }

    VCSStatus ClientConnection::DestroyCaptureDevice(int apcd_id)
    {
        return m_pImpl->DestroyCaptureDevice(apcd_id);
    }

    VCSStatus ClientConnection::CreateRenderDevice(vxa_aprd *render_device, int *aprd_id)
    {
        return m_pImpl->CreateRenderDevice(render_device, aprd_id);
    }

    VCSStatus ClientConnection::DestroyRenderDevice(int apcd_id)
    {
        return m_pImpl->DestroyRenderDevice(apcd_id);
    }
#endif


    VCSStatus ClientConnection::Connect(const Uri &server)
    {
        return m_pImpl->Connect(server);
    }

    VCSStatus ClientConnection::Login(const AccountName &accountName, const char *password, const char *captureDevice, const char *renderDevice)
    {
        return m_pImpl->Login(accountName, password, captureDevice, renderDevice);
    }

    VCSStatus ClientConnection::Logout(const AccountName &accountName)
    {
        return m_pImpl->Logout(accountName);
    }

    VCSStatus ClientConnection::JoinChannel(const AccountName &accountName, const Uri &channelUri, const char *access_token)
    {
        return m_pImpl->JoinChannel(accountName, channelUri, access_token);
    }

    VCSStatus ClientConnection::LeaveChannel(const AccountName &accountName, const Uri &channelUri)
    {
        return m_pImpl->LeaveChannel(accountName, channelUri);
    }

    VCSStatus ClientConnection::LeaveAll(const AccountName &accountName)
    {
        return m_pImpl->LeaveAll(accountName);
    }

    void ClientConnection::Disconnect(const Uri &server)
    {
        return m_pImpl->Disconnect(server);
    }

    VCSStatus ClientConnection::BlockUsers(const AccountName &accountName, const std::set<Uri> &usersToBlock)
    {
        return m_pImpl->BlockUsers(accountName, usersToBlock);
    }

    VCSStatus ClientConnection::UnblockUsers(const AccountName &accountName, const std::set<Uri> &usersToUnblock)
    {
        return m_pImpl->UnblockUsers(accountName, usersToUnblock);
    }

    //VCSStatus ClientConnection::IssueGetStats(const AccountName &accountName, bool reset)
    //{
    //    return m_pImpl->IssueGetStats(accountName, reset);
    //}

    VCSStatus ClientConnection::StartPlayFileIntoChannels(const AccountName &accountName, const char *filename)
    {
        return m_pImpl->StartPlayFileIntoChannels(accountName, filename);
    }
    VCSStatus ClientConnection::StopPlayFileIntoChannels(const AccountName &accountName)
    {
        return m_pImpl->StopPlayFileIntoChannels(accountName);
    }
    VCSStatus ClientConnection::KickUser(const AccountName &accountName, const Uri &channelUri, const Uri &userUri)
    {
        return m_pImpl->KickUser(accountName, channelUri, userUri);
    }

    // Audio Input Functions

    const std::vector<AudioDeviceId> &ClientConnection::GetAvailableAudioInputDevices() const
    {
        return m_pImpl->GetAudioInputDevices();
    }

    AudioDeviceId ClientConnection::GetApplicationChosenAudioInputDevice() const
    {
        return m_pImpl->GetApplicationChosenAudioInputDevice();
    }

    const AudioDeviceId &ClientConnection::GetOperatingSystemChosenAudioInputDevice() const
    {
        return m_pImpl->GetOperatingSystemChosenAudioInputDevice();
    }

    VCSStatus ClientConnection::SetApplicationChosenAudioInputDevice(const AudioDeviceId &deviceName)
    {
        return m_pImpl->SetApplicationChosenAudioInputDevice(deviceName);
    }

    void ClientConnection::UseOperatingSystemChosenAudioInputDevice()
    {
        m_pImpl->UseOperatingSystemChosenAudioInputDevice();
    }

    bool ClientConnection::IsUsingOperatingSystemChosenAudioInputDevice() const
    {
        return m_pImpl->IsUsingOperatingSystemChosenAudioInputDevice();
    }

    // Audio Output Devices

    const std::vector<AudioDeviceId> &ClientConnection::GetAvailableAudioOutputDevices() const
    {
        return m_pImpl->GetAudioOutputDevices();
    }

    AudioDeviceId ClientConnection::GetApplicationChosenAudioOutputDevice() const
    {
        return m_pImpl->GetApplicationChosenAudioOutputDevice();
    }

    const AudioDeviceId &ClientConnection::GetOperatingSystemChosenAudioOutputDevice() const
    {
        return m_pImpl->GetOperatingSystemChosenAudioOutputDevice();
    }

    bool ClientConnection::IsUsingOperatingSystemChosenAudioOutputDevice() const
    {
        return m_pImpl->IsUsingOperatingSystemChosenAudioOutputDevice();
    }

    VCSStatus ClientConnection::SetApplicationChosenAudioOutputDevice(const AudioDeviceId &deviceName)
    {
        return m_pImpl->SetApplicationChosenAudioOutputDevice(deviceName);
    }

    void ClientConnection::UseOperatingSystemChosenAudioOutputDevice()
    {
        m_pImpl->UseOperatingSystemChosenAudioOutputDevice();
    }

    int ClientConnection::GetMasterAudioInputDeviceVolume() const
    {
        return m_pImpl->GetMasterAudioInputDeviceVolume();
    }
    VCSStatus ClientConnection::SetMasterAudioInputDeviceVolume(int volume)
    {
        return m_pImpl->SetMasterAudioInputDeviceVolume(volume);
    }
    int ClientConnection::GetMasterAudioOutputDeviceVolume() const
    {
        return m_pImpl->GetMasterAudioOutputDeviceVolume();
    }
    VCSStatus ClientConnection::SetMasterAudioOutputDeviceVolume(int volume)
    {
        return m_pImpl->SetMasterAudioOutputDeviceVolume(volume);
	}

	VCSStatus ClientConnection::SetVoiceActivateDetectionSensitivity(int sensitivity)
	{
		return m_pImpl->SetVoiceActivateDetectionSensitivity(sensitivity);
	}

	VCSStatus ClientConnection::SetVADAutomaticParameterSelection(bool enabled)
	{
		return m_pImpl->SetVADAutomaticParameterSelection(enabled);
	}

    int ClientConnection::GetChannelAudioOutputDeviceVolume(const AccountName &accountName, const Uri &channelUri) const
    {
        return m_pImpl->GetChannelAudioOutputDeviceVolume(accountName, channelUri);
    }

    VCSStatus ClientConnection::SetChannelAudioOutputDeviceVolume(const AccountName &accountName, const Uri &channelUri, int volume)
    {
        return m_pImpl->SetChannelAudioOutputDeviceVolume(accountName, channelUri, volume);
    }

	VCSStatus ClientConnection::SetSessionVolume(const AccountName & accountName, const Uri & channelUri, int volume)
	{
		return m_pImpl->SetSessionVolume(accountName, channelUri, volume);;
	}

    int ClientConnection::GetParticipantAudioOutputDeviceVolumeForMe(const AccountName &accountName, const Uri &target, const Uri &channelUri) const
    {
        return m_pImpl->GetParticipantAudioOutputDeviceVolumeForMe(accountName, target, channelUri);
    }

    VCSStatus ClientConnection::SetParticipantAudioOutputDeviceVolumeForMe(const AccountName &accountName, const Uri &target, const Uri &channelUri, int volume)
    {
        return m_pImpl->SetParticipantAudioOutputDeviceVolumeForMe(accountName, target, channelUri, volume);
    }

    VCSStatus ClientConnection::SetParticipantMutedForAll(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted)
    {
        return m_pImpl->SetParticipantMutedForAll(accountName, target, channelUri, muted);
    }

    bool ClientConnection::GetParticipantMutedForAll(const AccountName &accountName, const Uri &targetUser, const Uri &channelUri) const
    {
        return m_pImpl->GetParticipantMutedForAll(accountName, targetUser, channelUri);
    }


    VCSStatus ClientConnection::SetParticipantMutedForMe(const AccountName &accountName, const Uri &target, const Uri &channelUri, bool muted)
    {
        return m_pImpl->SetParticipantMutedForMe(accountName, target, channelUri, muted);
    }

    ChannelTransmissionPolicy ClientConnection::GetChannelTransmissionPolicy(const AccountName &accountName) const
    {
        return m_pImpl->GetChannelTransmissionPolicy(accountName);
    }

    VCSStatus ClientConnection::SetTransmissionToSpecificChannel(const AccountName &accountName, const Uri &channelUri)
    {
        return m_pImpl->SetTransmissionToSpecificChannel(accountName, channelUri);
    }

	VCSStatus ClientConnection::Set3DPosition(const AccountName &accountName, const Uri &channel, const Vector &speakerPosition, const Vector &listenerPosition, const Vector &listenerForward, const Vector &listenerUp)
	{
		return m_pImpl->Set3DPosition(accountName, channel, speakerPosition, listenerPosition, listenerForward, listenerUp);
	}

    VCSStatus ClientConnection::SetTransmissionToAll(const AccountName &accountName)
    {
        return m_pImpl->SetTransmissionToAll(accountName);
    }

    VCSStatus ClientConnection::SetTransmissionToNone(const AccountName &accountName)
    {
        return m_pImpl->SetTransmissionToNone(accountName);
    }

	bool ClientConnection::HasConnectedChannel(const AccountName & accountName)
	{
		return m_pImpl->HasConnectedChannel(accountName);
	}

    VCSStatus ClientConnection::StartAudioOutputDeviceTest(const char *filename)
    {
        return m_pImpl->StartAudioOutputDeviceTest(filename);
    }

    void ClientConnection::StopAudioOutputDeviceTest()
    {
        return m_pImpl->StopAudioOutputDeviceTest();
    }

    bool ClientConnection::AudioOutputDeviceTestIsRunning() const
    {
        return m_pImpl->AudioOutputDeviceTestIsRunning();
    }

    VCSStatus ClientConnection::StartAudioInputDeviceTestRecord()
    {
        return m_pImpl->StartAudioInputDeviceTestRecord();
    }

    void ClientConnection::StopAudioInputDeviceTestRecord()
    {
        return m_pImpl->StopAudioInputDeviceTestRecord();
    }

    VCSStatus ClientConnection::StartAudioInputDeviceTestPlayback()
    {
        return m_pImpl->StartAudioInputDeviceTestPlayback();
    }

    void ClientConnection::StopAudioInputDeviceTestPlayback()
    {
        return m_pImpl->StopAudioInputDeviceTestPlayback();
    }

    bool ClientConnection::AudioInputDeviceTestIsRecording() const
    {
        return m_pImpl->AudioInputDeviceTestIsRecording();
    }

    bool ClientConnection::AudioInputDeviceTestIsPlayingBack() const
    {
        return m_pImpl->AudioInputDeviceTestIsPlayingBack();
    }

    bool ClientConnection::AudioInputDeviceTestHasAudioToPlayback() const {
        return m_pImpl->AudioInputDeviceTestHasAudioToPlayback();
    }

    void ClientConnection::SetAudioOutputDeviceMuted(bool value)
    {
        return m_pImpl->SetAudioOutputDeviceMuted(value);
    }

    bool ClientConnection::GetAudioOutputDeviceMuted() const
    {
        return m_pImpl->GetAudioOutputDeviceMuted();
    }

    void ClientConnection::SetAudioInputDeviceMuted(bool value)
    {
        return m_pImpl->SetAudioInputDeviceMuted(value);
    }

    bool ClientConnection::GetAudioInputDeviceMuted() const
    {
        return m_pImpl->GetAudioInputDeviceMuted();
    }

    void ClientConnection::EnteredBackground()
    {
        m_pImpl->EnteredBackground();
    }
    
    void ClientConnection::WillEnterForeground()
    {
        m_pImpl->WillEnterForeground();
    }
    
    void ClientConnection::OnBackgroundIdleTimeout()
    {
        m_pImpl->OnBackgroundIdleTimeout();
    }
}

#if defined(__APPLE__)
void debugPrint(const char *s)
{
    fprintf(stderr, "%s", s);
}
#elif defined(__ANDROID__)
void debugPrint(const char *s)
{
    // commented out trying to address lost crashes from https://jira.vivox.com/browse/AN-91
    // __android_log_print(ANDROID_LOG_INFO, "vra", "%s", s);
}
#elif defined(WIN32)
void debugPrint(const char *x)
{
    OutputDebugStringA(x);
}
#else
//#error debugPrint needs to be implemented for this platform.

void debugPrint(const char *s)
{
	fprintf(stderr, "%s", s);
}

#endif
