// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "Engine/EngineTypes.h"
#include "CollisionQueryParams.h"
#include "EngineDefines.h"
#include "PhysxUserData.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Physics/PhysicsInterfaceTypes.h"
#include "PhysicsPublic.h"
#include "BodyInstance.generated.h"

class UBodySetup;
class UPhysicalMaterial;
class UPrimitiveComponent;
struct FBodyInstance;
struct FCollisionNotifyInfo;
struct FCollisionShape;
struct FConstraintInstance;
struct FPropertyChangedEvent;
struct FShapeData;
class UPrimitiveComponent;

struct FShapeData;

ENGINE_API int32 FillInlineShapeArray_AssumesLocked(PhysicsInterfaceTypes::FInlineShapeArray& Array, const FPhysicsActorHandle& Actor, EPhysicsSceneType InSceneType = PST_MAX);

UENUM(BlueprintType)
namespace EDOFMode
{
	enum Type
	{
		/*Inherits the degrees of freedom from the project settings.*/
		Default,
		/*Specifies which axis to freeze rotation and movement along.*/
		SixDOF,
		/*Allows 2D movement along the Y-Z plane.*/
		YZPlane,
		/*Allows 2D movement along the X-Z plane.*/
		XZPlane,
		/*Allows 2D movement along the X-Y plane.*/
		XYPlane,
		/*Allows 2D movement along the plane of a given normal*/
		CustomPlane,
		/*No constraints.*/
		None
	};
}

struct FCollisionNotifyInfo;
template <bool bCompileStatic> struct FInitBodiesHelper;

USTRUCT()
struct ENGINE_API FCollisionResponse
{
	GENERATED_USTRUCT_BODY()

	FCollisionResponse();
	FCollisionResponse(ECollisionResponse DefaultResponse);

	/** Set the response of a particular channel in the structure. */
	void SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse);

	/** Set all channels to the specified response */
	void SetAllChannels(ECollisionResponse NewResponse);

	/** Replace the channels matching the old response with the new response */
	void ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse);

	/** Returns the response set on the specified channel */
	FORCEINLINE_DEBUGGABLE ECollisionResponse GetResponse(ECollisionChannel Channel) const { return ResponseToChannels.GetResponse(Channel); }
	const FCollisionResponseContainer& GetResponseContainer() const { return ResponseToChannels; }

	/** Set all channels from ChannelResponse Array **/
	void SetCollisionResponseContainer(const FCollisionResponseContainer& InResponseToChannels);
	void SetResponsesArray(const TArray<FResponseChannel>& InChannelResponses);
	void UpdateResponseContainerFromArray();

	bool operator==(const FCollisionResponse& Other) const;
	bool operator!=(const FCollisionResponse& Other) const
	{
		return !(*this == Other);
	}

private:

#if 1// @hack until PostLoad is disabled for CDO of BP - WITH_EDITOR
	/** Anything that updates array does not have to be done outside of editor
	 *	because we won't save outside of editor
	 *	During runtime, important data is ResponseToChannel
	 *	That is the data we care during runtime. But that data won't be saved.
	 */
	bool RemoveReponseFromArray(ECollisionChannel Channel);
	bool AddReponseToArray(ECollisionChannel Channel, ECollisionResponse Response);
	void UpdateArrayFromResponseContainer();
#endif

	/** Types of objects that this physics objects will collide with. */
	// we have to still load them until resave
	UPROPERTY(transient)
	FCollisionResponseContainer ResponseToChannels;

	/** Custom Channels for Responses */
	UPROPERTY(EditAnywhere, Category = Custom)
	TArray<FResponseChannel> ResponseArray;

	friend struct FBodyInstance;
};

enum class BodyInstanceSceneState : uint8
{
	NotAdded,
	AwaitingAdd,
	Added,
	AwaitingRemove,
	Removed
};


/** Whether to override the sync/async scene used by a dynamic actor*/
UENUM(BlueprintType)
enum class EDynamicActorScene : uint8
{
	//Use whatever the body instance wants
	Default,	
	//use sync scene
	UseSyncScene,	
	//use async scene
	UseAsyncScene	
};

/** Container for a physics representation of an object */
USTRUCT(BlueprintType)
struct ENGINE_API FBodyInstance
{
	GENERATED_USTRUCT_BODY()

	/** 
	 *	Index of this BodyInstance within the SkeletalMeshComponent/PhysicsAsset. 
	 *	Is INDEX_NONE if a single body component
	 */
	int32 InstanceBodyIndex;

	/** When we are a body within a SkeletalMeshComponent, we cache the index of the bone we represent, to speed up sync'ing physics to anim. */
	int16 InstanceBoneIndex;

private:
	/** Enum indicating what type of object this should be considered as when it moves */
	UPROPERTY(EditAnywhere, Category=Custom)
	TEnumAsByte<enum ECollisionChannel> ObjectType;


	/** Extra mask for filtering. Look at declaration for logic */
	FMaskFilter MaskFilter;

	/**
	* Type of collision enabled.
	* 
	*	No Collision      : Will not create any representation in the physics engine. Cannot be used for spatial queries (raycasts, sweeps, overlaps) or simulation (rigid body, constraints). Best performance possible (especially for moving objects)
	*	Query Only        : Only used for spatial queries (raycasts, sweeps, and overlaps). Cannot be used for simulation (rigid body, constraints). Useful for character movement and things that do not need physical simulation. Performance gains by keeping data out of simulation tree.
	*	Physics Only      : Only used only for physics simulation (rigid body, constraints). Cannot be used for spatial queries (raycasts, sweeps, overlaps). Useful for jiggly bits on characters that do not need per bone detection. Performance gains by keeping data out of query tree
	*	Collision Enabled : Can be used for both spatial queries (raycasts, sweeps, overlaps) and simulation (rigid body, constraints).
	*/
	UPROPERTY(EditAnywhere, Category=Custom)
	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled;

public:
	// Current state of the physics body for tracking deferred addition and removal.
	BodyInstanceSceneState CurrentSceneState;

