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
#include "Vxc.h"
#include "VxcRequests.h"

#pragma pack(push)
#pragma pack(8)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The response for vx_req_connector_create
 * 
 * The XML format of this response can be found here: \ref Connector_Create_1
 *
 * \see vx_req_connector_create
 * \ingroup connector
 */
typedef struct vx_resp_connector_create {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * Valid on success,  handle value for this initialized Connector instance
     */
    VX_HANDLE connector_handle;
    /**
     * Version number of SDK
     */
    char* version_id;
} vx_resp_connector_create_t;

/**
 * The response for vx_req_connector_initiate_shutdown
 * 
 * The XML for this response can be found here: \ref Connector_InitiateShutdown_1
 * 
 * \see vx_req_connector_initiate_shutdown 
 * \ingroup connector
 */
typedef struct vx_resp_connector_initiate_shutdown {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * DEPRECATED
     * @deprecated
     */
    char* client_name;
} vx_resp_connector_initiate_shutdown_t;

/**
 * The response for vx_req_account_login
 *
 * The XML for this response can be found here: \ref Account_Login_1
 * 
 * \see vx_req_account_login
 * \ingroup login
 */
typedef struct vx_resp_account_login {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Valid on success, handle value for this initialized account login
    */
    VX_HANDLE account_handle;
    /**
    * The ID of the account
    */
    int account_id;
    /**
    * Display Name of the account, if available
    */
    char* display_name;
    /**
     * The uri of the user
     */
    char *uri;
    /**
    * Number of aliases associated with this account
    */
    int num_aliases;
    /**
     * the uri for list based buddy presence. Available only in select systems.
     */
    char *buddy_list_uri;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
} vx_resp_account_login_t;

/**
 * The response for vx_req_account_authtoken_login
 *
 * The XML for this response can be found here: \ref Account_AuthTokenLogin_1
 * 
 * \see vx_req_account_authtoken_login
 * \ingroup login
 */
typedef struct vx_resp_account_authtoken_login {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Valid on success, handle value for this initialized account login
    */
    VX_HANDLE account_handle;
    /**
    * The ID of the account
    */
    int account_id;
    /**
    * User Name of the account
    */
    char* user_name;
    /**
    * Display Name of the account, if available
    */
    char* display_name;
    /**
     * The uri of the user
     */
    char *uri;
    /**
    * Number of aliases associated with this account
    */
    int num_aliases;
    /**
     * the uri for list based buddy presence. Available only in select systems.
     */
    char *buddy_list_uri;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
} vx_resp_account_authtoken_login_t;

/**
 * The response for vx_req_account_anonymous_login
 *
 * The XML for this response can be found here: \ref Account_AnonymousLogin_1
 * 
 * \see vx_req_account_anonymous_login
 * \ingroup login
 */
typedef struct vx_resp_account_anonymous_login {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Valid on success, handle value for this initialized account login
    */
    VX_HANDLE account_handle;
    /**
    * The ID of the account
    */
    int account_id;
    /**
    * Display Name of the account, if available
    */
    char* displayname;
    /**
     * The uri of the user
     */
    char *uri;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
} vx_resp_account_anonymous_login_t;

/**
 * The response for vx_req_account_logout
 *
 * The XML for this response can be found here: \ref Account_Logout_1
 *
 * \see vx_req_account_logout
 * \ingroup login
 */
typedef struct vx_resp_account_logout {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_logout_t;

/**
 * The response for vx_req_account_set_login_properties
 *
 * The XML for this response can be found here:
 *
 * \see vx_req_account_set_login_properties
 * \ingroup obsolete
 */
typedef struct vx_resp_account_set_login_properties {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_set_login_properties_t;

/**
 * The response for vx_req_sessiongroup_create
 * 
 * The XML for this response can be found here:
 *
 * \see vx_req_sessiongroup_create
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_create {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Valid on success, handle value for this created session
    */
    VX_HANDLE sessiongroup_handle;
} vx_resp_sessiongroup_create_t;

/**
 * The response for vx_req_sessiongroup_terminate
 *
 * The XML for this response can be found here:
 *
 * \see vx_req_sessiongroup_terminate
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_terminate {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_terminate_t;

/**
 * The response for vx_req_sessiongroup_add_session
 *
 * The XML for this response can be found here:
 *
 * \see vx_req_sessiongroup_add_session
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_add_session {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Valid on success, handle value for this created session
    */
    VX_HANDLE session_handle;
} vx_resp_sessiongroup_add_session_t;

/**
 * The response for vx_req_sessiongroup_remove_session
 *
 * The XML for this response can be found here: \ref SessionGroup_RemoveSession_1
 *
 * \see vx_req_sessiongroup_remove_session
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_remove_session {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_remove_session_t;

#ifndef VX_DISABLE_SESSIONGRP_FOCUS

/**
 * The response for vx_req_sessiongroup_set_focus
 *
 * The XML for this response can be found here: \ref SessionGroup_SetFocus_1
 *
 * \see vx_req_sessiongroup_set_focus
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_set_focus {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_set_focus_t;

/**
 * The response for vx_req_sessiongroup_unset_focus
 *
 * The XML for this response can be found here: \ref SessionGroup_UnsetFocus_1
 *
 * \see vx_req_sessiongroup_unset_focus
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_unset_focus {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_unset_focus_t;

/**
 * The response for vx_req_sessiongroup_reset_focus
 *
 * The XML for this response can be found here: \ref SessionGroup_ResetFocus_1
 *
 * \see vx_req_sessiongroup_reset_focus
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_reset_focus {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_reset_focus_t;
#endif

/**
 * The response for vx_req_sessiongroup_set_tx_session
 *
 * The XML for this response can be found here: \ref SessionGroup_SetTxSession_1
 *
 * \see vx_req_sessiongroup_set_tx_session
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_set_tx_session {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_set_tx_session_t;

/**
 * The response for vx_req_sessiongroup_set_tx_all_sessions
 * 
 * The XML for this response can be found here: \ref SessionGroup_SetTxAllSessions_1
 *
 * \see vx_req_sessiongroup_set_tx_all_sessions
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_set_tx_all_sessions {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_set_tx_all_sessions_t;

/**
 * The response for vx_req_sessiongroup_set_tx_no_session
 * 
 * The XML for this response can be found here: \ref SessionGroup_SetTxNoSession_1
 *
 * \see vx_req_sessiongroup_set_tx_no_session
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_set_tx_no_session {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_set_tx_no_session_t;

/**
 * The response for vx_req_sessiongroup_set_session_3d_position
 * 
 * The XML for this response can be found here: \ref SessionGroup_SetSession3DPosition_1
 *
 * \see vx_req_sessiongroup_set_session_3d_position
 * \ingroup sessiongroup
 */
