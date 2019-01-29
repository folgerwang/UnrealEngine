/**********************************************************************************
\file      OVR_Avatar.h
\brief     OVR Avatar SDK public header file
\copyright (c) Facebook Technologies, LLC and its affiliates.  All rights reserved.
***********************************************************************************/
#ifndef OVR_Avatar_h
#define OVR_Avatar_h

#include "OVR_Avatar_Defs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/// Opaque types
///
typedef uint64_t ovrAvatarAssetID;
typedef struct ovrAvatarMessage_ ovrAvatarMessage;
typedef struct ovrAvatarSpecification_ ovrAvatarSpecification;
typedef struct ovrAvatarAsset_ ovrAvatarAsset;
typedef struct ovrAvatar_ ovrAvatar;
typedef struct ovrAvatarRenderPart_ ovrAvatarRenderPart;
typedef struct ovrAvatarPacket_ ovrAvatarPacket;
typedef struct ovrAvatarSpecificationRequest _ovrAvatarSpecificationRequest;

/// The maximum joint count that can be in a skinned mesh renderer.
///
#define OVR_AVATAR_MAXIMUM_JOINT_COUNT 64

//Basic plugin management

/// Initialize the SDK.
/// \param appId the Oculus application ID.
///
OVRN_EXPORT (void) ovrAvatar_Initialize(const char* appId);

#ifdef __ANDROID__
#include <jni.h>
OVRN_EXPORT (void) ovrAvatar_InitializeAndroid(const char* appId, jobject activity, JNIEnv* jni);
OVRN_EXPORT (void) ovrAvatar_InitializeAndroidUnity(const char* appId);
#endif

/// Shut down the SDK.
///
OVRN_EXPORT (void) ovrAvatar_Shutdown();

//Message system

/// Tells you the type of message recieved.
///
typedef enum ovrAvatarMessageType_ {
    ovrAvatarMessageType_AvatarSpecification, ///< An avatar specification being returned
    ovrAvatarMessageType_AssetLoaded,         ///< An asset has finished loading
    ovrAvatarMessageType_Count
} ovrAvatarMessageType;


/// Avatar Logging Level
/// Matches the Android Log Levels
typedef enum ovrAvatarLogLevel_ {
	ovrAvatarLogLevel_Unknown = 0,
	ovrAvatarLogLevel_Default, 
	ovrAvatarLogLevel_Verbose,
	ovrAvatarLogLevel_Debug,
	ovrAvatarLogLevel_Info,
	ovrAvatarLogLevel_Warn,
	ovrAvatarLogLevel_Error,
	ovrAvatarLogLevel_Fatal,
	ovrAvatarLogLevel_Silent,

} ovrAvatarLogLevel;

typedef enum ovrAvatarAssetLevelOfDetail_ {
	ovrAvatarAssetLevelOfDetail_One = 1,
	ovrAvatarAssetLevelOfDetail_Three = 3,
	ovrAvatarAssetLevelOfDetail_Five = 5
} ovrAvatarAssetLevelOfDetail;

/// The avatar specification for the requested user ID.
///
typedef struct ovrAvatarMessage_AvatarSpecification_ {
	ovrAvatarSpecification* avatarSpec;   ///< The avatar specification
	uint64_t                oculusUserID; ///< The Oculus user ID associated with this specification
} ovrAvatarMessage_AvatarSpecification;

/// The asset that has finished loading.
///
typedef struct ovrAvatarMessage_AssetLoaded_ {
	ovrAvatarAssetID assetID; 			///< The asset id
	ovrAvatarAsset* asset;    			///< The opaque asset type
	ovrAvatarAssetLevelOfDetail lod; 	///< The level of detail of the asset
} ovrAvatarMessage_AssetLoaded;

/// Remove a message from the message queue if there are any.
/// \return the opaque message, or null if nothing is there
///
OVRN_EXPORT (ovrAvatarMessage*) ovrAvatarMessage_Pop();

/// Find the type of an opaque message.
/// \param msg the message
/// \return the message type
///
OVRN_EXPORT (ovrAvatarMessageType) ovrAvatarMessage_GetType(
	const ovrAvatarMessage* msg);

/// Get the avatar specification message contents.
/// \param msg the message
/// \return the avatar specification message contents
///
OVRN_EXPORT (const ovrAvatarMessage_AvatarSpecification*) ovrAvatarMessage_GetAvatarSpecification(
	const ovrAvatarMessage* msg);

/// Get the asset loaded message.
/// \param msg the message
/// \return the asset loaded message contents
///
OVRN_EXPORT (const ovrAvatarMessage_AssetLoaded*) ovrAvatarMessage_GetAssetLoaded(
	const ovrAvatarMessage* msg);

/// Free the message when you're done processing the contents.
/// \param msg the message
///
OVRN_EXPORT (void) ovrAvatarMessage_Free(
	const ovrAvatarMessage* msg);

/// Avatar capabilities mask
///
typedef enum ovrAvatarCapabilities_ {
	ovrAvatarCapability_Body = 1 << 0,  ///< This avatar will have a body and head
	ovrAvatarCapability_Hands = 1 << 1, ///< This avatar will have hands
	ovrAvatarCapability_Base = 1 << 2,  ///< This avatar will have a base cone
	ovrAvatarCapability_Voice = 1 << 3, ///< This avatar will have voice visualization
	ovrAvatarCapability_BodyTilt = 1 << 4, ///< This avatar will have a body translation with respect to the eyeOrientation
	ovrAvatarCapability_All = -1        ///< This avatar will have all available capabilities
}ovrAvatarCapabilities;

//Avatar creation and destruction

/// Create a new Specification Request
/// \param userID of the User spec to load
///
OVRN_EXPORT(ovrAvatarSpecificationRequest*) ovrAvatarSpecificationRequest_Create(
	uint64_t userID);

/// Destroy the Specification Request
/// \param specificationRequest, the specification request to be destroyed.
///
OVRN_EXPORT(void) ovrAvatarSpecificationRequest_Destroy(
	ovrAvatarSpecificationRequest* specificationRequest);