	/** The set of values used in considering when put this body to sleep. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics)
	ESleepFamily SleepFamily;

	/** Locks physical movement along specified axis.*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Mode"))
	TEnumAsByte<EDOFMode::Type> DOFMode;

public:

	/** If true Continuous Collision Detection (CCD) will be used for this component */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Collision)
	uint8 bUseCCD : 1;

	/**	Should 'Hit' events fire when this object collides during physics simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Collision, meta = (DisplayName = "Simulation Generates Hit Events"))
	uint8 bNotifyRigidBodyCollision : 1;

	/**	Enable contact modification. Assumes custom contact modification has been provided (see FPhysXContactModifyCallback) */
	uint8 bContactModification : 1;

	/////////
	// SIM SETTINGS

	/** 
	 * If true, this body will use simulation. If false, will be 'fixed' (ie kinematic) and move where it is told. 
	 * For a Skeletal Mesh Component, simulating requires a physics asset setup and assigned on the SkeletalMesh asset.
	 * For a Static Mesh Component, simulating requires simple collision to be setup on the StaticMesh asset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bSimulatePhysics : 1;

	/** If true, mass will not be automatically computed and you must set it directly */
	UPROPERTY(EditAnywhere, Category = Physics, meta = (InlineEditConditionToggle))
	uint8 bOverrideMass : 1;

	/** If object should have the force of gravity applied */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Physics)
	uint8 bEnableGravity : 1;

	/** If true and is attached to a parent, the two bodies will be joined into a single rigid body. Physical settings like collision profile and body settings are determined by the root */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics, meta = (editcondition = "!bSimulatePhysics"))
	uint8 bAutoWeld : 1;

	/** If object should start awake, or if it should initially be sleeping */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bSimulatePhysics"))
	uint8 bStartAwake:1;

	/**	Should 'wake/sleep' events fire when this object is woken up or put to sleep by the physics simulation. */
	UPROPERTY(EditAnywhere,AdvancedDisplay, BlueprintReadOnly, Category = Physics)
	uint8 bGenerateWakeEvents : 1;

	/** If true, it will update mass when scale changes **/
	UPROPERTY()
	uint8 bUpdateMassWhenScaleChanges:1;

	/** When a Locked Axis Mode is selected, will lock translation on the specified axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta=(DisplayName = "Lock Axis Translation"))
	uint8 bLockTranslation : 1;
	
	/** When a Locked Axis Mode is selected, will lock rotation to the specified axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta=(DisplayName = "Lock Axis Rotation"))
	uint8 bLockRotation : 1;

	/** Lock translation along the X-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "X"))
	uint8 bLockXTranslation : 1;

	/** Lock translation along the Y-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Y"))
	uint8 bLockYTranslation : 1;

	/** Lock translation along the Z-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Z"))
	uint8 bLockZTranslation : 1;

	/** Lock rotation about the X-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "X"))
	uint8 bLockXRotation : 1;

	/** Lock rotation about the Y-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Y"))
	uint8 bLockYRotation : 1;

	/** Lock rotation about the Z-axis*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Z"))
	uint8 bLockZRotation : 1;

	/** Override the default max angular velocity */
	UPROPERTY(EditAnywhere, Category = Physics, meta = (editcondition = "bSimulatePhysics", InlineEditConditionToggle))
	uint8 bOverrideMaxAngularVelocity : 1;

	/**
	* If true, this body will be put into the asynchronous physics scene. If false, it will be put into the synchronous physics scene.
	* If the body is static, it will be placed into both scenes regardless of the value of bUseAsyncScene.
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics)
	uint8 bUseAsyncScene : 1;


	/** 
	 * @HACK:
	 * These are ONLY used when the 'p.EnableDynamicPerBodyFilterHacks' CVar is set (disabled by default).
	 * Some games need to dynamically modify collision per skeletal body. These provide game code a way to 
	 * do that, until we're able to refactor how skeletal bodies work.
	 */
	uint8 bHACK_DisableCollisionResponse : 1;
	/* By default, an owning skel mesh component will override the body's collision filter. This will disable that behavior. */
	uint8 bHACK_DisableSkelComponentFilterOverriding : 1;

protected:

	/** Whether this body instance has its own custom MaxDepenetrationVelocity*/
	UPROPERTY(EditAnywhere, Category = Physics, meta=(InlineEditConditionToggle))
	uint8 bOverrideMaxDepenetrationVelocity : 1;

	/** Whether this instance of the object has its own custom walkable slope override setting. */
	UPROPERTY(EditAnywhere, Category = Physics, meta = (InlineEditConditionToggle))
	uint8 bOverrideWalkableSlopeOnInstance : 1;

	/** 
	 * Internal flag to allow us to quickly check whether we should interpolate when substepping 
	 * e.g. kinematic bodies that are QueryOnly do not need to interpolate as we will not be querying them
	 * at a sub-position.
	 * This is complicated by welding, where multiple the CollisionEnabled flag of the root must be considered.
	 */
	UPROPERTY()
	uint8 bInterpolateWhenSubStepping : 1;

	/** Whether we are pending a collision profile setup */
	uint8 bPendingCollisionProfileSetup : 1;

	uint8 bHasSharedShapes : 1;

public:
	/** Current scale of physics - used to know when and how physics must be rescaled to match current transform of OwnerComponent. */
	FVector Scale3D;

	/////////
	// COLLISION SETTINGS

#if WITH_EDITORONLY_DATA
	/** Types of objects that this physics objects will collide with. */
	UPROPERTY() 
	struct FCollisionResponseContainer ResponseToChannels_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