typedef struct vx_resp_sessiongroup_set_session_3d_position {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_set_session_3d_position_t;

/**
 * The response for vx_req_sessiongroup_control_audio_injection
 * 
 * The XML for this response can be found here: \ref SessionGroup_ControlAudioInjection_1
 *
 * \see vx_req_sessiongroup_control_audio_injection
 * \ingroup adi
 */
typedef struct vx_resp_sessiongroup_control_audio_injection {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_sessiongroup_control_audio_injection_t;

/**
 * DEPRECATED - the response for vx_req_session_create
 * 
 * The XML for this response can be found here: \ref Session_Create_1
 *
 * \see vx_req_session_create
 * \deprecated
 */
typedef struct vx_resp_session_create {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Valid on success,  handle value for this created session group
    */
    VX_HANDLE sessiongroup_handle;
    /**
    * Valid on success,  handle value for this created session
    */
    VX_HANDLE session_handle;
} vx_resp_session_create_t;

/**
 * The response for vx_req_session_media_connect_t
 * 
 * The XML for this response can be found here: \ref Session_MediaConnect_1
 *
 * \see vx_req_session_media_connect
 * \ingroup session
 */
typedef struct vx_resp_session_media_connect {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_media_connect_t;

/**
 * The response for vx_req_session_media_disconnect_t
 * 
 * The XML for this response can be found here: \ref Session_MediaDisconnect_1
 *
 * \see vx_req_session_media_disconnect
 * \ingroup session
 */
typedef struct vx_resp_session_media_disconnect {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_media_disconnect_t;

/**
 * The response for vx_req_session_text_connect_t
 * 
 * The XML for this response can be found here: \ref Session_TextConnect_1
 *
 * \see vx_req_session_text_connect
 * \ingroup session
 */
typedef struct vx_resp_session_text_connect {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_text_connect_t;

/**
 * The response for vx_req_session_text_disconnect_t
 * 
 * The XML for this response can be found here: \ref Session_TextDisconnect_1
 *
 * \see vx_req_session_text_disconnect
 * \ingroup session
 */
typedef struct vx_resp_session_text_disconnect {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_text_disconnect_t;

/**
 * The response for vx_req_session_terminate_t
 * 
 * The XML for this response can be found here: \ref Session_Terminate_1
 *
 * \see vx_req_session_terminate
 * \ingroup session
 */
typedef struct vx_resp_session_terminate {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_terminate_t;

/**
 * The response for vx_req_session_mute_local_speaker_t
 * 
 * The XML for this response can be found here: \ref Session_MuteLocalSpeaker_1
 *
 * \see vx_req_session_mute_local_speaker
 * \ingroup session
 */
typedef struct vx_resp_session_mute_local_speaker {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_mute_local_speaker_t;

/**
 * The response for vx_req_session_set_local_speaker_volume_t
 * 
 * The XML for this response can be found here: \ref Session_SetLocalSpeakerVolume_1
 *
 * \see vx_req_session_set_local_speaker_volume
 * \ingroup session
 */
typedef struct vx_resp_session_set_local_speaker_volume {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_set_local_speaker_volume_t;

/**
 * The response for vx_req_session_channel_invite_user_t
 * 
 * The XML for this response can be found here: \ref Session_InviteUser_1
 *
 * \see vx_req_session_channel_invite_user
 * \ingroup session
 */
typedef struct vx_resp_session_channel_invite_user {
    vx_resp_base_t base;
} vx_resp_session_channel_invite_user_t;

/**
 * The response for vx_req_session_set_participant_volume_for_me_t
 * 
 * The XML for this response can be found here: \ref Session_SetParticipantVolumeForMe_1
 *
 * \see vx_req_session_set_participant_volume_for_me
 * \ingroup session
 */
typedef struct vx_resp_session_set_participant_volume_for_me {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_set_participant_volume_for_me_t;

/**
 * The response for vx_req_session_set_participant_mute_for_me_t
 * 
 * The XML for this response can be found here: \ref Session_SetParticipantMuteForMe_1
 *
 * \see vx_req_session_set_participant_mute_for_me
 * \ingroup session
 */
typedef struct vx_resp_session_set_participant_mute_for_me {
    vx_resp_base_t base;
} vx_resp_session_set_participant_mute_for_me_t;

/**
 * The response for vx_req_session_set_3d_position_t
 * 
 * The XML for this response can be found here: \ref Session_Set3DPosition_1
 *
 * \see vx_req_session_set_3d_position
 * \ingroup session
 */
typedef struct vx_resp_session_set_3d_position {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_set_3d_position_t;

/**
 * The response for vx_req_session_set_voice_font_t
 * 
 * The XML for this response can be found here: \ref Session_SetVoiceFont_1
 *
 * \see vx_req_session_set_voice_font
 * \ingroup voicefonts
 */
typedef struct vx_resp_session_set_voice_font {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_set_voice_font_t;


/**
 * The response for vx_req_account_channel_get_participants_t
 * 
 * \see vx_req_account_channel_get_participants
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_get_participants {
    /**
    * The common properties for all responses.
    */
    vx_resp_base_t base;
    /**
    * The number of participants active in the channel
    */
    int participant_count;
    /**
    * The list of participants active in the channel
    */
    vx_participant_t** participants;
    /**
     * The page number of the result set
     */
    int page;
    /**
     * The number of the first result returned in this set
     */
    int from;
    /**
     * The number of the last result returned in this set
     */
    int to;
    /**
     * The total number of results in the result set (not just this page)
     */
    int total;
} vx_resp_account_channel_get_participants_t;

/**
 * The response for vx_req_account_channel_change_owner_t
 *
 * \see vx_req_account_channel_change_owner_t
 */
typedef struct vx_resp_account_channel_change_owner {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_channel_change_owner_t;
/**
 * The response for vx_req_account_send_user_app_data_t
 *
 * \see vx_req_account_send_user_app_data_t
 */
typedef struct vx_resp_account_send_user_app_data {
    /**
    * The common properties for all responses.
    */
    vx_resp_base_t base;
} vx_resp_account_send_user_app_data_t;
/**
 * The response for vx_req_account_channel_create_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelCreate_1
 *
 * \see vx_req_account_channel_create
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_create {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * If the create call is successful the ChannelURI will be returned
    */
    char* channel_uri;
} vx_resp_account_channel_create_t;

/**
 * The response for vx_req_account_channel_update_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelUpdate_1
 *
 * \see vx_req_account_channel_update
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_update {
    vx_resp_base_t base;
} vx_resp_account_channel_update_t;

/**
 * The response for vx_req_account_channel_delete_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelFavoriteDelete_1
 *
 * \see vx_req_account_channel_delete
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_delete {
    vx_resp_base_t base;
} vx_resp_account_channel_delete_t;


/**
 * The response for vx_req_account_channel_favorites_get_list_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelFavoritesGetList_1
 *
 * \see vx_req_account_channel_favorites_get_list
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_favorites_get_list {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The number of favorite groups returned in the result set
    */
    int group_count;
    /**
    * The number of favorites returned in the result set
    */
    int favorite_count;
    /**
    * The list of favorite groups returned in the result set
    */
    vx_channel_favorite_group_t** groups;
    /**
    * The list of favorites returned in the result set
    */
    vx_channel_favorite_t** favorites;
} vx_resp_account_channel_favorites_get_list_t;

/**
 * The response for vx_req_account_channel_favorite_set_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelFavoriteSet_1
 *
 * \see vx_req_account_channel_favorite_set
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_favorite_set {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The ID of the newly created favorite
    */
    int channel_favorite_id;
} vx_resp_account_channel_favorite_set_t;

/**
 * The response for vx_req_account_channel_favorite_delete_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelFavoriteDelete_1
 *
 * \see vx_req_account_channel_favorite_delete
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_favorite_delete {
    vx_resp_base_t base;
} vx_resp_account_channel_favorite_delete_t;

/**
 * The response for vx_req_account_channel_favorite_group_set_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelFavoriteGroupSet_1
 *
 * \see vx_req_account_channel_favorite_group_set
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_favorite_group_set {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The ID of the newly created favorite
    */
    int group_id;
} vx_resp_account_channel_favorite_group_set_t;

/**
 * The response for vx_req_account_channel_favorite_group_delete_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelFavoriteGroupDelete_1
 *
 * \see vx_req_account_channel_favorite_group_delete
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_favorite_group_delete {
    vx_resp_base_t base;
} vx_resp_account_channel_favorite_group_delete_t;

/**
 * The response for vx_req_account_channel_get_info_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelGetInfo_1
 *
 * \see vx_req_account_channel_get_info
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_get_info {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Structure containing information about the specified channel
    */
    vx_channel_t* channel;
} vx_resp_account_channel_get_info_t;

/**
 * The response for vx_req_account_channel_search_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelSearch_1
 *
 * \see vx_req_account_channel_search
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_search {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * The page number of the result set
     */
    int page;
    /**
     * The number of the first result returned in this set
     */
    int from;
    /**
     * The number of the last result returned in this set
     */
    int to;
    /**
     * The number of channels returned in the channels array
     */
    int channel_count;
     /**
     * The list of channels
     */
    vx_channel_t** channels;
    
} vx_resp_account_channel_search_t;

#ifndef VX_DISABLE_PRESENCE
/**
 * The response for vx_req_account_buddy_search_t
 * 
 * The XML for this response can be found here: \ref Account_AccountSearch_1
 *
 * \see vx_req_account_buddy_search
 * \ingroup obsolete
 */
typedef struct vx_resp_account_buddy_search {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * The page number of the result set
     */
    int page;    
    /**
     * The number of the first result returned in this set
     */
    int from;
    /**
     * The number of the last result returned in this set
     */
    int to;
    /**
     * The number of channels returned in the result set
     */
    int buddy_count;
    /**
     * The list of buddies returned from the search
     */
    vx_buddy_t** buddies;
} vx_resp_account_buddy_search_t;
#endif

/**
 * The response for vx_req_account_channel_add_moderator_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelAddModerator_1
 *
 * \see vx_req_account_channel_add_moderator
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_add_moderator {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_channel_add_moderator_t;

/**
 * The response for vx_req_account_channel_remove_moderator_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelRemoveModerator_1
 *
 * \see vx_req_account_channel_remove_moderator
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_remove_moderator {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_channel_remove_moderator_t;

/**
 * The response for vx_req_account_channel_get_moderators_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelGetModerators_1
 *
 * \see vx_req_account_channel_get_moderators
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_get_moderators {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Number of entries in the "participants" array
    */
    int participants_size;
    /**
    * List of moderators for this channel
    */
    vx_participant_t** participants;
} vx_resp_account_channel_get_moderators_t;

#ifndef VX_DISABLE_ACL
/**
 * The response for vx_req_account_channel_add_acl_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelAddACL_1
 *
 * \see vx_req_account_channel_add_acl
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_add_acl {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_channel_add_acl_t;

/**
 * The response for vx_req_account_channel_remove_acl_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelRemoveACL_1
 *
 * \see vx_req_account_channel_remove_acl
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_remove_acl {
    vx_resp_base_t base;
} vx_resp_account_channel_remove_acl_t;

/**
 * The response for vx_req_account_channel_get_acl_t
 * 
 * The XML for this response can be found here: \ref Account_ChannelGetACL_1
 *
 * \see vx_req_account_channel_get_acl
 * \ingroup obsolete
 */
typedef struct vx_resp_account_channel_get_acl {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Number of entries in the "participants" array
    */
    int participants_size;
    /**
    * List of users in the Channel's Access Control List.
    * NOTE: Only the uri field on the Participant structures is guaranteed to be present.
    */
    vx_participant_t** participants;
} vx_resp_account_channel_get_acl_t;

#endif

/**
 * The response for vx_req_channel_mute_user_t
 * 
 * The XML for this response can be found here: \ref Channel_MuteUser_1
 *
 * \see vx_req_channel_mute_user
 * \ingroup channel
 */
typedef struct vx_resp_channel_mute_user {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_channel_mute_user_t;

/**
 * The response for vx_req_channel_ban_user_t
 * 
 * The XML for this response can be found here: \ref Channel_BanUser_1
 *
 * \see vx_req_channel_ban_user
 * \ingroup obsolete
 */
typedef struct vx_resp_channel_ban_user {
    vx_resp_base_t base;
} vx_resp_channel_ban_user_t;

/**
 * The response for vx_req_channel_get_banned_users_t
 * 
 * The XML for this response can be found here: \ref Channel_GetBannedUsers_1
 *
 * \see vx_req_channel_get_banned_users
 * \ingroup obsolete
 */
typedef struct vx_resp_channel_get_banned_users {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Number of entries in the "banned_users" array
    */
    int banned_users_count;
    /**
    * List of users banned from the specified channel
    */
    vx_participant_t** banned_users;
} vx_resp_channel_get_banned_users_t;

/**
 * The response for vx_req_channel_kick_user_t
 * 
 * The XML for this response can be found here: \ref Channel_KickUser_1
 *
 * \see vx_req_channel_kick_user
 * \ingroup obsolete
 */
typedef struct vx_resp_channel_kick_user {
    vx_resp_base_t base;
} vx_resp_channel_kick_user_t;

/**
 * The response for vx_req_channel_mute_all_users_t
 * 
 * The XML for this response can be found here: \ref Channel_MuteAllUsers_1
 *
 * \see vx_req_channel_mute_all_users
 * \ingroup channel
 */
typedef struct vx_resp_channel_mute_all_users {
    vx_resp_base_t base;
} vx_resp_channel_mute_all_users_t;

/**
 * The response for vx_req_channel_set_lock_mode_t
 * 
 * The XML for this response can be found here: \ref Channel_SetLockMode_1
 *
 * \see vx_req_channel_set_lock_mode
 * \ingroup obsolete
 */
typedef struct vx_resp_channel_set_lock_mode {
    vx_resp_base_t base;
} vx_resp_channel_set_lock_mode_t;

/**
 * The response for vx_req_connector_mute_local_mic_t
 * 
 * The XML for this response can be found here: \ref Connector_MuteLocalMic_1
 *
 * \see vx_req_connector_mute_local_mic
 * \ingroup devices
 */
typedef struct vx_resp_connector_mute_local_mic {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_connector_mute_local_mic_t;

/**
 * The response for vx_req_connector_mute_local_speaker_t
 * 
 * The XML for this response can be found here: \ref Connector_MuteLocalSpeaker_1
 *
 * \see vx_req_connector_mute_local_speaker
 * \ingroup devices
 */
typedef struct vx_resp_connector_mute_local_speaker {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_connector_mute_local_speaker_t;

/**
 * DEPRECATED - use vx_resp_aux_set_mic_level_t instead.
 * The response for vx_req_connector_set_local_mic_volume_t
 * 
 * The XML for this response can be found here: \ref Connector_SetLocalMicVolume_1
 *
 *
 * \see vx_req_connector_set_local_mic_volume
 * \ingroup obsolete
 * \deprecated 
 */
typedef struct vx_resp_connector_set_local_mic_volume {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_connector_set_local_mic_volume_t;

/**
 * DEPRECATED - use vx_resp_aux_set_speaker_level_t instead.
 * The response for vx_req_connector_set_local_speaker_volume_t
 * 
 * The XML for this response can be found here: \ref Connector_SetLocalSpeakerVolume_1
 *
 *
 * \see vx_req_connector_set_local_speaker_volume
 * \ingroup obsolete
 * \deprecated 
 */
typedef struct vx_resp_connector_set_local_speaker_volume {
    vx_resp_base_t base;
} vx_resp_connector_set_local_speaker_volume_t;

/**
 * The response for vx_req_connector_get_local_audio_info_t
 * 
 * The XML for this response can be found here: \ref Connector_GetLocalAudioInfo_1
 *
 * \see vx_req_connector_get_local_audio_info
 * \ingroup devices
 */
typedef struct vx_resp_connector_get_local_audio_info {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The level of the audio, a number between 0 and 100 where 50 represents 'normal' speaking volume
    */
    int speaker_volume;
    /**
    * A number, either true (mute) and false (unmute)
    */
    int is_speaker_muted;
    /**
    * The level of the audio, a number between 0 and 100 where 50 represents 'normal' speaking volume
    */
    int mic_volume;
    /**
    * 1 (mute) or 0 (unmute)
    */
    int is_mic_muted;
} vx_resp_connector_get_local_audio_info_t;

#ifndef VX_DISABLE_PRESENCE
/**
 * The response for vx_req_account_buddy_set_t
 * 
 * The XML for this response can be found here: \ref Account_BuddySet_1
 *
 * \see vx_req_account_buddy_set
 * \ingroup buddy
 */
typedef struct vx_resp_account_buddy_set {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The account id of the buddy
    */
    int account_id;
} vx_resp_account_buddy_set_t;

/**
 * The response for vx_req_account_buddy_delete_t
 * 
 * The XML for this response can be found here: \ref Account_BuddyDelete_1
 *
 * \see vx_req_account_buddy_delete
 * \ingroup buddy
 */
typedef struct vx_resp_account_buddy_delete {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_buddy_delete_t;

/**
 * The response for vx_req_account_buddygroup_set_t
 * 
 * The XML for this response can be found here: \ref Account_BuddyGroupSet_1
 *
 * \see vx_req_account_buddygroup_set
 * \ingroup buddy
 */
typedef struct vx_resp_account_buddygroup_set {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * The ID of the group being set (may be updated from 0 value if group is newly created)
     */
    int group_id;
} vx_resp_account_buddygroup_set_t;

/**
 * The response for vx_req_account_buddygroup_delete_t
 * 
 * The XML for this response can be found here: \ref Account_BuddyGroupDelete_1
 *
 * \see vx_req_account_buddygroup_delete
 * \ingroup buddy
 */
typedef struct vx_resp_account_buddygroup_delete {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_buddygroup_delete_t;

/**
 * DEPRECATED - The response for vx_req_account_list_buddies_and_groups_t
 * 
 * The XML for this response can be found here: \ref Account_ListBuddiesAndGroups_1
 *
 * \see vx_req_account_list_buddies_and_groups
 * \ingroup buddy
 * \deprecated
 */
typedef struct vx_resp_account_list_buddies_and_groups {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Number of buddies found in the list
    */
    int buddy_count;
    /**
    * Number of groups found in the list
    */
    int group_count;
    /**
    * A collection of buddy structures
    */
    vx_buddy_t** buddies;
    /**
    * A collection of group structures
    */
    vx_group_t** groups;
} vx_resp_account_list_buddies_and_groups_t;
#endif

/**
 * The response for vx_req_session_send_message_t
 * 
 * The XML for this response can be found here: \ref Session_SendMessage_1
 *
 * \see vx_req_session_send_message
 * \ingroup buddy
 */
typedef struct vx_resp_session_send_message {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_send_message_t;

#ifndef VX_DISABLE_PRESENCE
/**
 * The response for vx_req_account_set_presence_t
 * 
 * The XML for this response can be found here: \ref Account_SetPresence_1
 *
 * \see vx_req_account_set_presence
 * \ingroup buddy
 */
typedef struct vx_resp_account_set_presence {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_set_presence_t;

/**
 * The response for vx_req_account_send_subscription_reply_t
 * 
 * The XML for this response can be found here: \ref Account_SendSubscriptionReply_1
 *
 * \see vx_req_account_send_subscription_reply
 * \ingroup buddy
 */
typedef struct vx_resp_account_send_subscription_reply {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_send_subscription_reply_t;
#endif

/**
 * The response for vx_req_session_send_notification_t
 * 
 * The XML for this response can be found here: \ref Session_SendNotification_1
 *
 * \see vx_req_session_send_notification
 * \ingroup session
 */
typedef struct vx_resp_session_send_notification {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_send_notification_t;

/**
 * The response for vx_req_session_send_dtmf_t
 * 
 * The XML for this response can be found here: \ref Session_SendDTMF_1
 *
 * \see vx_req_session_send_dtmf
 * \ingroup obsolete
 */
typedef struct vx_resp_session_send_dtmf {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_session_send_dtmf_t;

#ifndef VX_DISABLE_PRESENCE
/**
 * The response for vx_req_account_create_block_rule_t
 * 
 * The XML for this response can be found here: \ref Account_CreateBlockRule_1
 *
 * \see vx_req_account_create_block_rule
 * \ingroup buddy
 */
typedef struct vx_resp_account_create_block_rule {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_create_block_rule_t;

/**
 * The response for vx_req_account_delete_block_rule_t
 * 
 * The XML for this response can be found here: \ref Account_DeleteBlockRule_1
 *
 * \see vx_req_account_delete_block_rule
 * \ingroup buddy
 */
typedef struct vx_resp_account_delete_block_rule {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_delete_block_rule_t;

/**
 * The response for vx_req_account_list_block_rules_t
 * 
 * The XML for this response can be found here: \ref Account_ListBlockRules_1
 *
 * \see vx_req_account_list_block_rules
 * \ingroup buddy
 */
typedef struct vx_resp_account_list_block_rules {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Number of block rules returned in the list
    */
    int rule_count;
    /**
    * List of block rules for the given account
    */
    vx_block_rule_t** block_rules;
} vx_resp_account_list_block_rules_t;

/**
 * The response for vx_req_account_create_auto_accept_rule_t
 * 
 * The XML for this response can be found here: \ref Account_CreateAutoAcceptRule_1
 *
 * \see vx_req_account_create_auto_accept_rule
 * \ingroup buddy
 */
typedef struct vx_resp_account_create_auto_accept_rule {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_create_auto_accept_rule_t;

/**
 * The response for vx_req_account_delete_auto_accept_rule_t
 * 
 * The XML for this response can be found here: \ref Account_DeleteAutoAcceptRule_1
 *
 * \see vx_req_account_delete_auto_accept_rule
 * \ingroup buddy
 */
typedef struct vx_resp_account_delete_auto_accept_rule {
    vx_resp_base_t base;
} vx_resp_account_delete_auto_accept_rule_t;

/**
 * The response for vx_req_account_list_auto_accept_rules_t
 * 
 * The XML for this response can be found here: \ref Account_ListAutoAcceptRules_1
 *
 * \see vx_req_account_list_auto_accept_rules
 * \ingroup buddy
 */
typedef struct vx_resp_account_list_auto_accept_rules {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * Number of block rules returned in the list
    */
    int rule_count;
    /**
    * The list of block rules for the given account
    */
    vx_auto_accept_rule_t** auto_accept_rules;
} vx_resp_account_list_auto_accept_rules_t;
#endif

/**
 * \deprecated - DEPRECATED
 *
 * The response for vx_req_account_update_account_t
 * 
 * The XML for this response can be found here: \ref Account_UpdateAccount_1
 * \see vx_req_account_update_account
 */
typedef struct vx_resp_account_update_account {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_update_account_t;

/**
 * \deprecated - DEPRECATED
 * 
 * the response for vx_req_account_get_account_t
 * 
 * The XML for this response can be found here: \ref Account_GetAccount_1
 *
 * \see vx_req_account_get_account
 */
typedef struct vx_resp_account_get_account {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * A pointer to a 'vx_account_t' struct, containing information about the account
    */
    vx_account_t* account;
} vx_resp_account_get_account_t;

/*
 * The response for vx_req_account_send_sms_t
 * 
 * The XML for this response can be found here: \ref Account_SendSMS_1
 *
 * \see vx_req_account_send_sms
 */
typedef struct vx_resp_account_send_sms {
    /*
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_send_sms_t;


/**
 * The response for vx_req_aux_connectivity_info_t
 * 
 * The XML for this response can be found here: \ref Aux_ConnectivityInfo_1
 *
 * \see vx_req_aux_connectivity_info
 * \ingroup diagnostics
 */
typedef struct vx_resp_aux_connectivity_info {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    int count;
    /**
    * List of the test results for each test
    */
    vx_connectivity_test_result_t** test_results;
    /**
     * The well known ip address used.
     */
    char* well_known_ip;
    /**
     * The stun server used.
     */
    char* stun_server;
    /** 
     * The echo server used.
     */
    char* echo_server;
    /** 
     * The echo port used.
     */
    int echo_port;
    /**
     * The timeout used.
     */
    int timeout;
    /**
     * The first (primary) simulated port for SIP testing
     */
    int first_sip_port;
    /**
     * The second (fallback) simulated port for SIP testing
     */
    int second_sip_port;
    /**
     * The simulated RTP port
     */
    int rtp_port;
    /**
     * The simulated RTCP port
     */
    int rtcp_port;
} vx_resp_aux_connectivity_info_t;

/**
 * The response for vx_req_aux_get_render_devices_t
 * 
 * The XML for this response can be found here: \ref Aux_GetRenderDevices_1
 *
 * \see vx_req_aux_get_render_devices
 * \ingroup devices
 */
typedef struct vx_resp_aux_get_render_devices {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The number of render devices found
    */
    int count;
    /**
    * An array of pointers to 'vx_device_t' structs 
    */
    vx_device_t** render_devices;
    /**
    * The render device currently in use by the Vivox SDK.
    */
    vx_device_t* current_render_device;
    /**
    * The effective render device
    */
    vx_device_t* effective_render_device;
    /**
     * The default system render device
     */
    vx_device_t* default_render_device;
    /**
     * The default communication render device
     */
    vx_device_t* default_communication_render_device;
} vx_resp_aux_get_render_devices_t;

/**
 * The response for vx_req_aux_get_capture_devices_t
 * 
 * The XML for this response can be found here: \ref Aux_GetCaptureDevices_1
 *
 * \see vx_req_aux_get_capture_devices
 * \ingroup devices
 */
typedef struct vx_resp_aux_get_capture_devices {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The number of capture devices found
    */
    int count;
    /**
    * The list of capture devices
    */
    vx_device_t** capture_devices;
    /**
    * The current capture device
    */
    vx_device_t* current_capture_device;
    /**
    * The effective capture device
    */
    vx_device_t* effective_capture_device;
    /**
     * The default system capture device
     */
    vx_device_t* default_capture_device;
    /**
     * The default communication capture device
     */
    vx_device_t* default_communication_capture_device;
} vx_resp_aux_get_capture_devices_t;

/**
 * The response for vx_req_aux_set_render_device_t
 * 
 * The XML for this response can be found here: \ref Aux_SetRenderDevice_1
 *
 * \see vx_req_aux_set_render_device
 * \ingroup devices
 */
typedef struct vx_resp_aux_set_render_device {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_set_render_device_t;

/**
 * The response for vx_req_aux_set_capture_device_t
 * 
 * The XML for this response can be found here: \ref Aux_SetCaptureDevice_1
 *
 * \see vx_req_aux_set_capture_device
 * \ingroup devices
 */
typedef struct vx_resp_aux_set_capture_device {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_set_capture_device_t;

/**
 * The response for vx_req_aux_get_mic_level_t
 * 
 * The XML for this response can be found here: \ref Aux_GetMicLevel_1
 *
 * \see vx_req_aux_get_mic_level
 * \ingroup devices
 */
typedef struct vx_resp_aux_get_mic_level {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * The master 'microphone' level. 
     * Non negative integer value between 0 and 100 (inclusive). +6 increase represents a 
     * doubling of energy, +20 increase represents a ten fold increase in energy. Default value is 50.
     */
    int level;
} vx_resp_aux_get_mic_level_t;

/**
 * The response for vx_req_aux_get_speaker_level_t
 * 
 * The XML for this response can be found here: \ref Aux_GetSpeakerLevel_1
 *
 * \see vx_req_aux_get_speaker_level
 * \ingroup devices
 */
typedef struct vx_resp_aux_get_speaker_level {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * The master 'speaker' level. 
    * Non negative integer value between 0 and 100 (inclusive). +6 increase represents a doubling of energy, 
    * +20 increase represents a ten fold increase in energy. Default value is 50.
    */
    int level;
} vx_resp_aux_get_speaker_level_t;

/**
 * The response for vx_req_aux_set_mic_level_t
 * 
 * The XML for this response can be found here: \ref Aux_SetMicLevel_1
 *
 * \see vx_req_aux_set_mic_level
 * \ingroup devices
 */
typedef struct vx_resp_aux_set_mic_level {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_set_mic_level_t;

/**
 * The response for vx_req_aux_set_speaker_level_t
 * 
 * The XML for this response can be found here: \ref Aux_SetSpeakerLevel_1
 *
 * \see vx_req_aux_set_speaker_level_t
 * \ingroup devices
 */
typedef struct vx_resp_aux_set_speaker_level {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_set_speaker_level_t;

/**
 * The response for vx_req_aux_render_audio_start_t
 * 
 * The XML for this response can be found here: \ref Aux_RenderAudioStart_1
 *
 * \see vx_req_aux_render_audio_start
 * \ingroup devices
 */
typedef struct vx_resp_aux_render_audio_start {
    vx_resp_base_t base;
} vx_resp_aux_render_audio_start_t;

/**
 * The response for vx_req_aux_render_audio_modify_t
 * 
 */
typedef struct vx_resp_aux_render_audio_modify {
    vx_resp_base_t base;
} vx_resp_aux_render_audio_modify_t;

/**
 * The response for vx_req_aux_get_vad_properties_t
 * 
 * \see vx_req_aux_set_vad_properties
 * \ingroup devices
 */
typedef struct vx_resp_aux_get_vad_properties {
    vx_resp_base_t base;
    /**
    * The 'Hangover time' - the time (in milliseconds) that it takes for the VAD to switch back to silence 
    * from speech mode after the last speech frame has been detected.
    */
    int vad_hangover;
    /**
    * The 'vad sensitivity' - A dimensionless value between 0 and 100, indicating the 'sensitivity of the VAD'.
    * Increasing this value corresponds to decreasing the sensitivity of the VAD (i.e. '0' is most 
    * sensitive, while 100 is 'least sensitive').
    */
    int vad_sensitivity;
    /**
    * The 'vad noise floor' - A dimensionless value between 0 and 20000 (default 576) that controls how the vad separates speech from background noise
    */
    int vad_noise_floor;
    /**
    * VAD Automatic Parameter Selection - If this mode is 1 (enabled), then vad_hangover, vad_sensitivity, and vad_noise_floor will be ignored and the VAD will optimize parameters automatically
    */
    int vad_auto;
} vx_resp_aux_get_vad_properties_t;

/**
 * The response for vx_req_aux_set_vad_properties_t
 * 
 * \see vx_req_aux_set_vad_properties
 * \ingroup devices
 */
typedef struct vx_resp_aux_set_vad_properties {
    vx_resp_base_t base;
} vx_resp_aux_set_vad_properties_t;

/**
 * The response for vx_req_aux_render_audio_stop_t
 * 
 * The XML for this response can be found here: \ref Aux_RenderAudioStop_1
 *
 * \see vx_req_aux_render_audio_stop
 * \ingroup devices
 */
typedef struct vx_resp_aux_render_audio_stop {
    vx_resp_base_t base;
} vx_resp_aux_render_audio_stop_t;

/**
 * The response for vx_req_aux_capture_audio_start_t
 * 
 * The XML for this response can be found here: \ref Aux_CaptureAudioStart_1
 *
 * \see vx_req_aux_capture_audio_start
 * \ingroup devices
 */
typedef struct vx_resp_aux_capture_audio_start {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_capture_audio_start_t;
/**
 * The response for vx_req_aux_capture_audio_stop_t
 * 
 * The XML for this response can be found here: \ref Aux_CaptureAudioStop_1
 *
 * \see vx_req_aux_capture_audio_stop
 * \ingroup devices
 */
typedef struct vx_resp_aux_capture_audio_stop {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * An opaque pointer to the captured audio buffer. This pointer is not marshalled 
     * when working with the VivoxVoiceService.
     */
    void *audioBufferPtr;
} vx_resp_aux_capture_audio_stop_t;

/**
 * The response for vx_req_aux_global_monitor_keyboard_mouse_t
 * 
 * The XML for this response can be found here: \ref Aux_GlobalMonitorKeyboardMouse_1
 *
 * \see vx_req_aux_global_monitor_keyboard_mouse
 * \ingroup devices
 */
typedef struct vx_resp_aux_global_monitor_keyboard_mouse {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_global_monitor_keyboard_mouse_t;

/**
 * The response for vx_req_aux_set_idle_timeout_t
 * 
 * The XML for this response can be found here: \ref Aux_SetIdleTimeout_1
 *
 * \see vx_req_aux_set_idle_timeout
 * \ingroup devices
 */
typedef struct vx_resp_aux_set_idle_timeout {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_set_idle_timeout_t;

/**
 * The response for vx_req_aux_create_account_t
 * 
 * The XML for this response can be found here: \ref Aux_CreateAccount_1
 *
 * \see vx_req_aux_create_account
 */
typedef struct vx_resp_aux_create_account {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_create_account_t;

/**
 * The response for vx_req_aux_reactivate_account_t
 * 
 * The XML for this response can be found here: \ref Aux_ReactivateAccount_1
 *
 * \see vx_req_aux_reactivate_account
 */
typedef struct vx_resp_aux_reactivate_account {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_reactivate_account_t;

/**
 * The response for vx_req_aux_deactivate_account_t
 * 
 * The XML for this response can be found here: \ref Aux_DeactivateAccount_1
 *
 * \see vx_req_aux_deactivate_account
 */
typedef struct vx_resp_aux_deactivate_account {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_deactivate_account_t;

/**
 * The response for vx_req_account_post_crash_dump
 * 
 * This response is not applicable to the XML interface.
 *
 * \see vx_req_account_post_crash_dump
 * \ingroup diagnostics
 */
typedef struct vx_resp_account_post_crash_dump {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_post_crash_dump_t;

/**
 * The response for vx_req_aux_reset_password
 * 
 * The XML for this response can be found here: \ref Aux_ResetPassword_1
 *
 * \see vx_req_aux_reset_password
 */
typedef struct vx_resp_aux_reset_password {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_reset_password_t;


/**
 * The response for vx_req_account_get_session_fonts_t
 * 
 * The XML for this response can be found here: \ref Account_GetSessionFonts_1
 *
 * \see vx_req_account_get_session_fonts
 * \ingroup voicefonts
 */
typedef struct vx_resp_account_get_session_fonts {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * List of session fonts
    */
    vx_voice_font_t** session_fonts;
    /**
    * Number of session fonts
    */
    int session_font_count;
} vx_resp_account_get_session_fonts_t;

/**
 * The response for vx_req_account_get_template_fonts_t
 * 
 * The XML for this response can be found here: \ref Account_GetTemplateFonts_1
 *
 * \see vx_req_account_get_template_fonts
 * \ingroup voicefonts
 */
typedef struct vx_resp_account_get_template_fonts {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
    * List of session fonts
    */
    vx_voice_font_t** template_fonts;
    /*
    * Number of session fonts
    */
    int template_font_count;
} vx_resp_account_get_template_fonts_t;

/**
 * The response for vx_req_aux_start_buffer_capture_t
 * 
 * The XML for this response can be found here: \ref Aux_StartBufferCapture_1
 *
 * \see vx_req_aux_start_buffer_capture
 * \ingroup adi
 */
typedef struct vx_resp_aux_start_buffer_capture {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_start_buffer_capture_t;

/**
 * The response for vx_req_aux_play_audio_buffer_t
 * 
 * The XML for this response can be found here: \ref Aux_PlayAudioBuffer_1
 *
 * \see vx_req_aux_play_audio_buffer
 * \ingroup adi
 */
typedef struct vx_resp_aux_play_audio_buffer {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_play_audio_buffer_t;

/**
 * The response for vx_req_aux_diagnostic_state_dump_t
 *
 * \see vx_req_aux_diagnostic_state_dump
 */
typedef struct vx_resp_aux_diagnostic_state_dump {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * The number of connector objects in the state dump.
     */
    int state_connector_count;
    /**
     * Collection of connector objects in the state dump.
     */
    vx_state_connector_t** state_connectors;
    /**
    * The render device currently in use by the Vivox SDK.
    */
    vx_device_t* current_render_device;
    /**
    * The effective render device
    */
    vx_device_t* effective_render_device;
    /**
    * The current capture device
    */
    vx_device_t* current_capture_device;
    /**
    * The effective capture device
    */
    vx_device_t* effective_capture_device;

} vx_resp_aux_diagnostic_state_dump_t;

/**
 * The response for vx_req_account_web_call_t
 *
 * \see vx_req_account_web_call
 */
typedef struct vx_resp_account_web_call {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * The HTTP content type
     */
    char *content_type;
    /**
     * The content length
     */
    int content_length;
    /**
     * The content
     */
    char *content;
} vx_resp_account_web_call_t;

/**
 * Gets the network statistics associated with a particular sessiongroup
 *
 * \see vx_req_sessiongroup_get_stats
 */
typedef struct vx_resp_sessiongroup_get_stats {
    /**
     * The common properties for all responsess.
     */
    vx_resp_base_t base;
    /**
     * RESERVED FOR FUTURE USE
     */
    int insufficient_bandwidth;
    /**
     * RESERVED FOR FUTURE USE
     */
    int min_bars;
    /**
     * RESERVED FOR FUTURE USE
     */
    int max_bars;
    /**
     * @deprecated
     * An indication of the network quality in the range of 1-5 with 5 indicating the best quality.
     */
    int current_bars;
    /**
     * RESERVED FOR FUTURE USE
     */
    int pk_loss;
    /**
     * The number of packets received
     */
    int incoming_received;
    /**
     * RESERVED FOR FUTURE USE
     */
    int incoming_expected;
    /**
     * The number of packets lost in the network
     */
    int incoming_packetloss;
    /**
     * The number of packets received too late to be useful and discarded
     */
    int incoming_out_of_time;
    /**
     * The number of packets received but discarded because the local queue overflowed
     */
    int incoming_discarded;
    /**
     * The number of packets sent
     */
    int outgoing_sent;
    /** 
     * The number of render device underruns - mobile platforms only
     */
    int render_device_underruns;
    /**
     * The number of render device overruns - mobile platforms only
     */
    int render_device_overruns;
    /**
     * The number of render device errors - mobile platforms only
     */
    int render_device_errors;
    /**
     * The SIP call ID
     */
    char *call_id;
    /**
     * Flag indicating whether Packet Loss Concealment (error correction) has happened
     */
    int plc_on;
    /**
     * The number of 10ms synthetic frames generated by Packet Loss Concealment
     */
    int plc_synthetic_frames;
    /**
     * Codec negotiated in the current call
     */
    char *codec_name;
    /**
     * @deprecated
     */
    int codec_mode;
    /**
     * Minimum Network Latency Detected (seconds) - zero if no latency measurements made
     */
    double min_latency;
    /**
     * Maximum Network Latency Detected (seconds) - zero if no latency measurements made
     */
    double max_latency;
    /**
     * Latency Measurement Count - the number of times latency was measured
     */
    int latency_measurement_count;
    /**
     * Latency Sum - total number of seconds of measured network latency
     */
    double latency_sum;
    /**
     * Last Latency Measured
     */
    double last_latency_measured;
    /**
     * Latency Packets Lost - the number of times we received latency packet where we did not receive the prior expected response.
     */
    int latency_packets_lost;
    /**
     * RFactor - computation of quality
     */
    double r_factor;
    /**
     * Number of latency measurement request packets sent
     */
    int latency_packets_sent;
    /**
     * Number of latency measurement response packets lost
     */
    int latency_packets_dropped;
    /**
     * Number of latency measurement packets that were too short or otherwise malformed.
     */
    int latency_packets_malformed;
    /**
     * Number of latency measurement packets that arrived before they were sent.
     * This can occur if there are clock adjustments
     */
    int latency_packets_negative_latency;
    /**
     * The beginning of the sample period (in fractional seconds since Midnight Jan 1st 1970 GMT)
     */
    double sample_interval_begin;
    /**
     * The end of the sample period (in fractional seconds since Midnight Jan 1st 1970 GMT)
     */
    double sample_interval_end;
    /**
     * The number of intervals where 0, 1, 2, 3, or 4 or greater audio frames were read from the capture device
     */
    int capture_device_consecutively_read_count[5];
    /**
     * OPUS bit rate, that was used for encoding the last transmitted OPUS packet,
     * -1 if no OPUS packets were transmitted
     */
    int current_opus_bit_rate;
    /**
     * OPUS complexity, that was used for encoding the last transmitted OPUS packet,
     * -1 if no OPUS packets were transmitted
     */
    int current_opus_complexity;
    /**
     * OPUS VBR mode (vx_opus_vbr_mode), that was used for encoding the last transmitted OPUS packet,
     * -1 if no OPUS packets were transmitted
     */
    int current_opus_vbr_mode;
    /**
     * OPUS bandwith (vx_opus_bandwidth), that was used for encoding the last transmitted OPUS packet,
     * -1 if no OPUS packets were transmitted
     */
    int current_opus_bandwidth;
    /**
     * OPUS max ppcket size limit, that was used for encoding the last transmitted OPUS packet,
     * -1 if no OPUS packets were transmitted
     */
    int current_opus_max_packet_size;
} vx_resp_sessiongroup_get_stats_t;

/**
 * The response for vx_req_account_send_message_t
 * 
 * The XML for this response can be found here: \ref Account_SendMessage_1
 *
 * \see vx_req_account_send_message
 * \ingroup buddy
 */
typedef struct vx_resp_account_send_message {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_account_send_message_t;

/**
 * The response for vx_req_aux_notify_application_state_change_t
 * 
 * The XML for this response can be found here: \ref Aux_NotifyApplicationStateChange_1
 *
 * \see vx_req_aux_notify_application_state_change
 */
typedef struct vx_resp_aux_notify_application_state_change {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
} vx_resp_aux_notify_application_state_change_t;


/**
 * The response for vx_req_account_control_communications_t
 * 
 * The XML for this response can be found here: \ref Account_ControlCommunications_1
 *
 * \see vx_req_account_control_communications
 */
typedef struct vx_resp_account_control_communications {
    /**
     * The common properties for all responses.
     */
    vx_resp_base_t base;
    /**
     * Line feed separated list of blocked URIs (only set for vx_control_communications_operation_list, otherwise null)
     */
     char *blocked_uris;
} vx_resp_account_control_communications_t;

#ifndef VIVOX_TYPES_ONLY
/**
 * Used to deallocate a response object of any type.
 * \ingroup memorymanagement
 */
VIVOXSDK_DLLEXPORT int destroy_resp(vx_resp_base_t *pCmd);
#endif

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