/// Set the Combine Meshes Flag on the spec
/// \param request to set the field
/// \param useCombinedMesh 
///
OVRN_EXPORT(void) ovrAvatarSpecificationRequest_SetCombineMeshes(
	ovrAvatarSpecificationRequest* request, 
	bool useCombinedMesh);


OVRN_EXPORT(void) ovrAvatarSpecificationRequest_SetLevelOfDetail(
	ovrAvatarSpecificationRequest* request,
	const ovrAvatarAssetLevelOfDetail lod);
/// Avatar Specification Request Functions

/// Request the avatar specification for a given user ID.
/// \param userID the Oculus user ID for the avatar
/// This will dispatch an async request for the avatar specification. When it's done, it will return via the message system. \see ovrAvatarMessage_AvatarSpecification
///
OVRN_EXPORT (void) ovrAvatar_RequestAvatarSpecification(
	uint64_t userID);

/// Request the avatar specification for a given specification request.
/// \param spec, the spec for the avatar you are requesting
/// This will dispatch an async request for the avatar specification. When it's done, it will return via the message system. \see ovrAvatarMessage_AvatarSpecification
///
OVRN_EXPORT(void) ovrAvatar_RequestAvatarSpecificationFromSpecRequest(
	ovrAvatarSpecificationRequest* spec);

/// Create an avatar.
/// \param avatarSpecification the avatar specification. \see ovrAvatar_RequestAvatarSpecification
/// \param capabilities the capabilities set for this avatar - these capabilities can be toggled on and off later with ovrAvatar_SetActiveCapabilities
/// \return an opaque pointer to the avatar state
/// \sa ovrAvatar_SetActiveCapabilities
/// \sa ovrAvatar_Destroy
///
OVRN_EXPORT (ovrAvatar*) ovrAvatar_Create(
	const ovrAvatarSpecification* avatarSpecification,
	ovrAvatarCapabilities capabilities);

/// Destroy an avatar when you no longer need it.
/// \param avatar an opaque pointer to the avatar state
/// \sa ovrAvatar_Create
///
OVRN_EXPORT (void) ovrAvatar_Destroy(ovrAvatar* avatar);

/// 3D Vector Type
///
typedef struct ovrAvatarVector3f_ {
	float x, y, z;
} ovrAvatarVector3f;

/// 4D Vector Type
///
typedef struct ovrAvatarVector4f_ {
	float x, y, z, w;
} ovrAvatarVector4f;

/// Quaternion Type
///
typedef struct ovrAvatarQuatf_ {
	float x, y, z, w;
} ovrAvatarQuatf;

/// Transform Type
///
typedef struct ovrAvatarTransform_ {
	ovrAvatarVector3f position;
	ovrAvatarQuatf orientation;
	ovrAvatarVector3f scale;
} ovrAvatarTransform;

/// Button Mask
///
typedef enum ovrAvatarButton_ {
	ovrAvatarButton_One = 0x0001,      ///< X/A pressed
	ovrAvatarButton_Two = 0x0002,      ///< Y/B pressed
	ovrAvatarButton_Three = 0x0004,    ///< Select/Oculus button pressed
	ovrAvatarButton_Joystick = 0x0008, ///< Joystick button pressed
} ovrAvatarButton;

/// Touch Mask
///
typedef enum ovrAvatarTouch_ {
	ovrAvatarTouch_One = 0x0001,       ///< Capacitive touch for X/A button
	ovrAvatarTouch_Two = 0x0002,       ///< Capacitive touch for Y/B button
	ovrAvatarTouch_Joystick = 0x0004,  ///< Capacitive touch for thumbstick
	ovrAvatarTouch_ThumbRest = 0x0008, ///< Capacitive touch for thumb rest
	ovrAvatarTouch_Index = 0x0010,     ///< Capacitive touch for index trigger
	ovrAvatarTouch_Pointing = 0x0040,  ///< Index finger is pointing
	ovrAvatarTouch_ThumbUp = 0x0080,   ///< Thumb is up
} ovrAvatarTouch;

/// Controller Types
///
typedef enum ovrAvatarControllerType_
{
	ovrAvatarControllerType_Touch,
	ovrAvatarControllerType_Malibu,
	ovrAvatarControllerType_Go,

	ovrAvatarControllerType_Count,
} ovrAvatarControllerType;

/// Look and Feel of Avatar Version
typedef enum ovrAvatarLookAndFeelVersion_
{
	ovrAvatarLookAndFeelVersion_Unknown = -1,
	ovrAvatarLookAndFeelVersion_One = 0,
	ovrAvatarLookAndFeelVersion_Two = 1,

} ovrAvatarLookAndFeelVersion;

/// Set the look and feel version for the avatar
/// \param request to set the field
/// \param version of avatar to request
OVRN_EXPORT(void) ovrAvatarSpecificationRequest_SetLookAndFeelVersion(
	ovrAvatarSpecificationRequest* request,
	const ovrAvatarLookAndFeelVersion version);

OVRN_EXPORT(void) ovrAvatarSpecificationRequest_SetFallbackLookAndFeelVersion(
	ovrAvatarSpecificationRequest* request,
	const ovrAvatarLookAndFeelVersion version);

/// Input state for touch controller
///
typedef struct ovrAvatarHandInputState_ {
	ovrAvatarTransform transform; ///< Transform for this controller
	uint32_t buttonMask;          ///< Flags mask specifying which buttons are currently pressed \sa ovrAvatarButton
	uint32_t touchMask;           ///< Flags mask specifying which portions of the controller are currently touched \sa ovrAvatarTouch
	float joystickX;              ///< x-axis position of the thumbstick
	float joystickY;              ///< y-axis position of the thumbstick
	float indexTrigger;           ///< current value of the index finger trigger
	float handTrigger;            ///< current value of the hand trigger (underneath the middle finger)
	bool isActive;                ///< true if the controller is currently active
} ovrAvatarHandInputState;

/// Update the pose for the body component of the avatar.
/// \param avatar an opaque pointer to the avatar state
/// \param headPose the transform of the center eye (half way between the two eyes)
///
OVRN_EXPORT (void) ovrAvatarPose_UpdateBody(ovrAvatar* avatar,
	                                      ovrAvatarTransform headPose);

