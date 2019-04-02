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
    /* The Vivox request structs. These structs all contain a vx_request action in the beginning so they can be identified, and
    have a cookie (VX_COOKIE). */
/**
 * Used to connect the application to the Vivox service.
 *
 * The XML format of this request can be found here: \ref Connector_Create_1
 *
 * \see vx_resp_connector_create
 * \ingroup connector
 */
typedef struct vx_req_connector_create {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * @deprecated - do not use.
     */
    char* client_name;
    /**
     * URL for the Vivox account management server; this value is provided by Vivox
     */
    char* acct_mgmt_server;
    /**
     * Optional parameter specifying the minimum port to be used by the SDK.
     * This is an optional parameter.  If a range of ports on the client needs to be specified enter the minimum port number
     * and the maximum port number to create a range of ports for the Vivox SDK to use.  If a range is not set, the Vivox SDK will use random open ports.
     * The specified range must be at least 32 ports.  Only ports above 1024 are allowed.
     */
    int minimum_port;
     /**
     * Optional parameter specifying the maximum port to be used by the SDK.
     * This is an optional parameter.  If a range of ports on the client needs to be specified enter the minimum port number
     * and the maximum port number to create a range of ports for the Vivox SDK to use.  If a range is not set, the Vivox SDK will use random open ports.
     * The specified range must be at least 32 ports.  Only ports above 1024 are allowed.
     */
    int maximum_port;
    /**
    * Control how the SDK attempts to traverse NAT devices.
    */
    vx_attempt_stun attempt_stun;
    /**
    * Specifies which mode to start the connector in.
    * Valid options are connector_mode_legacy and connector_mode_normal.
    * Legacy is the default.  New implementations should use the Normal setting.
    */
    vx_connector_mode mode;
    /**
    * The folder where any logs will be created.
    */
    char* log_folder;
    /**
    * This will be prepended to beginning of each log file.
    */
    char* log_filename_prefix;
    /**
    * The suffix or extension to be appended to each log file.
    */
    char* log_filename_suffix;
    /**
     * Specifies the log level to be used by the Vivox SDK.
     *    - 0: NONE - No logging
     *    - 1: ERROR - Log errors only
     *    - 2: WARNING - Log errors and warnings
     *    - 3: INFO - Log errors, warnings and info
     *    - 4: DEBUG - Log errors, warnings, info and debug
     */
    int log_level; // <= 0 to turn off
    /**
    * The type of session handles to be generated.
    */
    vx_session_handle_type session_handle_type;
    /**
     * A unique string value that identifies that the application that is integrated with the SDK.
     * This is an optional value provided by Vivox.
     * Whatever is passed in on this field will be present on events received
     * by others pertaining to your identity:
     * - vx_evt_message_t
     * - vx_evt_subscription_t
     * - vx_evt_buddy_presence_t
     * - vx_evt_session_added_t
     */
    char* application;
    /**
     * The maximum number of calls that can be active at any one time.
     * The minumum is 2, the default is 3, and the maximum is 1000.
     * Each call is preallocated at a cost of 56K per call.
     */
    int max_calls;
    /**
     * whether to allow cross domain logins
     */
    int allow_cross_domain_logins;
    /**
     * default codec
     * @deprecated - use the configured_codecs field instead
     */
    int default_codec;
    /**
     * A unique alpha numeric only string value that identifies the application that is integrated with the SDK.
     * This is an optional value.
     * Do not set this value, or contact your Vivox representative for more information.
     * Whatever is passed in on this field will be present on SIP messages in the user agent string
     *
     * This is restricted to alpha numeric characters only and is also restricted to 3 characters
     */
    char* user_agent_id;

    /*
     * in form ip-address:starting-port
     */
    char *media_probe_server;
     /**
      * The DNS name of an HTTP proxy server that must be used inorder to
      * successfully connect to the account management server.
     */
    char *http_proxy_server_name;
     /**
     * The port that the HTTP proxy server is listening on, defaults to 80 if not set.
     */
    int   http_proxy_server_port;
    /**
     * OBSOLETE
     * handle multiple participants with the same SIP URI more gracefully in channel
     */
    int   enable_duplicate_participant_uris;
    /**
     * The preferred connector handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *connector_handle;
    /**
     * bitmask specifying the set of codecs this application permits
     */
    unsigned int configured_codecs;
} vx_req_connector_create_t;

#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_connector_create object
 * \ingroup connector
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_create_create(vx_req_connector_create_t ** req);
#endif
/**
 * Shutdown should be called when the application is shutting down to gracefully release resources.
 *
 * The XML format of this response can be found here \ref Connector_InitiateShutdown_1
 *
 * \see vx_resp_connector_initiate_shutdown
 * \ingroup connector
 */
