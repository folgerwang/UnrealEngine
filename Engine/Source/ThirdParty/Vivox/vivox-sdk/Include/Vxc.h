#pragma once
/* Copyright (c) 2013-2018 by Mercer Road Corp
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

#include <time.h>
#include "VxcExports.h"

#define VIVOX_SDK_HAS_CRASH_REPORTING 1
#define VIVOX_SDK_HAS_VOICE_FONTS 1
#define VIVOX_SDK_HAS_GROUP_IM 1
#define VIVOX_SDK_HAS_MUTE_SCOPE 1
#define VIVOX_SDK_HAS_PARTICIPANT_TYPE 1
#define VIVOX_SDK_HAS_NETWORK_MESSAGE 1
#define VIVOX_SDK_HAS_AUX_DIAGNOSTIC_STATE 1
#define VIVOX_SDK_SESSION_RENDER_AUDIO_OBSOLETE 1
#define VIVOX_SDK_SESSION_GET_LOCAL_AUDIO_INFO_OBSOLETE 1
#define VIVOX_SDK_SESSION_MEDIA_RINGBACK_OBSOLETE 1
#define VIVOX_SDK_SESSION_CONNECT_OBSOLETE 1
#define VIVOX_SDK_SESSION_CHANNEL_GET_PARTICIPANTS_OBSOLETE 1
#define VIVOX_SDK_ACCOUNT_CHANNEL_CREATE_AND_INVITE_OBSOLETE 1
#define VIVOX_SDK_EVT_SESSION_PARTICIPANT_LIST_OBSOLETE 1
#define VIVOX_SDK_HAS_INTEGRATED_PROXY 1
#define VIVOX_SDK_HAS_NO_CHANNEL_FOLDERS 1
#define VIVOX_SDK_HAS_NO_SCORE 1
#define VIVOX_SDK_HAS_GENERIC_APP_NOTIFICATIONS_ONLY 1
#define VIVOX_SDK_HAS_FRAME_TOTALS 1
#define VIVOX_SDK_NO_LEGACY_RECORDING 1
#define VIVOX_SDK_NO_IS_AD_PLAYING 1 
#define VIVOX_SDK_HAS_ACCOUNT_SEND_MSG 1
#define VIVOX_SDK_HAS_PLC_STATS 1
#define VIVOX_SDK_HAS_DEVICE_ADDED_REMOVED 1
#define VIVOX_SDK_HAS_ADVANCED_AUDIO_LEVELS 1
#define VIVOX_SDK_HAS_AUDIO_UNIT_CALLBACKS 1

#include "VxcTypes.h"

#pragma pack(push)
#pragma pack(8)

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Request cookie type. Used when passing in requests to the SDK.
     */
    typedef char* VX_COOKIE;
    /**
     * Generic handle type for state objects (connectors, accounts, session groups, session, etc)
     */
    typedef VX_COOKIE VX_HANDLE;

    typedef unsigned int VX_SDK_HANDLE;

#ifndef VIVOX_TYPES_ONLY
    /** 
     * Used to allocate and initialize a cookie.
     * \ingroup memorymanagement
     */
    VIVOXSDK_DLLEXPORT int vx_cookie_create(const char* value, VX_COOKIE* cookie);
    /**
     * Used to free a cookie
     * \ingroup memorymanagement
     */
    VIVOXSDK_DLLEXPORT int vx_cookie_free(VX_COOKIE* cookie);