/// Update the voice visualization component of the avatar.
/// \param avatar an opaque pointer to the avatar state
/// \param voiceSampleCount number of samples passed in
/// \param voiceSamples array of voice samples
///
typedef const float VOICE_SAMPLES_[];
OVRN_EXPORT (void) ovrAvatarPose_UpdateVoiceVisualization(
	ovrAvatar* avatar,
	uint32_t voiceSampleCount,
	VOICE_SAMPLES_);

/// Update the hand components of the avatar.
/// \param avatar an opaque pointer to the avatar state
/// \param inputStateLeft the input state of the left touch controller
/// \param inputStateRight the input state of the right touch controller
///
OVRN_EXPORT (void) ovrAvatarPose_UpdateHands(ovrAvatar* avatar,
                                           ovrAvatarHandInputState inputStateLeft,
                                           ovrAvatarHandInputState inputStateRight);

/// Update the hand components of the avatar.
/// \param avatar an opaque pointer to the avatar state
/// \param inputStateLeft the input state of a 3Dof controller left hand
/// \param inputStateRight the input state of the 3Dof controller right hand
/// \param type the type of 3dof controller being updated
///
OVRN_EXPORT(void) ovrAvatarPose_Update3DofHands(ovrAvatar* avatar,
	ovrAvatarHandInputState* inputStateLeft,
	ovrAvatarHandInputState* inputStateRight,
	ovrAvatarControllerType type);

/// Finalize the updates to the avatar's pose.
/// \param avatar an opaque pointer to the avatar state
/// \param deltaSeconds number of seconds that have elapsed since the last call to this function
///
OVRN_EXPORT (void) ovrAvatarPose_Finalize(ovrAvatar* avatar, float deltaSeconds);

//Showing controllers

/// Set the left controller's visibility.
/// \param avatar an opaque pointer to the avatar state
/// \param visible boolean controlling visibility
///
OVRN_EXPORT (void) ovrAvatar_SetLeftControllerVisibility(ovrAvatar* avatar, bool visible);

/// Set the right controller's visibility.
/// \param avatar an opaque pointer to the avatar state
/// \param visible boolean controlling visibility
///
OVRN_EXPORT (void) ovrAvatar_SetRightControllerVisibility(ovrAvatar* avatar, bool visible);

/// Set the left hand's visibility.
/// \param avatar an opaque pointer to the avatar state
/// \param visible boolean controlling visibility
///
OVRN_EXPORT (void) ovrAvatar_SetLeftHandVisibility(ovrAvatar* avatar, bool visible);

/// Set the right hand's visibility.
/// \param avatar an opaque pointer to the avatar state
/// \param visible boolean controlling visibility
///
OVRN_EXPORT (void) ovrAvatar_SetRightHandVisibility(ovrAvatar* avatar, bool visible);


//Hand poses/grips

/// Hand gestures
///
typedef enum ovrAvatarHandGesture_ {
    ovrAvatarHandGesture_Default,
    ovrAvatarHandGesture_GripSphere,
    ovrAvatarHandGesture_GripCube,
    ovrAvatarHandGesture_Count
} ovrAvatarHandGesture;

/// Set the left hand gesture.
/// \param avatar an opaque pointer to the avatar state
/// \param gesture the gesture to set for the left hand - specifying ovrAvatarHandGesture_Default enables default hand animations and poses, other gestures disable them
/// \sa ovrAvatar_SetLeftHandCustomGesture
///
OVRN_EXPORT (void) ovrAvatar_SetLeftHandGesture(ovrAvatar* avatar, ovrAvatarHandGesture gesture);

/// Set the right hand gesture.
/// \param avatar an opaque pointer to the avatar state
/// \param gesture the gesture to set for the right hand - specifying ovrAvatarHandGesture_Default enables default hand animations and poses, other gestures disable them
/// \sa ovrAvatar_SetRightHandCustomGesture
///
OVRN_EXPORT (void) ovrAvatar_SetRightHandGesture(ovrAvatar* avatar, ovrAvatarHandGesture gesture);

/// Set the left hand custom gesture. Using this disables the default hand animations. To restore them, use ovrAvatar_SetLeftHandGesture with ovrAvatarHandGesture_Default.
/// \param avatar an opaque pointer to the avatar state
/// \param jointCount length of the joint array being passed in - this should be the same as the number of joints in the hand
/// \param customJointTransforms an array of transforms for the joints in the hand
/// \sa ovrAvatar_SetLeftHandGesture
///
OVRN_EXPORT (void) ovrAvatar_SetLeftHandCustomGesture(ovrAvatar* avatar, uint32_t jointCount, const ovrAvatarTransform *customJointTransforms);

/// Set the right hand custom gesture. Using this disables the default hand animations. To restore them, use ovrAvatar_SetRightHandGesture with ovrAvatarHandGesture_Default.
/// \param avatar an opaque pointer to the avatar state
/// \param jointCount length of the joint array being passed in - this should be the same as the number of joints in the hand
/// \param customJointTransforms an array of transforms for the joints in the hand
/// \sa ovrAvatar_SetRightHandGesture
///
OVRN_EXPORT (void) ovrAvatar_SetRightHandCustomGesture(ovrAvatar* avatar, uint32_t jointCount, const ovrAvatarTransform *customJointTransforms);

/// Set the currently active capabilities for an avatar.
/// \param avatar an opaque pointer to the avatar state
/// \param capabilities mask of currently active capabilities - toggling a capability toggles that capability being rendered for the avatar, this can be any subset of the capabilities specified at creation time
/// \sa ovrAvatar_Create
///
OVRN_EXPORT (void) ovrAvatar_SetActiveCapabilities(ovrAvatar* avatar, ovrAvatarCapabilities capabilities);

//Base manipulation

/// Use the default base position for the avatar.
/// \param avatar an opaque pointer to the avatar state
/// \sa ovrAvatar_SetCustomBasePosition
///
OVRN_EXPORT (void) ovrAvatar_ClearCustomBasePosition(ovrAvatar* avatar);