typedef struct vx_req_connector_initiate_shutdown {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_connector_create request.
    */
    VX_HANDLE connector_handle;
    /**
     * DEPRECATED
     * @deprecated
     */
    char* client_name;
} vx_req_connector_initiate_shutdown_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_connector_initiate_shutdown object
 * \ingroup connector
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_initiate_shutdown_create(vx_req_connector_initiate_shutdown_t ** req);
#endif
/**
* Used to login and logout of specific user account(s). It may only be called after Connector initialization has completed successfully
*
* The XML format of this request can be found here: \ref Account_Login_1
*
* \see vx_resp_account_login\n
* \ingroup login
*/
typedef struct vx_req_account_login {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_connector_create request.
    */
    VX_HANDLE connector_handle;
    /**
     * User's account name.
     */
    char* acct_name;
    /**
     * User's account password.
     */
    char* acct_password;
    /**
     * Use to control how the SDK responds to incoming voice calls.
     * mode_verify_answer is the default.
     * mode_auto_answer is deprecated.
     * mode_busy_answer is will automatically answer every call with a 486 busy.
     */
    vx_session_answer_mode answer_mode;
    /**
     * This flag is used to determine if the user is going to enable text or not for all sessions.
     * Setting this to text_mode_enabled means that text will be enabled and text_mode_disabled
     * means that text will not be enabled.  The default value is text_mode_enabled.
     */
    vx_text_mode enable_text;
    /**
    * This is an integer that specifies how often the SDK will send participant property events while in a channel.
    * If this is not set the default will be "on state change", which means that the events will be sent when
    * the participant starts talking, stops talking, is muted, is unmuted.
    *
    * The valid values are:
    * -   0 - Never
    * -   5 - 10 times per second
    * -  10 - 5 times per second
    * -  50 - 1 time per second
    * - 100 - on participant state change (this is the default)
    */
    int participant_property_frequency;
    /**
    * This determines whether or not to enable and allow buddy and presence information.
    * If this is set to 0 (default), buddy lists and presence status
    * will not be retrieved by the SDK and any command relating to buddies
    * or presence submitted to the SDK will be rejected.  Setting to 1 will enable buddy
    * and presense functionality.  0 is default.
    */
    int enable_buddies_and_presence;
    /**
     * This determines how the SDK will handle incoming buddy subscriptions.
     *
     * Valid values are:
     * - 0 - mode_auto_accept
     * - 1 - mode_auto_add
     * - 2 - mode_block
     * - 3 - mode_hide
     * - 4 - mode_application
     */
    vx_buddy_management_mode buddy_management_mode;
    /**
     * DEPRECATED.
     * This flag has been deprecated.  This is now a no-op and will not affect anything.
     * @deprecated
     */
    int enable_client_ringback;
    /**
     * If set to 1, the SDK will post crash dumps that occur on SDK threads to Vivox
     * so that they can be automatically analyzed.
     */
    int autopost_crash_dumps;
    /**
    * Used to override the account management server set in vx_req_connector_create.
    * If set to NULL or empty the account management server will be the one used in the vx_req_connector_create command,
    * otherwise this is the one that will be used.
    */
    char *acct_mgmt_server;
    /**
     * An application token as provided by the developer. This is required, if and only if the acct_mgmt_server is configured to require app tokens.
     *
     * This ensures that one application cannot use resources of another application, when sharing the same backend.
     */
    char *application_token;
    /**
     * A unique string value that identifies that the application that is integrated with the SDK.
     * This is an optional value provided by Vivox.
     * Whatever is passed in on this field will be present on events received
     * by others pertaining to your identity:
     * - vx_evt_message_t
     * - vx_evt_subscription_t
     * - vx_evt_buddy_presence_t
     * - vx_evt_session_added_t
     *
     * If set, this overrides the corresponding value in connector create
     */
    char* application_override;
    /**
     * The actual client IP address, to be passed in requests to the Vivox
     * network.  This is only supported in the server SDK, otherwise it is
     * ignored.
     */
    char* client_ip_override;
    /**
     * when set to 1, the buddy and watchers list is downloaded from the Vivox backend.
     * when set to 0, the application is responsible for setting buddies and watchers
     * using the appropriate requests.
     *
     * the default value is 1.
     */
    int enable_presence_persistence;
    /*
     * The preferred account handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *account_handle;
} vx_req_account_login_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_account_login object
 * \ingroup login
 */
VIVOXSDK_DLLEXPORT int vx_req_account_login_create(vx_req_account_login_t ** req);
#endif

/**
* Used to login of specific user account(s) using an authorization token (authtoken) retrieved from the Vivox server API.
* It may only be called after Connector initialization has completed successfully
*
* The XML format of this request can be found here: \ref Account_AuthTokenLogin_1
*
* \see vx_resp_account_authtoken_login\n
* \ingroup login
*/
typedef struct vx_req_account_authtoken_login {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_connector_create request
    */
    VX_HANDLE connector_handle;
    /**
     * User's auth token.
     * The auth token is created by the clients server making the viv_adm_auth.php call.
     */
    char* authtoken;
    /**
     * This flag is used to determine if the user is going to enable text or not for all sessions.
     * Setting this to text_mode_enabled means that text will be enabled and text_mode_disabled
     * means that text will not be enabled.  The default value is text_mode_enabled.
     */
    vx_text_mode enable_text;
    /**
    * This is an integer that specifies how often the SDK will send participant property events while in a channel.
    * If this is not set the default will be "on state change", which means that the events will be sent when
    * the participant starts talking, stops talking, is muted, is unmuted.
    *
    * The valid values are:
    * -   0 - Never
    * -   5 - 10 times per second
    * -  10 - 5 times per second
    * -  50 - 1 time per second
    * - 100 - on participant state change (this is the default)
    */
    int participant_property_frequency;
    /**
    * This determines whether or not to enable and allow buddy and presence information.
    * If this is set to 0 (default), buddy lists and presence status
    * will not be retrieved by the SDK and any command relating to buddies
    * or presence submitted to the SDK will be rejected.  Setting to 1 will enable buddy
    * and presense functionality.  0 is default.
    */
    int enable_buddies_and_presence;
    /**
     * This determines how the SDK will handle incoming buddy subscriptions.
     *
     * Valid values are:
     * - 0 - mode_auto_accept
     * - 1 - mode_auto_add
     * - 2 - mode_block
     * - 3 - mode_hide
     * - 4 - mode_application
     */
    vx_buddy_management_mode buddy_management_mode;
    /**
     * If set to 1, the SDK will post crash dumps that occur on SDK threads to Vivox
     * so that they can be automatically analyzed.
     */
    int autopost_crash_dumps;
    /**
    * Used to override the account management server set in vx_req_connector_create.
    * If set to NULL or empty the account management server will be the one used in the vx_req_connector_create command,
    * otherwise this is the one that will be used.
    */
    char *acct_mgmt_server;
    /**
     * An application token as provided by the developer. This is required, if and only if the acct_mgmt_server is configured to require app tokens.
     *
     * This ensures that one application cannot use resources of another application, when sharing the same backend.
     */
    char *application_token;
    /**
     * A unique string value that identifies that the application that is integrated with the SDK.
     * This is an optional value provided by Vivox.
     * Whatever is passed in on this field will be present on events received
     * by others pertaining to your identity:
     * - vx_evt_message_t
     * - vx_evt_subscription_t
     * - vx_evt_buddy_presence_t
     * - vx_evt_session_added_t
     *
     * If set, this overrides the corresponding value in connector create
     */
    char* application_override;
    /**
     * Use to control how the SDK responds to incoming voice calls.
     * mode_verify_answer is the default.
     * mode_auto_answer is deprecated.
     * mode_busy_answer is will automatically answer every call with a 486 busy.
     */
    vx_session_answer_mode answer_mode;
    /**
     * The actual client IP address, to be passed in requests to the Vivox
     * network.  This is only supported in the server SDK, otherwise it is
     * ignored.
     */
    char* client_ip_override;
    /**
     * when set to 1, the buddy and watchers list is downloaded from the Vivox backend.
     * when set to 0, the application is responsible for setting buddies and watchers
     * using the appropriate requests.
     *
     * the default value is 1.
     */
    int enable_presence_persistence;
    /*
     * The preferred account handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *account_handle;
} vx_req_account_authtoken_login_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_account_authtoken_login object
 * \ingroup login
 */
VIVOXSDK_DLLEXPORT int vx_req_account_authtoken_login_create(vx_req_account_authtoken_login_t ** req);
#endif

/**
* Used to login anonymously to the Vivox network.  This will allow a user to login and receive a temporary SIP Uri
* and a user generated display name if supplied.  It may only be called after Connector initialization has completed successfully.
*
* The XML format of this request can be found here: \ref Account_AnonymousLogin_1
*
* \see vx_resp_account_anonymous_login\n
* \ingroup login
*/
typedef struct vx_req_account_anonymous_login {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_connector_create request
    */
    VX_HANDLE connector_handle;
    /**
     * User's display name, this will be used as the display name that will be seen by others.
     */
    char* displayname;
    /**
    * This is an integer that specifies how often the SDK will send participant property events while in a channel.
    * If this is not set the default will be "on state change", which means that the events will be sent when
    * the participant starts talking, stops talking, is muted, is unmuted.
    *
    * The valid values are:
    * -   0 - Never
    * -   5 - 10 times per second
    * -  10 - 5 times per second
    * -  50 - 1 time per second
    * - 100 - on participant state change (this is the default)
    */
    int participant_property_frequency;
    /**
    * RESERVED FOR FUTURE USE: This determines whether or not to enable and allow buddy and presence information.  If this is set to 0 (default), buddy lists and presence status
    * will not be retrieved by the SDK and any command relating to buddies or presence submitted to the SDK will be rejected.  Setting to 1 will enable buddy
    * and presense functionality.  0 is default.
    */
    int enable_buddies_and_presence;
    /**
     * RESERVED FOR FUTURE USE: This determines how the SDK will handle incoming buddy subscriptions.
     */
    vx_buddy_management_mode buddy_management_mode;
    /**
     * If set to 1, the SDK will post crash dumps that occur on SDK threads to Vivox
     * so that they can be automatically analyzed.
     */
    int autopost_crash_dumps;
    /**
    * Used to override the account management server set in vx_req_connector_create.
    * If set to NULL or empty the account management server will be the one used in the vx_req_connector_create command,
    * otherwise this is the one that will be used.
    */
    char *acct_mgmt_server;
    /**
     * An application token as provided by the developer. This is required, if and only if the acct_mgmt_server is configured to require app tokens.
     *
     * This ensures that one application cannot use resources of another application, when sharing the same backend.
     */
    char *application_token;
    /**
     * A unique string value that identifies that the application that is integrated with the SDK.
     * This is an optional value provided by Vivox.
     * Whatever is passed in on this field will be present on events received
     * by others pertaining to your identity:
     * - vx_evt_message_t
     * - vx_evt_subscription_t
     * - vx_evt_buddy_presence_t
     * - vx_evt_session_added_t
     *
     * If set, this overrides the corresponding value in connector create
     */
    char* application_override;
    /**
     * The actual client IP address, to be passed in requests to the Vivox
     * network.  This is only supported in the server SDK, otherwise it is
     * ignored.
     */
    char* client_ip_override;
    /**
     * when set to 1, the buddy and watchers list is downloaded from the Vivox backend.
     * when set to 0, the application is responsible for setting buddies and watchers
     * using the appropriate requests.
     *
     * the default value is 1.
     */
    int enable_presence_persistence;
    /*
     * The preferred account handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *account_handle;
    /**
     * Desired username portion of the participant URI.
     * 
     * acct_name has the following restrictions:
     * - Name must start and end with a period ('.')
     * - Only alpha-numeric, space and the following characters are allowed: = + - _ . ! ~ ( )
     * - The length of acct_name can be no more than 63 bytes
     * - Multiple adjacent spaces are not permitted.
     */
    char *acct_name;
    /**
     * A Vivox Access Token to authorize the operation.
     */
    char *access_token;
} vx_req_account_anonymous_login_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_account_anonymous_login object
 * \ingroup login
 */
VIVOXSDK_DLLEXPORT int vx_req_account_anonymous_login_create(vx_req_account_anonymous_login_t ** req);
#endif

/**
 * Used to log a user out of the Vivox System.
 *
 * The XML format of this request can be found here: \ref Account_Logout_1
 *
 * \see vx_resp_account_logout
 * \ingroup login
 */
typedef struct vx_req_account_logout {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * DEPRECATED- This is a generic string that will be returned in the logout
    * event.  It can be used by applications to determine the reason for logout
    * if needed.
    */
    char* logout_reason;
} vx_req_account_logout_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_account_logout object
 * \ingroup login
 */
VIVOXSDK_DLLEXPORT int vx_req_account_logout_create(vx_req_account_logout_t ** req);
#endif

/**
* This can be used to set some of the properties passed in on the login command.
*
* The XML format of this request can be found here: \ref Account_SetLoginProperties_1
*
* \see vx_resp_account_set_login_properties
* \ingroup obsolete
*/
typedef struct vx_req_account_set_login_properties {
     /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful login' request
    */
    VX_HANDLE account_handle;
    /**
     * Use to control how the SDK responds to incoming voice calls.  mode_verify_answer is the default.
    */
    vx_session_answer_mode answer_mode;
    /**
    * This is an integer that specifies how often the SDK will send participant property events while in a channel.
    * If this is not set the default will be "on state change", which means that the events will be sent when
    * the participant starts talking, stops talking, is muted, is unmuted.
    *
    * The valid values are:
    * -   0 - Never
    * -   5 - 10 times per second
    * -  10 - 5 times per second
    * -  50 - 1 time per second
    * - 100 - on participant state change (this is the default)
    */
    int participant_property_frequency;
} vx_req_account_set_login_properties_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * used to allocate and initialize a vx_req_account_set_login_properties object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_set_login_properties_create(vx_req_account_set_login_properties_t ** req);
#endif

/**
 * The type of password hash algorithm to use when joining password protected channels
 */
typedef enum {
    /**
     * The password is sent in cleartext
     */
    password_hash_algorithm_cleartext,
    /**
     * The password is sent as a SHA1 hash of the password and SIP URI of the user, base64 encoded, with any trailing '=' characters stripped off.
     */
    password_hash_algorithm_sha1_username_hash,
} vx_password_hash_algorithm_t;

/**
* Session Groups are container objects to hold one ore more Sessions.
* Session Groups are used to support multi-channel mode.  A Session Group is local to the application and does not imply symmetrical association
* or 'view' for other participants in the individual channels.  If the client application needs to provide the ability for users to listen to multiple
* channels at the same time , the Session Group is used to add/remove channels (e.g. sessions).  Some examples of multi-channel use, might be within a MMOG
* with hierarchical organizational structure such as  fleets, nested groups or gangs; in other situations an multi-channel mode may be used to provide
* one channel to talk to the others located  'near' you, and a second channel  to bridge non-proximate participants from many regions in a Virtual World.
* The Session Group is also used to manage the channels in a Session Group.  There are cases where one channel may need to be the "focus" channel, and all
* others are in the background (essentially making the focus channel louder and the others quieter).  A Session Group Handle is required to change these
* aspects within the Session Group.
* Note that a SessionGroup is automatically created if the session create command is used.
*
* The XML format of this command can be found here: \ref SessionGroup_Create_1
*
* \see vx_resp_sessiongroup_create\n
*
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_create {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * SessionGroup Type.  \see vx_sessiongroup_type
    */
    vx_sessiongroup_type type;
    /**
     * The number of seconds of audio to keep on the client, this is used to create a DVR like feature but for audio.
     * For applications that desire delayed playback
     * the number of seconds of historical conversation kept (relative
     * to now). If loop mode is not desired, set this to zero.
     * The default value is 0.
     */
    int loop_mode_duration_seconds;
    /**
     * The device id for the capture device to use. NULL or empty to use last selected capture device.
     */
    char *capture_device_id;

    /**
     * The device id for the render device to use. NULL or empty to use the last selected render device.
     */
    char *render_device_id;
    /**
     * DEPRECATED
     * @deprecated
     */
    char *alias_username;
    /**
     * The preferred session group handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *sessiongroup_handle;
} vx_req_sessiongroup_create_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_create object
 *
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_create_create(vx_req_sessiongroup_create_t ** req);
#endif

/**
* Used to terminate a session group, this will also end any established sessions within the session group.
*
* The XML for this request can be found here: \ref SessionGroup_Terminate_1
*
* \see vx_resp_sessiongroup_terminate
*
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_terminate {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_create request
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_terminate_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_terminate object.
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_terminate_create(vx_req_sessiongroup_terminate_t ** req);
#endif

/**
* Sessions typically represent a connection to a media session with one or more participants.
* Used to generate an 'outbound' call to another user or channel.
* The specifics depend on the media types involved.   A session handle is required to control the local user functions within the session  (or remote users if the
* current account has rights to do so).  Currently creating a session automatically connects to the audio media, there is no need to call Session.Connect at this time,
* this is reserved for future use.
*
* The XML for this request can be found here: \ref SessionGroup_AddSession_1
*
* \see vx_resp_sessiongroup_add_session
*
* \ingroup sessiongroup
*/
#define MAX_CHANNEL_URI_LENGTH 1024
typedef struct vx_req_sessiongroup_add_session {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Returned from successful creation of a Session Group
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * This is the URI of the terminating point of the session (ie who/what is being called).
     * URI's must be valid and are restricted to the following rules:
     * - 1) The characters must be:
     *    - a) alphanumeric: abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890
     *    - b) valid non-alphanumeric: -_.!~*'()&=+$,;?/
     *    - c) escape sequence:
     *       - i) percent (%) followed by 2 hex characters (0-9 or A-F) which represent the
     *            ascii value of the character being encoded, examples are:
     *          - 1) space - %20
     *          - 2) @     - %40
     *          - 3) :     - %3A
     * - 2) URI length must not exceed MAX_CHANNEL_URI_LENGTH
     */
    char* uri;
    /**
     * This is the display name of the entity being called (user or channel).
     */
    char* name;
    /**
     * This is the password used if the user is joining a password protected channel.
     */
    char* password;
    /**
     * This flag is used to determine if the session is going to connect to audio or not.
     * Setting this to 1 means that Audio will be connected and 0 means that Audio will not be connected.  The default value is 0.
     */
    int connect_audio;
    /**
     * This parameter indicates the hashing algorithm used on the password as passed into the SDK.
     * This can be either password_hash_algorithm_cleartext, or password_hash_algorithm_sha1_username_hash.
     * If the former, the password passed in is in clear text. If the latter, the password passed in is
     * the SHA1 hash of the password and SIP URI of the user, base64 encoded, with any trailing '=' characters stripped off.
     */
    vx_password_hash_algorithm_t password_hash_algorithm;
    /**
     * The ID of the voice font to apply to the session.
     * A list of available voice font IDs must be obtained with vx_req_account_get_session_fonts.
     * Set to 0 for none.
     */
    int session_font_id;
    /**
     * This flag is used to determine if the session is going to connect to text or not.
     * Setting this to 1 means that Text will be connected and 0 means that Text will not be connected.  The default value is 0.
     */
    int connect_text;
    /**
     * The jitter buffer compensation for this SIP session.
     * This integer value represents (in ms), the amount of
     * 'compensation' to be applied to counteract network jitter. Valid values are 1 - 150 ms.
     * If this field is not initialized, or if is set to 1, the SDK applies the default values for the
     * given type of Session (60 ms for Morpheus Sessions, 80 ms for P2P sessions)
     */
    int jitter_compensation;
    /**
     * The preferred session handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *session_handle;
    /**
     * A Vivox Access Token to authorize the operation.
     */
    char *access_token;
} vx_req_sessiongroup_add_session_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_add_session object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_add_session_create(vx_req_sessiongroup_add_session_t ** req);
#endif

/**
* Used to 'end' an established session (ie: hang-up or disconnect).
*
* The XML for this request can be found here: ref SessionGroup_RemoveSession_1\n
*
* \see vx_resp_sessiongroup_remove_session
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_remove_session {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_remove_session_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_remove_session object
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_remove_session_create(vx_req_sessiongroup_remove_session_t ** req);
#endif

#ifndef VX_DISABLE_SESSIONGRP_FOCUS

/**
* Used when there are multiple sessions in a session group to set the audio focus of a session.
* The affect of this request is to 'bring to foreground' the specific session within the group.
* The audible volume will be relatively raised with respect to the channels that are not in 'focus'.
* To set all the sessions equal (e.g. no specific focus) the Unset Focus request can be made.
*
* The XML for this request can be found here: \ref SessionGroup_SetFocus_1
*
* \see vx_resp_sessiongroup_set_focus
*
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_set_focus {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * DEPRECATED: this field is ignored.
     * Handle returned from successful vx_req_sessiongroup_create request
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_set_focus_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_set_focus object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_set_focus_create(vx_req_sessiongroup_set_focus_t ** req);
#endif

/**
* To unset the focus of a specific channel that previously was set to have focus, this request may be issued.
*
* The XML for this request can be found here: \ref SessionGroup_UnsetFocus_1
*
* \see vx_resp_sessiongroup_unset_focus
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_unset_focus {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * DEPRECATED: this field is ignored.
     * Handle returned from successful vx_req_sessiongroup_create request
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_unset_focus_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_unset_focus object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_unset_focus_create(vx_req_sessiongroup_unset_focus_t ** req);
#endif

/**
* To reset all sessions in a group such that they have equal 'priority' or focus this request may be issued.
*
* The XML for this request can be found here: \ref SessionGroup_SetFocus_1
*
* \see vx_resp_sessiongroup_reset_focus
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_reset_focus {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_reset_focus_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_reset_focus object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_reset_focus_create(vx_req_sessiongroup_reset_focus_t ** req);
#endif
#endif

/**
* Sets the session within the session group to which to user's voice will be transmitted.
* If the user is currently transmitting to all sessions, using this command will force
* transmission to only the specified session.
*
* The XML for this request can be found here: \ref SessionGroup_SetTxSession_1
*
* \see vx_resp_sessiongroup_set_tx_session
*
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_set_tx_session {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * DEPRECATED: this field is ignored.
     * Handle returned from successful vx_req_sessiongroup_create request
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_set_tx_session_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_set_tx_session object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_set_tx_session_create(vx_req_sessiongroup_set_tx_session_t ** req);
#endif

/**
* Used to transmit the user's voice to all sessions within the specified session group.
*
* The XML for this request can be found here: \ref SessionGroup_SetTxAllSessions_1
*
* \see vx_resp_sessiongroup_set_tx_all_sessions
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_set_tx_all_sessions {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_create request
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_set_tx_all_sessions_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_set_tx_all_sessions object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_set_tx_all_sessions_create(vx_req_sessiongroup_set_tx_all_sessions_t ** req);
#endif

/**
* Disables transmission to all sessions within a session group.
*
* The XML for this request can be found here: \ref SessionGroup_SetTxNoSession_1
*
* \see vx_resp_sessiongroup_set_tx_no_session
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_set_tx_no_session {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_create request
     */
    VX_HANDLE sessiongroup_handle;
} vx_req_sessiongroup_set_tx_no_session_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_set_tx_no_session object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_set_tx_no_session_create(vx_req_sessiongroup_set_tx_no_session_t ** req);
#endif

/**
* Sets the position of any entire channel such that all audio from that channel appears
* to be coming from that position.
*
* The XML for this request can be found here: \ref SessionGroup_SetSession3DPosition_1
*
* \see vx_resp_sessiongroup_set_session_3d_position
* \ingroup sessiongroup
*/
typedef struct vx_req_sessiongroup_set_session_3d_position {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * Handle returned from successful vx_req_sessiongroup_create request
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * The position of the virtual 'mouth', this is the position where the audio will originate from for this user.
     * This 3 vector is a right handed Cartesian coordinate, with the positive axis pointing
     * towards the speaker's right, the positive Y axis pointing up, and the positive Z axis
     * pointing towards the speaker.
     */
    double speaker_position[3]; // {x, y, z}
    /**
     * DEPRECATED
     * @deprecated
     */
    double speaker_at_orientation[3]; // {x, y, z}
} vx_req_sessiongroup_set_session_3d_position_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_set_session_3d_position object
 * \ingroup sessiongroup
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_set_session_3d_position_create(vx_req_sessiongroup_set_session_3d_position_t ** req);
#endif

