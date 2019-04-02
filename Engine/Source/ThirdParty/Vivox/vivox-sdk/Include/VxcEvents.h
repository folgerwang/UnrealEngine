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

#pragma pack(push)
#pragma pack(8)

#ifdef __cplusplus
extern "C" {
#endif


/**
 * This event message is sent whenever the login state of the particular Account has transitioned from one value to another.
 * 
 * The XML for this event can be found here: \ref AccountLoginStateChangeEvent
 *
 * \ingroup login
 */
typedef struct vx_evt_account_login_state_change {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * New state of the entity, please refer to the codes at the end of this doc for specific state codes
    */
    vx_login_state_change_state state;
    /**
    * Handle returned from successful Account request
    */
    VX_HANDLE account_handle;
    /**
    * Code used to identify why a state change has been made
    */
    int status_code;
    /**
    * Text (in English) to describe the Status Code
    */
    char* status_string;
    /**
     * originating login request cookie
     *
     * This is here to because the login_state_logging_in state change event comes before the response with the handle
     */
    VX_COOKIE cookie;
    /**
     * originating login request cookie (non-marshallable)
     *
     * This is here to because the login_state_logging_in state change event comes before the response with the handle
     */
    void *vcookie;
} vx_evt_account_login_state_change_t;

#ifndef VX_DISABLE_PRESENCE
/**
 * Presented when a buddy has issued presence information.
 *
 * The XML for this event can be found here: \ref BuddyPresenceEvent
 *
 * \ingroup buddy
 */
typedef struct vx_evt_buddy_presence {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * DEPRECATED
     * @deprecated
     */
    vx_buddy_presence_state state;
    /**
    * Handle returned from successful 'login' request 
    */
    VX_HANDLE account_handle;
    /**
    * The URI of the sender of the message
    */
    char* buddy_uri;
    /**
    * New presense of the buddy, please refer to the codes in table \ref vx_buddy_presence_state
    */
    vx_buddy_presence_state presence;
    /**
    * Custom message string when presence is set to custom.
    */
    char* custom_message;
    /**
     * The displayname if the buddy_uri had a displayname
     */
    char* displayname;
    /**
     * The application of the buddy who's presence is being reported.  
     * May be NULL or empty.
     */
    char* application;
    /**
     * The contact address (Uri) of the buddy who's presence is being reported.  
     * May be NULL or empty.
     */
    char* contact;
    /**
     * RESERVED FOR FUTURE USE: The priority of the buddy who's presence is being reported.  
     * May be NULL or empty.
     */
    char* priority;
    /**
     * The unique id of the instance of the buddy who's presence is being reported.  
     * May be NULL or empty.
     */
    char* id;
} vx_evt_buddy_presence_t;

/**
 * Generated when a buddy wants to request presence visibility.  
 * This event will not be presented if an auto-accept or auto-block rule matches the requesting 
 * buddy_uri. Typically the application would use this event to prompt a user to explicitly 
 * accept or deny the request for presence.  Optionally the application might create and store 
 * an auto-accept or auto-block rule based upon the users' choice. The application should generate a 
 * vx_req_account_send_subscription_reply_t resquest based upon application logic and/or end-user response. 
 * The subscription_handle value must be extracted and returned as a parameter to  
 * vx_req_account_send_subscription_reply_t
 *
 * The XML for this event can be found here: \ref SubscriptionEvent
 *
 * \see vx_req_account_buddy_set
 * \ingroup buddy
 */
typedef struct vx_evt_subscription {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle returned from successful 'login' request 
     */
    VX_HANDLE account_handle;
    /**
     * The URI of the buddy who's presence is being sent
     */
    char* buddy_uri;
    /**
     * The identifier of the subscription event. 
     * Used when forming a reply with vx_req_account_send_subscription_reply_t
     */
    char* subscription_handle;
    /**
     * "subscription_presence" is currently the only supported value
     */
    vx_subscription_type subscription_type;
    /**
     * The displayname if the buddy_uri had a displayname
     */
    char* displayname;
    /**
     * The application of the buddy who's subscription is being reported.  
     * May be NULL or empty.
     */
    char* application;
    /**
    * NOT CURRENTLY IMPLEMENTED
    * Optional message supplied by the initiating user on vx_req_account_buddy_set_t.
    */
    char* message;
} vx_evt_subscription_t;
#endif

/**
 * Received when another user has started or stopped typing, or raised or lowered their hand, 
 * within the context of a session.
 *
 * The XML for this event can be found here: \ref SessionNotificationEvent
 *
 * \ingroup session
 */
typedef struct vx_evt_session_notification {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * DEPRECATED
     * @deprecated
     */
    vx_session_notification_state state;
    /**
     * The handle of the session to which this event applies.
     */
    VX_HANDLE session_handle;
    /**
    * The URI of the buddy who's presence is being sent
    */
    char* participant_uri;
    /**
    * New notification type from the buddy, please refer to the codes in table \ref vx_notification_type
    */
    vx_notification_type notification_type;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
    /**
     * The message is from the current logged in user
     */
    int is_current_user;
} vx_evt_session_notification_t;


/**
* Presented when an incoming message has arrived from a participant in an open session with text enabled.
*
* The XML for this event can be found here: \ref MessageEvent
*
* \ingroup session
*/
typedef struct vx_evt_message {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * DEPRECATED
     * @deprecated
     */
    vx_message_state state;
    /**
     * Handle returned from successful SessionGroup 'create' request 
     * \see vx_req_sessiongroup_create
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Handle returned from successful Session 'add' request 
     * \see vx_req_sessiongroup_add_session
     */
    VX_HANDLE session_handle;
    /**
     * The URI of the sender of the message
     */
    char* participant_uri;
    /**
     * Content type of the message
     */
    char* message_header;
    /**
     * The contents of the message
     */
    char* message_body;
    /**
     * The displayname if the participant_uri had a displayname
     */
    char* participant_displayname;
    /**
     * The application of the entity who is sending the message.  
     * May be NULL or empty.
     */
    char* application;
    /**
     * the identity that the original sender wished to present.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the original sender.
     */
    char *alias_username;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
    /**
     * The message is from the current logged in user
     */
    int is_current_user;
} vx_evt_message_t;


/**
 * The auxiliary 'audio properties' events are used by the SDK sound system to present 
 * audio information to the application, which may be used to create a visual representation of the speaker 
 * (for example, a so called 'VU' meter). These events are presented at one half the rate of the audio capture rate.
 *
 * The XML for this event can be found here: \ref AuxAudioPropertiesEvent
 *
 * \ingroup devices
 */
typedef struct vx_evt_aux_audio_properties {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * DEPRECATED
     * @deprecated
     */
    vx_aux_audio_properties_state state;
    /**
    * Flag used to determine if the mic is active.
    *   - 1 - Indicates that the capture device is detecting 'speech activity' (as determined by the built in 
    *         Vivox Voice Activity Detector). 
    *   - 0 - Indicates no speech energy has been detected.
    */
    int mic_is_active;
    /**
    * Indicates the current value of the master 'microphone' volume, as set using the 'set mic volume' method above. 
    * Non negative value between 0 and 100 (inclusive)
    */
    int mic_volume;
    /**
    * The instantaneous (fast) energy at the capture device. 
    * A value from 0.0 to 1.0, when graphed will show behavior like an analog VU Meter. (NOTE: For an unsmoothed dBFS value of fast energy, see fast_energy_meter and its companion *_meter values below)
    */
    double mic_energy;
    /**
    * Indicates the current value of the master 'speaker' volume, as set using the 'set speaker volume' method above. 
    * Non negative value between 0 and 100 (inclusive)
    */
    int speaker_volume;
    /**
    * The energy associated with any rendered audio 
    */
    double speaker_energy;
    /**
    * whether or not voice is detected in the rendered audio stream at this moment
    */
    int speaker_is_active;
    /**
    * The instantaneous (fast) energy at the capture device.
    * This is a floating point number between 0 and 1. Logarithmically spaced representing -Inf dBFS to +0dBFS
    */
    double fast_energy_meter;
    /**
    * The current noise floor estimate
    * This is a floating point number between 0 and 1. Logarithmically spaced representing -Inf dBFS to +0dBFS
    */
    double noise_floor_meter;
    /**
    * The current magnitude that 'fast energy' must surpass to activate speech.  This ranges between noise_floor and -9dBFS.
    * This is a floating point number between 0 and 1. Logarithmically spaced representing -Inf dBFS to +0dBFS
    */
    double speech_threshold_meter;
} vx_evt_aux_audio_properties_t;


/**
 * For vx_evt_buddy_changed_t and vx_evt_buddy_group_changed_t objects, indicates whether the object is deleted 
 * or "set", which means either added or updated.
 * \ingroup buddy
 */
typedef enum {
    /**
     * Buddy or group was added or updated
     */
    change_type_set = 1,
    /**
     * Buddy or group was deleted
     */
    change_type_delete = 2,
} vx_change_type_t;

#ifndef VX_DISABLE_PRESENCE
/**
* Presented when a buddy is either set (added or update) or removed.
*
* The XML for this event can be found here: \ref BuddyChangedEvent
*
* \ingroup buddy
*/
typedef struct vx_evt_buddy_changed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful 'login' request 
    */
    VX_HANDLE account_handle;
    /**
    * Indicates the change type.
    * Set or delete.
    */
    vx_change_type_t change_type;
    /**
    * The uri of the buddy
    */
    char *buddy_uri;
    /**
    * The display name of the buddy
    */
    char *display_name;
    /**
    * Application specific buddy data
    */
    char *buddy_data;
    /**
    * The group the buddy belongs to
    */
    int group_id;
    /**
    * The account id of the buddy
    * @deprecated
    */
    int account_id;
} vx_evt_buddy_changed_t;