private:

	/** Collision Profile Name **/
	UPROPERTY(EditAnywhere, Category=Custom)
	FName CollisionProfileName;

	/** Custom Channels for Responses*/
	UPROPERTY(EditAnywhere, Category = Custom)
	struct FCollisionResponse CollisionResponses;

protected:
	/** The maximum velocity used to depenetrate this object*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideMaxDepenetrationVelocity", ClampMin = "0.0", UIMin = "0.0"))
	float MaxDepenetrationVelocity;

	/**Mass of the body in KG. By default we compute this based on physical material and mass scale.
	*@see bOverrideMass to set this directly */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideMass", ClampMin = "0.001", UIMin = "0.001", DisplayName = "MassInKg"))
	float MassInKgOverride;

	/** The body setup holding the default body instance and its collision profile. */
	TWeakObjectPtr<UBodySetup> ExternalCollisionProfileBodySetup;

	/** Update the substepping interpolation flag */
	void UpdateInterpolateWhenSubStepping();

public:

	/** Whether we should interpolate when substepping. @see bInterpolateWhenSubStepping */
	bool ShouldInterpolateWhenSubStepping() const { return bInterpolateWhenSubStepping; }

	/** Returns the mass override. See MassInKgOverride for documentation */
	float GetMassOverride() const { return MassInKgOverride; }

	/** Sets the mass override */
	void SetMassOverride(float MassInKG, bool bNewOverrideMass = true);

	bool GetRigidBodyState(FRigidBodyState& OutState);

	/** 'Drag' force added to reduce linear movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	float LinearDamping;

	/** 'Drag' force added to reduce angular movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	float AngularDamping;

	/** Locks physical movement along a custom plane for a given normal.*/
	UPROPERTY(EditAnywhere, Category = Physics, meta = (DisplayName = "Plane Normal"))
	FVector CustomDOFPlaneNormal;

	/** User specified offset for the center of mass of this object, from the calculated location */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics, meta = (DisplayName = "Center Of Mass Offset"))
	FVector COMNudge;

	/** Per-instance scaling of mass */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics)
	float MassScale;

	/** Per-instance scaling of inertia (bigger number means  it'll be harder to rotate) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category = Physics)
	FVector InertiaTensorScale;

public:
	/** Use the collision profile found in the given BodySetup's default BodyInstance */
	void UseExternalCollisionProfile(UBodySetup* InExternalCollisionProfileBodySetup);

	void ClearExternalCollisionProfile();

	/** Locks physical movement along axis. */
	void SetDOFLock(EDOFMode::Type NewDOFMode);

	FVector GetLockedAxis() const;
	void CreateDOFLock();

	static EDOFMode::Type ResolveDOFMode(EDOFMode::Type DOFMode);

	/** Constraint used to allow for easy DOF setup per bodyinstance */
	FConstraintInstance* DOFConstraint;

	/** The parent body that we are welded to*/
	FBodyInstance* WeldParent;