/**
* Used to start and stop session group audio injection
*
* The XML for this request can be found here: \ref SessionGroup_ControlAudioInjection_1
*
* \see vx_resp_sessiongroup_control_audio_injection
* \ingroup adi

*/
typedef struct vx_req_sessiongroup_control_audio_injection {
   /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /*
     * Whether to start or stop the audio injection
     */
    vx_sessiongroup_audio_injection_control_type audio_injection_control_type;
    /**
    * Handle returned from successful vx_req_sessiongroup_create request
    */
    VX_HANDLE sessiongroup_handle;
    /**
     * The full pathname for the WAV file to use for audio injection (MUST be single channel, 16-bit PCM, with the same sample rate as the negotiated audio codec)
    */
    char *filename;

} vx_req_sessiongroup_control_audio_injection_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_control_audio_injection object
 * \ingroup adi
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_control_audio_injection_create(vx_req_sessiongroup_control_audio_injection_t ** req);
#endif


/**
* DEPRECATED.
* Sessions typically represent a connection to a media session with one or more participants.
* Used to generate an 'outbound' call to another user or channel.  The specifics depend on the
* media types involved.   A session handle is required to control the local user functions within
* the session  (or remote users if the current account has rights to do so).  Currently creating
* a session automatically connects to the audio media, there is no need to call Session.Connect at
* this time, this is reserved for future use. This request can be used as a shortcut to creating
* a Session Group, and then adding a Session to the Session Group (see section ?7.1)
* This request will create a new Session Group, create a new Session, and return the handles to
* both in the response.
*
* \deprecated
*/
typedef struct vx_req_session_create {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * This is the display name of the entity being called (user or channel)
     */
    char* name;
    /**
     * This is the URI of the terminating point of the session (ie who/what is being called).
     * URI's must be valid and are restricted to the following rules:
     * - 1) The characters must be:
     *    - a) alphanumeric: abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890
     *    - b) valid non-alphanumeric: -_.!~*'()&=+$,;?/
     *    - c) escape sequence:
     *       - i) percent (%) followed by 2 hex characters (0-9 or A-F) which represent the
     *            ascii value of the character being encoded, examples are:
     *          - 1) space - %20
     *          - 2) @     - %40
     *          - 3) :     - %3A
     */
    char* uri;
    /**
     * This is the password used if the user is joining a password protected channel
     */
    char* password;
    /**
     * This flag is used to determine if the session is going to join audio or not.
     * Setting this to 1 means that Audio will be joined and 0 means that Audio will
     * not be joined.  The default value is 1.
     */
    int connect_audio;
    /**
    * DEPRECATED - Replaced with connect_audio.
    * This flag is used to determine if the session is going to join audio or not.
    * Setting this to 1 means that Audio will be joined and 0 means that Audio will
    * not be joined.  The default value is 1.
    * @deprecated
    */
    int join_audio; // DEPRECATED  1 true, <= 0 false
    /**
    * DEPRECATED - Replaced with connect_text.
    * Text is now enabled at the account level and is defaulted to have text enabled.
    *
    * This flag is used to determine if the session is going to join text or not.
    * Setting this to 1 means that text will be joined and 0 means that text will not be joined.
    * The default value is 0.
    * @deprecated
    */
    int join_text;  // DEPRECATED  1 true, <= 0 false
    /**
    * This parameter indicates the hashing algorithm used on the password as passed into the SDK.
    * This can be either password_hash_algorithm_cleartext, or password_hash_algorithm_sha1_username_hash.
    * If the former, the password passed in is in clear text. If the latter, the password passed in is the
    * SHA1 hash of the password and SIP URI of the user, base64 encoded, with any trailing '=' characters stripped off.
    */
    vx_password_hash_algorithm_t password_hash_algorithm;
    /**
     * This flag is used to determine if the session is going to join text or not.
     * Setting this to 1 means that Text will be joined and 0 means that Text will not
     * be joined.  The default value is 1.
     */
    int connect_text;
    /**
     * The ID of the voice font to apply to the session.
     * A list of available voice font IDs must be obtained with vx_req_account_get_session_fonts.
     * Set to 0 for none.
     */
    int session_font_id;
    /**
     * The jitter buffer compensation for this SIP session.
     * This integer value represents (in ms), the amount of
     * 'compensation' to be applied to counteract network jitter. Valid values are 1 - 150 ms.
     * If this field is not initialized, or if is set to 1, the SDK applies the default values for the
     * given type of Session (60 ms for Morpheus Sessions, 80 ms for P2P sessions)
     */
    int jitter_compensation;
    /**
     * DEPRECATED
     * @deprecated
     */
    char *alias_username;
    /**
     * the preferred session group handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *sessiongroup_handle;
    /**
     * the preferred session handle. This must not match any existing handle. If empty, one will be generated automatically.
     * Preferred handles must include at least one non numeric character.
     */
    char *session_handle;
    /**
     * A Vivox Access Token to authorize the operation.
     */
    char *access_token;
} vx_req_session_create_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * DEPRECATED. used to allocate and initialize a vx_req_session_create object
 * \deprecated
 */
VIVOXSDK_DLLEXPORT int vx_req_session_create_create(vx_req_session_create_t ** req);
#endif


/**
* If a session has already been created, this can be used to connect media (Audio) in a Session.
* This can be used to make an outbound call or to answer in inbound call.
* For inbound sessions (based upon an even status), this request is used to 'answer' a call (for
* audio only. text sessions do not need to be explicitly "answered")
*
* The XML for the request can be found here: \ref Session_MediaConnect_1
*
* \see vx_resp_session_media_connect
* \ingroup session
*/
typedef struct vx_req_session_media_connect {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * DEPRECATED - do not use, this structure member is ignored.
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * The ID of the voice font to apply to the session.
     * A list of available voice font IDs must be obtained with vx_req_account_get_session_fonts.
     * Set to 0 for none.
     */
    int session_font_id;
    /**
     * DEPRECATED
     * @deprecated
     */
    vx_media_type media;        //DEPRECATED
    /**
     * DEPRECATED
     * @deprecated
     *
     * Use the capture_device_id on the session group.
     *
     * The device id for the capture device to use. NULL or empty to use last selected capture device.
     */
    char *capture_device_id;
    /**
     * DEPRECATED
     * @deprecated
     *
     * Use the render_device_id on the session group.
     *
     * The device id for the render device to use. NULL or empty to use the last selected render device.
     */
    char *render_device_id;
    /**
     * The jitter buffer compensation for this SIP session.
     * This integer value represents (in ms), the amount of
     * 'compensation' to be applied to counteract network jitter. Valid values are 1 - 150 ms.
     * If this field is not initialized, or if is set to 1, the SDK applies the default values for the
     * given type of Session (60 ms for Morpheus Sessions, 80 ms for P2P sessions)
     */
    int jitter_compensation;
    /**
     * The number of seconds of audio to keep on the client, this is used to create a DVR like feature but for audio.
     * For applications that desire delayed playback
     * the number of seconds of historical conversation kept (relative
     * to now). If loop mode is not desired, set this to zero.
     * The default value is 0.
     */
    int loop_mode_duration_seconds;
} vx_req_session_media_connect_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_media_connect object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_media_connect_create(vx_req_session_media_connect_t ** req);
#endif

/**
* If a session has already been created and connected, this can be used to disconnect the media (Audio) in the session.
* This can be used to hang up a call that the user is in, or to refuse an incoming call.
*
* The XML for this request can be found here: \ref Session_MediaDisconnect_1
*
* \see vx_resp_session_media_disconnect
* \ingroup session
*/
typedef struct vx_req_session_media_disconnect {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * DEPRECATED - do not use, this structure member is ignored.
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * DEPRECATED
     * @deprecated
     */
    vx_media_type media;
    /**
     * Termination status.
     */
    vx_termination_status termination_status;
} vx_req_session_media_disconnect_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_media_disconnect object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_media_disconnect_create(vx_req_session_media_disconnect_t ** req);
#endif

/**
* If a session has already been created, this can be used to add text (IM) to a Session.
*
* The XML for the this request can be found here: \ref Session_TextConnect_1
*
* \see vx_resp_session_text_connect
* \ingroup session
*/
typedef struct vx_req_session_text_connect {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * DEPRECATED - do not use, this structure member is ignored.
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
} vx_req_session_text_connect_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_text_connect object
 */
VIVOXSDK_DLLEXPORT int vx_req_session_text_connect_create(vx_req_session_text_connect_t ** req);
#endif

/**
* If a session has already been created and connected, this can be used to disconnect the text (IM) in the session.
*
* The XML for this request can be found here: \ref Session_TextDisconnect_1
*
* \see vx_req_session_text_disconnect
* \ingroup session
*/
typedef struct vx_req_session_text_disconnect {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * DEPRECATED.  do not use, - this structure member is ignored.
     * @deprecated
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
} vx_req_session_text_disconnect_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_text_disconnect object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_text_disconnect_create(vx_req_session_text_disconnect_t ** req);
#endif

/**
* Used to 'end' an established session (ie: hang-up or disconnect).
*
* The XML for the request can be found here: \ref Session_Terminate_1
*
* \see vx_req_session_terminate
* \ingroup session
*/
typedef struct vx_req_session_terminate {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
} vx_req_session_terminate_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_terminate object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_terminate_create(vx_req_session_terminate_t ** req);
#endif

/**
* Used to mute or unmute the speaker for the specified session
*
* The XML for this request can be found here: \ref Session_MuteLocalSpeaker_1
*
* \see vx_req_session_mute_local_speaker
* \ingroup session
*/
typedef struct vx_req_session_mute_local_speaker {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * Used to specify mute or unmute.
     * 1 to mute, or 0 to unmute.
     */
    int mute_level;
    /**
    * The scope of the mute command.
    * Default is mute_scope_audio.
    */
    vx_mute_scope scope;
} vx_req_session_mute_local_speaker_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_mute_local_speaker object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_mute_local_speaker_create(vx_req_session_mute_local_speaker_t ** req);
#endif

/**
* Used to set the speaker volume for the specified session
*
* The XML for this request can be found here: \ref Session_SetLocalSpeakerVolume_1
*
* \see vx_resp_session_set_local_speaker_volume
* \ingroup session
*/
typedef struct vx_req_session_set_local_speaker_volume {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * The level of the audio, a number between 0 and 100 where 50 represents 'normal' speaking volume.
     */
    int volume;
} vx_req_session_set_local_speaker_volume_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_set_local_speaker_volume object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_set_local_speaker_volume_create(vx_req_session_set_local_speaker_volume_t ** req);
#endif


/**
 * This command sends an invitation to the specified user to join the channel corresponding to the provided session handle.
 * The user sending the invite will not receive any feedback after the invite call is made.  Therefore the user sending the
 * invitation will not know if the other user received the invitation, if they are online, if they rejected or ignored
 * the invitation.  They will only know that the user accepted the inviation if they see that user enter the channel.
 *
 * Sending an invite to a user is not the same as making a P2P call.  You will not get the events that you recieve
 * when a P2P call is made.  There will be no ringing, or feedback as to what the other user is doing.  You will have
 * to join the channel yourself, then send the invite, which causes the channel on the server to call the other user.
 * You will not know what happens in the call from the server to the other user unless the other user answers the call
 * and enters the channel.
 *
 * Invites work well for inviting PSTN callers into a channel.
 *
 * The XML for this request may be found here: \ref Session_InviteUser_1
 *
 * \see vx_resp_session_channel_invite_user
 * \ingroup session
 */
typedef struct vx_req_session_channel_invite_user {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * The SIP URI of the participant to invite.
     */
    char* participant_uri;
} vx_req_session_channel_invite_user_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to create a vx_req_session_channel_invite_user_t request.
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_channel_invite_user_create(vx_req_session_channel_invite_user_t ** req);
#endif

/**
 * Used to set the volume for a articipant in the current session.
 * It only affects the audio heard locally and does not change the audio as heard by any of the other participants.
 * These changes are only valid for the current session; if the participant audio that is set,
 * leaves the session and rejoins - the settings are lost.
 *
 * The XML for this request may be found here: \ref Session_SetParticipantVolumeForMe_1
 *
 * \see vx_resp_session_set_participant_volume_for_me
 * \ingroup session
 */
typedef struct vx_req_session_set_participant_volume_for_me {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * The fully specified URI of the Participant
     */
    char* participant_uri;
    /**
     * This is the volume level that has been set by the user, this should not change often and is a value between 0 and 100
     * where 50 represents 'normal' speaking volume.
     */
    int volume;
} vx_req_session_set_participant_volume_for_me_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_set_participant_volume_for_me object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_set_participant_volume_for_me_create(vx_req_session_set_participant_volume_for_me_t ** req);
#endif

/**
* Used to mute a participant in the current session.
* It only affects the audio heard locally and does not change the audio as heard by any of the
* other participants.  These changes are only valid for the current session;
* if the participant audio that is set, leaves the session and rejoins - the settings are lost.
*
* This command only applies to channels (3D and static) and is not applicable for person-to-person calls.
*
* The XML for this request may be found here: \ref Session_SetParticipantMuteForMe_1
*
* \see vx_resp_session_set_participant_mute_for_me
* \ingroup session
*/
typedef struct vx_req_session_set_participant_mute_for_me {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * The fully specified URI of the Participant
     */
    char* participant_uri;
    /**
     *  Indicated whether or not to mute or unmute the specified participant.  1 = mute, 0 = unmute.
     */
    int mute;
    /**
    * NOT CURRENTLY IMPLEMENTED.
    * Only audio will be muted no matter what scope is set.
    * The scope of the mute command.  Default is mute_scope_audio.
    */
    vx_mute_scope scope;
} vx_req_session_set_participant_mute_for_me_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_set_participant_mute_for_me object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_set_participant_mute_for_me_create(vx_req_session_set_participant_mute_for_me_t ** req);
#endif

/**
 * Used to control whether or not the SDK returns a response for potential high volume
 * requests - specifically - vx_req_session_set_3d_position_t request.
 */
typedef enum {
    /**
     * Return a response to the application
     */
    req_disposition_reply_required,
    /**
     * Do not return a response to the application
     */
    req_disposition_no_reply_required
} req_disposition_type_t;