/**
* Presented when a buddy group is set (added or updated) or removed
*
* The XML for this event can be found here: \ref BuddyGroupChangedEvent
*
* \ingroup buddy
*/
typedef struct vx_evt_buddy_group_changed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful 'login' request 
    */
    VX_HANDLE account_handle;
    /**
    * Indicates the change type.
    * Set or delete.
    */
    vx_change_type_t change_type;
    /**
    * The id for the group
    */
    int group_id;
    /**
    * The display name for the group
    */
    char *group_name;
    /**
    * Application specific group data
    */
    char *group_data;
} vx_evt_buddy_group_changed_t;

/**
 * Presented when the buddy or group list undergoes a significant change. 
 * This event is always received after login, and can be used to build the initial buddy and group UI.
 *
 * The XML for this event can be found here: \ref BuddyAndGroupListChangedEvent
 *
 * \ingroup buddy
 */
typedef struct vx_evt_buddy_and_group_list_changed {
    /**
     * The common properties for all events.
     */
    vx_evt_base_t base;
    /**
     * Handle returned from successful 'login' request 
     */
    VX_HANDLE account_handle;
    /**
     * Count of buddies
     */
    int buddy_count;
    /**
     * An array of pointers to buddies
     */
    vx_buddy_t **buddies;
    /**
     * Count of groups
     */
    int group_count;
    /**
     * An array of buddy group pointers
     */
    vx_group_t **groups;
} vx_evt_buddy_and_group_list_changed_t;
#endif