protected:

	/**
	* Custom walkable slope override setting for this instance.
	* @see GetWalkableSlopeOverride(), SetWalkableSlopeOverride()
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideWalkableSlopeOnInstance"))
	struct FWalkableSlopeOverride WalkableSlopeOverride;

	/**	Allows you to override the PhysicalMaterial to use for simple collision on this body. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision)
	class UPhysicalMaterial* PhysMaterialOverride;

public:
	/** The maximum angular velocity for this instance */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics, meta = (editcondition = "bOverrideMaxAngularVelocity"))
	float MaxAngularVelocity;


	/** If the SleepFamily is set to custom, multiply the natural sleep threshold by this amount. A higher number will cause the body to sleep sooner. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics)
	float CustomSleepThresholdMultiplier;

	/** Stabilization factor for this body if Physics stabilization is enabled. A higher number will cause more aggressive stabilization at the risk of loss of momentum at low speeds. A value of 0 will disable stabilization for this body.*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Physics)
	float StabilizationThresholdMultiplier;

	/**	Influence of rigid body physics (blending) on the mesh's pose (0.0 == use only animation, 1.0 == use only physics) */
	/** Provide appropriate interface for doing this instead of allowing BlueprintReadWrite **/
	UPROPERTY()
	float PhysicsBlendWeight;

	/** This physics body's solver iteration count for position. Increasing this will be more CPU intensive, but better stabilized.  */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Physics)
	int32 PositionSolverIterationCount;

	/** This physics body's solver iteration count for velocity. Increasing this will be more CPU intensive, but better stabilized. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Physics)
	int32 VelocitySolverIterationCount;

public:

	const FPhysicsActorHandle& GetPhysicsActorHandle() const;
	const FPhysicsActorHandle& GetActorReferenceWithWelding() const;

	// Internal physics representation of our body instance
	FPhysicsActorHandle ActorHandle;

	TSharedPtr<TArray<ANSICHAR>> CharDebugName;

	/** PrimitiveComponent containing this body.   */
	TWeakObjectPtr<class UPrimitiveComponent> OwnerComponent;

	/** BodySetup pointer that this instance is initialized from */
	TWeakObjectPtr<UBodySetup> BodySetup;

	/** Constructor **/
	FBodyInstance();

	/**  
	 * Update profile data if required
	 * 
	 * @param : bVerifyProfile - if true, it makes sure it has correct set up with current profile, if false, it overwrites from profile data
	 *								(for backward compatibility)
	 * 
	 **/
	void LoadProfileData(bool bVerifyProfile);

	/** Helper struct to specify spawn behavior */
	struct FInitBodySpawnParams
	{
		ENGINE_API FInitBodySpawnParams(const UPrimitiveComponent* PrimComp);

		/** Whether the created physics actor will be static */
		bool bStaticPhysics;

		/** Whether to use the BodySetup's PhysicsType to override if the instance simulates*/
		bool bPhysicsTypeDeterminesSimulation;

		/** Whether to override the physics scene used for simulation */
		EDynamicActorScene DynamicActorScene;

		/** An aggregate to place the body into */
		FPhysicsAggregateHandle Aggregate;
	};

	void InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* PrimComp, FPhysScene* InRBScene)
	{
		InitBody(Setup, Transform, PrimComp, InRBScene, FInitBodySpawnParams(PrimComp));
	}


	/** Initialise a single rigid body (this FBodyInstance) for the given body setup
	*	@param Setup The setup to use to create the body
	*	@param Transform Transform of the body
	*	@param PrimComp The owning component
	*	@param InRBScene The physics scene to place the body into
	*	@param SpawnParams The parameters for determining certain spawn behavior
	*	@param InAggregate An aggregate to place the body into
	*/
	void InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* PrimComp, FPhysScene* InRBScene, const FInitBodySpawnParams& SpawnParams);

	/** Validate a body transform, outputting debug info
	 *	@param Transform Transform to debug
	 *	@param DebugName Name of the instance for logging
	 *	@param Setup Body setup for this instance
	 */
	static bool ValidateTransform(const FTransform &Transform, const FString& DebugName, const UBodySetup* Setup);

	/** Standalone path to batch initialize large amounts of static bodies, which will be deferred till the next scene update for fast scene addition.
	 *	@param Bodies
	 *	@param Transforms
	 *	@param BodySetup
	 *	@param PrimitiveComp
	 *	@param InRBScene
	 */
	static void InitStaticBodies(const TArray<FBodyInstance*>& Bodies, const TArray<FTransform>& Transforms, UBodySetup* BodySetup, class UPrimitiveComponent* PrimitiveComp, FPhysScene* InRBScene);


	/** Get the scene that owns this body. */
	FPhysScene* GetPhysicsScene() const;

	/** Initialise dynamic properties for this instance when using physics - this must be done after scene addition.
	 *  Note: This function is not thread safe. Make sure to obtain the appropriate physics scene locks before calling this function
	 */
	void InitDynamicProperties_AssumesLocked();

	/** Build the sim and query filter data (for simple and complex shapes) based on the settings of this BodyInstance (and its associated BodySetup)  */
	void BuildBodyFilterData(FBodyCollisionFilterData& OutFilterData) const;

	/** Build the flags to control which types of collision (sim and query) shapes owned by this BodyInstance should have. */
	static void BuildBodyCollisionFlags(FBodyCollisionFlags& OutFlags, ECollisionEnabled::Type UseCollisionEnabled, bool bUseComplexAsSimple);

	/** 
	 *	Utility to get all the shapes from a FBodyInstance 
	 *	Shapes belonging to sync actor are first, then async. Number of shapes belonging to sync actor is returned.
	 *	NOTE: This function is not thread safe. You must hold the physics scene lock while calling it and reading/writing from the shapes
	 */
	int32 GetAllShapes_AssumesLocked(TArray<FPhysicsShapeHandle>& OutShapes) const;

	/**
	 * Terminates the body, releasing resources
	 * @param bNeverDeferRelease In some cases orphaned actors can have their internal release deferred. If this isn't desired this flag will override that behavior
	 */
	void TermBody(bool bNeverDeferRelease = false);

	/** 
	 * Takes two body instances and welds them together to create a single simulated rigid body. Returns true if success.
	 */
	bool Weld(FBodyInstance* Body, const FTransform& RelativeTM);

	/** 
	 * Takes a welded body and unwelds it. This function does not create the new body, it only removes the old one */
	void UnWeld(FBodyInstance* Body);

	/** Finds all children that are technically welded to us (for example kinematics are welded but not as far as physx is concerned) and apply the actual physics engine weld on them*/
	void ApplyWeldOnChildren();

	/**
	 * After adding/removing shapes call this function to update mass distribution etc... */
	void PostShapeChange();

	/**
	 * Update Body Scale
	 * @param	InScale3D		New Scale3D. If that's different from previous Scale3D, it will update Body scale.
	 * @param	bForceUpdate	Will refresh shape dimensions from BodySetup, even if scale has not changed.
	 * @return true if succeed
	 */
	bool UpdateBodyScale(const FVector& InScale3D, bool bForceUpdate = false);

	/** Dynamically update the vertices of per-poly collision for this body. */
	void UpdateTriMeshVertices(const TArray<FVector> & NewPositions);

	/** Returns the center of mass of this body (in world space) */
	FVector GetCOMPosition() const
	{
		return GetMassSpaceToWorldSpace().GetLocation();
	}

	/** Returns the mass coordinate system to world space transform (position is world center of mass, rotation is world inertia orientation) */
	FTransform GetMassSpaceToWorldSpace() const;
	FTransform GetMassSpaceLocal() const;

	/** TODO: this only works at runtime when the physics state has been created. Any changes that result in recomputing mass properties will not properly remember this */
	void SetMassSpaceLocal(const FTransform& NewMassSpaceLocalTM);

	/** Draws the center of mass as a wire star */
	void DrawCOMPosition(class FPrimitiveDrawInterface* PDI, float COMRenderSize, const FColor& COMRenderColor);

	/** Utility for copying properties from one BodyInstance to another. */
	void CopyBodyInstancePropertiesFrom(const FBodyInstance* FromInst);

	/** Find the correct PhysicalMaterial for simple geometry on this body */
	UPhysicalMaterial* GetSimplePhysicalMaterial() const;

	/** Find the correct PhysicalMaterial for simple geometry on a given body and owner. This is really for internal use during serialization */
	static UPhysicalMaterial* GetSimplePhysicalMaterial(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> Owner, TWeakObjectPtr<UBodySetup> BodySetupPtr);

	/** Get the complex PhysicalMaterial array for this body */
	TArray<UPhysicalMaterial*> GetComplexPhysicalMaterials() const;

	/** Find the correct PhysicalMaterial for simple geometry on a given body and owner. This is really for internal use during serialization */
	static void GetComplexPhysicalMaterials(const FBodyInstance* BodyInstance, TWeakObjectPtr<UPrimitiveComponent> Owner, TArray<UPhysicalMaterial*>& OutPhysicalMaterials);

	/** Get the complex PhysicalMaterials for this body */
	void GetComplexPhysicalMaterials(TArray<UPhysicalMaterial*> &PhysMaterials) const;

	/** Returns the slope override struct for this instance. If we don't have our own custom setting, it will return the setting from the body setup. */
	const struct FWalkableSlopeOverride& GetWalkableSlopeOverride() const;

	/** Sets a custom slope override struct for this instance. Implicitly sets bOverrideWalkableSlopeOnInstance to true. */
	void SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride);

	bool UseAsyncScene(const FPhysScene* PhysScene) const;

	bool HasSharedShapes() const{ return bHasSharedShapes; }

	/** Indicates whether this body should use the async scene. Must be called before body is init'd, will assert otherwise. Will have no affect if there is no async scene. */
	void SetUseAsyncScene(bool bNewUseAsyncScene);

	/** Returns true if the body is not static */
	bool IsDynamic() const;

	/** Returns true if the body is non kinematic*/
	bool IsNonKinematic() const;

	/** Returns the body's mass */
	float GetBodyMass() const;
	/** Return bounds of physics representation */
	FBox GetBodyBounds() const;
	/** Return the body's inertia tensor. This is returned in local mass space */
	FVector GetBodyInertiaTensor() const;


	/** Set this body to be fixed (kinematic) or not. */
	void SetInstanceSimulatePhysics(bool bSimulate, bool bMaintainPhysicsBlending=false);
	/** Makes sure the current kinematic state matches the simulate flag */
	void UpdateInstanceSimulatePhysics();
	/** Returns true if this body is simulating, false if it is fixed (kinematic) */
	bool IsInstanceSimulatingPhysics() const;
	/** Should Simulate Physics **/
	bool ShouldInstanceSimulatingPhysics() const;
	/** Returns whether this body is awake */
	bool IsInstanceAwake() const;
	/** Wake this body */
	void WakeInstance();
	/** Force this body to sleep */
	void PutInstanceToSleep();
	/** Gets the multiplier to the threshold where the body will go to sleep automatically. */
	float GetSleepThresholdMultiplier() const;
	/** Add custom forces and torques on the body. The callback will be called more than once, if substepping enabled, for every substep.  */
	void AddCustomPhysics(FCalculateCustomPhysics& CalculateCustomPhysics);
	/** Add a force to this body */
	void AddForce(const FVector& Force, bool bAllowSubstepping = true, bool bAccelChange = false);
	/** Add a force at a particular position (world space when bIsLocalForce = false, body space otherwise) */
	void AddForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping = true, bool bIsLocalForce = false);
	/** Clear accumulated forces on this body */
	void ClearForces(bool bAllowSubstepping = true);

	/** Add a torque to this body */
	void AddTorqueInRadians(const FVector& Torque, bool bAllowSubstepping = true, bool bAccelChange = false);
	/** Clear accumulated torques on this body */
	void ClearTorques(bool bAllowSubstepping = true);

	/** Add a rotational impulse to this body */
	void AddAngularImpulseInRadians(const FVector& Impulse, bool bVelChange);

	/** Add an impulse to this body */
	void AddImpulse(const FVector& Impulse, bool bVelChange);
	/** Add an impulse to this body and a particular world position */
	void AddImpulseAtPosition(const FVector& Impulse, const FVector& Position);
	/** Set the linear velocity of this body */
	void SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent, bool bAutoWake = true);

	/** Set the angular velocity of this body */
	void SetAngularVelocityInRadians(const FVector& NewAngVel, bool bAddToCurrent, bool bAutoWake = true);

	/** Set the maximum angular velocity of this body */
	void SetMaxAngularVelocityInRadians(float NewMaxAngVel, bool bAddToCurrent, bool bUpdateOverrideMaxAngularVelocity = true);

	/** Get the maximum angular velocity of this body */
	float GetMaxAngularVelocityInRadians() const;

	/** Set the maximum depenetration velocity the physics simulation will introduce */
	void SetMaxDepenetrationVelocity(float MaxVelocity);
	/** Set whether we should get a notification about physics collisions */
	void SetInstanceNotifyRBCollision(bool bNewNotifyCollision);
	/** Enables/disables whether this body is affected by gravity. */
	void SetEnableGravity(bool bGravityEnabled);
	/** Enables/disables contact modification */
	void SetContactModification(bool bNewContactModification);

	/** Enable/disable Continuous Collidion Detection feature */
	void SetUseCCD(bool bInUseCCD);

	/** Custom projection for physics (callback to update component transform based on physics data) */
	FCalculateCustomProjection OnCalculateCustomProjection;

	/** Called whenever mass properties have been re-calculated. */
	FRecalculatedMassProperties OnRecalculatedMassProperties;

	/** See if this body is valid. */
	bool IsValidBodyInstance() const;

	/** Get current transform in world space from physics body. */
	FTransform GetUnrealWorldTransform(bool bWithProjection = true, bool bForceGlobalPose = false) const;

	/** Get current transform in world space from physics body. */
	FTransform GetUnrealWorldTransform_AssumesLocked(bool bWithProjection = true, bool bForceGlobalPose = false) const;

	/**
	 *	Move the physics body to a new pose.
	 *	@param	bTeleport	If true, no velocity is inferred on the kinematic body from this movement, but it moves right away.
	 */
	void SetBodyTransform(const FTransform& NewTransform, ETeleportType Teleport, bool bAutoWake = true);

	/** Get current velocity in world space from physics body. */
	FVector GetUnrealWorldVelocity() const;

	/** Get current velocity in world space from physics body. */
	FVector GetUnrealWorldVelocity_AssumesLocked() const;

	/** Get current angular velocity in world space from physics body. */
	FVector GetUnrealWorldAngularVelocityInRadians() const;

	/** Get current angular velocity in world space from physics body. */
	FVector GetUnrealWorldAngularVelocityInRadians_AssumesLocked() const;

	/** Get current velocity of a point on this physics body, in world space. Point is specified in world space. */
	FVector GetUnrealWorldVelocityAtPoint(const FVector& Point) const;

	/** Get current velocity of a point on this physics body, in world space. Point is specified in world space. */
	FVector GetUnrealWorldVelocityAtPoint_AssumesLocked(const FVector& Point) const;

	/** Set physical material override for this body */
	void SetPhysMaterialOverride(class UPhysicalMaterial* NewPhysMaterial);

	/** Set a new contact report force threhold.  Threshold < 0 disables this feature. */
	void SetContactReportForceThreshold(float Threshold);

	/** Set the collision response of this body to a particular channel */
	void SetResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);

	/** Get the collision response of this body to a particular channel */
	FORCEINLINE_DEBUGGABLE ECollisionResponse GetResponseToChannel(ECollisionChannel Channel) const { return CollisionResponses.GetResponse(Channel); }

	/** Set the response of this body to all channels */
	void SetResponseToAllChannels(ECollisionResponse NewResponse);

	/** Replace the channels on this body matching the old response with the new response */
	void ReplaceResponseToChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse);

	/** Set the response of this body to the supplied settings */
	void SetResponseToChannels(const FCollisionResponseContainer& NewReponses);

	/** Get Collision ResponseToChannels container for this component **/
	FORCEINLINE_DEBUGGABLE const FCollisionResponseContainer& GetResponseToChannels() const { return CollisionResponses.GetResponseContainer(); }

	/** Set the movement channel of this body to the one supplied */
	void SetObjectType(ECollisionChannel Channel);

	/** Get the movement channel of this body **/
	FORCEINLINE_DEBUGGABLE ECollisionChannel GetObjectType() const { return ObjectType; }

	/** Controls what kind of collision is enabled for this body and allows optional disable physics rebuild */
	void SetCollisionEnabled(ECollisionEnabled::Type NewType, bool bUpdatePhysicsFilterData = true);