/**
* Used to set and get the 3D position of the speaker and listener.
* For most applications, the speaker and listener positions are expected to be the same.
* However, the SDK supports the de coupling of these positions (for the implementation
* of a camera 'audio' zoom, for instance where the virtual 'mouth' is where the avatar is
* and the virtual 'ears' are where the camera view is from).
*
* The XML for this request may be found here: \ref Session_Set3DPosition_1
*
* \see vx_req_session_set_3d_position
* \ingroup session
*/
typedef struct vx_req_session_set_3d_position {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_add_session request
     */
    VX_HANDLE session_handle;
    /**
     * The position of the virtual 'mouth'.
     * This 3 vector is a right handed Cartesian coordinate, with the positive axis pointing
     * towards the speaker's right, the positive Y axis pointing up, and the positive Z axis
     * pointing towards the speaker.
     */
    double speaker_position[3]; // {x, y, z}
    /**
     * The velocity of the virtual 'mouth'.
     * This 3 vector is also in the above right handed Cartesian coordinate system.
     */
    double speaker_velocity[3]; // {x, y, z}
    /**
     * This 3 vector is a unit or 'direction' vector, representing the direction or 'heading' of the speaker.
     * The default value is [0 0 -1] (Heading 'forward' or 'North')
     */
    double speaker_at_orientation[3]; // {x, y, z}
    /**
     * This 3 vector is a unit or 'direction' vector, representing the 'up' direction of the speaker.
     * The default value is [0 1 0]
     */
    double speaker_up_orientation[3]; // {x, y, z}
    /**
     * This 3 vector is a unit or 'direction' vector, representing the 'left' direction of the speaker.
     * The default value is [-1 0 0]
     */
    double speaker_left_orientation[3]; // {x, y, z}
    /**
     * The position of the virtual 'ear'.
     * This 3 vector is a right handed Cartesian coordinate, with the positive X axis pointing towards
     * the speaker's right, the positive Y axis pointing up, and the positive Z axis pointing towards the speaker.
     */
    double listener_position[3]; // {x, y, z}
    /**
     * The velocity of the virtual 'ear'.
     * This 3 vector is also in the above right handed Cartesian coordinate system.
     */
    double listener_velocity[3]; // {x, y, z}
    /**
     * This 3 vector is a unit or 'direction' vector, representing the direction or 'heading' of the listener.
     * The default value is [0 0 -1] (Heading 'forward' or 'North')
     */
    double listener_at_orientation[3]; // {x, y, z}
    /**
     * This 3 vector is a unit or 'direction' vector, representing the 'up' direction of the listener.
     * The default value is [0 1 0]
     */
    double listener_up_orientation[3]; // {x, y, z}
    /**
     * This 3 vector is a unit or 'direction' vector, representing the 'left' direction of the listener.
     * The default value is [-1 0 0]
     */
    double listener_left_orientation[3]; // {x, y, z}
    /**
     * An enum type, reserved for future use. The default value is 'orientation_default = 0'
     */
    orientation_type type;
    /**
     * Used to control whether a response is sent back to the application for this request.
     */
    req_disposition_type_t req_disposition_type;
} vx_req_session_set_3d_position_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_set_3d_position object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_set_3d_position_create(vx_req_session_set_3d_position_t ** req);
#endif

/**
* Used to apply a voice font to an already-established voice session.
*
* The XML for this request may be found here: \ref Session_SetVoiceFont_1
*
* \see vx_resp_session_set_voice_font
* \ingroup session
*/
typedef struct vx_req_session_set_voice_font {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_sessiongroup_add_session request
    */
    VX_HANDLE session_handle;
    /**
    * The ID of the voice font to apply to the session.  A list of available voice font IDs must
    * be obtained with vx_req_account_get_session_fonts.
    * Set to 0 for none. The default value is 0.
    */
    int session_font_id;
} vx_req_session_set_voice_font_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_set_voice_font object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_set_voice_font_create(vx_req_session_set_voice_font_t ** req);
#endif

/**
 * Retrieves a list of all participants in a channel
 * \ingroup obsolete
 */
typedef struct vx_req_account_channel_get_participants {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request.
    */
    VX_HANDLE account_handle;
    /**
    * The channel to return the active particpants for
    */
    char* channel_uri;
    /**
    * The page number of the results to return
    */
    int page_number;
    /**
    * The number of results to return per page
    */
    int page_size;
} vx_req_account_channel_get_participants_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_get_participants object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_get_participants_create(vx_req_account_channel_get_participants_t ** req);
#endif

/**
 * Changes ownership of a channel to a specified account.
 * Can only be performed by the current owner of the channel.
 * Original owner will remain a moderator of the channel.
 * \ingroup obsolete
 */
typedef struct vx_req_account_channel_change_owner {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request.
    */
    VX_HANDLE account_handle;
    /**
    * The channel who's ownership is changing
    */
    char* channel_uri;
    /**
    * The URI of the target owner of the specified channel
    */
    char* new_owner_uri;
} vx_req_account_channel_change_owner_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_change_owner object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_change_owner_create(vx_req_account_channel_change_owner_t ** req);
#endif

/**
 * Sends specified data to a user directly.
 * \see vx_evt_user_app_data
 */
typedef struct vx_req_account_send_user_app_data {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request.
    */
    VX_HANDLE account_handle;
    /**
    * The URI of the account to send the data to.
    */
    char* to_uri;
    /**
    * The type of the data being sent. Note that multipart mime types are not supported (e.g multipart/digest etc).
    */
    char* content_type;
    /**
    * The content of the message being sent
    */
    char* content;
} vx_req_account_send_user_app_data_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_send_user_app_data object
 */
VIVOXSDK_DLLEXPORT int vx_req_account_send_user_app_data_create(vx_req_account_send_user_app_data_t ** req);
#endif


/**
* Used to create a channel.
*
* The XML for this request may be found here: \ref Account_ChannelCreate_1
*
* \see vx_resp_account_channel_create
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_create {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel Name
    */
    char* channel_name;
    /**
    * Channel Description
    */
    char* channel_desc;
    /**
    * Used to indicate the type of channel to be created, this can be a static channel or a positional channel.
    */
    vx_channel_type channel_type;
    /**
    * Indicates whether or not this channel is persistent.
    * 0 for false (default for create), 1 for true, -1 for unchanged (update only, default for update).
    *
    * If channel_type == dir, this does not apply.
    */
    int set_persistent; // 1 true, <= 0 false
    /**
    * Indicated whether or not this channel is password protected.
    * 0 for false (default for create), 1 for true, -1 for unchanged (update only, default for update).
    *
    * If channel_type == dir, this does not apply.
    */
    int set_protected;  // 1 true, <= 0 false
    /**
    * This is the password of this channel, it is only used if this is a Protected channel and set_protected = 1.
    *
    * If channel_type == dir, this does not apply.
    */
    char* protected_password;
    /**
    * DEPRECATED.
    * The forecast number of participants in the channel.  When creating a channel, use 0 for server default.
    * When updating a channel, use 0 to leave value unchanged.
    * @deprecated
    */
    int capacity;     /**< DEPRECATED */
    /**
    * Maximum number of participants allowed in the channel.
    * When creating a channel, use 0 for server default.  When updating a channel, use 0 to leave value unchanged
    *
    * If channel_type == dir, this does not apply.
    */
    int max_participants;
    /**
    * Used to specify the mode of the channel, this tells the server how users should be added to the channel when they join.
    * The mode of the channel can be:
    * - 0 - none
    * - 1 - normal
    * - 2 - presentation
    * - 3 - lecture
    * - 4 - open
    *
    * If channel_type == dir, this does not apply.
    */
    vx_channel_mode channel_mode;
    /**
    * This is the distance beyond which a participant is considered 'out of range'.
    * When participants cross this threshold distance from a particular listening position in a
    * positional channel, a roster update event is fired, which results in an entry being added
    * (or removed, as the case may be) from the user's speakers list. No audio is received for
    * participants beyond this range. The default channel value of this parameter is 60.
    * This will use server defaults on create, and will leave existing values unchanged on update
    *
    * If channel_type == dir, this does not apply.
    */
    int max_range;
    /**
    * This is the distance from the listener below which the 'gain rolloff' effects for a given
    * audio rolloff model (see below) are not applied.
    * In effect, it is the 'audio plateau' distance (in the sense that the gain is constant up
    * this distance, and then falls off). The default value of this channel parameter is 3.  This
    * will use server defaults on create, and will leave existing values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
    */
    int clamping_dist;
    /**
    * This value indicates how sharp the audio attenuation will 'rolloff' between the clamping and maximum distances.
    * Larger values will result in steeper rolloffs. The extent of rolloff will depend on the distance model chosen.
    * Default value is 1.1. This will use server defaults on create, and will leave existing values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
    */
    double roll_off;
    /**
    * The (render side) loudness for all speakers in this channel.
    * Note that this is a receive side value, and should not in practice be raised above, say 2.5.
    * The default value is 1.7. This will use server defaults on create, and will leave existing
    * values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
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
    * Used to make the communication to/from the channel encrypted (0 = not encrypted (default), 1 = RTP encrypted)
    *
    * If channel_type == dir, this does not apply.
    */
    int encrypt_audio;   /*0=default, 1=encrypt RTP */
} vx_req_account_channel_create_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_create object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_create_create(vx_req_account_channel_create_t ** req);
#endif

/**
 * Used to update a channel's properties.
 * Only the fields set will be updated, any fields not set
 * will be left at the existing value.  The default values for strings are NULL and integers are -1,
 * by leaving fields set to their default values (or specifically setting them to these values)
 * essentially tells the SDK to not change the value of this field.
 *
 * The XML for this request may be found here: \ref Account_ChannelUpdate_1
 *
 * @see vx_req_account_channel_create_t vx_resp_account_channel_update
 * \ingroup obsolete
 */
typedef struct vx_req_account_channel_update {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * The URI of the channel to update.
     */
    char* channel_uri;
    /**
     * Used to update the channel name.
     */
    char* channel_name;
    /**
     * Used to update the channel description.
     */
    char* channel_desc;
    /**
     * Used to update the persistent status of the channel (1 = persistent, 0 = non-persistent).
     *
     * If channel_type == dir, this does not apply.
     */
    int set_persistent;
    /**
     * Used to update the protected status of the channel (1 = password protected, 0=not password protetcted).
     * If set_protected is set to 1 then protected_password must be set as well.
     *
     * If channel_type == dir, this does not apply.
     */
    int set_protected;
    /**
     * If the channel has set_protected set to 0 thie field will be ignored, if set_protected
     * is set to 1 the password is required.
     *
     * If channel_type == dir, this does not apply.
     */
    char* protected_password;
    /**
     * DEPRECATED
     * @deprecated
    */
    int capacity;     /**< DEPRECATED */
    /**
     * Used to update the maximum number of participants allowed in the channel.
     *
     * If channel_type == dir, this does not apply.
     */
    int max_participants;
    /**
     * Used to update the channel mode (normal = 1, presentation = 2, lecture = 3, open = 4)
     *
     * If channel_type == dir, this does not apply.
     */
    vx_channel_mode channel_mode;
    /**
    * This is the distance beyond which a participant is considered 'out of range'.
    * When participants cross this threshold distance from a particular
    * listening position in a positional channel, a roster update event is fired, which results in an entry being added (or removed, as the case may be)
    * from the user's speakers list. No audio is received for participants beyond this range. The default channel value of this parameter is 60..
    * This will use server defaults on create, and will leave existing values unchanged on update
    *
    * If channel_type == dir, this does not apply.
    */
    int max_range;
    /**
    * This is the distance from the listener below which the 'gain rolloff' effects for a given
    * audio rolloff model (see below) are not applied.
    * In effect, it is the 'audio plateau' distance (in the sense that the gain is constant up this distance, and then falls off).
    * The default value of this channel parameter is 3.  This will use server defaults on create, and will leave existing values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
    */
    int clamping_dist;
    /**
    * This value indicates how sharp the audio attenuation will 'rolloff' between the clamping and maximum distances.
    * Larger values will result in steeper rolloffs. The extent of rolloff will depend on the distance model chosen.
    * Default value is 1.1. This will use server defaults on create, and will leave existing values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
    */
    double roll_off;
    /**
    * The (render side) loudness for all speakers in this channel.
    * Note that this is a receive side value, and should not in practice be raised above, say 2.5.
    * The default value is 1.7. This will use server defaults on create, and will leave existing
    * values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
    */
    double max_gain;
    /**
    * There are four possible values in this drop down field:
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
    * The default audio model is 1- Inverse Distance Clamped. This will use server defaults on
    * create, and will leave existing values unchanged on update.
    *
    * If channel_type == dir, this does not apply.
    */
    int dist_model;
    /**
    * Used to update the encrypted state of the channel (0 = not encrypted (default), 1 = RTP encrypted)
    *
    * If channel_type == dir, this does not apply.
    */
    int encrypt_audio;
} vx_req_account_channel_update_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_update object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_update_create(vx_req_account_channel_update_t ** req);
#endif

/**
 * Used to delete a channel
 *
 * The XML for this request may be found here: \ref Account_ChannelDelete_1
 *
 * \see vx_resp_account_channel_delete
 * \ingroup obsolete
 */
typedef struct vx_req_account_channel_delete {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * The handle for the current account
     */
    VX_HANDLE account_handle;
    /**
     * The URI of the channel to delete.
     */
    char* channel_uri;
} vx_req_account_channel_delete_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_delete object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_delete_create(vx_req_account_channel_delete_t ** req);
#endif

/**
* This will retrieve all available channel favorites and channel favorite groups for the currently logged in user.
*
* The XML for this request can be found here: \ref Account_ChannelFavoritesGetList_1
*
* \see vx_resp_account_channel_favorites_get_list
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_favorites_get_list {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
} vx_req_account_channel_favorites_get_list_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_favorites_get_list object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_favorites_get_list_create(vx_req_account_channel_favorites_get_list_t ** req);
#endif

/**
* Channel favorites are a mechanism that allows users to store a list of commonly used channels at the account level.
* The "Set" command below is used to add and update channel favorites.  If a Favorite ID is specified, it will be treated as an update.
* Otherwise, a channel favorite will be created.  Upon successful creation, a Favorite ID will be returned in the response.
*
* The XML for this request can be found here: \ref Account_ChannelFavoriteSet_1
*
* \see vx_resp_account_channel_favorite_set
*/
typedef struct vx_req_account_channel_favorite_set {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel favorite ID.
    * Supply only when updating a favorite.  Leave blank when creating a new favorite.
    */
    int channel_favorite_id;
    /**
    * The readable "nickname" for the channel favorite
    */
    char* channel_favorite_label;
    /**
    * Channel URI to be added as a favorite
    */
    char* channel_favorite_uri;
    /**
    * Data to be stored with the favorite
    */
    char* channel_favorite_data;
    /**
    * The ID of the channel favorite folder to add this favorite to.
    * 0 is default, implies no group.
    */
    int channel_favorite_group_id;
} vx_req_account_channel_favorite_set_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_favorite_set object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_favorite_set_create(vx_req_account_channel_favorite_set_t ** req);
#endif