/**
* The vx_evt_keyboard_mouse_t event is raised to indicate to the application that a particular 
* keyboard/mouse button combination has been pressed or cleared.
*
* The XML for this event can be found here: \ref KeyboardMouseEvent
*
* \ingroup devices
*
* \attention \attention Not supported on the PLAYSTATION(R)3 platform
* \attention Not supported on the iPhone mobile digital device platform
*/
typedef struct vx_evt_keyboard_mouse {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * The name of the binding as set in vx_req_aux_global_monitor_keyboard_mouse_t
    */
    char *name;
    /**
    * 1 if the key/mouse button combination corresponding to this name is down, and 0 when it has been cleared.
    */
    int is_down;
} vx_evt_keyboard_mouse_t;

/**
* The vx_evt_idle_state_changed_t event is raised to indicate to the application that the user 
* has transitioned from the between idle and non-idle states (and vice-versa).
*
* The XML for this event can be found here: \ref IdleStateChangedEvent
*
* \ingroup devices
*
* \attention \attention Not supported on the PLAYSTATION(R)3 platform
* \attention Not supported on the iPhone mobile digital device platform
*/
typedef struct vx_evt_idle_state_changed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * 1 if the user is idle, 0 if the user is not idle.
    */
    int is_idle;
} vx_evt_idle_state_changed_t;

/**
 * The common structure for holding call related statistics
 */
typedef struct vx_call_stats {
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
} vx_call_stats_t;