private:

	ECollisionEnabled::Type GetCollisionEnabled_CheckOwner() const;

public:
	/** Get the current type of collision enabled */
	FORCEINLINE ECollisionEnabled::Type GetCollisionEnabled(bool bCheckOwner = true) const
	{
		return (bCheckOwner ? GetCollisionEnabled_CheckOwner() : CollisionEnabled.GetValue());
	}

	/**  
	 * Set Collision Profile Name (deferred)
	 * This function is called by constructors when they set ProfileName
	 * This will change current CollisionProfileName, but collision data will not be set up until the physics state is created
	 * or the collision profile is accessed.
	 * @param InCollisionProfileName : New Profile Name
	 */
	void SetCollisionProfileNameDeferred(FName InCollisionProfileName);

	/**  
	 * Set Collision Profile Name
	 * This function should be called outside of constructors to set profile name.
	 * This will change current CollisionProfileName to be this, and overwrite Collision Setting
	 * 
	 * @param InCollisionProfileName : New Profile Name
	 */
	void SetCollisionProfileName(FName InCollisionProfileName);

	/** Updates the mask filter. */
	void SetMaskFilter(FMaskFilter InMaskFilter);

	/** Return the ignore mask filter. */
	FORCEINLINE FMaskFilter GetMaskFilter() const { return MaskFilter; }
	/** Returns the collision profile name that will be used. */
	FName GetCollisionProfileName() const;

	/** return true if it uses Collision Profile System. False otherwise*/
	bool DoesUseCollisionProfile() const;

	/** Modify the mass scale of this body */
	void SetMassScale(float InMassScale=1.f);

	/** Update instance's mass properties (mass, inertia and center-of-mass offset) based on MassScale, InstanceMassScale and COMNudge. */
	void UpdateMassProperties();

	/** Update instance's linear and angular damping */
	void UpdateDampingProperties();

	/** Update the instance's material properties (friction, restitution) */
	void UpdatePhysicalMaterials();

	/** 
	 *  Apply a material directly to the passed in shape. Note this function is very advanced and requires knowledge of shape sharing as well as threading. Note: assumes the appropriate locks have been obtained
	 *  @param  PShape					The shape we are applying the material to
	 *  @param  SimplePhysMat			The material to use if a simple shape is provided (or complex materials are empty)
	 *  @param  ComplexPhysMats			The array of materials to apply if a complex shape is provided
	 *	@param	bSharedShape			If this is true it means you've already detached the shape from all actors that use it (attached shared shapes are not writable).
	 */
	static void ApplyMaterialToShape_AssumesLocked(const FPhysicsShapeHandle& InShape, UPhysicalMaterial* SimplePhysMat, const TArrayView<UPhysicalMaterial*>& ComplexPhysMats, const bool bSharedShape);

	/** Note: This function is not thread safe. Make sure you obtain the appropriate physics scene lock before calling it*/
	void ApplyMaterialToInstanceShapes_AssumesLocked(UPhysicalMaterial* SimplePhysMat, TArray<UPhysicalMaterial*>& ComplexPhysMats);

	/** Update the instances collision filtering data */ 
	void UpdatePhysicsFilterData();

	friend FArchive& operator<<(FArchive& Ar,FBodyInstance& BodyInst);

	/** Get the name for this body, for use in debugging */
	FString GetBodyDebugName() const;

	/** 
	 *  Trace a ray against just this bodyinstance
	 *  @param  OutHit					Information about hit against this component, if true is returned
	 *  @param  Start					Start location of the ray
	 *  @param  End						End location of the ray
	 *	@param	bTraceComplex			Should we trace against complex or simple collision of this body
	 *  @param bReturnPhysicalMaterial	Fill in the PhysMaterial field of OutHit
	 *  @return true if a hit is found
	 */
	bool LineTrace(struct FHitResult& OutHit, const FVector& Start, const FVector& End, bool bTraceComplex, bool bReturnPhysicalMaterial = false) const;

	/** 
	 *  Trace a shape against just this bodyinstance
	 *  @param  OutHit          	Information about hit against this component, if true is returned
	 *  @param  Start           	Start location of the box
	 *  @param  End             	End location of the box
	 *  @param  ShapeWorldRotation  The rotation applied to the collision shape in world space.
	 *  @param  CollisionShape		Collision Shape
	 *	@param	bTraceComplex		Should we trace against complex or simple collision of this body
	 *  @return true if a hit is found
	 */
	bool Sweep(struct FHitResult& OutHit, const FVector& Start, const FVector& End, const FQuat& ShapeWorldRotation, const FCollisionShape& Shape, bool bTraceComplex) const;

	/**
	 *  Test if the bodyinstance overlaps with the specified shape at the specified position/rotation
	 *
	 *  @param  Position		Position to place the shape at before testing
	 *  @param  Rotation		Rotation to apply to the shape before testing
	 *	@param	CollisionShape	Shape to test against
	 *  @param  OutMTD			The minimum translation direction needed to push the shape out of this BodyInstance. (Optional)
	 *  @return true if the geometry associated with this body instance overlaps the query shape at the specified location/rotation
	 */
	bool OverlapTest(const FVector& Position, const FQuat& Rotation, const struct FCollisionShape& CollisionShape, FMTDResult* OutMTD = nullptr) const;

	/**
	 *  Test if the bodyinstance overlaps with the specified body instances
	 *
	 *  @param  Position		Position to place our shapes at before testing (shapes of this BodyInstance)
	 *  @param  Rotation		Rotation to apply to our shapes before testing (shapes of this BodyInstance)
	 *  @param  Bodies			The bodies we are testing for overlap with. These bodies will be in world space already
	 *  @return true if any of the bodies passed in overlap with this
	 */
	bool OverlapTestForBodies(const FVector& Position, const FQuat& Rotation, const TArray<FBodyInstance*>& Bodies) const;
	bool OverlapTestForBody(const FVector& Position, const FQuat& Rotation, FBodyInstance* Body) const;

	/**
	 *  Determines the set of components that this body instance would overlap with at the supplied location/rotation
	 *  @note The overload taking rotation as an FQuat is slightly faster than the version using FRotator (which will be converted to an FQuat)..
	 *  @param  InOutOverlaps   Array of overlaps found between this component in specified pose and the world (new overlaps will be appended, the array is not cleared!)
	 *  @param  World			World to use for overlap test
	 *  @param  pWorldToComponent Used to convert the body instance world space position into local space before teleporting it to (Pos, Rot) [optional, use when the body instance isn't centered on the component instance]
	 *  @param  Pos             Location to place the component's geometry at to test against the world
	 *  @param  Rot             Rotation to place components' geometry at to test against the world
	 *  @param  TestChannel		The 'channel' that this ray is in, used to determine which components to hit
	 *  @param  Params
	 *  @param  ResponseParams
	 *	@param	ObjectQueryParams	List of object types it's looking for. When this enters, we do object query with component shape
	 *  @return TRUE if OutOverlaps contains any blocking results
	 */
	bool OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FQuat& Rot,    ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;
	bool OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;

	/**
	 * Add an impulse to this bodyinstance, radiating out from the specified position.
	 *
	 * @param Origin		Point of origin for the radial impulse blast, in world space
	 * @param Radius		Size of radial impulse. Beyond this distance from Origin, there will be no affect.
	 * @param Strength		Maximum strength of impulse applied to body.
	 * @param Falloff		Allows you to control the strength of the impulse as a function of distance from Origin.
	 * @param bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no effect).
	 */
	void AddRadialImpulseToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bVelChange = false);

	/**
	 *	Add a force to this bodyinstance, originating from the supplied world-space location.
	 *
	 *	@param Origin		Origin of force in world space.
	 *	@param Radius		Radius within which to apply the force.
	 *	@param Strength		Strength of force to apply.
	 *  @param Falloff		Allows you to control the strength of the force as a function of distance from Origin.
	 *  @param bAccelChange If true, Strength is taken as a change in acceleration instead of a physical force (i.e. mass will have no effect).
	 *  @param bAllowSubstepping Whether we should sub-step this radial force. You should only turn this off if you're calling it from a sub-step callback, otherwise there will be energy loss
	 */
	void AddRadialForceToBody(const FVector& Origin, float Radius, float Strength, uint8 Falloff, bool bAccelChange = false, bool bAllowSubstepping = true);

	/**
	 * Get distance to the body surface if available
	 * It is only valid if BodyShape is convex
	 * If point is inside distance it will be 0
	 * Returns false if geometry is not supported
	 *
	 * @param Point				Point in world space
	 * @param OutDistanceSquared How far from the instance the point is. 0 if inside the shape
	 * @param OutPointOnBody	Point on the surface of body closest to Point
	 * @return true if a distance to the body was found and OutDistanceSquared has been populated
	 */
	bool GetSquaredDistanceToBody(const FVector& Point, float& OutDistanceSquared, FVector& OutPointOnBody) const;

	/**
	* Get the square of the distance to the body surface if available
	* It is only valid if BodyShape is convex
	* If point is inside or shape is not convex, it will return 0.f
	*
	* @param Point				Point in world space
	* @param OutPointOnBody	Point on the surface of body closest to Point
	*/
	float GetDistanceToBody(const FVector& Point, FVector& OutPointOnBody) const;

	/** 
	 * Returns memory used by resources allocated for this body instance ( ex. physics resources )
	 **/
	void GetBodyInstanceResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) const;

	/**
	 * UObject notification by OwningComponent
	 */
	void FixupData(class UObject* Loader);

	const FCollisionResponse& GetCollisionResponse() const { return CollisionResponses; }

	/** Applies a deferred collision profile */
	void ApplyDeferredCollisionProfileName();

	/** Returns the original owning body instance. This is needed for welding */
	const FBodyInstance* GetOriginalBodyInstance(const FPhysicsShapeHandle& InShape) const;

	/** Returns the relative transform between root body and welded instance owned by the shape.*/
	const FTransform& GetRelativeBodyTransform(const FPhysicsShapeHandle& InShape) const;

	/** Check if the shape is owned by this body instance */
	bool IsShapeBoundToBody(const FPhysicsShapeHandle& Shape) const;