/**
 * Used to delete a channel favorite
 *
 * The XML for this request can be found here: \ref Account_ChannelFavoriteDelete_1
 *
 * @see vx_resp_account_channel_favorite_delete
 * \ingroup obsolete
 */
typedef struct vx_req_account_channel_favorite_delete {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
     /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * The id of channel favorite to delete
     */
    int channel_favorite_id;
} vx_req_account_channel_favorite_delete_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_favorite_delete object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_favorite_delete_create(vx_req_account_channel_favorite_delete_t ** req);
#endif

/**
* Channel favorite groups are created to store channel favorites.  These groups cannot be nested.
* The "Set" command below is used to add and update channel favorite groups.  If a Favorite Group ID is specified, it will be treated as an update.
* Otherwise, a channel favorite group will be created.  Upon successful creation, a Favorite Group ID will be returned in the response
*
* The XML for this request can be found here: \ref Account_ChannelFavoriteGroupSet_1
*
* @see vx_resp_account_channel_favorite_group_set
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_favorite_group_set {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel favorite group ID.  Supply only when updating a favorite group.  Leave blank when creating a new favorite group
    */
    int channel_favorite_group_id;
    /**
    * Name of the favorite group
    */
    char* channel_favorite_group_name;
    /**
    * Data to be stored with the favorite group
    */
    char* channel_favorite_group_data;
} vx_req_account_channel_favorite_group_set_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_favorite_group_set object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_favorite_group_set_create(vx_req_account_channel_favorite_group_set_t ** req);
#endif

/**
 * Use to delete a channel favorite group
 *
 * The XML for this request can be found here: \ref Account_ChannelFavoriteGroupDelete_1
 *
 * \see vx_resp_account_channel_favorite_group_delete
 * \ingroup obsolete
 */
typedef struct vx_req_account_channel_favorite_group_delete {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
     * The id of the channel favorite group to delete
     */
    int channel_favorite_group_id;
} vx_req_account_channel_favorite_group_delete_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_favorite_group_delete object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_favorite_group_delete_create(vx_req_account_channel_favorite_group_delete_t ** req);
#endif

/**
* This will return all available information about a channel given the channel's URI.
*
* The XML for this request can be found here: \ref Account_ChannelGetInfo_1
*
* \see vx_resp_account_channel_get_info
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_get_info {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Must reference an account with moderator privileges
    */
    VX_HANDLE account_handle;
    /**
    * Channel URI
    */
    char* channel_uri;
} vx_req_account_channel_get_info_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_get_info object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_get_info_create(vx_req_account_channel_get_info_t ** req);
#endif

/**
* Channels can be searched on a certain criteria.
* The results are returned as an array of channel objects.
*
* The XML for this request can be found here: \ref Account_ChannelSearch_1
*
* \see vx_resp_account_channel_search
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_search {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The page number of the results to return
    */
    int page_number;
    /**
    * The number of results to return per page
    */
    int page_size;
    /**
    * The name of the channel to search on.
    * Leave blank if the search shouldn't take channel name into consideration.
    */
    char* channel_name;
    /**
    * The description of the channel to search on.
    * Leave blank if the search shouldn't take channel description into consideration.
    */
    char* channel_description;
    /**
    * If set to 1, will only return channels with at least one active participant.
    * Any value other than 1 will return the full set.
    */
    int channel_active;
    /**
     * The type of channel to return (NOT CURRENTLY SUPPORTED).
     */
    vx_channel_search_type channel_type;
    /**
    * Find only channels current user can moderate.
    * Default is all.
    */
    vx_channel_moderation_type moderation_type;
    /**
    * Looks for all search criteria with "begins with" masks.
    * '*' is not allowed in search criteia when this is set.  Default is 0 (off).
    */
    int begins_with;
} vx_req_account_channel_search_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_search object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_search_create(vx_req_account_channel_search_t ** req);
#endif

#ifndef VX_DISABLE_PRESENCE
/**
* Used to search for accounts.  The results are returned as an array of buddy objects.
*
* The XML for this request can be found here: \ref Account_AccountSearch_1
*
* \see vx_resp_account_buddy_search
* \ingroup buddy
*/
typedef struct vx_req_account_buddy_search {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * The page number of the results to return
     */
    int page_number;
    /**
     * The number of results to return per page
     */
    int page_size;
    /**
     * The first name of the buddy to search for.
     * Leave blank if the search shouldn't take this into consideration.
     */
    char* buddy_first_name;
    /**
     * The last name of the buddy to search for.
     * Leave blank if the search shouldn't take this into consideration.
     */
    char* buddy_last_name;
    /**
     * The user name of the buddy to search for.
     * Leave blank if the search shouldn't take this into consideration.
     */
    char* buddy_user_name;
    /**
     * The email address of the buddy to search for.
     * Leave blank if the search shouldn't take this into consideration.
     */
    char* buddy_email;
    /**
    * Looks for all search criteria with "begins with" masks.
    * '*' is not allowed in search criteia when this is set.  Default is 0 (off).
    */
    int begins_with;
    /**
    * Searchs the display name
    */
    char *buddy_display_name;
} vx_req_account_buddy_search_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_buddy_search object
 * \ingroup buddy
 */
VIVOXSDK_DLLEXPORT int vx_req_account_buddy_search_create(vx_req_account_buddy_search_t ** req);
#endif
#endif

/**
* Used to change the accounts that may act as moderator for the channel.
*
* The XML for this request can be found here: \ref Account_ChannelAddModerator_1
*
* \see vx_resp_account_channel_add_moderator
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_add_moderator {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel URI
    */
    char* channel_uri;
    /**
    * Channel Name
    */
    char* channel_name;
    /**
    * Fully specified URI of the user being added or removed from the moderator group
    */
    char* moderator_uri;
} vx_req_account_channel_add_moderator_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_add_moderator object
* \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_add_moderator_create(vx_req_account_channel_add_moderator_t ** req);
#endif

/**
* Used to remove a user from the channel moderator's list.
*
* The XML for this request can be found here: \ref Account_ChannelRemoveModerator_1
*
* \see vx_resp_account_channel_remove_moderator
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_remove_moderator {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * The URI of the channel
     */
    char* channel_uri;
    /**
     * The Name of the channel
     */
    char* channel_name;
    /**
     * The URI of the moderator to remove
     */
    char* moderator_uri;
} vx_req_account_channel_remove_moderator_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_remove_moderator object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_remove_moderator_create(vx_req_account_channel_remove_moderator_t ** req);
#endif

/**
* Used to retrieve the list of moderators for the specified Channel.
*
* The XML for this request can be found here: \ref Account_ChannelGetModerators_1
*
* \see vx_resp_account_channel_get_moderators
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_get_moderators {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel URI
    */
    char* channel_uri;
} vx_req_account_channel_get_moderators_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_get_moderators object
* \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_get_moderators_create(vx_req_account_channel_get_moderators_t ** req);
#endif

#ifndef VX_DISABLE_ACL
/**
 * Used to add the accounts that are allowed to participate in the channel (Access Control List).
*
* The XML for this request can be found here: \ref Account_ChannelAddACL_1
*
* \see vx_resp_account_channel_add_acl
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_add_acl {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel URI
    */
    char* channel_uri;
    /**
    * Fully specified URI of the user being added to the channel Access Control List.
    */
    char* acl_uri;
} vx_req_account_channel_add_acl_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_add_acl object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_add_acl_create(vx_req_account_channel_add_acl_t ** req);
#endif

/**
 * Used to remove an access control lists entry from a channel.
*
* The XML for this request can be found here: \ref Account_ChannelRemoveACL_1
*
* \see vx_resp_account_channel_remove_acl
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_remove_acl {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
     * Channel URI
     */
    char* channel_uri;
    /**
    * Fully specified URI of the user being removed from the channel Access Control List.
     */
    char* acl_uri;
} vx_req_account_channel_remove_acl_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_remove_acl object
* \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_remove_acl_create(vx_req_account_channel_remove_acl_t ** req);
#endif

/**
* Used to retrieve the Access Control List for the specified Channel.
*
* The XML for this request can be found here: \ref Account_ChannelGetACL_1
*
* \see vx_resp_account_channel_get_acl
* \ingroup obsolete
*/
typedef struct vx_req_account_channel_get_acl {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel URI
    */
    char* channel_uri;
} vx_req_account_channel_get_acl_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_channel_get_acl object
* \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_account_channel_get_acl_create(vx_req_account_channel_get_acl_t ** req);
#endif

#endif // WEB_CLIENT
/**
* Used to mute or unmute a participant in a particular channel.
* This request will mute/unmute the user for all users in the channel.
* This request is only valid if the account making the request has moderator privileges on the target channel.
*
* The XML for this request can be found here: \ref Channel_MuteUser_1
*
* \see vx_resp_channel_mute_user
* \ingroup channel
*/
typedef struct vx_req_channel_mute_user {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
     * DEPRECATED - this field will be ignored.
     * The name of the channel
     * @deprecated
    */
    char* channel_name;
    /**
     * The URI of the channel where the muting will occur
     */
    char* channel_uri;
    /**
     * The URI of the participant to mute or unmute
     */
    char* participant_uri;
    /**
     * 1 to mute the user, 0 to unmute the user
     */
    int set_muted;
    /**
    * The scope of the mute command.  Default is mute_scope_audio.
    */
    vx_mute_scope scope;
    /**
     * A Vivox Access Token to authorize the operation.
     */
    char* access_token;
} vx_req_channel_mute_user_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_channel_mute_user object
 * \ingroup channel
*/
VIVOXSDK_DLLEXPORT int vx_req_channel_mute_user_create(vx_req_channel_mute_user_t ** req);
#endif

/**
* Used to ban a user from joining a channel, or to un-ban the user if the user has already been banned.
* This will kick the user out from the channel, and ban the user from rejoining the channel.
* This request is only valid if the account making the request has moderator privileges on the target channel.
*
* The XML for this request can be found here: \ref Channel_BanUser_1
*
* \see vx_resp_channel_ban_user
* \ingroup obsolete
*/
typedef struct vx_req_channel_ban_user {
   /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * DEPRECATED - this field will be ignored.
     * The name of the channel
     * @deprecated
     */
    char* channel_name;
    /**
     * The URI of the channel
     */
    char* channel_uri;
    /**
     * The uri of the participant to ban/unban.
     */
    char* participant_uri;
    /**
     * 1 to ban the user, 0 to unban the user.
     */
    int set_banned;
} vx_req_channel_ban_user_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_channel_ban_user object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_channel_ban_user_create(vx_req_channel_ban_user_t ** req);
#endif

/**
* Returns a list of users banned for the specified channel.
*
* The XML for this request can be found here: \ref Channel_GetBannedUsers_1
*
* \see vx_resp_channel_get_banned_users
* \ingroup obsolete
*/
typedef struct vx_req_channel_get_banned_users {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * Channel URI
    */
    char* channel_uri;
} vx_req_channel_get_banned_users_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_channel_get_banned_users object
* \ingroup obsolete
*/
VIVOXSDK_DLLEXPORT int vx_req_channel_get_banned_users_create(vx_req_channel_get_banned_users_t ** req);
#endif

/**
 * Used to kick a user out of a channel and automatically allow the user back in after 60 seconds.
*
* The XML for this request can be found here: \ref Channel_KickUser_1
*
* \see vx_resp_channel_kick_user
* \ingroup obsolete
*/
typedef struct vx_req_channel_kick_user {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * DEPRECATED - this field will be ignored.
     * The name of the channel
     * @deprecated
     */
    char* channel_name;
    /**
     * The URI of the channel
     */
    char* channel_uri;
    /**
     * The participant URI
     */
    char* participant_uri;
    /**
     * A Vivox Access Token to authorize the operation.
     */
    char* access_token;
} vx_req_channel_kick_user_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_channel_kick_user object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_channel_kick_user_create(vx_req_channel_kick_user_t ** req);
#endif

/**
* Used to mute or unmute all current users in a channel.
* New users joining the channel will not be muted.
*
* The XML for this request can be found here: \ref Channel_MuteAllUsers_1
*
* \see vx_resp_channel_mute_all_users
* \ingroup channel
*/
typedef struct vx_req_channel_mute_all_users {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * DEPRECATED - this field will be ignored.
     * The name of the channel
     * @deprecated
     */
    char* channel_name;
    /**
     * The URI of the channel
     */
    char* channel_uri;
    /**
     * 1 to mute, 0 to unmute
     */
    int set_muted;
    /**
    * The scope of the mute command.
    * Default is mute_scope_audio.
    */
    vx_mute_scope scope;
    /**
     * A Vivox Access Token to authorize the operation.
     */
    char* access_token;
} vx_req_channel_mute_all_users_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_channel_mute_all_users object
 * \ingroup channel
 */
VIVOXSDK_DLLEXPORT int vx_req_channel_mute_all_users_create(vx_req_channel_mute_all_users_t ** req);
#endif

/**
* Used to set the lock mode of a channel.
* A channel can be locked and no additional participants will be able to join.  This command
* can also be used to unlock the channel.  The user issuing this request must have moderator
* privileges.  A channel becomes unlocked once all participants have left the channel.
*
* The XML for this request can be found here: \ref Channel_SetLockMode_1
*
* \see vx_resp_channel_set_lock_mode
* \ingroup obsolete
*/
typedef struct vx_req_channel_set_lock_mode
{
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
    * The URI of the channel to set the lock status on
    */
    char* channel_uri;
    /**
    * The channel lock mode
    */
    vx_channel_lock_mode lock_mode;
} vx_req_channel_set_lock_mode_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_channel_set_lock_mode object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_channel_set_lock_mode_create(vx_req_channel_set_lock_mode_t ** req);
#endif

/**
* Used to mute or unmute the microphone
*
* The XML for this request can be found here: \ref Connector_MuteLocalMic_1
*
* \see vx_resp_connector_mute_local_mic
* \ingroup devices
*/
typedef struct vx_req_connector_mute_local_mic {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_connector_create request.
    */
    VX_HANDLE connector_handle;
    /**
    * Mute Level, either 1 (mute) or 0 (unmute).
    */
    int mute_level;
} vx_req_connector_mute_local_mic_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_connector_mute_local_mic object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_mute_local_mic_create(vx_req_connector_mute_local_mic_t ** req);
#endif