/**
* Sent when Session Media has been altered. 
*
* The XML for this event can be found here: \ref MediaStreamUpdatedEvent
*
* \ingroup session
*/
typedef struct vx_evt_media_stream_updated {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle created for session group
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Handle created for session
     */
    VX_HANDLE session_handle;
    /**
     * Code used to identify why a state change has been made.
     * This code is only useful if state is equal to 'session_media_disconnected'.
     * In this case the following rules apply:
     * 
     *   1. Any code < 400 can be ignored.
     *   2. 401 - A password is needed to join this channel. Typically an application will present a password dialog at this point. You may retry the request if you obtain a password from the user.
     *   3. 403 - If the call is to a channel, the user does not have sufficient privilege to join the channel, otherwise, the call has been declined.
     *   4. 404 - Destination (either a channel or other user) does not exist.
     *   5. 408 - The remote user did not answer the call. You may retry the request after 10s delay.
     *   6. 480 - The remote user is temporarily offline. You may retry the request after 10s delay.
     *   7. 486 - The remote user is busy (on another call). You may retry the request after 10s delay.
     *   8. 503 - The server is busy (overloaded). You may retry the request after 10s delay.
     *   9. 603 - The remote user has declined the call.
     *
     * The application should only retry a failed request if there is a chance the retry will succeed.  Those cases are marked above with "you may retry".
     *
     * It is recommended that the status_string field only be display as diagnostic information for status codes > 400, and not in the list above. 
     * This status_string is often generated by the network, which can include public PSTN networks as well. This can result in status_string values that 
     * are informative to a technician, but not to an end user, and may be subject to change. Applications should not depend on the value of this field.
     *
     * Applications should present an application specific message for each of the status codes outlined above.
     */
    int status_code;
    /**
     * Text (in English) to describe the Status Code.
     *
     * See the status_code field above.
     */
    char* status_string;
    /**
     * New state of the entity, please refer to the codes in table \ref vx_session_media_state
     */
    vx_session_media_state state;
    /**
     * This indicates if this is an incoming call or not.
     */
    int incoming;
    /**
     * The durable media identifier use to access value add services.
     */
    char *durable_media_id;
    /**
     * The current media probe server
     */
    char *media_probe_server;
    /**
     * call stats - NULL except for state session_media_disconnecting
     */
    vx_call_stats_t *call_stats;
} vx_evt_media_stream_updated_t;

/**
* Sent when Session Text has been altered.
*
* The XML for this event can be found here: \ref TextStreamUpdatedEvent
*
* \ingroup session
*/
typedef struct vx_evt_text_stream_updated {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle created for session group
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Handle created for session
     */
    VX_HANDLE session_handle;
    /**
     * Indicates if text is enabled on the session
     */
    int enabled;
    /**
     * Indicates the state of text, connected or disconnected
     */
    vx_session_text_state state;
    /**
     * Indicates if this is incoming or not
     */
    int incoming;
    /**
     * Code used to identify why a state change has been made.
     *
     * These codes are only useful for when state is equal to 'session_text_disconnected'.
     * See \ref vx_evt_media_stream_updated for a description of these status codes.
     */
    int status_code;
    /**
     * Text (in English) to describe the Status Code
     *
     * See \ref vx_evt_media_stream_updated for guidelines for using this field.
     */
    char* status_string;
} vx_evt_text_stream_updated_t;

/**
* Sent when a session group is added.
*
* The XML for this event can be found here: \ref SessionGroupAddedEvent
*
* \ingroup sessiongroup
*/
typedef struct vx_evt_sessiongroup_added {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Returned from successful Session Group 'create' request 
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Returned from successful Session Group 'login' request 
     */
    VX_HANDLE account_handle;
    /**
    * Session group type
    */
    vx_sessiongroup_type type;
    /**
     * the identity that will be presented on all subsequent communication from this session group to a remote user.
     */
    char *alias_username;
} vx_evt_sessiongroup_added_t;

/**
* Sent when a session group is removed.
*
* The XML for this event can be found here: \ref SessionGroupRemovedEvent
*
* \ingroup sessiongroup
*/
typedef struct vx_evt_sessiongroup_removed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Returned from successful Session Group 'create' request 
     */
    VX_HANDLE sessiongroup_handle;
} vx_evt_sessiongroup_removed_t;

/**
* Sent when a session is added.
*
* The XML for this event can be found here: \ref SessionAddedEvent
*
* \ingroup session
*/
typedef struct vx_evt_session_added {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful Session Group 'create' request 
    */
    VX_HANDLE sessiongroup_handle;
    /**
    * Handle returned from successful Session 'add' request
    */
    VX_HANDLE session_handle;
    /**
    * Full URI of the session (user/channel?)
    */
    char* uri;
    /**
    * Set to 1 if this session relates to a Channel,  0 if  not
    */
    int is_channel;
    /**
     * Set to 1 if this is a session that was added because it was an incoming call,
     * set to 0 for all other cases.
     */
    int incoming;
    /**
    * The name of the channel, if passed in when the channel is created.  
    * Always empty for incoming sessions.
    */
    char* channel_name;
    /**
     * DEPRECATED
     * @deprecated
     */
    char* displayname;
    /**
     * DEPRECATED
     * @deprecated
     */
    char* application;
    /**
     * the identity of the remote user, if p2p, or null if a channel call.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the remote user.
     */
    char *alias_username;
} vx_evt_session_added_t;

