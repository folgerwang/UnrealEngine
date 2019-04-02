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
#pragma once

#include <time.h>

#pragma pack(push)
#pragma pack(8)

#ifdef __cplusplus
extern "C" {
#endif

    /**
    * SDK Logging Levels
    */
    typedef enum {
        log_none = -1,
        /**
        * Errors only
        */
        log_error = 0,
        /**
        * Warnings only
        */
        log_warning = 1,
        /**
        * Generic Information
        */
        log_info = 2,
        /**
        * Detailed debugging information. Likely to have performance implications.
        */
        log_debug = 3,
        /**
        * The most verbose logging level. Likely to have performance implications.
        */
        log_trace = 4,
        /**
        * Log almost everything. Surely to have performance implications.
        */
        log_all = 5
    } vx_log_level;

    typedef enum { // type of the UDP packet
        vx_frame_type_rtp  = 0,
        vx_frame_type_rtcp = 1,
        vx_frame_type_sip_message   = 2,
        vx_frame_type_sip_keepalive = 3
    } vx_udp_frame_type;

    /**
    * Callback functions declaration in C99 style for oRTP and exosip libraries
    */
    /**
    * Called before any UDP frame is transmitted. This callback must be a non-blocking callback and it is recommended that this callback complete is less than 1 ms. 
    */
    typedef void (*pf_on_before_udp_frame_transmitted_t) ( 
        void*  callback_handle,  // the handle passed in the vx_sdk_config_t structure 
        vx_udp_frame_type    frame_type, // type of the UDP packet
        void*  payload_data,     // the data to be transmitted to the network 
        int    payload_data_len, // the len of that data 
        void** header_out,       // callback set - pointer to header data (NULL if no header) 
        int*   header_len_out,   // callback set - length of the header data (0 if no header) 
        void** trailer_out,      // callback set - pointer to trailer data (NULL if no trailer) 
        int*   trailer_len_out   // callback set - length of the trailer data (0 if no trailer) 
    );
        
    /**
    * Called after any UDP frame is transmitted. The application can use this callback to de-allocate the header and trailer if necessary. 
    */
    typedef void (*pf_on_after_udp_frame_transmitted_t) ( 
        void* callback_handle,  // the handle passed in the vx_sdk_config_t structure 
        vx_udp_frame_type   frame_type, // type of the UDP packet
        void* payload_data,     // the data to be transmitted to the network 
        int   payload_data_len, // the len of that data 
        void* header,           // the header data passed in pf_on_before_udp_frame_transmitted 
        int   header_len,       // length of the header data 
        void* trailer,          // the trailer data passed in pf_on_before_udp_frame_transmitted 
        int   trailer_len,      // length of the trailer data 
        int   sent_bytes        // the total number of bytes transmitted - < 0 indicates error 
    );

    /**
    * Called after thread is created. The application can use this callback to monitor and profile thread creation.
    */
    typedef void (*pf_on_thread_created_t)(void *callback_handle, const char *thread_name);

    /**
    * Called before thread is destructed. The application can use this callback to monitor and profile thread destruction.
    */
    typedef void (*pf_on_thread_exit_t)(void *callback_handle);

    /**
    * Configuration Options passed to vx_initialize3()
    * \ingroup initialization
    */
    typedef struct vx_sdk_config {
        /**
        * Number of threads used for encoding/decoding audio. Must be 1 for client SDKs.
        *
        */
        int num_codec_threads;

        /**
        * Number of threads used for voice processing. Must be 1 for client SDKs.
        */
        int num_voice_threads;

        /**
        * Number of threads used for web requests. Must be 1 for client SDKs.
        */
        int num_web_threads;

        /**
        * Render Source Max Queue Depth.
        */
        int render_source_queue_depth_max;

        /**
        * Render Source Initial Buffer Count.
        */
        int render_source_initial_buffer_count;

        /**
        * Upstream jitter frame count
        */
        int upstream_jitter_frame_count;

        /**
        * allow shared capture devices (shared in the Vivox context only).
        */
        int allow_shared_capture_devices;

        /**
        * max logins per user
        */
        int max_logins_per_user;

        /**
        * three letter app id;
        * Do not set this value, or contact your Vivox representative for more information.
        */
        char app_id[3];

        /**
        * Certificate data directory -- where cert. bundle is located.
        */
        char cert_data_dir[256];
        /**
        * Pointer to a function used to allocate memory
        */
        void * (*pf_malloc_func)(size_t bytes);
        /**
        * Pointer to a function used to free memory
        */
        void(*pf_free_func) (void* memory);
        /**
        * Pointer to a function used to realloc memory
        */
        void * (*pf_realloc_func)(void * memory, size_t bytes);
        /**
        * Pointer to a function used to allocate zeroed out memory
        */
        void * (*pf_calloc_func)(size_t num, size_t bytes);
        /**
        * Pointer to a function used to allocate aligned memory
        */
        void *(*pf_malloc_aligned_func)(size_t alignment, size_t size);
        /**
        * Pointer to a function used to free aligned memory
        */
        void(*pf_free_aligned_func)(void *memory);
        /**
        * 1 to use the pooled allocator (default 0)
        */
        int use_pooled_allocator;
        /**
        * handle for use in SPURS job queue functions (below - PS/3 only)
        */
        void *job_queue_handle;
        /**
        * function to queue job to SPURS synchronously (PS/3 only)
        * the job argument is really a pointer CellSpursJobHeader structure
        */
        int(*pf_queue_job_sync)(void *job_queue_handle, void *job, size_t size);
        /**
        * function to queue job to SPURS asynchronously (PS/3 only)
        * the job argument is really a pointer CellSpursJobHeader structure
        */
        int(*pf_queue_job_async)(void *job_queue_handle, void *job, size_t size);
        /**
        * Processor Affinity Mask for SDK Threads (XB360, XB1, and PS4)
        * On PS4 - this is the processor affinity mask passed to scePthreadSetaffinity(). The default is to not call scePthreadSetaffinity().
        * On XB360 - this is the dwHardwareThread passed to XSetThreadProcessor(). The default is to call XSetThreadProcessor(3).
        * On XB1 - this is the processor affinity mask passed to SetThreadAffinityMask(). The default is to not call SetThreadAffinityMask().
        */
        long long processor_affinity_mask;
        /**
        * Callback Handle for message and logging notifications
        */
        void *callback_handle;
        /**
        * Logging Callback
        */
        void(*pf_logging_callback)(void *callback_handle, vx_log_level level, const char *source, const char *message);
        /**
        * SDK Message Callback - when this is called, call vx_get_message() until there are no more messages
        */
        void(*pf_sdk_message_callback)(void *callback_handle);
        /**
        * Initial Log Level
        */
        vx_log_level initial_log_level;
        /**
        * Disable Audio Device Polling Using Timer
        */
        int disable_device_polling;
        /**
        * Diagnostic purposes only.
        */
        int force_capture_silence;
        /**
        * Enable advanced automatic settings of audio levels
        */
        int enable_advanced_auto_levels;

        /**
        * Called when an audio processing unit is started, from the audio processing thread.
        * No blocking operations should occur on this callback;
        */
        void(*pf_on_audio_unit_started)(void *callback_handle, const char *session_group_handle, const char *initial_target_uri);

        /**
        * Called when an audio processing unit is stopped, from the audio processing thread.
        * No blocking operations should occur on this callback;
        */
        void(*pf_on_audio_unit_stopped)(void *callback_handle, const char *session_group_handle, const char *initial_target_uri);

        /**
        * Called right after audio was read from the capture device
        * No blocking operations should occur on this callback;
        */
        void(*pf_on_audio_unit_after_capture_audio_read)(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame);

        /**
        * Called when an audio processing unit is about to send captured audio to the network, from the audio processing thread.
        * No blocking operations should occur on this callback;
        */
        void(*pf_on_audio_unit_before_capture_audio_sent)(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int is_speaking);

        /**
        * Called when an audio processing unit is about to write received audio to the render device, from the audio processing thread.
        * No blocking operations should occur on this callback;
        */
        void(*pf_on_audio_unit_before_recv_audio_rendered)(void *callback_handle, const char *session_group_handle, const char *initial_target_uri, short *pcm_frames, int pcm_frame_count, int audio_frame_rate, int channels_per_frame, int is_silence);

        /**
        * Number of 20 millisecond buffers for the capture device.
        */
        int capture_device_buffer_size_intervals;

        /**
        * Number of 20 millisecond buffers for the render device.
        */
        int render_device_buffer_size_intervals;

        /**
        * XBox One, Windows, and iOS.
        */
        int disable_audio_ducking;
        /**
        * Vivox Access Tokens (VAT) provide a more scalable, usable, and extensible replacement for the use of
        * Access Control Lists to control access to Vivox resources. This security token is generated by the
        * game server and then validated by the Vivox system to authorize certain Vivox operations at the time
        * that those operations are to be performed.
        */
        int use_access_tokens;

        /**
         * Set this to 1 if use_access_tokens is 1, and multiparty text is being used. This can also be controlled by setting
         * the VIVOX_ENABLE_MULTIPARTY_TEXT environment variable.
         */
        int enable_multiparty_text;

        /**
         * Default of 1 for most platforms. Changes to this value must be coordinated with Vivox.
         */
        int enable_dtx;
        
        /**
         * Default codec mask that will be used to initialize connector's configured_codecs.
         */
        unsigned int default_codecs_mask;
        
        /**
        * Called before any UDP frame is transmitted. This callback must be a non-blocking callback and it is recommended that this callback complete is less than 1 ms. 
        */
        pf_on_before_udp_frame_transmitted_t pf_on_before_udp_frame_transmitted;
        
        /**
        * Called after any UDP frame is transmitted. The application can use this callback to de-allocate the header and trailer if necessary. 
        */
        pf_on_after_udp_frame_transmitted_t pf_on_after_udp_frame_transmitted;

        /**
         * Enable Fast Network Change Detection. Default of 0.
         */
        int enable_fast_network_change_detection;

        /**
         * Use Operating System Configured Proxy Settings (Windows Only) (default: 0 or 1 if environment variable "VIVOX_USE_OS_PROXY_SETTINGS" is set)
         */
        int use_os_proxy_settings;

        /**
         * Enable Persistent Connections (Windows Only) (default: 0 or 1 if environment variable "VIVOX_ENABLE_PERSISTENT_HTTP" is set
         * Note that the use of proxies may interfere with behavior controlled by this setting.
         * Please contact your developer support representative before changing this value.
         */
        int enable_persistent_http;
        /**
         * Don't use this parameter, it has no effect.
         *
         * preferred server SIP port - 0 means use the network configuration.
         * This is for development purposes only. This can be set by the environment variable "VIVOX_PREFERRED_SIP_PORT".
         * Please note that setting this to an incorrect value could result in delays in logging in or joining channels
         */
        int preferred_sip_port;
        
        /**
         * Don't use this parameter, it has no effect.
         *
         * By default, on iOS, when we set the audio session to the category
         * 'PlayAndRecord' the receiver (the tiny speaker you put your ear up to when using the phone for voice calls) is used.
         * We will move output to the phone speakers by default, unless this is set to 1.
         */
        int default_render_to_receiver;

        /**
        * Don't use this parameter, it has no effect.
        * For platforms with soft mics, apply linear gain before processing. In dB.
         */
        float mic_makeup_gain;

        /**
        * Called after thread is created. The application can use this callback to monitor and profile thread creation.
        */
        pf_on_thread_created_t pf_on_thread_created;

        /**
        * Called before thread is destructed. The application can use this callback to monitor and profile thread destruction.
        */
        pf_on_thread_exit_t pf_on_thread_exit;

		/**
		 * If set, the provided function is called by the SDK before socket operations are attempted. If not set, the SDK assumes permission.
		 * This function should return non-zero if a socket operation initiated by the SDK is allowed, otherwise it should return zero.
		 * This function must be thread-safe and complete as soon as possible.
		 *
		 * Currently this function is called only on specific platforms. Please contact Vivox for more information.
		 */
		int (*pf_request_permission_for_network)();
    } vx_sdk_config_t;

#ifdef __cplusplus
}
#endif

#pragma pack(pop)