/**
* Used to mute or unmute the speaker (eg: playback of incoming audio).
* This setting will apply universally to all connected audio sessions.
*
* The XML for this request can be found here: \ref Connector_MuteLocalSpeaker_1
*
* \see vx_resp_connector_mute_local_speaker
* \ingroup devices
*/
typedef struct vx_req_connector_mute_local_speaker {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_connector_create request.
    */
    VX_HANDLE connector_handle;
    /**
    * Mute Level, either 1 (mute) or 0 (unmute).
    */
    int mute_level;
} vx_req_connector_mute_local_speaker_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_connector_mute_local_speaker object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_mute_local_speaker_create(vx_req_connector_mute_local_speaker_t ** req);
#endif

/**
* DEPRECATED - use vx_req_aux_set_mic_level_t instead.
* Used to set the microphone volume; raising or lowering the boost applied to the speaking volume heard by others.
* The setting is applies for any and all audio sessions that are connected.
*
* \deprecated
*
* The XML for this request can be found here: \ref Connector_SetLocalMicVolume_1
*
* \see vx_resp_connector_set_local_mic_volume
* \ingroup obsolete
*/
typedef struct vx_req_connector_set_local_mic_volume {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * DEPRECATED - this parameter is ignored.
    * Handle returned from successful vx_req_connector_create request.
    * @deprecated
    */
    VX_HANDLE connector_handle;
    /**
    * The level of the audio, a number between 0 and 100 where 50 represents 'normal' speaking volume
    */
    int volume; // a number between 0 and 100 where 50 represents "normal" speaking volume
} vx_req_connector_set_local_mic_volume_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_connector_set_local_mic_volume object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_set_local_mic_volume_create(vx_req_connector_set_local_mic_volume_t ** req);
#endif

/**
* DEPRECATED - use vx_req_aux_set_speaker_level_t instead.
* Used to set the speaker volume. This setting provides a relative volume that applies
* to all connected audio sessions.
*
* The XML for this request can be found here: \ref Connector_SetLocalSpeakerVolume_1
*
* \deprecated
*
* \see vx_resp_connector_set_local_speaker_volume
* \ingroup obsolete
*/
typedef struct vx_req_connector_set_local_speaker_volume {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * DEPRECATED - this parameter is ignored
    * Handle returned from successful vx_req_connector_create request.
    * @deprecated
    */
    VX_HANDLE connector_handle;
    /**
    * The level of the audio, a number between 0 and 100 where 50 represents 'normal' speaking volume
    */
    int volume; // a number between 0 and 100 where 50 represents "normal" speaking volume
} vx_req_connector_set_local_speaker_volume_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_connector_set_local_speaker_volume object
 * \ingroup obsolete
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_set_local_speaker_volume_create(vx_req_connector_set_local_speaker_volume_t ** req);
#endif

/**
* Used to retrieve the current volume and mute status of the local speakers and microphone.
*
* The XML for this request can be found here: \ref Connector_GetLocalAudioInfo_1
*
* \see vx_resp_connector_get_local_audio_info
* \ingroup devices
*/
typedef struct vx_req_connector_get_local_audio_info {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * DEPRECATED - this parameter is not longer required and is ignored.
    * Handle returned from successful vx_req_connector_create request.
    * @deprecated
    */
    VX_HANDLE connector_handle;
} vx_req_connector_get_local_audio_info_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_connector_get_local_audio_info object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_connector_get_local_audio_info_create(vx_req_connector_get_local_audio_info_t ** req);
#endif

#ifndef VX_DISABLE_PRESENCE
/**
* Used to add a buddy to the buddy list or update the properties of a buddy.
* The buddy will be added to the account implied by the account_handle provided in the call.
* All relevant data about a buddy must be supplied when updating the buddy.  If, for instance,
* a display name is not specified when updating a the data field, the display name will be blank.
*
* The XML for this request may be found here: \ref Account_BuddySet_1
*
* \see vx_resp_account_buddy_set
* \see vx_evt_subscription_t
* \ingroup buddies
*/
typedef struct vx_req_account_buddy_set {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The identifier of the buddy to add to the list.
    */
    char* buddy_uri;
    /**
    * Readable name for display only.  Not required.
    */
    char* display_name;
    /**
    * A place to store extra data about a buddy. Not required.
    */
    char* buddy_data;
    /**
    * The ID of the group to add the buddy to. Set to 0 to remove the buddy from a group.
    */
    int group_id;
    /**
    * NOT CURRENTLY IMPLEMENTED.
    * Optional personalize message the user will see if he/she receives a vx_evt_subscription_t event. 256 chars max.
    */
    char* message;
} vx_req_account_buddy_set_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_buddy_set object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_buddy_set_create(vx_req_account_buddy_set_t ** req);
#endif

/**
* Used to delete a buddy from the user's buddy list.
*
* The XML for this request may be found here: \ref Account_BuddySet_1
*
* \see vx_resp_account_buddy_delete
* \ingroup buddies
*/
typedef struct vx_req_account_buddy_delete {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The identifier of the buddy to be removed from the user's buddy list.
    */
    char* buddy_uri;
} vx_req_account_buddy_delete_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_buddy_delete object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_buddy_delete_create(vx_req_account_buddy_delete_t ** req);
#endif

/**
* Used to add a group to the buddy list or update the properties of a group.
* All relevant data about a buddy group must be supplied when updating the buddy.  If, for
* instance, a group data is not specified when updating the name field, the group data will be blank.
*
* The XML for this request may be found here: \ref Account_BuddyGroupSet_1
*
* \see vx_resp_account_buddygroup_set
* \ingroup buddies
*/
typedef struct vx_req_account_buddygroup_set {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The ID of the group to be set.
    * Set to 0 when creating a new group.
    */
    int group_id;
    /**
    * The readable name of the group being added or updated.
    */
    char* group_name;
    /**
    * A place to store extra data about a buddy group.
    * Not required.
    */
    char* group_data;
} vx_req_account_buddygroup_set_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_buddygroup_set object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_buddygroup_set_create(vx_req_account_buddygroup_set_t ** req);
#endif

/**
* Used to delete a buddy group from the account's buddy list.
* All buddies associated with the group being deleted will have no group association upon
* successful completion of this command.
*
* The XML for this request may be found here: \ref Account_BuddyGroupDelete_1
*
* \see vx_resp_account_buddygroup_delete
* \ingroup buddies
*/
typedef struct vx_req_account_buddygroup_delete {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The ID of the group to delete
    */
    int group_id;
} vx_req_account_buddygroup_delete_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_buddygroup_delete object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_buddygroup_delete_create(vx_req_account_buddygroup_delete_t ** req);
#endif

/**
* Retrieves a list of all buddies and buddy groups for a given account.
*/
typedef struct vx_req_account_list_buddies_and_groups {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
} vx_req_account_list_buddies_and_groups_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_list_buddies_and_groups object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_list_buddies_and_groups_create(vx_req_account_list_buddies_and_groups_t ** req);
#endif
#endif

/**
* Used to send a message to all participants in the session.
*
* The XML for this request may be found here: \ref Session_SendMessage_1
*
* \see vx_resp_session_send_message
* \ingroup session
*/
typedef struct vx_req_session_send_message {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_sessiongroup_add_session request
    */
    VX_HANDLE session_handle;
    /**
    * HTTP Content type of the message.
    * If an empty string is passed in this field will default to "text/plain".  This field
    * will be validated so it must be in a valid format.  For a full description of valid
    * content types please see RFC 2046 (http://www.ietf.org/rfc/rfc2046.txt).
    */
    char* message_header;
    /**
    * The contents of the message
    */
    char* message_body;
} vx_req_session_send_message_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_send_message object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_send_message_create(vx_req_session_send_message_t ** req);
#endif

#ifndef VX_DISABLE_PRESENCE

/**
* Used to set the presence (online/offline status etc) of an account.
*
* The XML for this request may be found here: \ref Account_SetPresence_1
*
* \see vx_resp_account_set_presence
* \ingroup buddies
*/
typedef struct vx_req_account_set_presence {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
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
     * DEPRECATED
     * @deprecated
     */
    char *alias_username;
} vx_req_account_set_presence_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_set_presence object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_set_presence_create(vx_req_account_set_presence_t ** req);
#endif


/**
* Used to respond to an incoming subscription event (e.g. a request from another user to put this user on their buddy list).
*
* The XML for this request may be found here: \ref Account_SendSubscriptionReply_1
*
* \see vx_resp_account_send_subscription_reply
* \ingroup buddies
*/
typedef struct vx_req_account_send_subscription_reply {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The presence rule type.
    * - rule_allow
    * - rule_block
    * - rule_hide
    */
    vx_rule_type rule_type;
    /**
    * Buddy will be automatically added to account's buddy list (1 or 0). Default is 0.
    */
    int auto_accept;
    /**
    * The identifier of the buddy who sent the initial subscription
    */
    char* buddy_uri;
    /**
    * The subscription identifier that was presented with the inbound subscription event
    */
    char* subscription_handle;
} vx_req_account_send_subscription_reply_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_send_subscription_reply object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_send_subscription_reply_create(vx_req_account_send_subscription_reply_t ** req);
#endif
#endif

/**
* Used to send notification about the properties of the current session (example: typing, not typing, etc).
*
* The XML for this request may be found here: \ref Session_SendNotification_1
*
* \see vx_resp_session_send_notification
* \ingroup session
*/
typedef struct vx_req_session_send_notification {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_sessiongroup_add_session request
    */
    VX_HANDLE session_handle;
    /**
    * Used for typing notifications (text) and hand-raising (audio)
    * The default value is notification_not_typing.
    */
    vx_notification_type notification_type;
} vx_req_session_send_notification_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_send_notification object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_session_send_notification_create(vx_req_session_send_notification_t ** req);
#endif

/**
* Used to send DTMF signal for a given session.
*
* The XML for this request may be found here: \ref Session_SendDTMF_1
*
* \see vx_resp_session_send_dtmf
* \ingroup obsolete
*
* \attention Not supported on the PLAYSTATION(R)3 platform
*/
typedef struct vx_req_session_send_dtmf {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_sessiongroup_add_session request
    */
    VX_HANDLE session_handle;
    /**
    * Tone signal to send.
    *   -  0 - dtmf_0
    *   -  1 - dtmf_1
    *   -  2 - dtmf_2
    *   -  3 - dtmf_3
    *   -  4 - dtmf_4
    *   -  5 - dtmf_5
    *   -  6 - dtmf_6
    *   -  7 - dtmf_7
    *   -  8 - dtmf_8
    *   -  9 - dtmf_9
    *   - 10 - dtmf_pound
    *   - 11 - dtmf_star
    *   - 12 - dtmf_A
    *   - 13 - dtmf_B
    *   - 14 - dtmf_C
    *   - 15 - dtmf_D
    */
    vx_dtmf_type dtmf_type;
} vx_req_session_send_dtmf_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_send_dtmf object
 * \ingroup obsolete
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 */
VIVOXSDK_DLLEXPORT int vx_req_session_send_dtmf_create(vx_req_session_send_dtmf_t ** req);
#endif

#ifndef VX_DISABLE_PRESENCE
/**
* Used to define rules to block individuals or groups of individuals.
*
* The XML for this request may be found here: \ref Account_CreateBlockRule_1
*
* \see vx_resp_account_create_block_rule
* \ingroup buddies
*/
typedef struct vx_req_account_create_block_rule {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * SIP URI of the buddy to block.
    */
    char* block_mask;
    /**
    * Blocks the visibility of presence, and/or communication.
    * Valid values: 0 (default) or 1. [not relevant for delete]
    */
    int presence_only;
} vx_req_account_create_block_rule_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_create_block_rule object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_create_block_rule_create(vx_req_account_create_block_rule_t ** req);
#endif

/**
 * Used to delete a block rule.
 *
 * The XML for this request may be found here: \ref Account_DeleteBlockRule_1
 *
 * \see vx_resp_account_delete_block_rule
 * \ingroup buddies
 */
typedef struct vx_req_account_delete_block_rule {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
    * SIP URI of the buddy.
     */
    char* block_mask;
} vx_req_account_delete_block_rule_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_delete_block_rule object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_delete_block_rule_create(vx_req_account_delete_block_rule_t ** req);
#endif

/**
* Retrieves a list of block rules for a given account.
* The order of rules returned in the list is unspecified
* and it is up to the calling application to sequence the resultant data.
*
* The XML for this request may be found here: \ref Account_ListBlockRules_1
*
* \see vx_resp_account_list_block_rules
* \ingroup buddies
*/
typedef struct vx_req_account_list_block_rules {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
} vx_req_account_list_block_rules_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_list_block_rules object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_list_block_rules_create(vx_req_account_list_block_rules_t ** req);
#endif

/**
* Used to set up rules to automatically accept individuals or groups of individuals when they
* request presence subscriptions.
*
* The XML for this request may be found here: \ref Account_CreateAutoAcceptRule_1
*
* \see vx_resp_account_create_auto_accept_rule
* \ingroup buddies
*/
typedef struct vx_req_account_create_auto_accept_rule {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * SIP URI of buddy
    */
    char* auto_accept_mask;
    /**
    * Will cause a requesting buddy that matches this rule to be automatically added to the
    * buddy list associated with the account_handle.
    * Valid values: 0 (default) or 1. [not relevant for delete]
    */
    int auto_add_as_buddy;
    /**
     * Optional, add the accept rule with this nickname
     */
    char* auto_accept_nickname;
} vx_req_account_create_auto_accept_rule_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_create_auto_accept_rule object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_create_auto_accept_rule_create(vx_req_account_create_auto_accept_rule_t ** req);
#endif

/**
 * Used to delete an auto accept rule.
 *
 * The XML for this request may be found here: \ref Account_DeleteAutoAcceptRule_1
 *
 * \see vx_resp_account_delete_auto_accept_rule
 * \ingroup buddies
 */
typedef struct vx_req_account_delete_auto_accept_rule {
     /**
     * The common properties for all requests
     */
    vx_req_base_t base;
     /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * SIP URI of buddy
     */
    char* auto_accept_mask;
} vx_req_account_delete_auto_accept_rule_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_delete_auto_accept_rule object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_delete_auto_accept_rule_create(vx_req_account_delete_auto_accept_rule_t ** req);
#endif

/**
* Retrieves a list of auto accept rules for a given account.
* The order of rules returned in the list is unspecified and
* it is up to he calling application to sequence the resultant data.
*
* The XML for this request may be found here: \ref Account_ListAutoAcceptRules_1
*
* \see vx_resp_account_list_auto_accept_rule
* \ingroup buddies
*/
typedef struct vx_req_account_list_auto_accept_rules {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
} vx_req_account_list_auto_accept_rules_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_list_auto_accept_rules object
 * \ingroup buddies
 */