/**
* Sent when a session is removed.
*
* The XML for this event can be found here: \ref SessionGroupRemovedEvent
*
* \ingroup session
*/
typedef struct vx_evt_session_removed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful Session Group 'create' request 
    */
    VX_HANDLE sessiongroup_handle;
    /**
    * Handle returned from successful Session 'add' request
    */
    VX_HANDLE session_handle;
    /**
    * Full URI of the session (user/channel?)
    */
    char* uri;
} vx_evt_session_removed_t;

/**
* Presented when a Participant is added to a session.  
* When joining a channel, a Participant Added Event will be raised for all active participants in the channel.
*
* The XML for this event can be found here: \ref ParticipantAddedEvent
*
* \ingroup session
*/
typedef struct vx_evt_participant_added {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful Session Group 'create' request 
    */
    VX_HANDLE sessiongroup_handle;
    /**
    * Handle returned from successful Session 'create' request
    */
    VX_HANDLE session_handle;
    /**
    * The URI of the participant whose state has changed
    */
    char* participant_uri;
    /**
    * The Account name of the participant 
    */
    char* account_name;
    /**
     * DEPRECATED - Please use displayname instead
     * @deprecated
    */
    char* display_name;
    /**
    * DEPRECATED, WILL NOT BE IMPLEMENTED
    * @deprecated
    */
    int participant_type;
    /**
     * DEPRECATED
     * @deprecated
    */
    char* application;
    /**
    * This field will indicate if the user is loggen in anonymously (as a guest)
    * 0 = anthenticated user, 1 = Anonymous (Guest) user
    * This is only supported on channel calls.
    */
    int is_anonymous_login;
    /**
    * The Display Name of the participant if in a channel or in a P2P initiated session.  
    * This field will not be populated for callee in a text initiated P2P session.
    *
    * The displayname field will contain the following information if available: 
    *   - 1) buddy display name (if not available then)
    *   - 2) sip display name (only available for the callee, not available for caller.  If this is not available then)
    *   - 3) account name (unless the account is out of domain then)
    *   - 4) uri without the sip: (ex: username@foo.vivox.com)
    */
    char* displayname;
    /**
     * the identity of the user,if p2p, or null if a channel call.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the remote user.
     */
    char *alias_username;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
    /**
     * The message is from the current logged in user
     */
    int is_current_user;
} vx_evt_participant_added_t;

/**
* Presented when a participant is removed from a session.
*
* The XML for this event can be found here: \ref ParticipantRemovedEvent
*
* \ingroup session
*/
typedef struct vx_evt_participant_removed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful Session Group 'create' request 
    */
    VX_HANDLE sessiongroup_handle;
    /**
    * Handle returned from successful Session 'create' request
    */
    VX_HANDLE session_handle;
    /**
    * The URI of the participant whose state has changed
    */
    char* participant_uri;
    /**
    * The Account name of the Participant 
    */
    char* account_name;
    /**
    * The reason the participant was removed from the session.  
    * Default is "left".  \ref vx_participant_removed_reason
    */
    vx_participant_removed_reason reason;
    /**
     * the identity of the user,if p2p, or null if a channel call.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the original sender.
     */
    char *alias_username;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel 
     */
    char *encoded_uri_with_tag;
    /**
     * The message is from the current logged in user
     */
    int is_current_user;
} vx_evt_participant_removed_t;