/// Set a custom base position for the avatar.
/// \param avatar an opaque pointer to the avatar state
/// \param position the position of the base of the avatar
/// \sa ovrAvatar_ClearCustomBasePosition
///
OVRN_EXPORT (void) ovrAvatar_SetCustomBasePosition(ovrAvatar* avatar, ovrAvatarVector3f position);

// Asset manifest helpers


/// Get the count of assets that are referenced by this avatar.
/// \param avatar an opaque pointer to the avatar state
/// \return asset count
/// \sa ovrAvatar_GetReferencedAsset
///
OVRN_EXPORT (uint32_t) ovrAvatar_GetReferencedAssetCount(ovrAvatar* avatar);

/// Get the referenced asset for an index.
/// \param avatar an opaque pointer to the avatar state
/// \param index the index of the referenced asset to get - must be between 0 and the asset count - 1, inclusive
/// \return asset id
/// \sa ovrAvatar_GetReferencedAssetCount
///
OVRN_EXPORT (ovrAvatarAssetID) ovrAvatar_GetReferencedAsset(ovrAvatar* avatar, uint32_t index);

/// Get the clothing alpha values for the combined body mesh.
/// \param Avatar to get values from
/// \param reference to the texture asset ID
/// \param reference to the alpha offset vector.
OVRN_EXPORT(void) ovrAvatar_GetCombinedMeshAlphaData(
	const ovrAvatar* avatar,
	ovrAvatarAssetID* textureID,
	ovrAvatarVector4f* offset);

//Avatar rendering

/// Material Layer Blend Modes
///
typedef enum ovrAvatarMaterialLayerBlendMode_ {
	ovrAvatarMaterialLayerBlendMode_Add,      ///< The color value of this layer is added to the layers before it.
	ovrAvatarMaterialLayerBlendMode_Multiply, ///< The color value of this layer is multiplied with the layers before it.
	ovrAvatarMaterialLayerBlendMode_Count
} ovrAvatarMaterialLayerBlendMode;

/// Material Layer Sample Modes
///
typedef enum ovrAvatarMaterialLayerSampleMode_ {
    ovrAvatarMaterialLayerSampleMode_Color,                ///< This layer's color value comes from the color constant.
    ovrAvatarMaterialLayerSampleMode_Texture,              ///< This layer's color value comes from the layer's texture sampler.
    ovrAvatarMaterialLayerSampleMode_TextureSingleChannel, ///< This layer's color value comes from a channel in the layer's texture sampler specified by the sample parameters.
    ovrAvatarMaterialLayerSampleMode_Parallax,             ///< This layer's color value comes from the layer's texture sampler with coordinates modified by the material's parallax texture.
    ovrAvatarMaterialLayerSampleMode_RDSM,                 ///< This layer's color value comes from the layer's texture sampler with coordinates modified by the material's roughness texture.
    ovrAvatarMaterialLayerSampleMode_Count
} ovrAvatarMaterialLayerSampleMode;

/// Material Mask Types
/// Masks programmatically control which portions of the surface this layer is applied to.
/// See reference shaders for implementation details.
///
typedef enum ovrAvatarMaterialMaskType_ {
	ovrAvatarMaterialMaskType_None,           ///< No mask is applied to this layer.
	ovrAvatarMaterialMaskType_Positional,     ///< A mask is applied to this based on vertex position.
	ovrAvatarMaterialMaskType_ViewReflection, ///< A mask is applied to this based on reflection of view vector.
	ovrAvatarMaterialMaskType_Fresnel,        ///< A mask is applied based on view direction's angle of incidence.
	ovrAvatarMaterialMaskType_Pulse,          ///< Like positional, but varying over time.
	ovrAvatarMaterialMaskType_Count
} ovrAvatarMaterialMaskType;

/// Material Layer State
///
typedef struct ovrAvatarMaterialLayerState_ {
	ovrAvatarMaterialLayerBlendMode     blendMode;         ///< Blend mode of the material layer
	ovrAvatarMaterialLayerSampleMode    sampleMode;        ///< Sample mode of the material layer
	ovrAvatarMaterialMaskType			maskType;          ///< Mask type of the material layer
	ovrAvatarVector4f					layerColor;        ///< Layer color
	ovrAvatarVector4f                   sampleParameters;  ///< Parameters driving sample mode
	ovrAvatarAssetID                    sampleTexture;     ///< Sample texture id (0 if unused)
	ovrAvatarVector4f					sampleScaleOffset; ///< UV scale and offset parameters for the sample texture
	ovrAvatarVector4f                   maskParameters;    ///< Parameters driving the layer mask
	ovrAvatarVector4f                   maskAxis;          ///< Axis used by some mask types
} ovrAvatarMaterialLayerState;

/// The maximum number of material layers.
///
#define OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT 8

/// Material state
///
typedef struct ovrAvatarMaterialState_ {
	ovrAvatarVector4f			baseColor;               ///< Underlying base color for the material
	ovrAvatarMaterialMaskType	baseMaskType;            ///< Mask type of the base color
	ovrAvatarVector4f			baseMaskParameters;      ///< Parameters for the base mask type
	ovrAvatarVector4f			baseMaskAxis;            ///< Axis used by some mask types
	ovrAvatarAssetID            alphaMaskTextureID;      ///< Texture id for the base alpha mask (0 if unused)
	ovrAvatarVector4f			alphaMaskScaleOffset;    ///< UV scale and offset parameters for the alpha mask
	ovrAvatarAssetID			normalMapTextureID;      ///< Texture id for the normal map (0 if unused)
	ovrAvatarVector4f			normalMapScaleOffset;    ///< UV scale and offset parameters for the normal map
	ovrAvatarAssetID            parallaxMapTextureID;    ///< Texture id for the parallax map (0 if unused)
	ovrAvatarVector4f			parallaxMapScaleOffset;  ///< UV scale and offset parameters for the parallax map
	ovrAvatarAssetID			roughnessMapTextureID;   ///< Texture id for the roughness map (0 if unused)
	ovrAvatarVector4f			roughnessMapScaleOffset; ///< UV scale and offset parameters for the roughness map
	uint32_t                    layerCount;              ///< Number of layers
	ovrAvatarMaterialLayerState layers[OVR_AVATAR_MAX_MATERIAL_LAYER_COUNT]; ///< State for each material layer
} ovrAvatarMaterialState;