VIVOXSDK_DLLEXPORT int vx_req_account_list_auto_accept_rules_create(vx_req_account_list_auto_accept_rules_t ** req);
#endif
#endif
/**
* \deprecated - DEPRECATED - use vx_req_account_web_call instead.
*
* Used to update information about the currently logged-in account.
*
* The XML for this request may be found here: \ref Account_UpdateAccount_1
*
* \see vx_resp_account_update_account
*/
typedef struct vx_req_account_update_account {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The display name for the account
    */
    char* displayname;
} vx_req_account_update_account_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_update_account object
 */
VIVOXSDK_DLLEXPORT int vx_req_account_update_account_create(vx_req_account_update_account_t ** req);
#endif

/**
* DEPRECATED - use vx_req_account_web_call instead.
*
* Returns basic information about the user's account
*
* The XML for this request may be found here: \ref Account_GetAccount_1
*
* \see vx_resp_account_get_account
* \deprecated
*/
typedef struct vx_req_account_get_account {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
     * DEPRECATED - this field is no longer used
     * @deprecated
     */
    char* uri;
} vx_req_account_get_account_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_get_account object
 */
VIVOXSDK_DLLEXPORT int vx_req_account_get_account_create(vx_req_account_get_account_t ** req);
#endif

/*
* Sends a SMS message to another user, if that user's phone and carrier have been properly set
* (see Update Account).
* If the recipient's phone or carrier is not set, a response will be immediately returned with a return code of 1.
* If the message could not be delivered for any other unforeseen reason downstream, the error will not be
* reported in the response.
*
* The XML for this request may be found here: \ref Account_SendSMS_1
*
* \see vx_resp_account_send_sms
*
* \attention Not supported on the PLAYSTATION(R)3 platform
*/
typedef struct vx_req_account_send_sms {
    /*
     * The common properties for all requests
     */
    vx_req_base_t base;
    /*
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /*
    * The URI of intended receiver of the message
    */
    char* recipient_uri;
    /*
    * The body of the message being sent
    */
    char* content;
} vx_req_account_send_sms_t;
#ifndef VIVOX_TYPES_ONLY
/*
 * Used to allocate and initialize a vx_req_account_send_sms object
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 */
VIVOXSDK_DLLEXPORT int vx_req_account_send_sms_create(vx_req_account_send_sms_t ** req);
#endif


/**
* Used to perform a network test.
* This command will probe the network and perform a suite of tests to determine if the application
* will have the connectivity required to perform the tasks needed to send and receive presence, IM, and media information.  This call is used
* to test network connectivity issues  such as creating a "Network Troubleshooter" tool that can be stand alone, or integrated into an application.
* This single call will return the results to all the tests performed, and the data can be displayed to the user.
* The SDK is shipped with a sample "Network Troubleshooter" that can be used as is, or as a reference to create a custom "Network Troubleshooter".
*
* The XML for this request may be found here: \ref Aux_ConnectivityInfo_1
*
* \see vx_resp_aux_connectivity_info
* \ingroup diagnostics
*/
typedef struct vx_req_aux_connectivity_info {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Provided by Vivox
    */
    char* well_known_ip;
    /**
    * Provided by Vivox
    */
    char* stun_server;
    /**
    * Provided by Vivox
    */
    char* echo_server;
    /**
    * Provided by Vivox
    */
    int echo_port;
    /**
    * Provided by Vivox
    */
    int timeout;
    /**
    * If this is specified, all other fields will be ignored and only the values obtained from the server will be used.
    * These parameters will be returned in the response..  If this field is NULL, the fields specified above will be used to perform the network test.
    */
    char* acct_mgmt_server;
} vx_req_aux_connectivity_info_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_connectivity_info object
* \ingroup diagnostics
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_connectivity_info_create(vx_req_aux_connectivity_info_t ** req);
#endif

/**
* Used to retrieve the list of audio render devices for the current computer.
*
* Most machines have at least one audio 'render' device. The Vivox SDK allows the application programmer to call this method without actually
* initializing the sdk or logging in (much like the network connectivity methods).
* This list might actually be smaller than the list of all available render devices.
*
* The XML for this request may be found here: \ref Aux_GetRenderDevices_1
*
* \see vx_resp_aux_get_render_devices
* \ingroup devices
*/
typedef struct vx_req_aux_get_render_devices {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
} vx_req_aux_get_render_devices_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_get_render_devices object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_get_render_devices_create(vx_req_aux_get_render_devices_t ** req);
#endif

/**
* The call returns the list of available audio capture devices.
* This list might actually be smaller than the list of all available capture devices.
*
* The XML for this request may be found here: \ref Aux_GetCaptureDevices_1
*
* \see vx_resp_aux_get_capture_devices
* \ingroup devices
*/
typedef struct vx_req_aux_get_capture_devices {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
} vx_req_aux_get_capture_devices_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_get_render_devices object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_get_capture_devices_create(vx_req_aux_get_capture_devices_t ** req);
#endif

/**
* The set render device method is used to instruct the Vivox SDK sound system to set the render device
* to a particular value.
* The 'render device specifier' is a string that must match exactly the 'device'
* string of the vx_device_t structs returned from a 'get render devices' call.
* This value is 'sticky'- It will be retained across various voice sessions
*
* Note that on PS4 the render_device_specifier can be the string representation of the PS4 User ID.
*
* The XML for this request may be found here: \ref Aux_SetRenderDevice_1
*
* \see vx_resp_aux_set_render_device
* \ingroup devices
*/
typedef struct vx_req_aux_set_render_device {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * A string specifier, it must be exactly the same as the 'device' string of a returned 'vx_device_t'
    * object by a 'Get Render Devices' call.
    *
    * On PS4, this can be the string representation of the user ID - e.g. "1", "2", "3" etc.
    */
    char* render_device_specifier;
} vx_req_aux_set_render_device_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_set_render_device object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_set_render_device_create(vx_req_aux_set_render_device_t ** req);
#endif

/**
* The set capture device method is used to instruct the Vivox SDK sound system to set the capture
* device to a particular value.
* The 'capture device specifier' is a string that must match exactly the 'device' string of the
* vx_device_t structs returned by a 'get capture devices' call. This value is 'sticky'- It will
* be retained across various voice sessions
*
* Note that on PS4 the capture_device_specifier can be the string representation of the PS4 User ID.
*
* The XML for this request may be found here: \ref Aux_SetCaptureDevice_1
*
* \see vx_resp_aux_set_capture_device
* \ingroup devices
*/
typedef struct vx_req_aux_set_capture_device {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * A string specifier, it must be exactly the same as the 'device' string of a returned 'vx_device_t'
    * object by a 'get capture devices' call.
    *
    * On PS4, this can be the string representation of the user ID.
    */
    char* capture_device_specifier;
} vx_req_aux_set_capture_device_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_set_capture_device object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_set_capture_device_create(vx_req_aux_set_capture_device_t ** req);
#endif

/**
* This method returns the master 'microphone' (capture) level of the Vivox SDK sound system.
* It is a number between 0 and 100, with a default value of 50. The scale is logarithmic, which
* means that a level increase of 6 represents a doubling in energy, while a level increase of 20
* represents a ten-fold increase in energy.
*
* The XML for this request may be found here: \ref Aux_GetMicLevel_1
*
* \see vx_resp_aux_get_mic_level
* \ingroup devices
*/
typedef struct vx_req_aux_get_mic_level {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
} vx_req_aux_get_mic_level_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_get_mic_level object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_get_mic_level_create(vx_req_aux_get_mic_level_t ** req);
#endif

/**
* This method returns the master 'speaker' (render) level of the Vivox SDK sound system.
* It is a number between 0 and 100, with a default value of 50. The scale is logrithmic,
* which means that a level increase of 6 represents a doubling in energy, while a level increase of 20
* represents a ten-fold increase in energy.
*
* The XML for this request may be found here: \ref Aux_GetSpeakerLevel_1
*
* \see vx_resp_aux_get_speaker_level
* \ingroup devices
*/
typedef struct vx_req_aux_get_speaker_level {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
} vx_req_aux_get_speaker_level_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_get_speaker_level object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_get_speaker_level_create(vx_req_aux_get_speaker_level_t ** req);
#endif

/**
* Used to set the master microphone level of the Vivox SDK sound system.
* Valid values are non-negative integers between 0 and 100 (inclusive). The initial (default) value is 50.
*
* The XML for this request may be found here: \ref Aux_SetMicLevel_1
*
* \see vx_resp_aux_set_mic_level
* \ingroup devices
*/
typedef struct vx_req_aux_set_mic_level {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * The master 'microphone' level.
     * Non negative integer value between 0 and 100 (inclusive). +6 increase represents a doubling of energy,
     * +20 increase represents a ten fold increase in energy. Default value is 50.
     */
    int level;
} vx_req_aux_set_mic_level_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_set_mic_level object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_set_mic_level_create(vx_req_aux_set_mic_level_t ** req);
#endif

/**
* Used to set the master speaker level of the Vivox SDK sound system.
* Valid values are non-negative integers between 0 and 100 (inclusive). The initial (default) value is 50.
*
* The XML for this request may be found here: \ref Aux_SetSpeakerLevel_1
*
* \see vx_resp_aux_set_speaker_level
* \ingroup devices
*/
typedef struct vx_req_aux_set_speaker_level {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * The master 'speaker' level.
    * Non negative integer value between 0 and 100 (inclusive). +6 increase represents a doubling of energy,
    * +20 increase represents a ten fold increase in energy. Default value is 50.
    */
    int level;
} vx_req_aux_set_speaker_level_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_set_speaker_level object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_set_speaker_level_create(vx_req_aux_set_speaker_level_t ** req);
#endif

/**
 * This method is used to ensure that a particular selected render device is functioning properly.
 * The 'sound_file_path' should point to a local directory path containing a monaural wav or au sound file.
 * If the loop value is set to 1, the sound system will play the file in a continuous loop,
 * until the 'render audio stop' method is called.
 *
 * The XML for this request may be found here: \ref Aux_RenderAudioStart_1
 *
 * \see vx_resp_aux_render_audio_start
 * \ingroup devices
 */
typedef struct vx_req_aux_render_audio_start {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * A local directory path name of a monaural 'wav' or 'au' sound file
     */
    char* sound_file_path;
    /**
     * If set to 1, the sound system will play the file in a continuous loop, until the 'render audio stop' method
     * is called; If set to 0, the file will play once or until the render audio stop' method
     * is called. The default is 0.
     */
    int loop;
    /**
     * DEPRECATED.  Debug use only.
     * @deprecated
     */
    char *path;
} vx_req_aux_render_audio_start_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_render_audio_start object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_render_audio_start_create(vx_req_aux_render_audio_start_t ** req);
#endif

/**
 * This method is used to modify the applied font of the currently rendering aux audio buffer.
 * If no audio buffer is currently being rendered, the method returns immediately
 *
 */
typedef struct vx_req_aux_render_audio_modify {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
    * The updated font string to apply to the currently playing audio buffer
    * Leave null to play font without changes.
    */
    char *font_str;
} vx_req_aux_render_audio_modify_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_render_audio_modify object
  */
VIVOXSDK_DLLEXPORT int vx_req_aux_render_audio_modify_create(vx_req_aux_render_audio_modify_t ** req);
#endif

/**
 * This method is used to get the connector VAD (Voice Activity Detector) properties.
 *
 * \see vx_req_aux_get_vad_properties
 * \ingroup devices
 */
typedef struct vx_req_aux_get_vad_properties {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
} vx_req_aux_get_vad_properties_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_get_vad_properties object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_get_vad_properties_create(vx_req_aux_get_vad_properties_t ** req);
#endif

/**
 * This method is used to set the connector VAD (Voice Activity Detector) properties.
 * It can only be invoked successfully if neither the capture audio thread or the msopenal are active.
 *
 * \see vx_req_aux_set_vad_properties
 * \ingroup devices
 */