/**
* Indicates special state of the local voice participant that is used to indicate
* that the participant is attemping to speak while the system is in a state
* that won't transmit the participant's audio.
*/
typedef enum {
    participant_diagnostic_state_speaking_while_mic_muted = 1,
    participant_diagnostic_state_speaking_while_mic_volume_zero = 2
} vx_participant_diagnostic_state_t;
/**
* Received when the properties of the participant change (mod muted, speaking, volume, energy, typing notifications)
*
* The XML for this event can be found here: \ref ParticipantUpdatedEvent
*
* \ingroup session
*/
typedef struct vx_evt_participant_updated {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
     /**
     * Returned from successful  Session Group 'create' request 
     */
    VX_HANDLE sessiongroup_handle;
     /**
     * Handle returned from successful  Session 'create' request
     */
    VX_HANDLE session_handle;
     /**
     * The URI of the participant whose properties are being updated
     */
    char* participant_uri;
     /**
     * Used to determine if the user has been muted by the moderator (0 - not muted, 1 - muted)
     */
    int is_moderator_muted;
     /**
     * Indicates if the participant is speaking
     */
    int is_speaking;
     /**
     * This is the volume level that has been set by the user, this should not change often and is a value between 
     * 0 and 100 where 50 represents 'normal' speaking volume.
     */
    int volume;
     /**
     * This is the energy, or intensity of the participants audio.  
     * This is used to determine how loud the user is speaking.  This is a value between 0 and 1.
     */
    double energy;
    /**
     * This indicates which media the user is participating in. 
     * See #VX_MEDIA_FLAGS_AUDIO and #VX_MEDIA_FLAGS_TEXT
     */ 
    int active_media;
    /**
    * Indicates whether or not this participant's audio is locally muted for the user
    */
    int is_muted_for_me;
    /**
    * NOT CURRENTLY IMPLEMENTED
    * Indicates whether or not this participant's text is locally muted for the user
    */
    int is_text_muted_for_me;
    /**
     * Used to determine if the user's text has been muted by the moderator (0 - not muted, 1 - muted)
     */
    int is_moderator_text_muted;
    /**
    * The type of the participant \see vx_participant_type
    */
    vx_participant_type type;
    /**
    * A list of diagnostic states to help tell the application that the participant is attempting to speak
    * but the system is not in a state to propogate that speech (mic muted etc).
    */
    vx_participant_diagnostic_state_t *diagnostic_states;
    /**
    * The total number of diagnostic states
    */
    int diagnostic_state_count;
    /**
     * the identity of the user,if p2p, or null if a channel call.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the original sender.
     */
    char *alias_username;
    /**
     * The encoded URI for the user with the tag. This uniquely identifies users that might appear multiple times in a channel
     */
    char *encoded_uri_with_tag;
    /**
     * The message is from the current logged in user
     */
    int is_current_user;
} vx_evt_participant_updated_t;

/**
 * This event is posted after a frame has been played.
 *
 * When playback has been stopped by the application, the first, current, and total frames will be equal to zero.
 *
 * The XML for this event can be found here: \ref SessionGroupPlaybackFramePlayedEvent
 *
 * \ingroup csr
 */
typedef struct vx_evt_sessiongroup_playback_frame_played {
    /*
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Returned from successful  Session Group 'create' request
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * The sequence number of the first frame
     */
    int first_frame;
    /**
     * The current frame seqno
     */
    int current_frame;
    /**
     * The total number of frames available
     */
    int total_frames;
} vx_evt_sessiongroup_playback_frame_played_t;

/**
* Sent when a session is updated.
*
* The XML for this event can be found here: \ref SessionUpdatedEvent
*
* \ingroup session
*/
typedef struct vx_evt_session_updated {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful Session Group 'create' request 
    */
    VX_HANDLE sessiongroup_handle;
    /**
    * Handle returned from successful Session 'add' request
    */
    VX_HANDLE session_handle;
    /**
    * Full URI of the session
    */
    char* uri;
    /**
    * Whether or not the session's audio is muted
    */
    int is_muted;
    /**
    * The volume of this session
    */
    int volume;
    /**
    * Whether or not the session is transmitting
    */
    int transmit_enabled;
    /**
    * Whether or not the session has focus
    */
    int is_focused;
    /**
     * The position of the virtual 'mouth'. 
     * This 3 vector is a right handed Cartesian coordinate, with the positive axis pointing
     * towards the speaker's right, the positive Y axis pointing up, and the positive Z axis 
     * pointing towards the speaker.
     */
    double speaker_position[3]; // {x, y, z}
    /**
    * The ID of the session font applied to this session.  
    * 0 = none.
    */
    int session_font_id;
    /**
    * Whether or not the session's text is muted
    */
    int is_text_muted;
    /**
    * Whether or not there is an audio ad playing in this session
    **/
    int is_ad_playing;
} vx_evt_session_updated_t;