/// Material state
///
typedef struct ovrAvatarPBSMaterialState_ {
	ovrAvatarVector4f               baseColor;               ///< Underlying base color for the material
	ovrAvatarAssetID                albedoTextureID;         ///< Texture id for the albedo map
	ovrAvatarVector4f               albedoMultiplier;        ///< Multiplier for albedo texture sample
	ovrAvatarAssetID                metallicnessTextureID;   ///< Texture id for the metal map
	float                           glossinessScale;         ///< Glossiness factor
	ovrAvatarAssetID                normalTextureID;         ///< Texture id for the normal map
	ovrAvatarAssetID                heightTextureID;         ///< Texture id for the height map
	ovrAvatarAssetID                occlusionTextureID;      ///< Texture id for the occlusion map
	ovrAvatarAssetID                emissionTextureID;       ///< Texture id for the emission map
	ovrAvatarVector4f               emissionMultiplier;      ///< Multiplier for emission texture sample
	ovrAvatarAssetID                detailMaskTextureID;     ///< Texture id for the detail mask map
	ovrAvatarAssetID                detailAlbedoTextureID;   ///< Texture id for the detail albedo map
	ovrAvatarAssetID                detailNormalTextureID;   ///< Texture id for the detail normal map
} ovrAvatarPBSMaterialState;

/// Pose for a skinned mesh object
///
typedef struct ovrAvatarSkinnedMeshPose_ {
    uint32_t jointCount; ///< Number of joints in the joint hierarchy
	ovrAvatarTransform jointTransform[OVR_AVATAR_MAXIMUM_JOINT_COUNT]; ///< Array of local transform from parent
	int jointParents[OVR_AVATAR_MAXIMUM_JOINT_COUNT]; ///< Array of indices of the parent joints
	const char * jointNames[OVR_AVATAR_MAXIMUM_JOINT_COUNT]; ///< Array of joint names
} ovrAvatarSkinnedMeshPose;

/// Visibility flags for avatar components
///
typedef enum ovrAvatarVisibilityFlags_ {
	ovrAvatarVisibilityFlag_FirstPerson = 1 << 0, ///< Visible in the first person view
	ovrAvatarVisibilityFlag_ThirdPerson = 1 << 1, ///< Visible in the third person view
	ovrAvatarVisibilityFlag_SelfOccluding = 1 << 2, ///< Is self occluding
} ovrAvatarVisibilityFlags;

/// Skinned Mesh object render information
///
typedef struct ovrAvatarRenderPart_SkinnedMeshRender_ {
	ovrAvatarTransform       localTransform; ///< Local transform for this render part
	uint32_t				 visibilityMask; ///< Visibility flags, \sa ovrAvatarVisibilityFlags
	ovrAvatarAssetID         meshAssetID;    ///< Asset id for this mesh
	ovrAvatarMaterialState   materialState;  ///< Material properties
	ovrAvatarSkinnedMeshPose skinnedPose;    ///< Pose for all the joints on this render part
} ovrAvatarRenderPart_SkinnedMeshRender;

/// Skinned Mesh object with a physically based renderer
///
typedef struct ovrAvatarRenderPart_SkinnedMeshRenderPBS_ {
	ovrAvatarTransform       localTransform;        ///< Local transform for this render part
	uint32_t				 visibilityMask;        ///< Visibility flags, \sa ovrAvatarVisibilityFlags
	ovrAvatarAssetID         meshAssetID;           ///< Asset id for this mesh
	ovrAvatarAssetID		 albedoTextureAssetID;  ///< Albedo texture asset id
	ovrAvatarAssetID		 surfaceTextureAssetID; ///< Surface texture asset id
	ovrAvatarSkinnedMeshPose skinnedPose;           ///< Pose for all the joints on this render part
} ovrAvatarRenderPart_SkinnedMeshRenderPBS;

/// Projector component
///
typedef struct ovrAvatarRenderPart_ProjectorRender_ {
	ovrAvatarTransform		localTransform;  ///< Transformation of projection volume onto the target
	uint32_t				componentIndex;  ///< Component to project onto
	uint32_t				renderPartIndex; ///< Render part to project onto
	ovrAvatarMaterialState	materialState;   ///< Material that is applied in that projection
} ovrAvatarRenderPart_ProjectorRender;

/// Skinned Mesh object render information
///
typedef struct ovrAvatarRenderPart_SkinnedMeshRenderPBSV2_ {
	ovrAvatarTransform          localTransform; ///< Local transform for this render part
	uint32_t                    visibilityMask; ///< Visibility flags, \sa ovrAvatarVisibilityFlags
	ovrAvatarAssetID            meshAssetID;    ///< Asset id for this mesh
	ovrAvatarPBSMaterialState   materialState;  ///< Material properties
	ovrAvatarSkinnedMeshPose    skinnedPose;    ///< Pose for all the joints on this render part
} ovrAvatarRenderPart_SkinnedMeshRenderPBS_V2;

/// Avatar component
///
typedef struct ovrAvatarComponent_ {
	ovrAvatarTransform transform; ///< Local transformation of this component
	uint32_t renderPartCount;     ///< Number of render parts for this component
	const ovrAvatarRenderPart* const* renderParts; ///< Array of render parts for this component
	const char* name;             ///< Name of the component
} ovrAvatarComponent;

/// Render part types
///
typedef enum ovrAvatarRenderPartType_ {
	ovrAvatarRenderPartType_SkinnedMeshRender,            ///< Skinned Mesh Component
	ovrAvatarRenderPartType_SkinnedMeshRenderPBS,         ///< Skinned Mesh Component with Physically Based Renderer
	ovrAvatarRenderPartType_ProjectorRender,              ///< Projector Component
	ovrAvatarRenderPartType_SkinnedMeshRenderPBS_V2,      ///< Physically Based Render Component Version 2
	ovrAvatarRenderPartType_Count                         ///< Total number of render part types
} ovrAvatarRenderPartType;