typedef struct vx_req_aux_set_vad_properties {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
    * The 'Hangover time' - the time (in milliseconds) that it takes for the VAD to
    * switch back to silence from speech mode after the last speech frame has been detected
    */
    int vad_hangover;
    /**
    * The 'vad sensitivity' - A dimensionless value between 0 and 100, indicating the 'sensitivity of the VAD'.
    * Increasing this value corresponds to decreasing the sensitivity of the VAD (i.e. '0' is most sensitive, while 100 is 'least sensitive')
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
} vx_req_aux_set_vad_properties_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_render_audio_modify object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_set_vad_properties_create(vx_req_aux_set_vad_properties_t ** req);
#endif


/**
 * Used to stop audio render playback immediately.
 * If no audio files were being played back, the method returns immediately
 *
 * The XML for this request may be found here: \ref Aux_RenderAudioStop_1
 *
 * \see vx_resp_aux_render_audio_stop
 * \ingroup devices
 */
typedef struct vx_req_aux_render_audio_stop {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
} vx_req_aux_render_audio_stop_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_render_audio_stop object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_render_audio_stop_create(vx_req_aux_render_audio_stop_t ** req);
#endif


/**
* This method used to ensure that a particular selected capture device is functioning properly.
* It can only be invoked successfully if the capture device is not currently in use (in a voice session, for example).
* As soon as the start audio capture method completes successfully, the Vivox SDK sound system starts sending events of type 'VxAudioEvent_t'
* (for a more detailed description of this struct, see below)  at one half the capture frame rate.
*
* The XML for this request may be found here: \ref Aux_CaptureAudioStart_1
*
* \see vx_resp_aux_capture_audio_start
* \ingroup devices
*/
typedef struct vx_req_aux_capture_audio_start {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * DEPRECATED
     * @deprecated
     */
    int duration;
    /**
     * loop audio to currently selected render device as well - default 0, set to 1 to loop audio to speaker
     */
    int loop_to_render_device;
} vx_req_aux_capture_audio_start_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_capture_audio_start object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_capture_audio_start_create(vx_req_aux_capture_audio_start_t ** req);
#endif

/**
* Used to stop audio capture immediately.
* If no audio files were being played back, the method returns immediately.
*
* The XML for this request may be found here: \ref Aux_CaptureAudioStop_1
*
* \see vx_resp_aux_capture_audio_stop
* \ingroup devices
*/
typedef struct vx_req_aux_capture_audio_stop {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
} vx_req_aux_capture_audio_stop_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_capture_audio_stop object
 * \ingroup devices
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_capture_audio_stop_create(vx_req_aux_capture_audio_stop_t ** req);
#endif

/**
* Gets the list of fonts eligible for use in a session by the current user
*
* The XML for this request may be found here: \ref Account_GetSessionFonts_1
*
* \see vx_resp_account_get_session_fonts
* \ingroup voicefonts
*/
typedef struct vx_req_account_get_session_fonts {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
} vx_req_account_get_session_fonts_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_get_session_fonts object
 * \ingroup voicefonts
 */
VIVOXSDK_DLLEXPORT int vx_req_account_get_session_fonts_create(vx_req_account_get_session_fonts_t ** req);
#endif

/**
* Gets the list of fonts the user can "try", including all user fonts and free root fonts.
*
* The XML for this request may be found here: \ref Account_GetTemplateFonts_1
*
* \see vx_resp_account_get_template_fonts
* \ingroup voicefonts
*/
typedef struct vx_req_account_get_template_fonts {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
} vx_req_account_get_template_fonts_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_get_template_fonts object
 * \ingroup voicefonts
 */
VIVOXSDK_DLLEXPORT int vx_req_account_get_template_fonts_create(vx_req_account_get_template_fonts_t ** req);
#endif

/**
* This request will start audio capture from the selected audio capture device.
* Audio is captured to the SDK internal capture buffer until the vx_req_aux_capture_audio_stop command is issued,
* or 10 minutes, whichever is earlier.
*
* The XML for this request may be found here: \ref Aux_StartBufferCapture_1
*
* \see vx_resp_aux_start_buffer_capture
* \ingroup adi
*/
typedef struct vx_req_aux_start_buffer_capture {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
} vx_req_aux_start_buffer_capture_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_start_buffer_capture object
 * \ingroup adi
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_start_buffer_capture_create(vx_req_aux_start_buffer_capture_t ** req);
#endif

/**
* Plays a buffer from memory captured with the vx_req_aux_start_buffer_capture command.
* Use vx_req_aux_render_audio_stop to terminate playback.
*
* The XML for this request may be found here: \ref Aux_PlayAudioBuffer_1
*
* \see vx_resp_aux_play_audio_buffer
* \ingroup adi
*/
typedef struct vx_req_aux_play_audio_buffer {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
    * The ID of the template font to apply to this buffer.  0 = none.
    */
    int template_font_id;
    /**
    * Fhe font delta to apply to the chosen template font.
    * Will be ignored if no template_font_id is supplied.  Leave null to play font without changes.
    */
    char* font_delta;
} vx_req_aux_play_audio_buffer_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_play_audio_buffer object
 * \ingroup adi
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_play_audio_buffer_create(vx_req_aux_play_audio_buffer_t ** req);
#endif

/**
 * Used to monitor the keyboard and mouse buttons while the application does not have focus.
 * When the vx_req_aux_global_monitor_keyboard_mouse_t request is issued to the SDK, the SDK will then raise
 * events of type vx_evt_keyboard_mouse_t when a key/mouse button combination has been pressed or released.
 * This only supports the first three mouse buttons.
 * The request may be issued multiple times for different combinations of key codes.
 *
 * Note: on Apple OS X, "Enable Access for Assistive Devices" must be on for this feature to work. This can
 * be found on the System Preferences/Universal Access Panel.
 *
 * The XML for this request may be found here: \ref Aux_GlobalMonitorKeyboardMouse_1
 *
 * \see vx_resp_aux_global_monitor_keyboard_mouse
 * \ingroup devices
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
typedef struct vx_req_aux_global_monitor_keyboard_mouse {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * An application defined name for the binding.
     * This name will be returned in the vx_evt_keyboard_mouse_t event when the key combination is pressed or released.
     */
    char * name;
    /**
     * The number of valid key codes in the codes member.
     * If this is zero, the binding for that name is cleared, and no more events will be received for that binding.
     */
    int code_count;
    /**
     * The key codes that must be pressed to activate this binding.
     */
    int codes[10];
} vx_req_aux_global_monitor_keyboard_mouse_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_global_monitor_keyboard_mouse object
 * \ingroup devices
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_global_monitor_keyboard_mouse_create(vx_req_aux_global_monitor_keyboard_mouse_t ** req);
#endif

/**
 * Used to monitor the keyboard and mouse to detect when a user has been idle for a particular period of time.
 * To do this, issue a request of type vx_req_aux_set_idle_timeout_t.
 * When this request has been issued, the SDK will raise an event of type vx_evt_idle_state_changed when the user
 * transitions to and from an idle state.
 *
 * The XML for this request may be found here: \ref Aux_SetIdleTimeout_1
 *
 * \see vx_resp_aux_set_idle_timeout
 * \ingroup devices
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 * \attention Mouse Movement events will not trigger the exit from idle condition
 */
typedef struct vx_req_aux_set_idle_timeout {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * The number of contiguous seconds without keyboard or mouse activity that must occur in
     * order for the idle state to change to "1" (idle).
     */
    int seconds;
} vx_req_aux_set_idle_timeout_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_set_idle_timeout object
 * \ingroup devices
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_set_idle_timeout_create(vx_req_aux_set_idle_timeout_t ** req);
#endif

/**
* Credentials for creating Vivox Accounts
*
* \attention Not supported on the PLAYSTATION(R)3 platform
* \attention Not supported on the iPhone mobile digital device platform
*/
typedef struct {
    /**
     * Currently unsupported. Must be null or empty
     */
    char *admin_username;
    /**
     * Currently unsupported. Must be null or empty
     */
    char *admin_password;
    /**
     * A signed XML grant document
     */
    char *grant_document;
    /**
     * The url of the Vivox account management server.
     */
    char *server_url;
} vx_generic_credentials;

/**
 * DEPRECATED.
 * Used to create a new user account
 *
 * The XML for this request may be found here: \ref Aux_CreateAccount_1
 *
 * \see vx_resp_aux_create_account
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 * \deprecated
 */
typedef struct vx_req_aux_create_account {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * The credentials to use to create the account
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    vx_generic_credentials credentials;
    /**
     * The user name
     */
    char *user_name;
    /**
     * The password
     */
    char *password;
    /**
     * The user's e-mail adddress.
     */
    char *email;
    /**
     * RESERVED FOR FUTURE USE
     */
    char *number;
    /**
     * RESERVED FOR FUTURE USE - The display name to use in the Vivox portal for this user.
     */
    char *displayname;
    /**
     * This first name to use for the user in the Vivox Portal.
     */
    char *firstname;
    /**
     * This first name to use for the user in the Vivox Portal.
     */
    char *lastname;
    /**
     * RESERVED FOR FUTURE USE - The user's phone number
     */
    char *phone;
    /**
     * RESERVED FOR FUTURE USE - The user's primary language
     */
    char *lang;
    /**
     * RESERVED FOR FUTURE USE - The user's age at the time of account creation
     */
    char *age;
    /**
     * RESERVED FOR FUTURE USE - The user's gender
     */
    char *gender;
    /**
     * RESERVED FOR FUTURE USE - The users's timezone
     */
    char *timezone;
    /**
     * RESERVED FOR FUTURE USE
     */
    char *ext_profile;
    /**
     * RESERVED FOR FUTURE USE
     */
    char *ext_id;
} vx_req_aux_create_account_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_create_account object
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_create_account_create(vx_req_aux_create_account_t ** req);
#endif

/**
 * DEPRECATED.
 * Used to reactivate an account that has been deactivated.
 *
 * The XML for this request may be found here: \ref Aux_ReactivateAccount_1
 *
 * \see vx_resp_aux_reactivate_account
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 * \deprecated
 */
typedef struct vx_req_aux_reactivate_account {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * The credentials to use to reactivate
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    vx_generic_credentials credentials;
    /**
     * The user name to reactivate
     */
    char *user_name;
} vx_req_aux_reactivate_account_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_create_account object
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_reactivate_account_create(vx_req_aux_reactivate_account_t ** req);
#endif

/**
 * DEPRECATED.
 * Used to deactivate an account.
 *
 * The XML for this request may be found here: \ref Aux_DeactivateAccount_1
 *
 * \see vx_resp_aux_deactivate_account
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 *
 * \deprecated
 */
typedef struct vx_req_aux_deactivate_account {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * The credentials to use to reactivate
     *
     * \attention Not supported on the PLAYSTATION(R)3 platform
     * \attention Not supported on the iPhone mobile digital device platform
     */
    vx_generic_credentials credentials;
    /**
     * The user name to reactivate
     */
    char *user_name;
} vx_req_aux_deactivate_account_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_create_account object
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_deactivate_account_create(vx_req_aux_deactivate_account_t ** req);
#endif

/**
 * Used to post crash dump data to the server
 *
 * This command is not application to the XML interface.
 *
 * \see vx_resp_account_post_crash_dump
 * \ingroup diagnostics
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
typedef struct vx_req_account_post_crash_dump {
    /**
     * The common properties for all requests
     */
    vx_req_base_t base;
    /**
     * The account to be used when posting the crash dump
     */
    VX_HANDLE account_handle;
    /**
     * The base64 encoded crash dump
     */
    char *crash_dump;
} vx_req_account_post_crash_dump_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_post_crash_dump object
 * \ingroup diagnostics
 *
 * \attention Not supported on PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_account_post_crash_dump_create(vx_req_account_post_crash_dump_t ** req);
#endif

/**
 * Used to reset a user's password.
 *
 * The XML for this request may be found here: \ref Aux_ResetPassword_1
 *
 * \see vx_resp_aux_reset_password
 */
typedef struct vx_req_aux_reset_password {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * The uri of the account to reset the password for
     */
    char *user_uri;
    /**
     * The email address associated with the account to reset the password for
     */
    char *user_email;
    /**
     * The server to perform the password reset on
     */
    char *server_url;
} vx_req_aux_reset_password_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_reset_password object
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_reset_password_create(vx_req_aux_reset_password_t ** req);
#endif

/**
 * Returns the SDK's current state.
 *
 * Used to get the current state of the SDK, this is used to get the initial state when
 * connecting to an out of process Vivox SDK that is already running.
 *
 * \see vx_resp_aux_diagnostic_state_dump
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
typedef struct vx_req_aux_diagnostic_state_dump {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
    * The level of detail to return
    */
    vx_diagnostic_dump_level level;
} vx_req_aux_diagnostic_state_dump_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_diagnostic_state_dump object
 *
 * \attention Not supported on the PLAYSTATION(R)3 platform
 * \attention Not supported on the iPhone mobile digital device platform
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_diagnostic_state_dump_create(vx_req_aux_diagnostic_state_dump_t ** req);
#endif


/**
 * Submits a generic web request to the Vivox account management server
 *
 * \see vx_resp_account_web_call
 */
typedef struct vx_req_account_web_call {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_account_login request
     */
    VX_HANDLE account_handle;
    /**
     * Relative path to original account management url
     *
     * eg: viv_chan_mod.php
     */
    char *relative_path;
    /**
     * Number of parameters
     */
    int parameter_count;
    /**
     * The parameters for the web call
     */
    vx_name_value_pairs_t parameters;
} vx_req_account_web_call_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_web_call object
 */
VIVOXSDK_DLLEXPORT int vx_req_account_web_call_create(vx_req_account_web_call_t ** req);
#endif


/**
 * Retrieve network related statistics for the call associated with the specified sessiongroup
 *
 * \see vx_resp_sessiongroup_get_stats
 */
typedef struct vx_req_sessiongroup_get_stats {
    /**
     * The common properties for all requests.
     */
    vx_req_base_t base;
    /**
     * Handle returned from successful vx_req_sessiongroup_create request
     */
    VX_HANDLE sessiongroup_handle;
    /**
     * Reset Stats - 1 to reset, 0 to let aggregrate (default 0)
     */
    int reset_stats;
} vx_req_sessiongroup_get_stats_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_sessiongroup_get_stats object
 *
 */
VIVOXSDK_DLLEXPORT int vx_req_sessiongroup_get_stats_create(vx_req_sessiongroup_get_stats_t ** req);
#endif


/**
* Used to send a message to a particular user.
*
* The XML for this request may be found here: \ref Account_SendMessage_1
*
* \see vx_resp_account_send_message
* \ingroup account
*/
typedef struct vx_req_account_send_message {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful login request
    */
    VX_HANDLE account_handle;
    /**
     * The destination uri
     */
    char *user_uri;
    /**
    * HTTP Content type of the message.
    * If an empty string is passed in this field will default to "text/plain".  This field
    * will be validated so it must be in a valid format.  For a full description of valid
    * content types please see RFC 2046 (http://www.ietf.org/rfc/rfc2046.txt).
    */
    char* message_header;
    /**
    * The contents of the message
    */
    char* message_body;
    /**
     * DEPRECATED
     * @deprecated
     */
    char* alias_username;
} vx_req_account_send_message_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_session_send_message object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_account_send_message_create(vx_req_account_send_message_t ** req);
#endif



/**
* Used to notify the SDK when important application level changes occur.
*
* The XML for this request may be found here: \ref Aux_NotifyApplicationStateChange_1
*
* \see vx_resp_aux_notify_application_state_change
*/
typedef struct vx_req_aux_notify_application_state_change {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * type of state change
    */
    vx_application_state_notification_type_t notification_type;
} vx_req_aux_notify_application_state_change_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_aux_notify_application_state_change object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_aux_notify_application_state_change_create(vx_req_aux_notify_application_state_change_t ** req);
#endif


/**
* Used to manage communications between users
*
* The XML for this request may be found here: \ref Account_ControlCommunications_1
*
* \see vx_resp_account_control_communications
* \ingroup session
*/
typedef struct vx_req_account_control_communications {
    /**
    * The common properties for all requests
    */
    vx_req_base_t base;
    /**
    * Handle returned from successful vx_req_account_login request
    */
    VX_HANDLE account_handle;
    /**
     * The operation to perform - block/unblock/list/clear
     * Note that this a bidirectional block, and that it should not be used in conjunction with vx_req_session_set_participant_mute_for_me (results will be unpredictable)
     */
    vx_control_communications_operation operation;
    /**
     * The list of linefeed separated users to block or unblock.
     * this should be NULL for operation of types vx_control_communications_operation_list or vx_control_communciations_operation_clear
     */
    char *user_uris;
} vx_req_account_control_communications_t;
#ifndef VIVOX_TYPES_ONLY
/**
 * Used to allocate and initialize a vx_req_account_control_communications object
 * \ingroup session
 */
VIVOXSDK_DLLEXPORT int vx_req_account_control_communications_create(vx_req_account_control_communications_t ** req);
#endif


#ifndef VIVOX_TYPES_ONLY
/**
 * Used to destroy a request of any type.
 *
 * \ingroup memorymanagement
 */
VIVOXSDK_DLLEXPORT int destroy_req(vx_req_base_t *pCmd);
#endif

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