/**
* Sent when a session group is updated.
*
* The XML for this event can be found here: \ref SessionGroupUpdatedEvent
*
* \ingroup sessiongroup
*/
typedef struct vx_evt_sessiongroup_updated {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Returned from successful Session Group 'create' request 
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * OBSOLETE
     * Whether or not in delayed playback. 
     * When in delayed playback, the mic is not active
     */
    int in_delayed_playback;
    /**
    * OBSOLETE
    * Playback speed
     */
    double current_playback_speed;
    /*
    * OBSOLETE
    * Playback mode
     */
    vx_sessiongroup_playback_mode current_playback_mode;
    /*
    * OBSOLETE
    * Whether or not playback is paused
     */
    int playback_paused;
    /**
    * OBSOLETE
    * Total capacity of the loop buffer
     */
    int loop_buffer_capacity;
    /**
     * OBSOLETE
     * Seqno of first frame in loop buffer.
     * This starts increasing when the loop buffer fills.
     */
    int first_loop_frame;
    /**
     * OBSOLETE
     * Total number of frames captured to loop buffer since recording started.
     * This peaks when the loop buffer fills.
     */
    int total_loop_frames_captured;
    /**
     * OBSOLETE
     * Sequence number of the last frame played
     */
    int last_loop_frame_played;
    /**
     * OBSOLETE
     * The filename currently being recorded (empty if no file being recorded)
     */
    char *current_recording_filename;
    /**
     * OBSOLETE
     * Total frames recorded to file
     */
    int total_recorded_frames;
    /**
     * OBSOLETE
     * the timestamp associated with the first frame in microseconds.
     * On non-Windows platforms, this is computed from gettimeofday().
     * On Windows platforms, this is computed from GetSystemTimeAsFileTime()
     *
     * This is for file based recording only.
     */
    long long first_frame_timestamp_us;
} vx_evt_sessiongroup_updated_t;

/**
* Received when certain media requests have completed
*
* The XML for this event can be found here: \ref MediaCompletionEvent
* \ingroup sessiongroup
*/
typedef struct vx_evt_media_completion {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle returned from successful SessionGroup 'create' request 
     * This field will stay empty if the completion type is 'aux_*'
     * \see vx_req_sessiongroup_create
     */
    VX_HANDLE sessiongroup_handle;
    /**
    * The type of media that has completed
    */
    vx_media_completion_type completion_type;
} vx_evt_media_completion_t;

/**
* The server may send messages to the SDK that the SDK doesn't need to consume.
* These messages will be propagated to the application via this event.  The
* application can choose to parse and consume these messages or ignore them.
*
*/
typedef struct vx_evt_server_app_data {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle returned from successful Account Login request
     * \see vx_req_account_login
     */
    VX_HANDLE account_handle;
    /**
    * The type of the incoming data
    */
    char* content_type;
    /**
    * The content of the message being received from the server
    */
    char* content;
} vx_evt_server_app_data_t;

/**
* This event is raised when a message from another user is received.  
* This is not to be confused with IMs... this is a peer-ro-peer communication mechanism for applications
* to communicate custom content.
* \see vx_req_account_send_user_app_data
*/
typedef struct vx_evt_user_app_data {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle returned from successful Account Login request
     * \see vx_req_account_login
     */
    VX_HANDLE account_handle;
    /**
    * URI of the account sending the message.
    */
    char* from_uri;
    /**
    * The type of the incoming data
    */
    char* content_type;
    /**
    * The content of the message being received from the specified account
    */
    char* content;
} vx_evt_user_app_data_t;


typedef enum {
    /**
     * message that was sent while the target user was offline.
     */
    vx_evt_network_message_type_offline_message = 1,
    vx_evt_network_message_type_admin_message = 2,
    vx_evt_network_message_type_sessionless_message = 3,
} vx_evt_network_message_type;

/**
 * This event is raised when the network sends a message to a user (as opposed to a user to user message).
 * At this time this includes messages that were stored and forwarded on behalf the user, and generic 
 * admin messages.
 */
typedef struct vx_evt_network_message {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Handle returned from successful Account Login request
     * \see vx_req_account_login
     */
    VX_HANDLE account_handle;
    /**
     * The type of the message
     */
    vx_evt_network_message_type network_message_type;
    /**
    * The type of the incoming data
    */
    char* content_type;
    /**
    * The content of the message being received from the server
    */
    char* content;
    /**
     * The sender of the message
     */
    char* sender_uri;
    /**
     * The sender display name
     */
    char *sender_display_name;
    /**
     * the identity that the original sender wished to present.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the original sender.
     */
    char *sender_alias_username;

    /**
     * the identity that the original sender wished to send to.
     * This is different than the participant_uri, which is the actual internal Vivox identity of the original sender.
     */
    char *receiver_alias_username;
} vx_evt_network_message_t;