public:
	// #PHYS2 Rename, not just for physx now.
	FPhysxUserData PhysxUserData;

	struct FWeldInfo
	{
		FWeldInfo(FBodyInstance* InChildBI, const FTransform& InRelativeTM)
			: ChildBI(InChildBI)
			, RelativeTM(InRelativeTM)
		{}

		FBodyInstance* ChildBI;
		FTransform RelativeTM;
	};

	const TMap<FPhysicsShapeHandle, FWeldInfo>* GetCurrentWeldInfo() const;

private:

	/**
	 * Invalidate Collision Profile Name
	 * This gets called when it invalidates the reason of Profile Name
	 * for example, they would like to re-define CollisionEnabled or ObjectType or ResponseChannels
	 */
	void InvalidateCollisionProfileName();

	/** Moves welded bodies within a rigid body (updates their shapes) */
	void SetWeldedBodyTransform(FBodyInstance* TheirBody, const FTransform& NewTransform);
		
	/**
	 * Return true if the collision profile name is valid
	 */
	static bool IsValidCollisionProfileName(FName InCollisionProfileName);

	template<typename AllocatorType>
	bool OverlapTestForBodiesImpl(const FVector& Position, const FQuat& Rotation, const TArray<FBodyInstance*, AllocatorType>& Bodies) const;

	friend class UPhysicsAsset;
	friend class UCollisionProfile;
	friend class FBodyInstanceCustomization;
	friend struct FUpdateCollisionResponseHelper;
	friend class FBodySetupDetails;
	
	friend struct FInitBodiesHelper<true>;
	friend struct FInitBodiesHelper<false>;
	friend class FBodyInstanceCustomizationHelper;
	friend class FFoliageTypeCustomizationHelpers;