#endif


    /**
     * Type of STUN probe to do
     */
    typedef enum {
        /**
         * Use the default as configured on the account management server.
         */
        attempt_stun_unspecified,
        /**
         * Use STUN
         */
        attempt_stun_on,
        /**
         * Don't use STUN
         */
        attempt_stun_off,
    } vx_attempt_stun;

    /**
     * Type of API mode to use.
     * Required setting is connector_mode_normal.
     */
    typedef enum {
        /**
         * The default and only valid value
         */
        connector_mode_normal=0,
        /**
         * This value is deprecated.
         * \deprecated
         */
        connector_mode_legacy,
    } vx_connector_mode;

    /**
     * Type of API mode to use.
     * Recommended setting is connector_mode_normal.
     */
    typedef enum {
        /**
        * Each handle will be unique for the lifetime of the connector
        */
        session_handle_type_unique=0,
        /**
        * Handles will be sequential integers
        */
        session_handle_type_legacy,
        /**
        * Handles will be heirarchical numeric
        */
        session_handle_type_heirarchical_numeric,
        /**
        * Handles will be heirarchical unique
        */
        session_handle_type_heirarchical_unique
    } vx_session_handle_type;

    /**
     * Type of logging for the applicaiton to use.
     * The Vivox SDK is cabable of logging to a native log file and/or sending log information
     * to the client applicaiton via a callback method registered with the SDK.
     */
    typedef enum {
        /**
         * Unused 
         */
        log_to_none=0,
        /**
         * Log to the native configured logfile 
         */
        log_to_file,
        /**
         * Send log information to the client applicaiton via the registered callback method
         */
        log_to_callback,
        /**
         * Log to the native configured log file and the client applicaiton via the registered callback method
         */
        log_to_file_and_callback,
    } vx_log_type;
        
    /**
     * Used as run time type indicator for all messages passed between application and SDK.
     */
    typedef enum {
        /**
         * Unused 
         */
        msg_none=0,
        /** 
         * Message is a request
         * @see vx_req_base_t
         */
        msg_request=1,
        /** 
         * Message is a response
         * @see vx_resp_base_t
         */
        msg_response,
        /** 
         * Message is an event
         * @see vx_evt_base_t
         */
        msg_event,
    } vx_message_type;

    typedef enum {
        /**
         * Stop a recording
         */
        VX_SESSIONGROUP_RECORDING_CONTROL_STOP = 0,
        /**
         * Start a recording
         */
        VX_SESSIONGROUP_RECORDING_CONTROL_START = 1,
        /**
         * Flush a continuous recording 
         */
        VX_SESSIONGROUP_RECORDING_CONTROL_FLUSH_TO_FILE = 2,
    } vx_sessiongroup_recording_control_type;

    typedef enum {
        /**
         * Stop audio injection
         */
        VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_STOP = 0,
        /**
         * Start audio injection (only if currently stopped)
         */
        VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_START = 1,
        /**
         * Restart audio injection (start if currently stopped. Stop if currently injecting, and restart)
         */
        VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_RESTART = 2,
        VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_MIN = VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_STOP,
        VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_MAX = VX_SESSIONGROUP_AUDIO_INJECTION_CONTROL_RESTART
    } vx_sessiongroup_audio_injection_control_type;


    typedef enum {
        /**
        * Stop playback
        *
        * When playback is stopped, it closes the playback file, and generates a media frame played event
        * with 0 for the first frame and 0 for the total frames.
        */
        VX_SESSIONGROUP_PLAYBACK_CONTROL_STOP = 0,
        /**
        * Start playback
        */
        VX_SESSIONGROUP_PLAYBACK_CONTROL_START = 1,
        /**
        * Pause a playback
        */
        VX_SESSIONGROUP_PLAYBACK_CONTROL_PAUSE = 3,
        /**
        * Unpause playback
        */
        VX_SESSIONGROUP_PLAYBACK_CONTROL_UNPAUSE = 4,
    } vx_sessiongroup_playback_control_type;

    typedef enum {
        /**
        * Normal mode playback
        */
        VX_SESSIONGROUP_PLAYBACK_MODE_NORMAL = 0,
        /**
        * Vox mode playback: Catch-up mode. 
        * Skip all silence periods. Playback at desired speed.
        */
        VX_SESSIONGROUP_PLAYBACK_MODE_VOX = 1,
    } vx_sessiongroup_playback_mode;

    /**
     * The ways that communication can be controlled.
     */
    typedef enum {
        /**
         * The issuing user will not hear the blocked user, *and* the blocked user will not hear the issuing user
         */
        vx_control_communications_operation_block = 0,
        /**
         * The issuing user will hear the blocked user, and the blocked user will hear the issuing user, unless the blocked 
         * user has blocked the issuing user as well.
         */
        vx_control_communications_operation_unblock = 1,
        vx_control_communications_operation_list = 2,
        vx_control_communications_operation_clear = 3
    } vx_control_communications_operation;

    typedef enum {
        media_type_none=0,
        media_type_text,
        media_type_audio,
        media_type_video,
        media_type_audiovideo,
    } vx_media_type;

    typedef enum {
        termination_status_none=0,
        termination_status_busy,
        termination_status_decline,
    } vx_termination_status;

    typedef enum {
        diagnostic_dump_level_all=0,
        diagnostic_dump_level_sessions,
    } vx_diagnostic_dump_level;

    typedef enum {
        media_ringback_none=0,
        media_ringback_ringing=1,    // 180
        //media_ringback_answer=2,     // 200
        media_ringback_busy=3,       // 486
        //media_ringback_terminated=4, // 487
    } vx_media_ringback;

    typedef enum {
        channel_type_normal=0,
        channel_type_positional = 2
    } vx_channel_type;

    typedef enum {
        channel_mode_none=0,
        channel_mode_normal=1,
        channel_mode_presentation=2,
        channel_mode_lecture=3,
        channel_mode_open=4,
        channel_mode_auditorium=5
    } vx_channel_mode;

    typedef enum {
        channel_search_type_all=0,
        channel_search_type_non_positional=1,
        channel_search_type_positional=2
    } vx_channel_search_type;

    typedef enum {
        channel_moderation_type_all=0,
        channel_moderation_type_current_user=1
    } vx_channel_moderation_type;
    
    /** The type of the sessiongroup specified at sessiongroup creation time */
    typedef enum {
        /**
        * Normal type for general use.
        */
        sessiongroup_type_normal=0,
        /**
        * Playback type.  
        * Only use this for playing back a Vivox recording.
        * Live sessions cannot be added to this type of sessiongroup.
        */
        sessiongroup_type_playback=1
    } vx_sessiongroup_type;

    /** The reason why a participant was removed from a session. */
    typedef enum {
        participant_left=0,
        participant_timeout=1,
        participant_kicked=2,
        participant_banned=3
    } vx_participant_removed_reason;

    typedef struct vx_message_base {
        vx_message_type type;
        VX_SDK_HANDLE sdk_handle;
        unsigned long long create_time_ms;
        unsigned long long last_step_ms;
    } vx_message_base_t;

    /** The set of requests that can be issued. */
    typedef enum {
        req_none=0,
        req_connector_create=1,
        req_connector_initiate_shutdown=2,
        req_account_login=3,
        req_account_logout=4,
        req_account_set_login_properties=5,
        req_sessiongroup_create=6,
        req_sessiongroup_terminate=7,
        req_sessiongroup_add_session=8,
        req_sessiongroup_remove_session=9,
#ifndef VX_DISABLE_SESSIONGRP_FOCUS
        req_sessiongroup_set_focus=10,
        req_sessiongroup_unset_focus=11,
        req_sessiongroup_reset_focus=12,
#endif
        req_sessiongroup_set_tx_session=13,
        req_sessiongroup_set_tx_all_sessions=14,
        req_sessiongroup_set_tx_no_session=15,
        req_session_create=16,                  /**< Do Not Use, use req_sessiongroup_add_session */
        req_session_media_connect=18,
        req_session_media_disconnect=19,
        req_session_terminate=21,
        req_session_mute_local_speaker=22,
        req_session_set_local_speaker_volume=23,
        req_session_channel_invite_user=25,
        req_session_set_participant_volume_for_me=26,
        req_session_set_participant_mute_for_me=27,
        req_session_set_3d_position=28,
        req_session_set_voice_font=29,
        req_account_channel_create=34,
        req_account_channel_update=35,
        req_account_channel_delete=36,
        req_account_channel_favorites_get_list=42,
        req_account_channel_favorite_set=43,
        req_account_channel_favorite_delete=44,
        req_account_channel_favorite_group_set=45,
        req_account_channel_favorite_group_delete=46,
        req_account_channel_get_info=47,
        req_account_channel_search=48,
        req_account_buddy_search=49,
        req_account_channel_add_moderator=50,
        req_account_channel_remove_moderator=51,
        req_account_channel_get_moderators=52,
#ifndef VX_DISABLE_ACL
        req_account_channel_add_acl=53,
        req_account_channel_remove_acl=54,
        req_account_channel_get_acl=55,
#endif
        req_channel_mute_user=56,
        req_channel_ban_user=57,
        req_channel_get_banned_users=58,
        req_channel_kick_user=59,
        req_channel_mute_all_users=60,
        req_connector_mute_local_mic=61,
        req_connector_mute_local_speaker=62,
        req_connector_set_local_mic_volume=63,
        req_connector_set_local_speaker_volume=64,
        req_connector_get_local_audio_info=65,
#ifndef VX_DISABLE_PRESENCE
        req_account_buddy_set=67,
        req_account_buddy_delete=68,
        req_account_buddygroup_set=69,
        req_account_buddygroup_delete=70,
        req_account_list_buddies_and_groups=71,
#endif
        req_session_send_message=72,
#ifndef VX_DISABLE_PRESENCE
        req_account_set_presence=73,
        req_account_send_subscription_reply=74,
#endif
        req_session_send_notification=75,
#ifndef VX_DISABLE_PRESENCE
        req_account_create_block_rule=76,
        req_account_delete_block_rule=77,
        req_account_list_block_rules=78,
        req_account_create_auto_accept_rule=79,
        req_account_delete_auto_accept_rule=80,
        req_account_list_auto_accept_rules=81,
#endif
        req_account_update_account=82, /* deprecated */
        req_account_get_account=83, /* deprecated */
        req_account_send_sms=84,
        req_aux_connectivity_info=86,
        req_aux_get_render_devices=87,
        req_aux_get_capture_devices=88,
        req_aux_set_render_device=89,
        req_aux_set_capture_device=90,
        req_aux_get_mic_level=91,
        req_aux_get_speaker_level=92,
        req_aux_set_mic_level=93,
        req_aux_set_speaker_level=94,
        req_aux_render_audio_start=95,
        req_aux_render_audio_stop=96,
        req_aux_capture_audio_start=97,
        req_aux_capture_audio_stop=98,
        req_aux_global_monitor_keyboard_mouse=99,
        req_aux_set_idle_timeout=100,
        req_aux_create_account=101,
        req_aux_reactivate_account=102,
        req_aux_deactivate_account=103,
        req_account_post_crash_dump=104,
        req_aux_reset_password=105,
        req_sessiongroup_set_session_3d_position=106,
        req_account_get_session_fonts=107,
        req_account_get_template_fonts=108,
        req_aux_start_buffer_capture=109,
        req_aux_play_audio_buffer=110,
        req_sessiongroup_control_recording=111,
        req_sessiongroup_control_playback=112,
        req_sessiongroup_set_playback_options=113,
        req_session_text_connect=114,
        req_session_text_disconnect=115,
        req_channel_set_lock_mode=116,
        req_aux_render_audio_modify=117,
        req_session_send_dtmf=118,
        req_aux_set_vad_properties=120,
        req_aux_get_vad_properties=121,
        req_sessiongroup_control_audio_injection=124,
        req_account_channel_change_owner=125,           /**< Not yet implemented (3030) */
        req_account_channel_get_participants=126,       /**< Not yet implemented (3030) */
        req_account_send_user_app_data=128,             /**< Not yet implemented (3030) */
        req_aux_diagnostic_state_dump=129,
        req_account_web_call=130,
        req_account_anonymous_login=131,
        req_account_authtoken_login=132,
        req_sessiongroup_get_stats=133,
        req_account_send_message=134,
        req_aux_notify_application_state_change=135,
        req_account_control_communications=136,
        req_max=req_account_control_communications+1
    } vx_request_type;

    /** Response types that will be reported back to the calling app. */
    typedef enum {
        resp_none=0,
        resp_connector_create=1,
        resp_connector_initiate_shutdown=2,
        resp_account_login=3,
        resp_account_logout=4,
        resp_account_set_login_properties=5,
        resp_sessiongroup_create=6,
        resp_sessiongroup_terminate=7,
        resp_sessiongroup_add_session=8,
        resp_sessiongroup_remove_session=9,
#ifndef VX_DISABLE_SESSIONGRP_FOCUS
        resp_sessiongroup_set_focus=10,
        resp_sessiongroup_unset_focus=11,
        resp_sessiongroup_reset_focus=12,
#endif
        resp_sessiongroup_set_tx_session=13,
        resp_sessiongroup_set_tx_all_sessions=14,
        resp_sessiongroup_set_tx_no_session=15,
        resp_session_create=16,                 /**< Do Not Use */
        resp_session_media_connect=18,
        resp_session_media_disconnect=19,
        resp_session_terminate=21,
        resp_session_mute_local_speaker=22,
        resp_session_set_local_speaker_volume=23,
        resp_session_channel_invite_user=25,
        resp_session_set_participant_volume_for_me=26,
        resp_session_set_participant_mute_for_me=27,
        resp_session_set_3d_position=28,
        resp_session_set_voice_font=29,
        resp_account_channel_get_list=33,
        resp_account_channel_create=34,
        resp_account_channel_update=35,
        resp_account_channel_delete=36,
        resp_account_channel_favorites_get_list=42,
        resp_account_channel_favorite_set=43,
        resp_account_channel_favorite_delete=44,
        resp_account_channel_favorite_group_set=45,
        resp_account_channel_favorite_group_delete=46,
        resp_account_channel_get_info=47,
        resp_account_channel_search=48,
        resp_account_buddy_search=49,
        resp_account_channel_add_moderator=50,
        resp_account_channel_remove_moderator=51,
        resp_account_channel_get_moderators=52,
#ifndef VX_DISABLE_ACL
        resp_account_channel_add_acl=53,
        resp_account_channel_remove_acl=54,
        resp_account_channel_get_acl=55,
#endif
        resp_channel_mute_user=56,
        resp_channel_ban_user=57,
        resp_channel_get_banned_users=58,
        resp_channel_kick_user=59,
        resp_channel_mute_all_users=60,
        resp_connector_mute_local_mic=61,
        resp_connector_mute_local_speaker=62,
        resp_connector_set_local_mic_volume=63,
        resp_connector_set_local_speaker_volume=64,
        resp_connector_get_local_audio_info=65,
#ifndef VX_DISABLE_PRESENCE
        resp_account_buddy_set=67,
        resp_account_buddy_delete=68,
        resp_account_buddygroup_set=69,
        resp_account_buddygroup_delete=70,
        resp_account_list_buddies_and_groups=71,
#endif
        resp_session_send_message=72,
#ifndef VX_DISABLE_PRESENCE
        resp_account_set_presence=73,
        resp_account_send_subscription_reply=74,
#endif
        resp_session_send_notification=75,
#ifndef VX_DISABLE_PRESENCE
        resp_account_create_block_rule=76,
        resp_account_delete_block_rule=77,
        resp_account_list_block_rules=78,
        resp_account_create_auto_accept_rule=79,
        resp_account_delete_auto_accept_rule=80,
        resp_account_list_auto_accept_rules=81,
#endif
        resp_account_update_account=82, // deprecated
        resp_account_get_account=83, // deprecated
        resp_account_send_sms=84,
        resp_aux_connectivity_info=86,
        resp_aux_get_render_devices=87,
        resp_aux_get_capture_devices=88,
        resp_aux_set_render_device=89,
        resp_aux_set_capture_device=90,
        resp_aux_get_mic_level=91,
        resp_aux_get_speaker_level=92,
        resp_aux_set_mic_level=93,
        resp_aux_set_speaker_level=94,
        resp_aux_render_audio_start=95,
        resp_aux_render_audio_stop=96,
        resp_aux_capture_audio_start=97,
        resp_aux_capture_audio_stop=98,
        resp_aux_global_monitor_keyboard_mouse=99,
        resp_aux_set_idle_timeout=100,
        resp_aux_create_account=101,
        resp_aux_reactivate_account=102,
        resp_aux_deactivate_account=103,
        resp_account_post_crash_dump=104,
        resp_aux_reset_password=105,
        resp_sessiongroup_set_session_3d_position=106,
        resp_account_get_session_fonts=107,
        resp_account_get_template_fonts=108,
        resp_aux_start_buffer_capture=109,
        resp_aux_play_audio_buffer=110,
        resp_sessiongroup_control_recording=111,
        resp_sessiongroup_control_playback=112,
        resp_sessiongroup_set_playback_options=113,
        resp_session_text_connect=114,
        resp_session_text_disconnect=115,
        resp_channel_set_lock_mode=116,
        resp_aux_render_audio_modify=117,
        resp_session_send_dtmf=118,
        resp_aux_set_vad_properties=120,
        resp_aux_get_vad_properties=121,
        resp_sessiongroup_control_audio_injection=124,
        resp_account_channel_change_owner=125,              /**< Not yet implemented (3030) */
        resp_account_channel_get_participants=126,          /**< Not yet implemented (3030) */
        resp_account_send_user_app_data=128,                /**< Not yet implemented (3030) */
        resp_aux_diagnostic_state_dump=129,                 
        resp_account_web_call=130,
        resp_account_anonymous_login=131,
        resp_account_authtoken_login=132,
        resp_sessiongroup_get_stats=133,
        resp_account_send_message=134,
        resp_aux_notify_application_state_change=135,
        resp_account_control_communications=136,
        resp_max=resp_account_control_communications+1
    } vx_response_type;
    
    /** Event types that will be reported back to the calling app. */
    typedef enum {
        evt_none=0,
        evt_account_login_state_change=2,
#ifndef VX_DISABLE_PRESENCE
        evt_buddy_presence=7,
        evt_subscription=8,
#endif
        evt_session_notification=9,
        evt_message=10,
        evt_aux_audio_properties=11,
#ifndef VX_DISABLE_PRESENCE
        evt_buddy_changed=15,
        evt_buddy_group_changed=16,
        evt_buddy_and_group_list_changed=17,
#endif
        evt_keyboard_mouse=18,
        evt_idle_state_changed=19,
        evt_media_stream_updated=20,
        evt_text_stream_updated=21,
        evt_sessiongroup_added=22,
        evt_sessiongroup_removed=23,
        evt_session_added=24,
        evt_session_removed=25,
        evt_participant_added=26,
        evt_participant_removed=27,
        evt_participant_updated=28,
        evt_sessiongroup_playback_frame_played=30,
        evt_session_updated=31,
        evt_sessiongroup_updated=32,
        evt_media_completion=33,
        evt_server_app_data=35,                 
        evt_user_app_data=36,                   
        evt_network_message=38,
        evt_voice_service_connection_state_changed=39,
#ifndef VX_DISABLE_PRESENCE
        evt_publication_state_changed = 40,
#endif
        evt_audio_device_hot_swap=41,
        evt_max=evt_audio_device_hot_swap+1
    } vx_event_type;
    
    typedef struct vx_req_base {
        vx_message_base_t message;
        vx_request_type type;
        VX_COOKIE cookie;
        void *vcookie;
    } vx_req_base_t;

    typedef struct vx_resp_base {
        vx_message_base_t message;
        vx_response_type type;
        int return_code;
        int status_code;
        char* status_string;
        vx_req_base_t* request;
        char *extended_status_info;
    } vx_resp_base_t;

    typedef struct vx_evt_base {
        vx_message_base_t message;
        vx_event_type type;
        char *extended_status_info;
    } vx_evt_base_t;

    typedef enum {
        ND_E_NO_ERROR = 0,
        ND_E_TEST_NOT_RUN,
        ND_E_NO_INTERFACE,
        ND_E_NO_INTERFACE_WITH_GATEWAY,
        ND_E_NO_INTERFACE_WITH_ROUTE,
        ND_E_TIMEOUT,
        ND_E_CANT_ICMP,
        ND_E_CANT_RESOLVE_VIVOX_UDP_SERVER,
        ND_E_CANT_RESOLVE_ROOT_DNS_SERVER,
        ND_E_CANT_CONVERT_LOCAL_IP_ADDRESS,
        ND_E_CANT_CONTACT_STUN_SERVER_ON_UDP_PORT_3478,
        ND_E_CANT_CREATE_TCP_SOCKET,
        ND_E_CANT_LOAD_ICMP_LIBRARY,
        ND_E_CANT_FIND_SENDECHO2_PROCADDR,
        ND_E_CANT_CONNECT_TO_ECHO_SERVER,
        ND_E_ECHO_SERVER_LOGIN_SEND_FAILED,
        ND_E_ECHO_SERVER_LOGIN_RECV_FAILED,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_STATUS,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_FAILED_STATUS,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_SESSIONID,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_SIPPORT,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_AUDIORTP,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_AUDIORTCP,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_VIDEORTP,
        ND_E_ECHO_SERVER_LOGIN_RESPONSE_MISSING_VIDEORTCP,
        ND_E_ECHO_SERVER_CANT_ALLOCATE_SIP_SOCKET,
        ND_E_ECHO_SERVER_CANT_ALLOCATE_MEDIA_SOCKET,
        ND_E_ECHO_SERVER_SIP_UDP_SEND_FAILED,
        ND_E_ECHO_SERVER_SIP_UDP_RECV_FAILED,
        ND_E_ECHO_SERVER_SIP_TCP_SEND_FAILED,
        ND_E_ECHO_SERVER_SIP_TCP_RECV_FAILED,
        ND_E_ECHO_SERVER_SIP_NO_UDP_OR_TCP,
        ND_E_ECHO_SERVER_SIP_NO_UDP,
        ND_E_ECHO_SERVER_SIP_NO_TCP,
        ND_E_ECHO_SERVER_SIP_MALFORMED_TCP_PACKET,
        ND_E_ECHO_SERVER_SIP_UDP_DIFFERENT_LENGTH,
        ND_E_ECHO_SERVER_SIP_UDP_DATA_DIFFERENT,
        ND_E_ECHO_SERVER_SIP_TCP_PACKETS_DIFFERENT,
        ND_E_ECHO_SERVER_SIP_TCP_PACKETS_DIFFERENT_SIZE,
        ND_E_ECHO_SERVER_LOGIN_RECV_FAILED_TIMEOUT,
        ND_E_ECHO_SERVER_TCP_SET_ASYNC_FAILED,
        ND_E_ECHO_SERVER_UDP_SET_ASYNC_FAILED,
        ND_E_ECHO_SERVER_CANT_RESOLVE_NAME
    } ND_ERROR;

    typedef enum {
        ND_TEST_LOCATE_INTERFACE,
        ND_TEST_PING_GATEWAY,
        ND_TEST_DNS,
        ND_TEST_STUN,
        ND_TEST_ECHO,
        ND_TEST_ECHO_SIP_FIRST_PORT,
        ND_TEST_ECHO_SIP_FIRST_PORT_INVITE_REQUEST,
        ND_TEST_ECHO_SIP_FIRST_PORT_INVITE_RESPONSE,
        ND_TEST_ECHO_SIP_FIRST_PORT_REGISTER_REQUEST,
        ND_TEST_ECHO_SIP_FIRST_PORT_REGISTER_RESPONSE,
        ND_TEST_ECHO_SIP_SECOND_PORT,
        ND_TEST_ECHO_SIP_SECOND_PORT_INVITE_REQUEST,
        ND_TEST_ECHO_SIP_SECOND_PORT_INVITE_RESPONSE,
        ND_TEST_ECHO_SIP_SECOND_PORT_REGISTER_REQUEST,
        ND_TEST_ECHO_SIP_SECOND_PORT_REGISTER_RESPONSE,
        ND_TEST_ECHO_MEDIA,
        ND_TEST_ECHO_MEDIA_LARGE_PACKET
    } ND_TEST_TYPE;

    /**
    * How incoming calls are handled.  Set at login.
    */
    typedef enum {
        /**
        * Not valid for use.
        */
        mode_none=0,
        /**
        * DEPRECATED: The incoming call will be automatically connected if a call is not already established.
        */
        mode_auto_answer=1,
        /**
        * Requires the client to explicitly answer the incoming call.
        */
        mode_verify_answer=2,
        /**
        * The incoming call will be automatically answered with a 486 busy.
        */
        mode_busy_answer=3,
    } vx_session_answer_mode;

    typedef enum {
        mode_auto_accept=0,
        mode_auto_add=1,
        mode_block,
        mode_hide,
        mode_application
    } vx_buddy_management_mode;

    typedef enum {
        rule_none=0,
        rule_allow,
        rule_block,
        rule_hide,
    } vx_rule_type;

    typedef enum {
        type_none=0,
        type_root=1,
        type_user=2,
    } vx_font_type;

    typedef enum {
        status_none=0,
        status_free=1,
        status_not_free=2,
    } vx_font_status;

