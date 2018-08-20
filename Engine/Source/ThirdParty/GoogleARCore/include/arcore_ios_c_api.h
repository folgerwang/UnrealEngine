#ifndef IPHONE_SHARED_ARCORE_GOOGLEAR_ARCORE_IOS_C_API_H_
#define IPHONE_SHARED_ARCORE_GOOGLEAR_ARCORE_IOS_C_API_H_

/**
 * C API for ARCore iOS. The C API here wraps the Objective-C API in
 * GARSession.h, GARFrame.h and GARAnchor.h and tries to provide the same
 * function signatures as the ARCore C API on Android.
 */

#include <stdint.h>

typedef struct ArSession_ ArSession;
typedef struct ArFrame_ ArFrame;
typedef struct ArPose_ ArPose;
typedef struct ArAnchorList_ ArAnchorList;
typedef struct ArAnchor_ ArAnchor;

typedef struct ARKitFrame_ ARKitFrame;
typedef struct ARKitAnchor_ ARKitAnchor;

// If compiling for C++11, use the 'enum underlying type' feature to enforce
// size for ABI compatibility. In pre-C++11, use int32_t for fixed size.
#if __cplusplus >= 201100
#define AR_DEFINE_ENUM(_type) enum _type : int32_t
#else
#define AR_DEFINE_ENUM(_type) \
typedef int32_t _type;      \
enum
#endif

/// Possible return values for API functions. The enum value here matches the
/// the value in ARCore C API except the error codes that are iOS specific.
AR_DEFINE_ENUM(ArStatus){
    AR_SUCCESS = 0,
    AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE = -101,
    AR_ERROR_INVALID_ARGUMENT = -1,
    AR_ERROR_NOT_TRACKING = -5,
    AR_ERROR_ANCHOR_NOT_SUPPORTED_FOR_HOSTING = -16};

/// Same as ARCore C API tracking state.
AR_DEFINE_ENUM(ArTrackingState){
    /// The object is currently tracked and its pose is current.
    AR_TRACKING_STATE_TRACKING = 0,
    
    /// ARCore has paused tracking this object, but may resume tracking it in
    /// the future. This can happen if device tracking is lost, if the user
    /// enters a new space, or if the Session is currently paused. When in this
    /// state, the positional properties of the object may be wildly inaccurate
    /// and should not be used.
    AR_TRACKING_STATE_PAUSED = 1,
    
    /// ARCore has stopped tracking this Trackable and will never resume
    /// tracking it.
    AR_TRACKING_STATE_STOPPED = 2};

/// This has the same values as ArCloudAnchorState from the Android C API,
/// as well as GARCloudAnchorState from the Objective-C API.
AR_DEFINE_ENUM(ArCloudAnchorState){
    /// The anchor is purely local. It has never been hosted using
    /// hostCloudAnchor, and has not been acquired using acquireCloudAnchor.
    AR_CLOUD_ANCHOR_STATE_NONE = 0,
    
    /// A hosting/resolving task for the anchor is in progress. Once the task
    /// completes in the background, the anchor will get a new cloud state after
    /// the next update() call.
    AR_CLOUD_ANCHOR_STATE_TASK_IN_PROGRESS = 1,
    
    /// A hosting/resolving task for this anchor completed successfully.
    AR_CLOUD_ANCHOR_STATE_SUCCESS = 2,
    
    /// A hosting/resolving task for this anchor finished with an internal
    /// error. The app should not attempt to recover from this error.
    AR_CLOUD_ANCHOR_STATE_ERROR_INTERNAL = -1,
    
    /// The app cannot communicate with the ARCore Cloud because of an invalid
    /// or unauthorized API key in the manifest, or because there was no API key
    /// present in the manifest.
    AR_CLOUD_ANCHOR_STATE_ERROR_NOT_AUTHORIZED = -2,
    
    /// The ARCore Cloud was unreachable. This can happen because of a number of
    /// reasons. The request sent to the server could have timed out with no
    /// response, there could be a bad network connection, DNS unavailability,
    /// firewall issues, or anything that could affect the device's ability to
    /// connect to the ARCore Cloud.
    AR_CLOUD_ANCHOR_STATE_ERROR_SERVICE_UNAVAILABLE = -3,
    
    /// The application has exhausted the request quota allotted to the given
    /// API key. The developer should request additional quota for the ARCore
    /// Cloud for their API key from the Google Developers Console.
    AR_CLOUD_ANCHOR_STATE_ERROR_RESOURCE_EXHAUSTED = -4,
    
    /// Hosting failed, because the server could not successfully process the
    /// dataset for the given anchor. The developer should try again after the
    /// device has gathered more data from the environment.
    AR_CLOUD_ANCHOR_STATE_ERROR_HOSTING_DATASET_PROCESSING_FAILED = -5,
    
    /// Resolving failed, because the ARCore Cloud could not find the provided
    /// cloud anchor ID.
    AR_CLOUD_ANCHOR_STATE_ERROR_CLOUD_ID_NOT_FOUND = -6,
    
    /// The server could not match the visual features provided by ARCore
    /// against the localization dataset of the requested cloud anchor ID. This
    /// means that the anchor pose being requested was likely not created in the
    /// user's surroundings.
    AR_CLOUD_ANCHOR_STATE_ERROR_RESOLVING_LOCALIZATION_NO_MATCH = -7,
    
    /// The anchor could not be resolved because the SDK used to host the anchor
    /// was newer than and incompatible with the version being used to acquire
    /// it.
    AR_CLOUD_ANCHOR_STATE_ERROR_RESOLVING_SDK_VERSION_TOO_OLD = -8,
    
    /// The anchor could not be acquired because the SDK used to host the anchor
    /// was older than and incompatible with the version being used to acquire
    /// it.
    AR_CLOUD_ANCHOR_STATE_ERROR_RESOLVING_SDK_VERSION_TOO_NEW = -9};