/// Get the materials for the body 
/// \param Avatar to get values from
OVRN_EXPORT(const ovrAvatarPBSMaterialState*) ovrAvatar_GetBodyPBSMaterialStates(
	const ovrAvatarRenderPart* renderPart,
	uint32_t* count);

/// Get the type of a render part.
/// \param renderPart an opaque pointer to the render part
/// \return type of the render part
///
OVRN_EXPORT (ovrAvatarRenderPartType) ovrAvatarRenderPart_GetType(
	const ovrAvatarRenderPart* renderPart);

/// Get the skinned mesh render part.
/// \param renderPart an opaque pointer to the render part
/// \return skinned mesh render part
///
OVRN_EXPORT (const ovrAvatarRenderPart_SkinnedMeshRender*) ovrAvatarRenderPart_GetSkinnedMeshRender(
	const ovrAvatarRenderPart* renderPart);

/// Get the skinned mesh with physically based rendering part.
/// \param renderPart an opaque pointer to the render part
/// \return skinned mesh render PBS part
///
OVRN_EXPORT (const ovrAvatarRenderPart_SkinnedMeshRenderPBS*) ovrAvatarRenderPart_GetSkinnedMeshRenderPBS(
	const ovrAvatarRenderPart* renderPart);

/// Get the skinned mesh with physically based rendering part.
/// \param renderPart an opaque pointer to the render part
/// \return skinned mesh render PBS V2 part
///
OVRN_EXPORT(const ovrAvatarRenderPart_SkinnedMeshRenderPBS_V2*) ovrAvatarRenderPart_GetSkinnedMeshRenderPBSV2(
	const ovrAvatarRenderPart* renderPart);

/// Get the projector render part.
/// \param renderPart an opaque pointer to the render part
/// \return projector render part
///
OVRN_EXPORT (const ovrAvatarRenderPart_ProjectorRender*) ovrAvatarRenderPart_GetProjectorRender(
	const ovrAvatarRenderPart* renderPart);

/// Get number of components for the avatar.
/// \param avatar opaque pointer to the avatar
/// \return count of avatar components
///
OVRN_EXPORT (uint32_t) ovrAvatarComponent_Count(const ovrAvatar* avatar);

/// Get avatar component at a given index.
/// \param avatar opaque pointer to the avatar
/// \param index the index of the component to get
/// \return count of avatar components
///
OVRN_EXPORT (const ovrAvatarComponent*) ovrAvatarComponent_Get(
	const ovrAvatar* avatar, uint32_t index);

//Avatar semantic information

/// Avatar Body Component
///
typedef struct ovrAvatarBodyComponent_ {
	ovrAvatarTransform  leftEyeTransform;      ///< Local transform for the left eye
	ovrAvatarTransform  rightEyeTransform;     ///< Local transform for the right eye
	ovrAvatarTransform  centerEyeTransform;    ///< Local transform for the center eye (average of the two eyes)
	const ovrAvatarComponent* renderComponent; ///< Pointer to the body component
} ovrAvatarBodyComponent;

/// Avatar Controller Component
///
typedef struct ovrAvatarControllerComponent_ {
	ovrAvatarHandInputState inputState;        ///< Input state vector for the hand
	const ovrAvatarComponent* renderComponent; ///< Render component for the controller
} ovrAvatarControllerComponent;

/// Avatar Base Component
///
typedef struct ovrAvatarBaseComponent_ {
	ovrAvatarVector3f basePosition;            ///< Base position
	const ovrAvatarComponent* renderComponent; ///< Render component for the base
} ovrAvatarBaseComponent;

/// Avatar Hand Component
///
typedef struct ovrAvatarHandComponent_ {
	ovrAvatarHandInputState inputState;        ///< Input state vector for the hand
	const ovrAvatarComponent* renderComponent; ///< Render component for the hand
} ovrAvatarHandComponent;

/// Get the Body Component.
/// \param avatar opaque pointer to the avatar
/// \return the body component
///
OVRN_EXPORT (const ovrAvatarBodyComponent*) ovrAvatarPose_GetBodyComponent(
	ovrAvatar* avatar);

/// Get the Base Component.
/// \param avatar opaque pointer to the avatar
/// \return the base component
///
OVRN_EXPORT (const ovrAvatarBaseComponent*) ovrAvatarPose_GetBaseComponent(
	ovrAvatar* avatar);

/// Get the Left Controller Component.
/// \param avatar opaque pointer to the avatar
/// \return the left controller component
///
OVRN_EXPORT (const ovrAvatarControllerComponent*) ovrAvatarPose_GetLeftControllerComponent(
	ovrAvatar* avatar);

/// Get the Right Controller Component.
/// \param avatar opaque pointer to the avatar
/// \return the right controller component
///
OVRN_EXPORT (const ovrAvatarControllerComponent*) ovrAvatarPose_GetRightControllerComponent(
	ovrAvatar* avatar);

/// Get the Left Hand Component.
/// \param avatar opaque pointer to the avatar
/// \return the left hand component
///
OVRN_EXPORT (const ovrAvatarHandComponent*) ovrAvatarPose_GetLeftHandComponent(
	ovrAvatar* avatar);

/// Get the Right Hand Component.
/// \param avatar opaque pointer to the avatar
/// \return the right hand component
///
OVRN_EXPORT (const ovrAvatarHandComponent*) ovrAvatarPose_GetRightHandComponent(
	ovrAvatar* avatar);

//Assets

/// Asset types
///
typedef enum ovrAvatarAssetType_ {
    ovrAvatarAssetType_Mesh,         ///< Mesh asset
    ovrAvatarAssetType_Texture,      ///< Texture asset
    ovrAvatarAssetType_Pose,         ///< Internal type (to be removed in a future version)
    ovrAvatarAssetType_Material,     ///< Material asset
    ovrAvatarAssetType_CombinedMesh, ///< Combined Mesh asset
	ovrAvatarAssetType_PBSMaterial,  ///< PBS Material asset

	ovrAvatarAssetType_FailedLoad,   ///< Indicates the asset failed to load
    ovrAvatarAssetType_Count         ///< Count of different asset types
} ovrAvatarAssetType;