#ifndef VX_DISABLE_PRESENCE
    typedef enum {
        subscription_presence=0,
    } vx_subscription_type;
#endif

    typedef enum {
        notification_not_typing = 0,
        notification_typing = 1,
        notification_hand_lowered = 2,
        notification_hand_raised = 3,
        notification_min = notification_not_typing,
        notification_max = notification_hand_raised
    } vx_notification_type;

    /** 
     * \attention Not supported on the PLAYSTATION(R)3 platform
     */
    typedef enum {
        dtmf_0=0,
        dtmf_1=1,
        dtmf_2=2,
        dtmf_3=3,
        dtmf_4=4,
        dtmf_5=5,
        dtmf_6=6,
        dtmf_7=7,
        dtmf_8=8,
        dtmf_9=9,
        dtmf_pound=10,
        dtmf_star=11,
        dtmf_A=12,
        dtmf_B=13,
        dtmf_C=14,
        dtmf_D=15,
        dtmf_max=dtmf_D,
    } vx_dtmf_type;

    typedef enum {
        text_mode_disabled = 0,
        text_mode_enabled,
    } vx_text_mode;

    typedef enum {
        channel_unlock = 0,
        channel_lock,
    } vx_channel_lock_mode;

    typedef enum {
        mute_scope_all = 0,
        mute_scope_audio = 1,
        mute_scope_text = 2,
    } vx_mute_scope;

    /**
    * Holds a recorded audio frame
    */
    typedef enum {
        VX_RECORDING_FRAME_TYPE_DELTA = 0,
        VX_RECORDING_FRAME_TYPE_CONTROL = 1
    } vx_recording_frame_type_t;

    typedef enum {
        op_none=0,
        op_safeupdate=1,
        op_delete,
    } vx_audiosource_operation;

    typedef enum {
        aux_audio_properties_none=0
    } vx_aux_audio_properties_state;

    typedef enum {
        login_state_logged_out=0,
        login_state_logged_in = 1,
        login_state_logging_in = 2,
        login_state_logging_out = 3,
        login_state_resetting = 4,
        login_state_error=100
    } vx_login_state_change_state;

#ifndef VX_DISABLE_PRESENCE
    typedef enum {
        publication_state_success=0,
        publication_state_transient_error = 1,
        publication_state_permanent_error = 2
    } vx_publication_state_change_state;

    typedef enum {
        buddy_presence_unknown=0,           /**< OBSOLETE */
        buddy_presence_pending=1,           /**< OBSOLETE */
        buddy_presence_online=2,
        buddy_presence_busy=3,
        buddy_presence_brb=4,
        buddy_presence_away=5,
        buddy_presence_onthephone=6,
        buddy_presence_outtolunch=7,
        buddy_presence_custom=8,            /**< OBSOLETE */
        buddy_presence_online_slc=9,        /**< OBSOLETE */
        buddy_presence_closed=0,            /**< OBSOLETE */
        buddy_presence_offline=0,
    } vx_buddy_presence_state;
#endif

    typedef enum {
        session_notification_none=0
    } vx_session_notification_state;

    typedef enum {
        message_none=0
    } vx_message_state;

    typedef enum {
        // NB: keep in sync with enum TextState in sessionproperties.h
        session_text_disconnected  = 0,
        session_text_connected     = 1, 
        session_text_connecting    = 2,
        session_text_disconnecting = 3
    } vx_session_text_state;

    typedef enum {
        // NB: keep in sync with enum MediaState in sessionproperties.h
        session_media_none          = 0, // deprecated: not used anywhere, was mapped to MediaStateDisconnected which is mapped back to session_media_disconnected
        session_media_disconnected  = 1,
        session_media_connected     = 2,
        session_media_ringing       = 3,
        session_media_hold          = 4, // deprecated: not used anywhere
        session_media_refer         = 5, // deprecated: not used anywhere
        session_media_connecting    = 6,
        session_media_disconnecting = 7
        // NB: MediaState has additional state: MediaStateIncoming, which is mapped to session_media_ringing or session_media_disconnected in different places
    } vx_session_media_state;

    typedef enum {
        participant_user=0,
        part_user=0,        // For backward compatibility
        participant_moderator=1,
        part_moderator=1,   // For backward compatibility
        participant_owner=2,
        part_focus=2,       // For backward compatibility
    } vx_participant_type;

    enum media_codec_type {
        media_codec_type_none = 0,
        media_codec_type_siren14 = 1,
        media_codec_type_pcmu = 2,
        media_codec_type_nm = 3,
        media_codec_type_speex = 4,
        media_codec_type_siren7 = 5,
        media_codec_type_opus = 6
    };

    typedef enum {
        orientation_default = 0,
        orientation_legacy = 1,
        orientation_vivox = 2
    } orientation_type;

    typedef enum {
        media_completion_type_none = 0,
        aux_buffer_audio_capture = 1,
        aux_buffer_audio_render = 2,
        sessiongroup_audio_injection = 3
    } vx_media_completion_type;

    /**
     * Participant media flags
     */
    #define VX_MEDIA_FLAGS_AUDIO 0x1
    #define VX_MEDIA_FLAGS_TEXT  0x2

#ifndef VX_DISABLE_PRESENCE
    // Buddy for state dump
    typedef struct vx_state_buddy_contact {
        vx_buddy_presence_state presence;
        char* display_name;
        char* application;
        char* custom_message;
        char* contact;
        char* priority;
        char* id;
    } vx_state_buddy_contact_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_buddy_contact_create(vx_state_buddy_contact_t** contact);
    VIVOXSDK_DLLEXPORT int vx_state_buddy_contact_free(vx_state_buddy_contact_t* contact);