#ifdef __cplusplus
extern "C" {
#endif
    
    // === ArSession methods ===
    
    /// Constructs a new ArSession. Multiple instances may be created, although this
    /// is not recommended.
    ///
    /// @param api_key             Your API key for the anchor service. Must be a
    ///                            non-empty 0-terminated string.
    /// @param bundle_identifier   The bundle identifier registered with your API
    ///                            key. If NULL, defaults to the value of
    ///                            [[NSBundle mainBundle] bundleIdentifier]. Must be
    ///                            a 0-terminated string or NULL.
    /// @param out_session_pointer Out parameter for new ArSession.
    /// @return AR_SUCCESS on success. Possible error values:
    ///         AR_UNAVAILABLE_DEVICE_NOT_COMPATIBLE - this device is not supported.
    ///         AR_ERROR_INVALID_ARGUMENT - invalid API key or NULL out param.
    ArStatus ArSession_create(const char* api_key, const char* bundle_identifier,
                              ArSession** out_session_pointer);
    
    /// Release resources used by an ARCore iOS session.
    void ArSession_destroy(ArSession* session);
    
    /// Report engine usage for analytics.
    ///
    /// @param session        The ARCore iOS session.
    /// @param engine_type    The engine type, e.g. 'Unity'.
    /// @param engine_version The engine version string.
    void ArSession_reportEngineType(ArSession* session, const char* engine_type,
                                    const char* engine_version);
    
    /// Feeds an ARKitFrame to the ArSession and gets back the corresponding
    /// ArFrame. This may be called inside an update loop method. It is highly
    /// recommended to run at >= 30fps, with best results when matching ARKit's
    /// frame rate. If the ARKitFrame passed in is the same as the previous one, the
    /// same instance of ArFrame will be returned (however, you will have to release
    /// it again).
    ///
    /// @param session       The ARCore iOS session
    /// @param arkit_frame   ARKitFrame to feed to the ArSession.
    /// @param out_ar_frame  Out param for returned ArFrame.
    /// @return AR_SUCCESS on success. Possible error values:
    ///         AR_ERROR_INVALID_ARGUMENT - one of the arguments is NULL.
    ///         AR_ERROR_IOS_FRAME_OUT_OF_ORDER - the frame is older than a
    /// previously- passed-in frame. Should not occur if calling synchronously from
    ///             update loop method or ARSessionDelegate didUpdateFrame: method,
    ///             and not calling from anywhere else.
    ArStatus ArSession_updateAndAcquireArFrame(ArSession* session,
                                               ARKitFrame* arkit_frame,
                                               ArFrame** out_ar_frame);
    
    /// Hosts an ARKitAnchor and acquires the resulting new ArAnchor.
    ///
    /// @param session          The ARCore iOS session
    /// @param arkit_anchor     The ARKit anchor to host.
    /// @param out_cloud_anchor Out param for returned ArAnchor.
    /// @return AR_SUCCESS on success. Possible error values:
    ///         AR_ERROR_INVALID_ARGUMENT - one of the arguments is NULL.
    ///         AR_ERROR_NOT_TRACKING - bad current tracking state.
    ArStatus ArSession_hostAndAcquireNewCloudAnchor(ArSession* session,
                                                    ARKitAnchor* arkit_anchor,
                                                    ArAnchor** out_cloud_anchor);
    
    /// Resolves a cloud anchor and acquires the resulting new ArAnchor. If
    /// resolving fails, the anchor will be automatically removed from the session
    /// and its tracking state will be set to AR_TRACKING_STATE_STOPPED.
    ///
    /// @param session          The ARCore iOS session
    /// @param cloud_anchor_id  The cloud anchor identifier. Must be a non-empty
    ///                         0-terminated string.
    /// @param out_cloud_anchor Out param for returned GARAnchor.
    /// @return AR_SUCCESS on success. Possible error values:
    ///         AR_ERROR_INVALID_ARGUMENT - one of the arguments is NULL, or
    ///             identifier is the empty string.
    ///         AR_ERROR_NOT_TRACKING - bad current tracking state.
    ArStatus ArSession_resolveAndAcquireNewCloudAnchor(ArSession* session,
                                                       const char* cloud_anchor_id,
                                                       ArAnchor** out_cloud_anchor);
    
    /// Returns all known ARCore anchors. Anchors forgotten by ARCore due to a call
    /// to ArAnchor_detach() or entering the STOPPED state will not be returned.
    ///
    /// @param[in]    session         The ARCore iOS session
    /// @param[inout] out_anchor_list The list to fill.  This list must have already
    ///     been allocated with ArAnchorList_create().  If previously used, the list
    ///     will first be cleared.
    void ArSession_getAllAnchors(const ArSession* session,
                                 ArAnchorList* out_anchor_list);
    // === ArFrame methods ===
    
    /// Gets the timestamp of the ArFrame. This is equal to the timestamp of the
    /// corresponding ARFrame but converted to nanoseconds.
    ///
    /// @param session          The ARCore iOS session
    /// @param frame            The ArFrame acquired from
    /// ArSession_updateAndAcquireArFrame().
    /// @param out_timestamp_ns Out param for returned timestamp in nanosecond.
    ///                         Defaults to 0 if frame is NULL.
    void ArFrame_getTimestamp(const ArSession* session, const ArFrame* frame,
                              int64_t* out_timestamp_ns);
    
    /// Release the acquired ArFrame from ArSession_updateAndAcquireArFrame.
    void ArFrame_release(ArFrame* frame);
    
    // === ARKitAnchor methods ===
    /// Create an ARKitAnchor using an ArPose.
    void ARKitAnchor_create(const ArPose* pose, ARKitAnchor** out_arkit_anchor);
    
    /// Release the ARKitAnchor created by ARKitAnchor_create.
    void ARKitAnchor_release(ARKitAnchor* out_arkit_anchor);
    // === ArAnchorList methods ===
    
    /// Creates an anchor list object.
    void ArAnchorList_create(const ArSession* session,
                             ArAnchorList** out_anchor_list);
    
    /// Releases the memory used by an anchor list object, along with all the anchor
    /// references it holds.
    void ArAnchorList_destroy(ArAnchorList* anchor_list);
    
    /// Retrieves the number of anchors in this list.
    void ArAnchorList_getSize(const ArSession* session,
                              const ArAnchorList* anchor_list, int32_t* out_size);
    
    /// Acquires a reference to an indexed entry in the list.  This call must
    /// eventually be matched with a call to ArAnchor_release().
    void ArAnchorList_acquireItem(const ArSession* session,
                                  const ArAnchorList* anchor_list, int32_t index,
                                  ArAnchor** out_anchor);
    
    // === ArAnchor methods ===
    
    /// Retrieves the pose of the anchor in the world coordinate space. This pose
    /// produced by this call may change each time
    /// ArSession_updateAndAcquireArFrame() is called. This pose should only be used
    /// for rendering if ArAnchor_getTrackingState() returns
    /// #AR_TRACKING_STATE_TRACKING.
    ///
    /// @param[in]    session  The ARCore iOS session.
    /// @param[in]    anchor   The anchor to retrieve the pose of.
    /// @param[inout] out_pose An already-allocated ArPose object into which the
    ///     pose will be stored.
    
    void ArAnchor_getPose(const ArSession* session, const ArAnchor* anchor,
                          ArPose* out_pose);
    
    /// Retrieves the current state of the pose of this anchor.
    
    void ArAnchor_getTrackingState(const ArSession* session, const ArAnchor* anchor,
                                   ArTrackingState* out_tracking_state);
    
    /// Removes an anchor from the session. Recommended to prevent ongoing
    /// processing costs for anchors that are no longer needed. This function does
    /// nothing if either argument is NULL.
    ///
    /// @param session The ARCore iOS session to remove the anchor from.
    /// @param anchor  The ArAnchor to remove.
    ///
    void ArAnchor_detach(ArSession* session, ArAnchor* anchor);
    
    /// Releases a reference to an anchor. This does not mean that the anchor will
    /// stop tracking, as it will be obtainable from e.g. ArSession_getAllAnchors()
    /// if any other references exist.
    ///
    /// This method may safely be called with @c nullptr - it will do nothing.
    
    void ArAnchor_release(ArAnchor* anchor);
    
    /// Acquires the cloud anchor ID of the anchor. The ID acquired is an ASCII
    /// null-terminated string. The acquired ID must be released after use by the
    /// @c ArString_release function. For anchors with cloud state
    /// #AR_CLOUD_ANCHOR_STATE_NONE or #AR_CLOUD_ANCHOR_STATE_TASK_IN_PROGRESS, this
    /// will always be an empty string.
    ///
    /// @param[in]    session             The ARCore iOS session.
    /// @param[in]    anchor              The anchor to retrieve the cloud ID of.
    /// @param[inout] out_cloud_anchor_id A pointer to the acquired ID string.
    void ArAnchor_acquireCloudAnchorId(ArSession* session, ArAnchor* anchor,
                                       char** out_cloud_anchor_id);
    
    /// Gets the current cloud anchor state of the anchor. This state is guaranteed
    /// not to change until update() is called.
    ///
    /// @param[in]    session   The ARCore iOS session.
    /// @param[in]    anchor    The anchor to retrieve the cloud state of.
    /// @param[inout] out_state The current cloud state of the anchor.
    void ArAnchor_getCloudAnchorState(const ArSession* session,
                                      const ArAnchor* anchor,
                                      ArCloudAnchorState* out_state);
    
    // === ArPose methods ===
    
    /// Allocates and initializes a new pose object.  @c pose_raw points to an array
    /// of 7 floats, describing the rotation (quaternion) and translation of the
    /// pose in the same order.
    ///
    /// The order of the values is: qx, qy, qz, qw, tx, ty, tz.
    ///
    /// If @c pose_raw is null, initializes with the identity pose.
    
    void ArPose_create(const ArSession* session, const float* pose_raw,
                       ArPose** out_pose);
    
    /// Releases memory used by a pose object.
    
    void ArPose_destroy(ArPose* pose);
    
    /// Extracts the quaternion rotation and translation from a pose object.
    /// @param[in]  session       The ARCore iOS session.
    /// @param[in]  pose          The pose to extract
    /// @param[out] out_pose_raw  Pointer to an array of 7 floats, to be filled with
    ///     the quaternion rotation and translation as described in ArPose_create().
    
    void ArPose_getPoseRaw(const ArSession* session, const ArPose* pose,
                           float* out_pose_raw);
    
    /// Converts a pose into a 4x4 transformation matrix.
    /// @param[in]  session                  The ARCore iOS session.
    /// @param[in]  pose                     The pose to convert
    /// @param[out] out_matrix_col_major_4x4 Pointer to an array of 16 floats, to be
    ///     filled with a column-major homogenous transformation matrix, as used by
    ///     OpenGL.
    
    void ArPose_getMatrix(const ArSession* session, const ArPose* pose,
                          float* out_matrix_col_major_4x4);
    
    void ArString_release(char* str);
    
#ifdef __cplusplus
}
#endif

#endif  // IPHONE_SHARED_ARCORE_GOOGLEAR_ARCORE_IOS_C_API_H_