private:

	void UpdateDebugRendering();

	/** Used to map between shapes and welded bodies. We do not create entries if the owning body instance is root*/
	TSharedPtr<TMap<FPhysicsShapeHandle, FWeldInfo>> ShapeToBodiesMap;

};

template<>
struct TStructOpsTypeTraits<FBodyInstance> : public TStructOpsTypeTraitsBase2<FBodyInstance>
{
	enum
	{
		WithCopy = false
	};
};


#if WITH_EDITOR

// Helper methods for classes with body instances
struct FBodyInstanceEditorHelpers
{
	ENGINE_API static void EnsureConsistentMobilitySimulationSettingsOnPostEditChange(UPrimitiveComponent* Component, FPropertyChangedEvent& PropertyChangedEvent);

private:
	FBodyInstanceEditorHelpers() {}
};
#endif

//////////////////////////////////////////////////////////////////////////
// BodyInstance inlines

/// @cond DOXYGEN_WARNINGS

FORCEINLINE_DEBUGGABLE bool FBodyInstance::OverlapMulti(TArray<struct FOverlapResult>& InOutOverlaps, const class UWorld* World, const FTransform* pWorldToComponent, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const FComponentQueryParams& Params, const FCollisionResponseParams& ResponseParams, const FCollisionObjectQueryParams& ObjectQueryParams) const
{
	// Pass on to FQuat version
	return OverlapMulti(InOutOverlaps, World, pWorldToComponent, Pos, Rot.Quaternion(), TestChannel, Params, ResponseParams, ObjectQueryParams);
}

/// @endcond

FORCEINLINE_DEBUGGABLE bool FBodyInstance::OverlapTestForBodies(const FVector& Position, const FQuat& Rotation, const TArray<FBodyInstance*>& Bodies) const
{
	return OverlapTestForBodiesImpl(Position, Rotation, Bodies);
}

FORCEINLINE_DEBUGGABLE bool FBodyInstance::OverlapTestForBody(const FVector& Position, const FQuat& Rotation, FBodyInstance* Body) const
{
	TArray<FBodyInstance*, TInlineAllocator<1>> InlineArray;
	InlineArray.Add(Body);
	return OverlapTestForBodiesImpl(Position, Rotation, InlineArray);
}

FORCEINLINE_DEBUGGABLE bool FBodyInstance::IsInstanceSimulatingPhysics() const
{
	return ShouldInstanceSimulatingPhysics() && IsValidBodyInstance();
}

extern template ENGINE_API bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Position, const FQuat& Rotation, const TArray<FBodyInstance*>& Bodies) const;
extern template ENGINE_API bool FBodyInstance::OverlapTestForBodiesImpl(const FVector& Position, const FQuat& Rotation, const TArray<FBodyInstance*, TInlineAllocator<1>>& Bodies) const;