#endif
    typedef vx_state_buddy_contact_t* vx_state_buddy_contact_ref_t;
    typedef vx_state_buddy_contact_ref_t* vx_state_buddy_contact_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_buddy_contact_list_create(int size, vx_state_buddy_contact_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_buddy_contact_list_free(vx_state_buddy_contact_t** list, int size);
#endif

    // Buddy for state dump
    typedef struct vx_state_buddy {
        char* buddy_uri;
        char* display_name;
        int parent_group_id;
        char* buddy_data;
        //char* account_name;
        int state_buddy_contact_count;
        vx_state_buddy_contact_t** state_buddy_contacts;
    } vx_state_buddy_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_buddy_create(vx_state_buddy_t** buddy);
    VIVOXSDK_DLLEXPORT int vx_state_buddy_free(vx_state_buddy_t* buddy);
#endif
    typedef vx_state_buddy_t* vx_state_buddy_ref_t;
    typedef vx_state_buddy_ref_t* vx_state_buddy_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_buddy_list_create(int size, vx_state_buddy_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_buddy_list_free(vx_state_buddy_t** list, int size);
#endif
    typedef struct vx_state_buddy_group {
        int group_id;
        char* group_name;
        char* group_data;
    } vx_state_buddy_group_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_buddy_group_create(vx_state_buddy_group_t** group);
    VIVOXSDK_DLLEXPORT int vx_state_buddy_group_free(vx_state_buddy_group_t* group);
#endif
    typedef vx_state_buddy_group_t* vx_state_buddy_group_ref_t;
    typedef vx_state_buddy_group_ref_t* vx_state_buddy_group_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_buddy_group_list_create(int size, vx_state_buddy_group_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_buddy_group_list_free(vx_state_buddy_group_t** list, int size);
#endif
#endif//VX_DISABLE_PRESENCE

    /** Channel participant. */
    typedef struct vx_participant {
        char* uri;
        char* first_name;
        char* last_name;
        char* display_name;
        char* username;
        int is_moderator;
        int is_moderator_muted;
        int is_moderator_text_muted;
        int is_muted_for_me;    //NOT CURRENTLY IMPLEMENTED
        int is_owner;
        int account_id; // @deprecated
    } vx_participant_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_participant_create(vx_participant_t** participant);
    VIVOXSDK_DLLEXPORT int vx_participant_free(vx_participant_t* participant);
#endif
    /** Creates a participant list with the given size. */
    typedef vx_participant_t* vx_participant_ref_t;
    typedef vx_participant_ref_t* vx_participant_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_participant_list_create(int size, vx_participant_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_participant_list_free(vx_participant_t** list, int size);
#endif

    /** State Participant
    * Used for state dump only
    */
    typedef struct vx_state_participant {
        char* uri;
        char* display_name;
        int is_audio_enabled;
        int is_text_enabled;
        int is_audio_muted_for_me;
        int is_text_muted_for_me;       //Not Currently Supported
        int is_audio_moderator_muted;
        int is_text_moderator_muted;
        int is_hand_raised;
        int is_typing;
        int is_speaking;
        int volume;
        double energy;
        vx_participant_type type;
        int is_anonymous_login;
    } vx_state_participant_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_participant_create(vx_state_participant_t** state_participant);
    VIVOXSDK_DLLEXPORT int vx_state_participant_free(vx_state_participant_t* state_participant);
#endif
    /** Creates a state_participant list with the given size. */
    typedef vx_state_participant_t* vx_state_participant_ref_t;
    typedef vx_state_participant_ref_t* vx_state_participant_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_participant_list_create(int size, vx_state_participant_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_participant_list_free(vx_state_participant_t** list, int size);
#endif

    /** State Session
    * Used for state dump only
    */
    typedef struct vx_state_session {
        char* session_handle;
        char* uri;
        char* name;
        int is_audio_muted_for_me;
        int is_text_muted_for_me;       //Not Currently Supported
        int is_transmitting;
        int is_focused;
        int volume;
        int session_font_id;
        int has_audio;
        int has_text;
        int is_incoming;
        int is_positional;
        int is_connected;
        int state_participant_count;
        vx_state_participant_t** state_participants;
        char* durable_media_id;
    } vx_state_session_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_session_create(vx_state_session_t** state_session);
    VIVOXSDK_DLLEXPORT int vx_state_session_free(vx_state_session_t* state_session);
#endif
    /** Creates a state_session list with the given size. */
    typedef vx_state_session_t* vx_state_session_ref_t;
    typedef vx_state_session_ref_t* vx_state_session_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_session_list_create(int size, vx_state_session_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_session_list_free(vx_state_session_t** list, int size);
#endif

    /** State SessionGroup
    * Used for state dump only
    */
    typedef struct vx_state_sessiongroup {
        char* sessiongroup_handle;
        int state_sessions_count;
        vx_state_session_t** state_sessions;
        int in_delayed_playback;
        double current_playback_speed;
        vx_sessiongroup_playback_mode current_playback_mode;
        int playback_paused;
        int loop_buffer_capacity;
        int first_loop_frame;
        int total_loop_frames_captured;
        int last_loop_frame_played;
        char* current_recording_filename;
        int total_recorded_frames;
    } vx_state_sessiongroup_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_sessiongroup_create(vx_state_sessiongroup_t** state_sessiongroup);
    VIVOXSDK_DLLEXPORT int vx_state_sessiongroup_free(vx_state_sessiongroup_t* state_sessiongroup);
#endif
    /** Creates a state_sessiongroup list with the given size. */
    typedef vx_state_sessiongroup_t* vx_state_sessiongroup_ref_t;
    typedef vx_state_sessiongroup_ref_t* vx_state_sessiongroup_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_sessiongroup_list_create(int size, vx_state_sessiongroup_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_sessiongroup_list_free(vx_state_sessiongroup_t** list, int size);
#endif

    /** State Account
    * Used for state dump only
    */
    typedef struct vx_state_account {
        char* account_handle;
        char* account_uri;
        char* display_name;
        int is_anonymous_login;
        int state_sessiongroups_count;
        vx_login_state_change_state state;
        vx_state_sessiongroup_t** state_sessiongroups;
        int state_buddy_count;
        int state_buddy_group_count;
#ifndef VX_DISABLE_PRESENCE
        vx_state_buddy_t** state_buddies;
        vx_state_buddy_group_t** state_buddy_groups;
#endif
    } vx_state_account_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_account_create(vx_state_account_t** state_account);
    VIVOXSDK_DLLEXPORT int vx_state_account_free(vx_state_account_t* state_account);
#endif
    /** Creates a state_account list with the given size. */
    typedef vx_state_account_t* vx_state_account_ref_t;
    typedef vx_state_account_ref_t* vx_state_account_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_account_list_create(int size, vx_state_account_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_account_list_free(vx_state_account_t** list, int size);
#endif

    /** State Connector
    * Used for state dump only
    */
    typedef struct vx_state_connector {
        char* connector_handle;
        int state_accounts_count;
        vx_state_account_t** state_accounts;
        int mic_vol;
        int mic_mute;
        int speaker_vol;
        int speaker_mute;
    } vx_state_connector_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_connector_create(vx_state_connector_t** state_connector);
    VIVOXSDK_DLLEXPORT int vx_state_connector_free(vx_state_connector_t* state_connector);
#endif
    /** Creates a state_connector list with the given size. */
    typedef vx_state_connector_t* vx_state_connector_ref_t;
    typedef vx_state_connector_ref_t* vx_state_connector_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_state_connector_list_create(int size, vx_state_connector_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_state_connector_list_free(vx_state_connector_t** list, int size);
#endif

    /** 
      * Channel struct. 
      */
    typedef struct vx_channel {
        /**
         * The name of the channel
         */
        char* channel_name;
        /**
         * channel_desc: The description of the channel
         */
        char* channel_desc;
        /**
         * Not currently implemented
         */
        char* host;
        /**
         * channel_id: The numeric identifier of the channel
         * @deprecated
         */
        int channel_id;
        /**
         * limit: The maximum number of participants allowed in the channel
         */
        int limit;
        /**
         * DEPRECATED.  capacity: The forecasted number of participants in the channel.
         * @deprecated
         */
        int capacity;     /**< DEPRECATED */
        /**
         * modified: The date and time the channel modified
         */
        char* modified;
        /**
         * owner: The uri of the channel owner
         */
        char* owner;
        /**
         * owner_user_name: The user name of the channel owner
         */
        char* owner_user_name;
        /**
         * is_persistent: Flag identifying this channel as persistent or not.  
         * If it is not persistent then it will be deleted automatically after a certain period of inactivity.
         */
        int is_persistent; /* 1 true, <= 0 false */
        /**
         * is_protected: A flag identifying this channel as being password protected or not
         */
        int is_protected; /* 1 true, <= 0 false */
        /**
         * @deprecated
         */
        int size;
        /**
         * type: This identifies this as a channel (0), positional(2)
         */
        int type;
        /**
         * mode: The mode of the channel is none (0), normal (1), presentation (2), lecture (3), open (4)
         */
        vx_channel_mode mode;
        /**
         * channel_uri: The URI of the channel, this is used to join the channel as well as perform moderator actions against the channel
         */
        char* channel_uri;
        /**
         * This is the distance beyond which a participant is considered 'out of range'. 
         * When participants cross this threshold distance from a particular listening position in 
         * a positional channel, a roster update event is fired, which results in an entry being 
         * added (or removed, as the case may be) from the user's speakers list. No audio is received 
         * for participants beyond this range. The default channel value of this parameter is 60.
         * This will use server defaults on create, and will leave existing values unchanged on update
         */
        int max_range;
        /**
         * This is the distance from the listener below which the 'gain rolloff' effects for a given 
         * audio rolloff model (see below) are not applied. 
         * In effect, it is the 'audio plateau' distance (in the sense that the gain is constant up 
         * this distance, and then falls off).  The default value of this channel parameter is 3.  
         * This will use server defaults on create, and will leave existing values unchanged on update.
         */
        int clamping_dist;
        /**
         * This value indicates how sharp the audio attenuation will 'rolloff' between the clamping 
         * and maximum distances. 
         * Larger values will result in steeper rolloffs. The extent of rolloff will depend on the 
         * distance model chosen.  Default value is 1.1. This will use server defaults on create, and 
         * will leave existing values unchanged on update.
         */
        double roll_off;
        /**
         * The (render side) loudness for all speakers in this channel. 
         * Note that this is a receive side value, and should not in practice be raised above, say 2.5. 
         * The default value is 1.7. This will use server defaults on create, and will leave existing 
         * values unchanged on update.
         */
        double max_gain;
        /**
        * This is the distance model for the channel, this tells the server which algorithm to use 
        * when computing attenuation. 
        * The audio from speakers will drop to 0 abruptly at the maximum distance.
        * There are four possible values in this field:
        *    - 0 - None: 
        *        - No distance based attenuation is applied. All speakers are rendered as if they 
        *          were in the same position as the listener. 
        *    - 1 - Inverse Distance Clamped: 
        *        - The attenuation increases in inverse proportion to the distance. The rolloff 
        *          factor n is the inverse of the slope of the attenuation curve. 
        *    - 2 - Linear Distance Clamped: 
        *        - The attenuation increases in linear proportion to the distance.The rolloff factor 
        *          is the negative slope of the attenuation curve.
        *    - 3 - Exponent Distance Clamped: 
        *        - The attenuation increases in inverse proportion to the distance raised to the 
        *          power of the rolloff factor. 
        * The default audio model is 1 - Inverse Distance Clamped. This will use server defaults on 
        * create, and will leave existing values unchanged on update.
        *
        * If channel_type == dir, this does not apply.
         */
        int dist_model;
        /** 
         * encrypt_audio: Whether or not the audio is encrypted
         */
        int encrypt_audio;
        /**
         * owner_display_name: The display name of the channel owner
         */
        char* owner_display_name;
        /**
         * active_participants: The number of participants in the channel
         */
        int active_participants;
    } vx_channel_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_channel_create(vx_channel_t** channel);
    VIVOXSDK_DLLEXPORT int vx_channel_free(vx_channel_t* channel);
#endif

    typedef vx_channel_t* vx_channel_ref_t;
    typedef vx_channel_ref_t* vx_channel_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_channel_list_create(int size, vx_channel_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_channel_list_free(vx_channel_t** list, int size);
#endif
    /** Channel Favorite struct. */
    typedef struct vx_channel_favorite {
        int favorite_id;
        int favorite_group_id;
        char* favorite_display_name;
        char* favorite_data;
        char* channel_uri;
        char* channel_description;
        int channel_limit;
        int channel_capacity;     /**< DEPRECATED */
        char* channel_modified;
        char* channel_owner_user_name;
        int channel_is_persistent; /* 1 true, <= 0 false */
        int channel_is_protected; /* 1 true, <= 0 false */
        int channel_size;
        char* channel_owner;
        char* channel_owner_display_name;
        int channel_active_participants;
    } vx_channel_favorite_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_create(vx_channel_favorite_t** channel);
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_free(vx_channel_favorite_t* channel);
#endif
    typedef vx_channel_favorite_t* vx_channel_favorite_ref_t;
    typedef vx_channel_favorite_ref_t* vx_channel_favorite_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_list_create(int size, vx_channel_favorite_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_list_free(vx_channel_favorite_t** list, int size);
#endif
    /** Channel Favorite Group struct. */
    typedef struct vx_channel_favorite_group {
        int favorite_group_id;
        char* favorite_group_name;
        char* favorite_group_data;
        char* favorite_group_modified;
    } vx_channel_favorite_group_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_group_create(vx_channel_favorite_group_t** channel);
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_group_free(vx_channel_favorite_group_t* channel);
#endif
    typedef vx_channel_favorite_group_t* vx_channel_favorite_group_ref_t;
    typedef vx_channel_favorite_group_ref_t* vx_channel_favorite_group_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_group_list_create(int size, vx_channel_favorite_group_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_channel_favorite_group_list_free(vx_channel_favorite_group_t** list, int size);
#endif
    /** Voice Font struct. */
    typedef struct vx_voice_font {
        int id;
        int parent_id;
        vx_font_type type;
        char* name;
        char* description;
        char* expiration_date;
        int expired;        //0 is false, 1 is true
        char* font_delta;
        char* font_rules;
        vx_font_status status;
    } vx_voice_font_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_voice_font_create(vx_voice_font_t** channel);
    VIVOXSDK_DLLEXPORT int vx_voice_font_free(vx_voice_font_t* channel);
#endif
    typedef vx_voice_font_t* vx_voice_font_ref_t;
    typedef vx_voice_font_ref_t* vx_voice_font_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_voice_font_list_create(int size, vx_voice_font_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_voice_font_list_free(vx_voice_font_t** list, int size);
    VIVOXSDK_DLLEXPORT int vx_string_list_create(int size, char *** list_out);
    VIVOXSDK_DLLEXPORT int vx_string_list_free(char ** list);
#endif

#ifndef VX_DISABLE_PRESENCE

    typedef struct vx_block_rule {
        char* block_mask;
        int presence_only;
    } vx_block_rule_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_block_rule_create(vx_block_rule_t** block_rule);
    VIVOXSDK_DLLEXPORT int vx_block_rule_free(vx_block_rule_t* block_rule);
#endif

    typedef vx_block_rule_t* vx_block_rule_ref_t;
    typedef vx_block_rule_ref_t* vx_block_rules_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_block_rules_create(int size, vx_block_rules_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_block_rules_free(vx_block_rule_t** list, int size);
#endif

    typedef struct vx_auto_accept_rule {
        char* auto_accept_mask;
        int auto_add_as_buddy;
        char* auto_accept_nickname;
    } vx_auto_accept_rule_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_auto_accept_rule_create(vx_auto_accept_rule_t** auto_accept_rule);
    VIVOXSDK_DLLEXPORT int vx_auto_accept_rule_free(vx_auto_accept_rule_t* auto_accept_rule);
#endif

    typedef vx_auto_accept_rule_t* vx_auto_accept_rule_ref_t;
    typedef vx_auto_accept_rule_ref_t* vx_auto_accept_rules_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_auto_accept_rules_create(int size, vx_auto_accept_rules_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_auto_accept_rules_free(vx_auto_accept_rule_t** list, int size);
#endif
#endif

typedef struct vx_user_channel {
        char* uri;
        char* name;
    } vx_user_channel_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_user_channel_create(vx_user_channel_t** user_channel);
    VIVOXSDK_DLLEXPORT int vx_user_channel_free(vx_user_channel_t* user_channel);
#endif

    typedef vx_user_channel_t* vx_user_channel_ref_t;
    typedef vx_user_channel_ref_t* vx_user_channels_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_user_channels_create(int size, vx_user_channels_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_user_channels_free(vx_user_channel_t** list, int size);
#endif

    /**
    * test_type: Enumeration that defines the test performed (see appendix ?15.8 for values).
    * error_code: Enumeration that defines the error or success of the test (see appendix ?15.9 for values).
    * test_additional_info: Any additional info for this test.  This may be IP addresses used, port numbers, error information, etc
    */
    typedef struct vx_connectivity_test_result {
        ND_TEST_TYPE test_type;
        ND_ERROR test_error_code;
        char* test_additional_info;
    } vx_connectivity_test_result_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_connectivity_test_result_create(vx_connectivity_test_result_t** connectivity_test_result, ND_TEST_TYPE tt);
    VIVOXSDK_DLLEXPORT int vx_connectivity_test_result_free(vx_connectivity_test_result_t* connectivity_test_result);
#endif
    typedef vx_connectivity_test_result_t* vx_connectivity_test_result_ref_t;
    typedef vx_connectivity_test_result_ref_t* vx_connectivity_test_results_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_connectivity_test_results_create(int size, vx_connectivity_test_results_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_connectivity_test_results_free(vx_connectivity_test_result_t** list, int size);
#endif
    typedef struct vx_account {
        char* uri;
        char* firstname;
        char* lastname;
        char* username;
        char* displayname;
        char* email;
        char* phone;
        char* carrier;      //Not currently implemented
        char* created_date;
    } vx_account_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_account_create(vx_account_t** account);
    VIVOXSDK_DLLEXPORT int vx_account_free(vx_account_t* account);
#endif

    /**
     * The type of device. 
     */
    typedef enum vx_device_type {
        /**
         * This type is a specific device.
         */
        vx_device_type_specific_device = 0,
        /**
         * This type means to use what ever the system has configured as a default, at the time of the call.
         * Don't switch devices mid-call if the default system device changes.
         */
        vx_device_type_default_system = 1,
        /**
         * This is the null device, which means that either input or output from/to that device will not occur.
         */
        vx_device_type_null = 2,
        /**
         * This type means to use what ever the system has configured as a default communication device, at the time of the call.
         * Don't switch devices mid-call if the default communication device changes.
         */
        vx_device_type_default_communication = 3,
    } vx_device_type_t;

    typedef struct vx_device {
        /**
         * The identifier to passed to vx_req_set_render_device or vx_req_set_capture_device
         */
        char* device;
        /**
         * The display name to present to the user
         */
        char* display_name;
        /**
         * The type of device
         */
        vx_device_type_t device_type;
    } vx_device_t;

#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_device_create(vx_device_t** device);
    VIVOXSDK_DLLEXPORT int vx_device_free(vx_device_t* device);
#endif
    typedef vx_device_t* vx_device_ref_t;
    typedef vx_device_ref_t* vx_devices_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_devices_create(int size, vx_devices_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_devices_free(vx_device_t** list, int size);
#endif

#ifndef VX_DISABLE_PRESENCE
    typedef struct vx_buddy {
        char* buddy_uri;
        char* display_name;
        int parent_group_id;
        char* buddy_data;
        int account_id;  // @deprecated
        char* account_name;
    } vx_buddy_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_buddy_create(vx_buddy_t** buddy);
    VIVOXSDK_DLLEXPORT int vx_buddy_free(vx_buddy_t* buddy);
#endif
    typedef vx_buddy_t* vx_buddy_ref_t;
    typedef vx_buddy_ref_t* vx_buddy_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_buddy_list_create(int size, vx_buddy_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_buddy_list_free(vx_buddy_t** list, int size);
#endif

#endif
    typedef struct vx_group {
        int group_id;
        char* group_name;
        char* group_data;
    } vx_group_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_group_create(vx_group_t** group);
    VIVOXSDK_DLLEXPORT int vx_group_free(vx_group_t* group);
#endif
    typedef vx_group_t* vx_group_ref_t;
    typedef vx_group_ref_t* vx_group_list_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_group_list_create(int size, vx_group_list_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_group_list_free(vx_group_t** list, int size);
#endif
    typedef struct vx_name_value_pair {
        /**
         * The name of the parameter
         */
        char* name;
        /**
         * The value of teh parameter
         */
        char* value;
    } vx_name_value_pair_t;

#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_name_value_pair_create(vx_name_value_pair_t** nvpair);
    VIVOXSDK_DLLEXPORT int vx_name_value_pair_free(vx_name_value_pair_t* nvpair);
#endif
    typedef vx_name_value_pair_t* vx_name_value_pair_ref_t;
    typedef vx_name_value_pair_ref_t* vx_name_value_pairs_t;
#ifndef VIVOX_TYPES_ONLY
    VIVOXSDK_DLLEXPORT int vx_name_value_pairs_create(int size, vx_name_value_pairs_t* list_out);
    VIVOXSDK_DLLEXPORT int vx_name_value_pairs_free(vx_name_value_pair_t** list, int size);
#endif

    /* Vivox SDK functions */
#ifndef VIVOX_TYPES_ONLY
    /**
     * Use this function to allocate string data to send to the SDK.
     * The function will NOT work until vx_initialize*() is called, and after vx_uninitialize() call.
     * \ingroup memorymanagement
     */
    VIVOXSDK_DLLEXPORT char* vx_strdup(const char*);
    /**
     * Use this function to free string data returned to the application.
     * This funciton is rarely used in practice.
     * The function will NOT work until vx_initialize*() is called, and after vx_uninitialize() call.
     * \ingroup memorymanagement
     */
    VIVOXSDK_DLLEXPORT int vx_free(char*);

    VIVOXSDK_DLLEXPORT int vx_unallocate(void *p);

    VIVOXSDK_DLLEXPORT void *vx_allocate(size_t nBytes);
    VIVOXSDK_DLLEXPORT void *vx_reallocate(void *p, size_t nBytes);
    VIVOXSDK_DLLEXPORT void *vx_calloc(size_t num, size_t bytesPerElement);

    VIVOXSDK_DLLEXPORT void *vx_allocate_aligned(size_t alignment, size_t size);
    VIVOXSDK_DLLEXPORT int vx_unallocate_aligned(void *p);

    /**
     * The VxSDK polling function.  Should be called periodically to check for any incoming events.  
     *
     * @param message           [out] The object containing the message data.
     * @return                  Status of the poll, 0 = Success, 1 = Failure, -1 = No Mesasge Available
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_get_message(vx_message_base_t** message);

    /** 
     * Execute the given request. This function will initialize the sdk automatically if not already initialized (see vx_initialize3())
     * 
     * @param request           The request object to execute.  This is of one of the vx_req_* structs. 
     * @return                  Success status of the request.
     * \ingroup messaging
     *
     * @deprecated - please use vx_issue_request2
     */
    VIVOXSDK_DLLEXPORT int vx_issue_request(vx_req_base_t* request);
    
    /** 
     * Execute the given request. This function will return an error if the SDK is not initialized (see vx_initialize3())
     * 
     * @param request           The request object to execute.  This is of one of the vx_req_* structs. 
     * @return                  Success status of the request.
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_issue_request2(vx_req_base_t* request);

    /** 
     * Execute the given request. This function will return an error if the SDK is not initialized (see vx_initialize3())
     * 
     * @param request           The request object to execute.  This is of one of the vx_req_* structs. 
     * @param request_count     if non-null, vx_issue_request3 will output the number of requests still outstanding
     *                          requests at a rate of 12 requests/second - an application can use this value to determine
     *                          if the application is issue requests at an unacceptable rate
     * @return                  Success status of the request.
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_issue_request3(vx_req_base_t* request, int *request_count);

    /**
     * Get the XML for the given request.
     *
     * @param request           The request object.
     * @param xml               [out] The xml string.
     * \ingroup xml
     */
     VIVOXSDK_DLLEXPORT int vx_request_to_xml(void* request, char** xml);
    
    /**
     * Get a request for the given XML string.
     * 
     * @param xml               XML string.
     * @param request           [out] The request struct.
     * @param error             [out] XML parse error string (if any error occurs).  NULL otherwise.
     * @return                  The request struct type.  req_none is returned if no struct could be created from the XML.
     * \ingroup xml
     */
    VIVOXSDK_DLLEXPORT vx_request_type vx_xml_to_request(const char* xml, void** request, char** error);
    
    /**
     * Get the XML for the given response.
     *
     * @param response           The response object.
     * @param xml               [out] The xml string.
     * \ingroup xml
     */
    VIVOXSDK_DLLEXPORT int vx_response_to_xml(void* response, char** xml);
    
    /**
     * Get a response for the given XML string.
     * 
     * @param xml               XML string.
     * @param response          [out] The response struct.
     * @param error             [out] XML parse error string (if any error occurs).  NULL otherwise.
     * @return                  The response struct type.  resp_none is returned if no struct could be created from the XML.
     * \ingroup xml
     */
    VIVOXSDK_DLLEXPORT vx_response_type vx_xml_to_response(const char* xml, void** response, char** error);
    
    /**
     * Get the XML for the given event.
     *
     * @param event           The event object.
     * @param xml               [out] The xml string.
     * \ingroup xml
     */
    VIVOXSDK_DLLEXPORT int vx_event_to_xml(void* event, char** xml);
    
    /**
     * Get a event for the given XML string.
     * 
     * @param xml               XML string.
     * @param event          [out] The event struct.
     * @param error             [out] XML parse error string (if any error occurs).  NULL otherwise.
     * @return                  The event struct type.  req_none is returned if no struct could be created from the XML.
     * \ingroup xml
     */
    VIVOXSDK_DLLEXPORT vx_event_type vx_xml_to_event(const char* xml, void** event, char** error);
    
    /**
     * Determine whether the XML refers to a request, response, or event.
     * \ingroup xml
     */
    VIVOXSDK_DLLEXPORT vx_message_type vx_get_message_type(const char* xml);

    /**
     * Get Millisecond Counter
     */
    VIVOXSDK_DLLEXPORT unsigned long long vx_get_time_ms(void);

    /**
     * Get Millisecond Counter
     */
    VIVOXSDK_DLLEXPORT unsigned long long vx_get_time_milli_seconds(void);

    /**
     * Get Microsecond Counter
     */
    VIVOXSDK_DLLEXPORT unsigned long long vx_get_time_micro_seconds(void);

    /**
     * Sleep the specified amount of milliseconds
     *
     * @param milli_seconds     [in] duration to "sleep" in milliseconds
     * @return                  Difference between the actual and the desired sleep time (in milliseconds)
     */
    VIVOXSDK_DLLEXPORT long long vx_sleep_milli_seconds(unsigned long long milli_seconds);

    /**
     * Register a callback that will be called when a message is placed on the queue.
     * The application should use this to signal the main application thread that will then wakeup and call vx_get_message;
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_register_message_notification_handler(void (* pf_handler)(void *), void *cookie);

    /**
     * Unregister a notification handler
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_unregister_message_notification_handler(void (* pf_handler)(void *), void *cookie);

    /**
     * Block the caller until a message is available.
     * Returns NULL if no message was available within the allotted time.
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT vx_message_base_t *vx_wait_for_message(int msTimeout);

    /**
     * 
     * Register a callback that will be called to initialize logging.
     * The application should use this to signal the main application thread that will then wakeup and call vx_get_message;
     * \ingroup diagnostics
     */
    VIVOXSDK_DLLEXPORT int vx_register_logging_initialization(vx_log_type log_type, 
                                                                const char* log_folder, 
                                                                const char* log_filename_prefix,
                                                                const char* log_filename_suffix,
                                                                int log_level,
                                                                void (* pf_handler)(const char* source, const char* level, const char* message));

    /**
    * Get the SDK log file path
    * \ingroup diagnostics
    */
    VIVOXSDK_DLLEXPORT char *vx_get_log_file_path();

    /**
     * Unregister the logging callback notification handler.  
     * The parameters are reserved for future use, please pass NULL for each of the 
     * paramters (ex: vx_unregister_logging_handler(0, 0);).  If a logging handler is 
     * registered then it must be unregistered before shutting down the SDK.
     * \ingroup diagnostics
     *
     * @param pf_handler - OBSOLETE AND UNUSED
     * @param cookie - OBSOLETE AND UNUSED
     */
    VIVOXSDK_DLLEXPORT int vx_unregister_logging_handler(void (* pf_handler)(void *), void *cookie);

    VIVOXSDK_DLLEXPORT int vx_create_account(const char* acct_mgmt_server, const char* admin_name, const char* admin_pw, const char* uname, const char* pw);

    /**
     * The number of crash dumps stored on disk
     * \ingroup diagnostics
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    VIVOXSDK_DLLEXPORT int vx_get_crash_dump_count(void);

    /**
     * Enable crash dump generation
     * \ingroup diagnostics
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    VIVOXSDK_DLLEXPORT int vx_set_crash_dump_generation_enabled(int value);

    /**
     * Determine if crash dump generation is enabled
     * \ingroup diagnostics
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    VIVOXSDK_DLLEXPORT int vx_get_crash_dump_generation(void);

    /**
     * Get the base64 encoded crash dump information
     * \ingroup diagnostics
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    VIVOXSDK_DLLEXPORT char *vx_read_crash_dump(int index);

    /**
     * Get the timestamp of a crash
     * \ingroup diagnostics
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    VIVOXSDK_DLLEXPORT time_t vx_get_crash_dump_timestamp(int index);

    /**
     * Delete the crash dump
     * \ingroup diagnostics
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    VIVOXSDK_DLLEXPORT int vx_delete_crash_dump(int index);

    /** 
     * The application should call this routine just before it exits.
     * vx_uninitialize() must have been called first, of this call will return an error
     * \ingroup memorymanagement
     */
    VIVOXSDK_DLLEXPORT int vx_on_application_exit(void);

    /**
     * Get the SDK Version info
     * \ingroup diagnostics
     */
    VIVOXSDK_DLLEXPORT const char *vx_get_sdk_version_info(void);

    /** 
     * Apply a vivox voice font to a wav file
     * 
     * @param fontDefinition - string containing the font "definition" (in XML format)
     * @param inputFile - string contaning path to the input wav file, contaning the "unmodified" voice
     * @param outputFile - string containing path to the output wav file, with font applied
     * @return                  0 if successful, non-zero if failed.
     * \ingroup voicefonts
     */
    VIVOXSDK_DLLEXPORT int vx_apply_font_to_file(const char *fontDefinition, const char *inputFile, const char *outputFile);

        /** 
     * Apply a vivox voice font to a wav file, and return the energy ratio (Output Energy/Input Energy)
     * 
     * @param fontDefinition - string containing the font "definition" (in XML format)
     * @param inputFile - string contaning path to the input wav file, contaning the "unmodified" voice
     * @param outputFile - string containing path to the output wav file, with font applied
     * @param energyRatio - Raw Energy ratio between the input and output audio
     * @return                  0 if successful, non-zero if failed.
     * \ingroup voicefonts
     */
    VIVOXSDK_DLLEXPORT int vx_apply_font_to_file_return_energy_ratio(const char *fontDefinition, const char *inputFile, const char *outputFile, double *energyRatio);

        /** 
     * Apply a vivox voice font to a vxz file, and return the energy ratio (Output Energy/Input Energy)
     * 
     * @param fontDefinition - string containing the font "definition" (in XML format)
     * @param inputFile - string contaning path to the input vxz file, contaning the "unmodified" voice
     * @param outputFile - string containing path to the output wav file, with font applied
     * @param energyRatio - Raw Energy ratio between the input and output audio
     * @return                  0 if successful, non-zero if failed.
     * \ingroup voicefonts
     */
    VIVOXSDK_DLLEXPORT int vx_apply_font_to_vxz_file_return_energy_ratio(const char *fontDefinition, const char *inputFile, const char *outputFile, double *energyRatio);


    /** 
     * Create a copy of the internal local audio buffer (associated with the vx_req_aux_start_buffer_capture_t request/response)
     * 
     * @param audioBufferPtr - void pointer (should be passed in uninitialized)
     * @return               - No return value is provided. However, on success the audioBufferPtr will points to a copy of the internal audio buffer, otherwise audioBufferPtr is set to NULL
     * \see vx_req_aux_start_buffer_capture
     * \ingroup adi
     */
    VIVOXSDK_DLLEXPORT void* vx_copy_audioBuffer(void *audioBufferPtr);

    /** 
     * Gets the duration of the audio buffer in seconds.
     * 
     * @param audioBufferPtr - void pointer (should be passed in uninitialized)
     * @return               - duration of the audio buffer in seconds
     * \see vx_req_aux_start_buffer_capture
     * \ingroup adi
     */
    VIVOXSDK_DLLEXPORT double vx_get_audioBuffer_duration (void *audioBufferPtr);

    /**
     * Gets the sample rate of the audio buffer.
     * 
     * @param audioBufferPtr - pointer to audio data in vivox proprietary format
     * @return               - sample rate of the buffer's data or 0 if the buffer doesn't exist
     * \see vx_req_aux_start_buffer_capture
     * \ingroup adi
     */
    VIVOXSDK_DLLEXPORT int vx_get_audioBuffer_sample_rate (void *audioBufferPtr);

    /**
     * Frees up all memory associated with an allocated vivox audioBufferPtr (generated by the vx_copy_audioBuffer() call)
     *
     * @param audioBufferPtr - Pointer to audio data in vivox proprietary format
     * \ingroup adi
     */
    VIVOXSDK_DLLEXPORT int vx_free_audioBuffer(void **audioBufferPtr);

    /**
     * Export audio data in an audioBufferPtr to a memory buffer as PCM
     *
     * @param audioBufferPtr - pointer to the pointer to audio data in vivox proprietary format
     * @param pcmBuffer      - pointer to the pre-allocated memory buffer
     * @param maxSamples     - length of the buffer in samples
     * @return               - number of samples copied to the buffer, or -1 if failed
     * \ingroup adi
     */
    VIVOXSDK_DLLEXPORT int vx_export_audioBuffer_to_pcm(void *audioBufferPtr, short* pcmBuffer, int maxSamples);

    /**
     * Export audio data in an audioBufferPtr to a wav file
     * 
     * @param audioBufferPtr - Pointer to the pointer to audio data in vivox proprietary format
     * @param outputFile     - string containing path to the output wav file
     * @return               -  0 if successful, non-zero if failed.
     * \ingroup adi
     */

    VIVOXSDK_DLLEXPORT int vx_export_audioBuffer_to_wav_file(void *audioBufferPtr, const char *outputFile);

    /** 
     * Set the default out of proc server address, once set requests issued using vx_issue_request will be sent
     * to the server at supplied address, instead of being handle in the current processes context.
     * 
     * @param address - address of out of proc server - "127.0.0.1" is the right value for most applications
     * @param port - port - 44125 is the right value for most applications
     * @return  -  0 if successful, non-zero if failed.
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_set_out_of_process_server_address(const char *address, unsigned short port);

    /** 
     * Allocate an sdk handle. 
     * This allows applications to control multiple out of process servers.
     * If address is zero, then requests using this handle will run in process. Set the req.message.sdk_handle field 
     * to this value to direct a request to a specific out of process SDK instance.
     * 
     * @param address - address of out of proc server - "127.0.0.1" is the right value for most applications
     * @param port - port - 44125 is the right value for most applications
     * @param handle - port - the returned SDK handle
     * @return  -  0 if successful, non-zero if failed.
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_alloc_sdk_handle(const char *address, unsigned short port, VX_SDK_HANDLE *handle);

    /** 
     * Frees the SDK handle
     * 
     * @param sdkHandle - the handle
     * @return  -  0 if successful, non-zero if failed.
     * \ingroup messaging
     */
    VIVOXSDK_DLLEXPORT int vx_free_sdk_handle(VX_SDK_HANDLE sdkHandle);

#ifdef SN_TARGET_PS3
    /**
     * @deprecated
     * Application must call this API before calling any other Vivox API.
     * @return               -  0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_initialize(void * (*pfMallocFunc)(size_t ), 
                                         void   (*pfFreeFunc) (void*), 
                     void *(*pfReallocFunc)(void *, size_t ));
                     
#else
    /**
     * @deprecated
     * Application must call this API before calling any other Vivox API.
     * @return               -  0 if successful, non-zero if failed.
     * @deprecated           - use vx_initialize3()
     */
    VIVOXSDK_DLLEXPORT int vx_initialize(void);

    /**
     * this structure contains configuration parameters for the SDK as a whole
     */
#endif



    /**
     * @deprecated
     * Application must call this API before calling any other Vivox API, except vx_get_default_config3();
     * @return               -  0 if successful, non-zero if failed.
     * @deprecated           - use vx_initialize3()
     */
    VIVOXSDK_DLLEXPORT int vx_initialize2(vx_sdk_config_t *config);


    /**
     * Application must call this API before calling any other Vivox API, except vx_get_default_config3();
     * @return               -  0 if successful, non-zero if failed.
     * \ingroup initialization
     */
    VIVOXSDK_DLLEXPORT int vx_initialize3(vx_sdk_config_t *config, size_t config_size);

    /**
     * Check if Vivox SDK was initialized with vx_initialize*() call, and not yet uninitialized with vx_uninitialize() call.
     * @return               -  0 if NOT initialized, non-zero if initialized.
     * \ingroup initialization
     */
    VIVOXSDK_DLLEXPORT int vx_is_initialized(void);

    /**
     * @deprecated 
     * Use vx_get_default_config3() instead
     *
     * Application must call this API before calling any other Vivox API.
     * @return               -  0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_get_default_config(vx_sdk_config_t *config);

    /**
     * Application must call this API before calling any other Vivox API.
     * @return               -  0 if successful, non-zero if failed.
     * \ingroup initialization
     */
    VIVOXSDK_DLLEXPORT int vx_get_default_config3(vx_sdk_config_t *config, size_t config_size);

    /**
     * Application must call this before exit
     * @return               -  0 if successful, non-zero if failed.
     * \ingroup initialization
     */
    VIVOXSDK_DLLEXPORT int vx_uninitialize(void);
#endif

#define VIVOX_V_V2_AUDIO_DATA_MONO_SIREN14_32000_EXPANDED 0x10001
    /**
     * @deprecated
     *
     * use VIVOX_V_V2_AUDIO_DATA_MONO_SIREN14_32000_EXPANDED instead of VIVOX_V_V2_AUDIO_DATA_MONO_SIREN14_32000.
     */
#define VIVOX_V_V2_AUDIO_DATA_MONO_SIREN14_32000 VIVOX_V_V2_AUDIO_DATA_MONO_SIREN14_32000_EXPANDED

#define VIVOX_V_V2_AUDIO_DATA_MONO_SIREN7_16000_EXPANDED 0x10008

#define VIVOX_V_V2_AUDIO_DATA_MONO_OPUS_48000_EXPANDED 0x10009

#define VIVOX_V_V2_AUDIO_DATA_MONO_PCMU_8000_COLLAPSED 0x20005
    /**
     * @deprecated
     *
     * use VIVOX_V_V2_AUDIO_DATA_MONO_PCMU_8000_COLLAPSED instead of VIVOX_V_V2_AUDIO_DATA_MONO_PCMU.
     */
#define VIVOX_V_V2_AUDIO_DATA_MONO_PCMU VIVOX_V_V2_AUDIO_DATA_MONO_PCMU_8000_COLLAPSED

#define VIVOX_V_V2_AUDIO_DATA_MONO_PCMU_8000_EXPANDED 0x10005

/**
 * configured_codecs is a mask of these constants
 */
#define VIVOX_VANI_PCMU         0x1 /* PCMU */
#define VIVOX_VANI_SIREN7       0x2 /* Siren7, 16kHz, 32kbps */
#define VIVOX_VANI_SIREN14      0x4 /* Siren14, 32kHz, 32kbps */
#define VIVOX_VANI_LEGACY_MASK  0x7
#define VIVOX_VANI_OPUS8       0x10 /* Opus, 48kHz, 8kbps */
#define VIVOX_VANI_OPUS40      0x20 /* Opus, 48kHz, 40kbps */
#define VIVOX_VANI_OPUS57      0x40 /* Opus, 48kHz, 57kbps */ /* proposed; pending research */
#define VIVOX_VANI_OPUS72      0x80 /* Opus, 48kHz, 72kbps */ /* proposed; pending research */
#define VIVOX_VANI_OPUS VIVOX_VANI_OPUS40
#define VIVOX_VANI_OPUS_MASK   0xf0

    typedef struct vx_stat_sample {
        double sample_count;
        double sum;
        double sum_of_squares;
        double mean;
        double stddev;
        double min;
        double max;
        double last;
    } vx_stat_sample_t;

    typedef struct vx_stat_thread {
        int interval;
        int count_poll_lt_1ms;
        int count_poll_lt_5ms;
        int count_poll_lt_10ms;
        int count_poll_lt_16ms;
        int count_poll_lt_20ms;
        int count_poll_lt_25ms;
        int count_poll_gte_25ms;
    } vx_stat_thread_t;

    typedef struct vx_system_stats {
        int ss_size;
        int ar_source_count;
        int ar_source_queue_limit;
        int ar_source_queue_overflows;
        int ar_source_poll_count;
        unsigned msgovrld_count;
        vx_stat_sample_t ar_source_free_buffers;
        vx_stat_sample_t ar_source_queue_depth;
        vx_stat_thread_t vp_thread;
        vx_stat_thread_t ticker_thread;
    } vx_system_stats_t ;

    /**
     * This describes the state of the application. Valid only on mobile platforms
     */
    typedef enum vx_application_state_notification_type {
        vx_application_state_notification_type_before_background,
        vx_application_state_notification_type_after_foreground,
        vx_application_state_notification_type_periodic_background_idle
    } vx_application_state_notification_type_t;

    #ifndef VIVOX_TYPES_ONLY
    /**
     * Get statistics about various system internals
     * @return               -  0 if successful, non-zero if failed
     */
    VIVOXSDK_DLLEXPORT int vx_get_system_stats(vx_system_stats_t *system_stats);

#define VX_VAR_DECODER_COUNT_LIMIT "DecoderLimit"
#define VX_VAR_DECODER_HANGOVER_LIMIT "DecoderHangoverLimit"
#define VX_VAR_RTP_ENCRYPTION "RTPEncryption"

    /**
     * Call this to get named variables
     * @param var_name the name of the variable
     * @param p_value where to store the value of the variable
     * @return 0 if successful, no-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_get_int_var(const char *var_name, int *p_value);
    /**
     * Call this to set named variables
     * @param var_name the name of the variable
     * @param value the integer value to set
     * @return 0 if successful, no-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_set_int_var(const char *var_name, int value);

    /**
     * Used to deallocate any message of any type
     * \ingroup memorymanagement
     */
    VIVOXSDK_DLLEXPORT int vx_destroy_message(vx_message_base_t *message);
#endif
    /**
     * Error Codes that are returned by the VXA subsystem
     */
    typedef enum {
       /** 
        * Method executed successfully.
        */
       VXA_SUCCESS = 0,
       /**
        * The caller provided an invalid parameter
        */
       VXA_INVALID_PARAMETER = 1,
       /**
        * The caller attempted to open a device that does not exist
        */
       VXA_DEVICE_DOES_NOT_EXIST = 2,
       /**
        * There was an unexpected operating system specific failure.
        */
       VXA_INTERNAL_ERROR = 3,
       /**
        * The method could not allocate enough memory to complete the request
        */
       VXA_OUT_OF_MEMORY = 4,
       /**
        * The method attempt to perform an invalid operation on the object in its current state.
        * Typically, this is an attempt to open an already open device, or read or write a closed device.
		* May also be called if VXA is not initialized 
        */
       VXA_INVALID_STATE = 5,
       /**
        * returned by "read()" functions when there is no more data available.
        */
       VXA_NO_MORE_DATA = 6,
       /**
        * returned by method if the feature is not supported.
        */
       VXA_FEATURE_NOT_SUPPORTED = 7,
       /**
        * returned by method if there is an attempt to create more than MAX_VXA_DEVICE_COUNT devices
        */
       VXA_MAX_DEVICES_EXCEEDED = 8,
       /**
        * returned by method if there are no more render buffers available
        */
       VXA_NO_BUFFERS_AVAILABLE = 9
    } VXA_ERROR_CODES;

    /**
     * Values passed when opening capture devices.
     */
    typedef enum {
       /**
        * Use acoustic echo cancellation. This flag is not universally implemented.
        */
       VXA_CAPTURE_OPEN_FLAG_USE_AEC = 0x00000001,
        /**
         * Use automatic gain control. This flag is not universally implemented.
         */
       VXA_CAPTURE_OPEN_FLAG_USE_AGC = 0x00000002
    } VXA_CAPTURE_FLAGS;    


    /**
     * The common return code for vxa* methods
     */
    typedef int vxa_status_t;

    /**
     * Vxa_capture_device_stats_t provides statistics that are useful in debugging
     * audio capture issues. XBox 360 only.
     */
    typedef struct vxa_capture_device_stats_t {
        int buffer_underrun_count;
        int buffer_overrun_count;
        int other_error_count;

        int audio_queue_read_count; // correlate with buffer_underrun_count
        int audio_queue_write_count;// correlate with buffer_overrun_count
    } vxa_capture_device_stats_t;

    /**
     * the callback interface used by applications that wish to implement their own 
     * audio capture device. XBox 360 only.
     */
    typedef struct vxa_apcd {
        /**
         * Used to get the user visible name of the device.
         * @return the user visible name of the device
         */
        const char * (*pf_get_display_name)(struct vxa_apcd *pThis);
        /**
         * Used to get the internal identifier of the device
         * @return the internal device identifier
         */
        const char * (* pf_get_internal_name)(struct vxa_apcd *pThis); // Get the internal device identifier
        /**
         * Opens the device to capture mono audio, PCM encoded, 16 bit interleaved at the provided sample rate.
         *
         *
         * @param samples_per_second - the number of audio samples per second that device should handle. 
         *                             It's the responsibility of the application to resample this if the 
         *                             actual hardware device does not handle the provided sample rate.
         * @param flags - unused 
         *
         */
        vxa_status_t (* pf_open)(struct vxa_apcd *pThis, int samples_per_second, int flags); 
        /**  
         * @return true if the device is open
         */
        int          (* pf_is_open)(struct vxa_apcd *pThis); // returns true if the device is currently open
        /**
          * write audio data to the device
          *
          * @param audio_data - pointer to the audio data
          * @param audio_data_size_bytes - the size of the data in bytes. This must be an integral of 2
          *
          * @return - VXA_SUCCESS if exactly audio_data_size_bytes read, VXA_NO_MORE_DATA if there is less than audio_data_size_bytes available.
          */
        vxa_status_t (* pf_read)(struct vxa_apcd *pThis, void *audio_data, int audio_data_size_bytes);
        /**
         * get statistics about the device. This function is optional.
         *
         * @param stats - a pointer to a stats structure. The application is responsible for allocating/deallocating this.
         */        
        vxa_status_t (* pf_get_stats)(struct vxa_apcd *pThis, vxa_capture_device_stats_t *stats);
        /**
         * close the device 
         */
        vxa_status_t (* pf_close)(struct vxa_apcd *pThis);
    } vxa_apcd_t;

    /**
     * Vxa_render_device_stats_t provides statistics that are useful in debugging
     * audio render issues. XBox 360 only.
     */
    typedef struct vxa_render_device_stats_t {
        int current_output_queue_depth_milliseconds;
        int buffer_underrun_count;
        int buffer_overrun_count;
        int other_error_count;
    
        int audio_queue_read_count;     // correlate with buffer_underrun_count
        int audio_queue_write_count;    // correlate with buffer_overrun_count
    
        int hardware_output_channels;
        int hardware_preferred_samplerate;
        int hardware_preferred_buffer_duration;
    } vxa_render_device_stats_t;

    /**
     * the callback interface used by applications that wish to implement their own 
     * audio render device. XBox 360 only.
     */
    typedef struct vxa_aprd {
        /**
         * Used to get the user visible name of the device.
         * @return the user visible name of the device
         */
        const char * (*pf_get_display_name)(struct vxa_aprd *pThis);
        /**
         * Used to get the internal identifier of the device
         * @return the internal device identifier
         */
        const char * (* pf_get_internal_name)(struct vxa_aprd *pThis);
        /**
         * Opens the device to render stereo audio, PCM encoded, 16 bit interleaved at the provided sample rate.
         *
         *
         * @param samples_per_second - the number of audio samples per second that device should handle. 
         *                             It's the responsibility of the application to resample this if the 
         *                             actual hardware device does not handle the provided sample rate.
         *
         */
        vxa_status_t (* pf_open)(struct vxa_aprd *pThis, int samples_per_second);
        /**  
         * @return true if the device is open
         */
        int          (* pf_is_open)(struct vxa_aprd *pThis);
        /**
          * get a buffer for audio data
          *
          * @param buffer_length_frames - the number of frames of stereo audio data
          * @param native_buffer - a place to receive the buffer handle. Note that the this may be null, and that 
          * the application should only return this to pf_release_buffer.
          */
        vxa_status_t (* pf_get_buffer)(struct vxa_aprd *pThis, int buffer_length_frames, void **native_buffer);
        
        /**
          * get a buffer for audio data
          *
          * @param buffer_length_frames - the number of frames of stereo audio data
          * @param native_buffer - the value returned from pf_get_buffer.
          * @param stereo_buffer - buffer_length_frames of stereo audio data
          */
        vxa_status_t (* pf_release_buffer)(struct vxa_aprd *pThis, int buffer_length_frames, void *native_buffer, void *stereo_buffer);

        /**
         * get statistics about the device. This function is optional.
         *
         * @param stats - a pointer to a stats structure. The application is responsible for allocating/deallocating this.
         */
        vxa_status_t (* pf_get_stats)(struct vxa_aprd *pThis, struct vxa_render_device_stats_t *stats);
        /**
         * close the device 
         */
        vxa_status_t (* pf_close)(struct vxa_aprd *pThis);
    } vxa_aprd_t;

    /**
     * The maximum number of application provided capture devices
     * The maximum number of application provided render devices
     */
    #define MAX_VXA_DEVICE_COUNT 32

#ifndef VIVOX_TYPES_ONLY
    /**
     * Used to create create an application provided capture device (APCD) - XBOX 360 Only
     * @param capture_device - pointer to the interface for the capture device
     * @param apcd_id - a pointer to a returned numeric id 
     *
     */
    VIVOXSDK_DLLEXPORT int vxa_apcd_create(vxa_apcd_t *capture_device, int *apcd_id);

    /**
     * Used to destroy an application provided capture device - XBOX 360 Only
     * @param apcd_id - the id returned from vxa_apcd_create
     */
    VIVOXSDK_DLLEXPORT int vxa_apcd_destroy(int apcd_id);

    /**
     * Used to create create an application provided render device (APRD)  - XBOX 360 Only
     * @param render_device - pointer to the interface for the application provided render device
     * @param aprd_id - a pointer to a returned numeric id 
     */
    VIVOXSDK_DLLEXPORT int vxa_aprd_create(vxa_aprd_t *render_device, int *aprd_id);

    /**
     * Used to destroy an application provided render device  - XBOX 360 Only
     * @param aprd_id - the id returned from vxa_aprd_create
     */
    VIVOXSDK_DLLEXPORT int vxa_aprd_destroy(int aprd_id);

    /**
    * generates a Vivox Access Token. 
    * 
    * !!! This function should only be called when prototyping, or debugging token generation server implementations. !!!
    * It should not be in production code since that would require the issuer/key pair to be resident client memory, which would create 
    * a security exposure.
    *
    * Supported on all platforms except XBox 360
    *
    * @param issuer standard issuer claim
    * @param expiration standard expiration time claim
    * @param vxa Vivox action, e.g. "login", "join", "kick", "mute"
    * @param serial number, to guarantee uniqueness within an epoch second
    * @param subject OPTIONAL - URI of the target of the actions "kick" and "mute", NULL otherwise.
    * @param from_uri SIP From URI
    * @param to_uri SIP To URI
    * @param key token-signing key
    * @param key_len length of key
    * @returns null-terminated buffer to be freed with vx_free() - if NULL is returned, an error occurred.
    *
    */
    VIVOXSDK_DLLEXPORT char *vx_debug_generate_token(const char *issuer, time_t expiration, const char *vxa, unsigned long long serial, const char *subject, const char *from_uri, const char *to_uri, const unsigned char *key, size_t key_len);
#endif

  /**
   * Synchronously downloads content of the specified URL with the GET method.
   *
   * @param url - [in] URL of the resource to download
   * @param response_code - [out] pointer to the returned response code (0 on error, >= 200 after the request finished)
   * @param content - [out] pointer to the downloaded content will be stored here. It is guaranteed to be NULL-terminated. The memory will be allocated automatically. Use vx_free_http() to free up the allocated buffers.
   * @param content_len - [out] pointer to the returned content length
   * @param content_type - [out] pointer to the downloaded content type will be stored here. It is guaranteed to be NULL-terminated. The memory will be allocated automatically. Use vx_free_http() to free up the allocated buffers.
   * @param content_type_len - [out] pointer to the returned content type length
   * @return 0 if successful, no-zero if failed.
   */

    VIVOXSDK_DLLEXPORT unsigned int vx_get_http(const char* url, unsigned int* response_code, char** content, size_t* content_len, char** content_type, size_t* content_type_len);
  
  /**
   * Free the memory allocated during vx_get_http() call. You need to call this function only if vx_get_http() returned zero.
   *
   * @param content - [in] the same value as was passed to vx_get_http()
   * @param content_type - [in] the same value as was passed to vx_get_http()
   */
    VIVOXSDK_DLLEXPORT int vx_free_http(char** content, char** content_type);


    /* Audio Quality Controls functions */

    /**
     * Values for OPUS VBR mode
     */
    typedef enum {
        /**
         * Constant bit rate mode
         */
        opus_mode_cbr = 0,
        /**
         * Limited variable bit rate mode, actual bit rate will never exceed the requested bit rate
         */
        opus_mode_lvbr,
        /**
         * Variable bit rate mode
         */
        opus_mode_vbr,
    } vx_opus_vbr_mode;

    /**
     * Values for OPUS bandwidth
     */
    typedef enum {
        /**
         * Automatic bandwidth (default)
         */
        opus_bandwidth_auto = 0,
        /**
         * Narrowband, 4kHz
         */
        opus_bandwidth_nb,
        /**
         * Medium-band, 6kHz
         */
        opus_bandwidth_mb,
        /**
         * Wideband, 8kHz
         */
        opus_bandwidth_wb,
        /**
         * Super-wideband, 12 kHz
         */
        opus_bandwidth_swb,
        /**
         * Fullband, 20 kHz
         */
        opus_bandwidth_fb,
    } vx_opus_bandwidth;

    /**
     * Set bit rate for all OPUS encoders
     *
     * @param bits_per_second - [in] The requested bit rate, 500-128000
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_set_bit_rate(int bits_per_second);

    /**
     * Get current OPUS bitrate
     *
     * @param p_bits_per_second - [out] pointer to the returned value
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_get_bit_rate(int *p_bits_per_second);
    
    /**
     * Set complexity for all OPUS encoders
     *
     * @param complexity - [in] The requested complexity, 0-10
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_set_complexity(int complexity);
    
    /**
     * Get current OPUS complexity
     *
     * @param p_complexity - [out] pointer to the returned value
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_get_complexity(int *p_complexity);
    
    /**
     * Set VBR mode for all OPUS encoders
     *
     * @param vbr_mode - [in] The requested mode, vx_opus_vbr_mode
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_set_vbr_mode(int vbr_mode);
    
    /**
     * Get current OPUS VBR mode
     *
     * @param p_vbr_mode - [out] pointer to the returned value (vx_opus_vbr_mode)
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_get_vbr_mode(int* p_vbr_mode);
    
    /**
     * Set bandwidth for all OPUS encoders
     *
     * @param bandwidth - [in] The requested bandwidth, vx_opus_bandwidth
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_set_bandwidth(int bandwidth);
    
    /**
     * Get current OPUS bandwidth
     *
     * @param p_bandwidth - [out] pointer to the returned value (vx_opus_bandwidth)
     * @return 0 if successful, non-zero if failed.
     */
    VIVOXSDK_DLLEXPORT int vx_opus_get_bandwidth(int* p_bandwidth);

    /**
     * Get a mask for all available codecs (to be used as configured_codecs)
     */
    VIVOXSDK_DLLEXPORT unsigned int vx_get_available_codecs_mask(void);

    /**
    * Get a recommended default mask for available codecs (to be used as configured_codecs)
    */
    VIVOXSDK_DLLEXPORT unsigned int vx_get_default_codecs_mask(void);

#ifdef __ANDROID__
  /**
   * Get the current period of the Memory Usage information dumping to logcat - Android only
   *
   * @return sampling interval in seconds, 0 if memory dump is turned off
   */
    VIVOXSDK_DLLEXPORT int vx_get_dump_memory_interval(void);
  
   /**
   * Starts the CPU load information dumping to logcat - Android only
   *
   * @param interval - [in] sampling interval in milliseconds. The recommended value is 1000 milliseconds. It is not recommended to have this value under 100 milliseconds.
   * @param report_interval - [in] reporting interval in milliseconds. SDK will dump the collected data to logcat once per reporting interval. The recommended value is 5000 milliseconds. It is not recommended to have this value under 1000 milliseconds.
   *
   * Larger report_interval reduces the average amount of information output by the SDK to logcat.
   * Smaller sampling intervals allows better short peak loads detection.
   */
    VIVOXSDK_DLLEXPORT int vx_cpumonitor_start(int interval, int report_interval);

  /**
   * Stops the CPU load information dumping to logcat - Android only
   */
    VIVOXSDK_DLLEXPORT int vx_cpumonitor_stop(void);
  
  /**
   * Starts the requested number of tight loop threads with the requested priority - Android only
   *
   * Android devices can scale the CPU frequency down or even stop some cores when the load is low. This will decrease the device performance, and affect the CPU load measurements provided by vx_cpumonitor_start() - the reported load will be higher. To prevent this, the vx_cpumonitor_start_eater() is provided. If low priority threads with tight loops will be started for each available CPU core, then the system will set each core frequency to its maximum value, and the measurement results will reflect the CPU load relative to the device's maximum possible performance. Please note that after working for some time at the maximum possible speed, the CPU will heat up, and the system will throttle it just to prevent overheating.
   *
   * @param nthreads - [in] the number of threads with tight loops to start. Passing 0 will start the number of threads matching the number of available device processor cores.
   * @param priority - [in] the scheduling priority to be used for each created thread. 0 will leave the default priority. Valid non-zero values are from -20 (the maximum priority, dangerous! don't use!) to 20 (the minimum possible priority). We recommend to use value 0 for priority.
   */
    VIVOXSDK_DLLEXPORT int vx_cpumonitor_start_eater(int nthreads, int priority);
  
  /**
   * Stops all the CPU eater threads started with vx_cpumonitor_start_eater() call - Android only
   */
    VIVOXSDK_DLLEXPORT int vx_cpumonitor_stop_eater(void);
#endif // __ANDROID__

    /**
    * Values for vx_crash_test( crash type )
    */
    typedef enum vx_crash_test_type {
        /**
        * crash on access to the zero pointer
        */
        vx_crash_access_zero_pointer = 0,
        /**
        * crash on access to a restricted page
        */
        vx_crash_access_violation,
        /**
        * overflows the program stack
        */
        vx_crash_stack_overflow,
        /**
        * corrupts the heap and tries to allocate more memory
        */
        vx_crash_heap_corruption,
#ifdef __clang__
        /**
        * corrupts the heap and tries to allocate more memory
        */
        vx_crash_builtin_trap,
#endif
    } vx_crash_test_type_t;

    /**
    * crashes the program
    *
    * Supported on all platforms
    *
    * @param crash_type - type of crash (see vx_crash_test_type)
    *
    * \ingroup diagnostics
    */
    VIVOXSDK_DLLEXPORT int vx_crash_test(vx_crash_test_type_t crash_type);

    /**
    * Changes api messages rate params
    *
    * @param messageSpeed - messages per second
    * @param fillBucket - is bool
    *
    * \ingroup diagnostics
    */
    VIVOXSDK_DLLEXPORT int vx_set_message_rate_params( unsigned bucketSize, float messageSpeed, int fillBucket );
    /**
    * Returns api messages rate params to default state
    *
    * \ingroup diagnostics
    */
    VIVOXSDK_DLLEXPORT int vx_set_default_message_rate_params(void);

#ifdef __ANDROID__
    VIVOXSDK_DLLEXPORT int vx_android_set_mic_mute( int mute );
#endif

    /**
     * Verifies whether the passed access token is well-formed.
     *
     * @param access_token - [in] access token token to check
     * @param error - [out] optional pointer to the returned verbose error description. Can be NULL. The returned string must be disposed with vx_free() call.
     * @return non-zero if access token looks well-formed, 0 otherwise.
     */
    VIVOXSDK_DLLEXPORT int vx_is_access_token_well_formed(const char* access_token, char** error);

    /**
     * The Vivox eXtended Data (VXD) received.
     *
     * \ingroup session
     */
    typedef struct vx_vxd {
        /**
         * Struct version. Must be sizeof(vx_vxd_t);
         */
        size_t version;
        /**
         * The URI of the participant whose properties are being updated
         */
        char* participant_uri;
        /**
         * The data received.
         */
        char* data;
        /**
         * The amount of data received.
         */  
        size_t data_size;
    } vx_vxd_t;

    /**
     * Send VXD into the channel.
     *
     * @param session_handle - [in] the session handle to send VXD to
     * @param data           - [in] the data to be sent
     * @param size           - [in] the data size in bytes
     * @return  -  0 if successful, non-zero if failed (SDK not initialized, invalid argument (session, ptr), size too big).
     *
     * \ingroup session
     */
    VIVOXSDK_DLLEXPORT int vx_vxd_send(VX_HANDLE session_handle, const char* data, size_t size);

    /**
     * Receive VXD from the channel.
     *
     * @param session_handle - [in]  the session handle to receive VXD from
     * @param vxd_ptr        - [out] on success will contain a pointer to the received VXD. Untouched on failure. The caller is responsible for disposing it with vx_vxd_destroy().
     * @return               -  0 if successful, non-zero if failed (SDK not initialized, not initialized, invalid argument (session, ptr), no more data).
     *
     * \ingroup session
     */
    VIVOXSDK_DLLEXPORT int vx_vxd_recv(VX_HANDLE session_handle, vx_vxd_t** vxd_ptr);

    /**
     * Dispose the VXD object returned by vx_vxd_recv().
     *
     * @param vxd - [in] VXD to dispose
     * @return    -  0 if successful, non-zero if failed (SDK not initialized).
     *
     * \ingroup session
     */
    VIVOXSDK_DLLEXPORT int vx_vxd_destroy(vx_vxd_t* vxd);


#ifdef __cplusplus
}
#endif

#pragma pack(pop)