/// Mesh Vertex
///
typedef struct ovrAvatarMeshVertex_ {
    float   x;  ///< X component of the position
    float   y;  ///< Y component of the position
    float   z;  ///< Z component of the position
    float   nx; ///< X component of the normal
    float   ny; ///< Y component of the normal
    float   nz; ///< Z component of the normal
	float	tx; ///< X component of the tangent
	float	ty; ///< Y component of the tangent
	float	tz; ///< Z component of the tangent
	float	tw; ///< W component of the tangent
    float   u;  ///< U component of texture coordinates
    float   v;  ///< V component of texture coordinates
    uint8_t blendIndices[4]; ///< Joint indices for skin weighting
    float   blendWeights[4]; ///< Blend weights for skin weighting
} ovrAvatarMeshVertex;

/// Mesh Vertex with color
///
typedef struct ovrAvatarMeshVertexV2_ {
	float   x;  ///< X component of the position
	float   y;  ///< Y component of the position
	float   z;  ///< Z component of the position
	float   nx; ///< X component of the normal
	float   ny; ///< Y component of the normal
	float   nz; ///< Z component of the normal
	float	tx; ///< X component of the tangent
	float	ty; ///< Y component of the tangent
	float	tz; ///< Z component of the tangent
	float	tw; ///< W component of the tangent
	float   u;  ///< U component of texture coordinates
	float   v;  ///< V component of texture coordinates
	float	r;  ///< r component of the color
	float	g;  ///< g component of the color
	float	b;  ///< b component of the color
	float	a;  ///< a component of the color
	uint8_t blendIndices[4]; ///< Joint indices for skin weighting
	float   blendWeights[4]; ///< Blend weights for skin weighting
} ovrAvatarMeshVertexV2;

/// Mesh Asset Data
///
typedef struct ovrAvatarMeshAssetData_ {
	uint32_t                   vertexCount;     ///< Vertex Count
	const ovrAvatarMeshVertex* vertexBuffer;    ///< Vertex Array
	uint32_t                   indexCount;      ///< Index Count
	const uint16_t*            indexBuffer;     ///< Index Array
	ovrAvatarSkinnedMeshPose   skinnedBindPose; ///< Bind pose
} ovrAvatarMeshAssetData;

/// Mesh Asset Data
///
typedef struct ovrAvatarMeshAssetDataV2_ {
	uint32_t                            vertexCount;     ///< Vertex Count
	const ovrAvatarMeshVertexV2*        vertexBuffer;    ///< Vertex Array
	uint32_t                            indexCount;      ///< Index Count
	const uint16_t*                     indexBuffer;     ///< Index Array
	ovrAvatarSkinnedMeshPose            skinnedBindPose; ///< Bind pose
} ovrAvatarMeshAssetDataV2;

/// Texture Format
///
typedef enum ovrAvatarTextureFormat_ {
	ovrAvatarTextureFormat_RGB24 = 0,                   ///< RGB 24bit uncompressed texture
	ovrAvatarTextureFormat_DXT1 = 1,                    ///< DXT1 compressed texture
	ovrAvatarTextureFormat_DXT5 = 2,                    ///< DXT5 compressed texture
	ovrAvatarTextureFormat_ASTC_RGB_6x6_DEPRECATED = 3, ///< Deprecated: ASTC compressed texture. 16 bytes of header data at the beginning. No mipmap data.
	ovrAvatarTextureFormat_ASTC_RGB_6x6_MIPMAPS = 4,    ///< ASTC compressed texture with mipmaps included directly afterwards. No header data included.
	ovrAvatarTextureFormat_Count                        ///< Count of different texture formats
}ovrAvatarTextureFormat;

/// Texture Asset Data
///
typedef struct ovrAvatarTextureAssetData_ {
	ovrAvatarTextureFormat	format;          ///< Texture format type
	uint32_t				sizeX;           ///< Texture X dimension
	uint32_t				sizeY;           ///< Texture Y dimension
	uint32_t				mipCount;        ///< Number of mipmap levels
	uint64_t				textureDataSize; ///< Texture buffer size
	const uint8_t*			textureData;     ///< Texture buffer
} ovrAvatarTextureAssetData;

/// Begin loading asset.
/// \param assetID asset ID
///
OVRN_EXPORT (void) ovrAvatarAsset_BeginLoading(
	ovrAvatarAssetID assetID);

/// Begin loading asset.
/// \param assetID asset ID
/// \param assetLOD asset Level of Detail
OVRN_EXPORT(void) ovrAvatarAsset_BeginLoadingLOD(
	const ovrAvatarAssetID assetID,
	const ovrAvatarAssetLevelOfDetail assetLOD);

/// Get asset type.
/// \param asset opaque pointer to asset
/// \return the type of asset
///
OVRN_EXPORT (ovrAvatarAssetType) ovrAvatarAsset_GetType(
	const ovrAvatarAsset* asset);

/// Get mesh asset data.
/// \param asset opaque pointer to asset
/// \return mesh asset data
///
OVRN_EXPORT (const ovrAvatarMeshAssetData*) ovrAvatarAsset_GetMeshData(
	const ovrAvatarAsset* asset);

/// Get combined mesh asset data.
/// \param asset opaque pointer to asset
/// \return mesh asset data
///
OVRN_EXPORT(const ovrAvatarMeshAssetDataV2*) ovrAvatarAsset_GetCombinedMeshData(
	const ovrAvatarAsset* asset);

/// Get combined mesh asset IDs.
/// \param asset opaque pointer to asset
/// \param count, the number of assets used to combine the mesh
/// \return array of asset IDs of size count.
///
OVRN_EXPORT(const ovrAvatarAssetID*) ovrAvatarAsset_GetCombinedMeshIDs(
	const ovrAvatarAsset* asset,
	uint32_t* count);