/**
 * This event is raised when the SDK is running out of process and the connection state of the 
 * Vivox Voice Service (VVS) changes.  
 * This event will be sent when the connection is made and when the connection is lost.  This 
 * will alert the applicaiton so that the VVS may be restarted if needed.
 */
typedef struct vx_evt_voice_service_connection_state_changed {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * Whether or not the voice service connection state is connected.
     */
    int connected;
    /**
     * The platform of the machine that the voice service is running on
     */
    char *platform;
    /**
     * The version of the voice service
     */
    char *version;
    /**
     * The data directory
     */
    char *data_directory;
    /**
     * Whether or not the network test ran.  
     */
    int network_test_run;
    /**
     * Whether or not the network test completed.
     */
    int network_test_completed;
    /**
     * Whether or not the network test passed or failed.  This may 
     * change depending on network connection and power state of the machine.
     */
    int network_test_state;
    /**
     * Whether or not the network is down.
     */
    int network_is_down;
} vx_evt_voice_service_connection_state_changed_t;



typedef enum {
    /*
    ** On some platforms (WinXP) with some drivers, polling to find out which devices has changed can take 
    ** a long time. When the SDK finds that polling can take a long time, it stops looking for a device changes.
    ** When this happens, vx_evt_audio_device_hot_swap is raised with event_type set to vx_audio_device_hot_swap_event_type_disabled_due_to_platform_constraints
    */
    vx_audio_device_hot_swap_event_type_disabled_due_to_platform_constraints = 0,
    /*
    ** When the active render device changes, vx_evt_audio_device_hot_swap is raised with event_type set to vx_audio_device_hot_swap_event_type_active_render_device_changed
    */
    vx_audio_device_hot_swap_event_type_active_render_device_changed = 1,
    /*
    ** When the active capture device changes, vx_evt_audio_device_hot_swap is raised with event_type set to vx_audio_device_hot_swap_event_type_active_capture_device_changed
    */
    vx_audio_device_hot_swap_event_type_active_capture_device_changed = 2,
    /*
    ** @future
    */
    vx_audio_device_hot_swap_event_type_audio_device_added = 3,
    /*
    ** @future
    */
    vx_audio_device_hot_swap_event_type_audio_device_removed = 4
} vx_audio_device_hot_swap_event_type_t;

#ifndef VX_DISABLE_PRESENCE
/**
 * 
 *
 * The XML for this event can be found here: \ref BuddyPresenceEvent
 *
 * \ingroup buddy
 */
typedef struct vx_evt_publication_state_changed {
    /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
    * Handle returned from successful 'login' request 
    */
    VX_HANDLE account_handle;
    /**
     * the alias associated with this publication
     */
    char *alias_username;
    /**
     * The state of the publication
     */
    vx_publication_state_change_state state;
    /**
    * The presence code.
    * Possible values are:
    *    - 0 - buddy_presence_offline
    *    - 2 - buddy_presence_online
    *    - 3 - buddy_presence_busy
    *    - 4 - buddy_presence_brb
    *    - 5 - buddy_presence_away
    *    - 6 - buddy_presence_onthephone
    *    - 7 - buddy_presence_outtolunch
    */
    vx_buddy_presence_state presence;
    /**
    * Custom message string when presence is set.
    */
    char* custom_message;
    /**
    * Code used to identify why a state change has been made
    */
    int status_code;
    /**
    * Text (in English) to describe the Status Code
    */
    char* status_string;
} vx_evt_publication_state_changed_t;
#endif//VX_DISABLE_PRESENCE

/**
 * This event is raised when there are signficant state changes due to a user plugging in or unplugging in an audio device
 */
typedef struct vx_evt_audio_device_hot_swap {
     /**
     * The common properties for all events
     */
    vx_evt_base_t base;
    /**
     * The type of hot swap event
     */
    vx_audio_device_hot_swap_event_type_t event_type;
    /**
     * In the case of vx_audio_device_hot_swap_event_type_active_render_device_changed or vx_audio_device_hot_swap_event_type_active_capture_device_changed, 
     * the new active device
     */
    vx_device_t *relevant_device;
} vx_evt_audio_device_hot_swap_t;

/**
 * Used to free any event of any type
 * \ingroup memorymanagement
 */
#ifndef VIVOX_TYPES_ONLY
VIVOXSDK_DLLEXPORT int destroy_evt(vx_evt_base_t *pCmd);
#endif

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