OVRN_EXPORT(const ovrAvatar*) ovrAvatarAsset_GetAvatar(
	const ovrAvatarAsset* asset);

/// Get texture asset data.
/// \param asset opaque pointer to asset
/// \return texture asset data
///
OVRN_EXPORT (const ovrAvatarTextureAssetData*) ovrAvatarAsset_GetTextureData(
	const ovrAvatarAsset* asset);

/// Get material asset data.
/// \param asset opaque pointer to asset
/// \return material asset data
///
OVRN_EXPORT(const ovrAvatarMaterialState*) ovrAvatarAsset_GetMaterialData(
	const ovrAvatarAsset* assetHandle);

/// Get PBS material asset data.
/// \param asset opaque pointer to asset
/// \return material asset data
///
OVRN_EXPORT(const ovrAvatarPBSMaterialState*) ovrAvatarAsset_GetPBSMaterialData(
	const ovrAvatarAsset* assetHandle);

//Avatar recording and playback

/// Begin packet recording.
/// \param avatar opaque pointer to the avatar
///
OVRN_EXPORT (void) ovrAvatarPacket_BeginRecording(ovrAvatar* avatar);

/// Finish packet recording.
/// \param avatar opaque pointer to the avatar
/// \return opaque pointer to the freshly recorded packet
///
OVRN_EXPORT (ovrAvatarPacket*) ovrAvatarPacket_EndRecording(ovrAvatar* avatar);

/// Get the size of a recorded packet.
/// \param packet opaque pointer to the packet
/// \return size of the packet in bytes
///
OVRN_EXPORT (uint32_t) ovrAvatarPacket_GetSize(const ovrAvatarPacket* packet);

/// Write a packet to a buffer.
/// \param packet opaque pointer to the packet
/// \param targetSize size of the buffer to write to
/// \param targetBuffer the buffer to write to
/// \return size of the packet in bytes
///
OVRN_EXPORT (bool) ovrAvatarPacket_Write(const ovrAvatarPacket* packet,
	                                   uint32_t targetSize,
									   uint8_t* targetBuffer);

/// Read a packet out of a buffer.
/// \param bufferSize size of the buffer in bytes
/// \param buffer the buffer to read from
/// \return opaque pointer to the new packet
///
typedef const uint8_t BUFFER_[];
OVRN_EXPORT (ovrAvatarPacket*) ovrAvatarPacket_Read(
	uint32_t bufferSize,
	BUFFER_);

/// Get duration of packet.
/// \param packet opaque pointer to the packet
/// \return duration in seconds
///
OVRN_EXPORT (float) ovrAvatarPacket_GetDurationSeconds(
	const ovrAvatarPacket* packet);

/// Free the packet.
/// \param packet opaque pointer to the packet
///
OVRN_EXPORT (void) ovrAvatarPacket_Free(ovrAvatarPacket* packet);

/// Update avatar pose from packet.
/// \param avatar an opaque pointer to the avatar state
/// \param packet an opaque pointer to the packet
/// \param secondsFromPacketStart time to update the pose to within the packet
///
OVRN_EXPORT (void) ovrAvatar_UpdatePoseFromPacket(
	ovrAvatar* avatar,
	const ovrAvatarPacket* packet,
	float secondsFromPacketStart);

/////////////////////////////////////
/// Integration section
///
/// These functions are used to allow csharp (unity) to access internal members without needing to marshal
/////////////////////////////////////

OVRN_EXPORT (ovrAvatarTransform) ovrAvatarSkinnedMeshRender_GetTransform(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (ovrAvatarTransform) ovrAvatarSkinnedMeshRenderPBS_GetTransform(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (ovrAvatarTransform) ovrAvatarSkinnedMeshRenderPBSV2_GetTransform(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (uint32_t) ovrAvatarSkinnedMeshRender_GetVisibilityMask(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (uint32_t) ovrAvatarSkinnedMeshRenderPBS_GetVisibilityMask(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (uint32_t) ovrAvatarSkinnedMeshRenderPBSV2_GetVisibilityMask(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (bool) ovrAvatarSkinnedMeshRender_MaterialStateChanged(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (bool) ovrAvatarSkinnedMeshRenderPBSV2_MaterialStateChanged(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (ovrAvatarMaterialState) ovrAvatarSkinnedMeshRender_GetMaterialState(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (ovrAvatarPBSMaterialState) ovrAvatarSkinnedMeshRenderPBSV2_GetPBSMaterialState(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (uint64_t) ovrAvatarSkinnedMeshRender_GetDirtyJoints(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (uint64_t) ovrAvatarSkinnedMeshRenderPBS_GetDirtyJoints(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (uint64_t) ovrAvatarSkinnedMeshRenderPBSV2_GetDirtyJoints(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (ovrAvatarTransform) ovrAvatarSkinnedMeshRender_GetJointTransform(const ovrAvatarRenderPart* renderPart, uint32_t jointIndex);
OVRN_EXPORT (ovrAvatarTransform) ovrAvatarSkinnedMeshRenderPBS_GetJointTransform(const ovrAvatarRenderPart* renderPart, uint32_t jointIndex);
OVRN_EXPORT (ovrAvatarTransform) ovrAvatarSkinnedMeshRenderPBSV2_GetJointTransform(const ovrAvatarRenderPart* renderPart, uint32_t jointIndex);
OVRN_EXPORT (ovrAvatarAssetID) ovrAvatarSkinnedMeshRenderPBS_GetAlbedoTextureAssetID(const ovrAvatarRenderPart* renderPart);
OVRN_EXPORT (ovrAvatarAssetID) ovrAvatarSkinnedMeshRenderPBS_GetSurfaceTextureAssetID(const ovrAvatarRenderPart* renderPart);

typedef void (* LoggingCallback) (const char * str);
OVRN_EXPORT(void) ovrAvatar_RegisterLoggingCallback(LoggingCallback callback);
OVRN_EXPORT(void) ovrAvatar_SetLoggingLevel(ovrAvatarLogLevel level);

#ifdef __cplusplus
}
#endif

#endif // OVR_Avatar_h
