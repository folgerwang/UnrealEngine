// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/**
* This file contains the core logic and types that support Object and RPC replication.
*
* These types don't dictate how RPCs are triggered or when an Object should be replicated,
* although there are some methods defined here that may be used in those determinations.
*
* Instead, the types here focus on how data from Objects, Structs, Containers, and Properties
* are generically tracked and serialized on both Clients and Servers.
*
* The main class is FRepLayout.
*/

#pragma once

#include "CoreMinimal.h"
#include "Misc/NetworkGuid.h"
#include "UObject/CoreNet.h"
#include "Engine/EngineTypes.h"
#include "UObject/GCObject.h"

class FGuidReferences;
class FNetFieldExportGroup;
class FRepLayout;
class UActorChannel;
class UNetConnection;
class UPackageMapClient;

// Properties will be copied in here so memory needs aligned to largest type
typedef TArray<uint8, TAlignedHeapAllocator<16>> FRepStateStaticBuffer;

enum class EDiffPropertiesFlags : uint32
{
	None = 0,
	Sync = (1 <<0),							//! Indicates that properties should be updated (synchronized), not just diffed.
	IncludeConditionalProperties = (1 <<1)	//! Whether or not conditional properties should be included.
};

ENUM_CLASS_FLAGS(EDiffPropertiesFlags);

/** Stores meta data about a given Replicated property. */
class FRepChangedParent
{
public:
	FRepChangedParent():
		Active(1),
		OldActive(1),
		IsConditional(0)
	{}

	/** Whether or not this property is currently Active (i.e., considered for replication). */
	uint32 Active: 1;

	/** The last updated state of Active, used to track when the Active state changes. */
	uint32 OldActive: 1;

	/**
	 * Whether or not this property has conditions that may exclude it from replicating to a given connection.
	 * @see FRepState::ConditionMap.
	 */
	uint32 IsConditional: 1;
};

/**
 * This class is used to store meta data about properties that is shared between connections,
 * including whether or not a given property is Conditional, Active, and any external data
 * that may be needed for Replays.
 *
 * TODO: This class (and arguably IRepChangedPropertyTracker) should be renamed to reflect
 *			what they actually do now.
 */
class FRepChangedPropertyTracker : public IRepChangedPropertyTracker
{
public:
	FRepChangedPropertyTracker(const bool InbIsReplay, const bool InbIsClientReplayRecording):
		bIsReplay(InbIsReplay),
		bIsClientReplayRecording(InbIsClientReplayRecording),
		ExternalDataNumBits(0)
	{}

	virtual ~FRepChangedPropertyTracker() {}

	//~ Begin IRepChangedPropertyTracker Interface.
	/**
	 * Manually set whether or not Property should be marked inactive.
	 * This will change the Active status for all connections.
	 *
	 * @see DOREPLIFETIME_ACTIVE_OVERRIDE
	 *
	 * @param RepIndex	Replication index for the Property.
	 * @param bIsActive	The new Active state.
	 */
	virtual void SetCustomIsActiveOverride(const uint16 RepIndex, const bool bIsActive) override
	{
		FRepChangedParent & Parent = Parents[RepIndex];

		checkSlow(Parent.IsConditional);

		Parent.Active = (bIsActive || bIsClientReplayRecording) ? 1 : 0;
		Parent.OldActive = Parent.Active;
	}

	/**
	 * Sets (or resets) the External Data.
	 * External Data is primarily used for Replays, and is used to track additional non-replicated
	 * data or state about an object.
	 *
	 * @param Src		Memory containing the external data.
	 * @param NumBits	Size of the memory, in bits.
	 */
	virtual void SetExternalData(const uint8* Src, const int32 NumBits) override
	{
		ExternalDataNumBits = NumBits;
		const int32 NumBytes = (NumBits + 7) >> 3;
		ExternalData.Reset(NumBytes);
		ExternalData.AddUninitialized(NumBytes);
		FMemory::Memcpy(ExternalData.GetData(), Src, NumBytes);
	}

	/** Whether or not this is being used for a replay (may be recording or playback). */
	virtual bool IsReplay() const override
	{
		return bIsReplay;
	}
	//~ End IRepChangedPropertyTracker Interface

	/** Activation data for top level Properties on the given Actor / Object. */
	TArray<FRepChangedParent>	Parents;

	/** Whether or not this is being used for a replay (may be recording or playback). */
	bool bIsReplay;

	/** Whether or not this is being used for a client replay recording. */
	bool bIsClientReplayRecording;

	TArray<uint8> ExternalData;
	uint32 ExternalDataNumBits;
};

class FRepLayout;
class FRepLayoutCmd;

/** Holds the unique identifier and offsets/lengths of a net serialized property used for Shared Serialization */
struct FRepSerializedPropertyInfo
{
	FRepSerializedPropertyInfo():
		BitOffset(0),
		BitLength(0),
		PropBitOffset(0),
		PropBitLength(0)
	{}

	/** Unique identifier for this property, may include array index and depth. */
	FGuid Guid;

	/** Bit offset into shared buffer of the shared data */
	int32 BitOffset;

	/** Length in bits of all serialized data for this property, may include handle and checksum. */
	int32 BitLength;

	/** Bit offset into shared buffer of the property data. */
	int32 PropBitOffset;

	/** Length in bits of net serialized property data only */
	int32 PropBitLength;
};

/** Holds a set of shared net serialized properties */
struct FRepSerializationSharedInfo
{
	FRepSerializationSharedInfo():
		SerializedProperties(MakeUnique<FNetBitWriter>(0)),
		bIsValid(false)
	{}

	void SetValid()
	{
		bIsValid = true;
	}

	bool IsValid() const
	{
		return bIsValid;
	}

	void Reset()
	{
		if (bIsValid)
		{
			SharedPropertyInfo.Reset();
			SerializedProperties->Reset();

			bIsValid = false;
		}
	}

	/**
	 * Creates a new SharedPropertyInfo and adds it to the SharedPropertyInfo list.
	 *
	 * @param Cmd				The command that represents the property we want to share.
	 * @param PropertyGuid		A guid used to identify the property.
	 * @param CmdIndex			Index of the property command. Only used if bDoChecksum is true.
	 * @param Handle			Relative Handle of the property command. Only used if bWriteHandle is true.
	 * @param Data				Pointer to the raw property memory that will be serialized.
	 * @param bWriteHandler		Whether or not we should write Command handles into the serialized data.
	 * @param bDoChecksum		Whether or not we should do checksums. Only used if ENABLE_PROPERTY_CHECKSUMS is enabled.
	 */
	const FRepSerializedPropertyInfo* WriteSharedProperty(
		const FRepLayoutCmd&	Cmd,
		const FGuid&			PropertyGuid,
		const int32				CmdIndex,
		const uint16			Handle,
		const uint8* RESTRICT	Data,
		const bool				bWriteHandle,
		const bool				bDoChecksum);

	/** Metadata for properties in the shared data blob. */
	TArray<FRepSerializedPropertyInfo> SharedPropertyInfo;

	/** Binary blob of net serialized data to be shared */
	TUniquePtr<FNetBitWriter> SerializedProperties;

private:

	/** Whether or not shared serialization data has been successfully built. */
	bool bIsValid;
};

/**
 * Represents a single changelist, tracking changed properties.
 *
 * Properties are tracked via Relative Property Command Handles.
 * Valid handles are 1-based, and 0 is reserved as a terminator.
 *
 * Arrays are tracked as a special case inline, where the first entry is the number of array elements,
 * followed by handles for each array element, and ending with their own 0 terminator.
 *
 * Arrays may be nested by continually applying that pattern.
 */
class FRepChangedHistory
{
public:
	FRepChangedHistory():
		Resend(false)
	{}

	void CountBytes(FArchive& Ar) const
	{
		Changed.CountBytes(Ar);
	}

	/** Range of the Packets that this changelist was last sent with. Used to track acknowledgments. */
	FPacketIdRange OutPacketIdRange;

	/** List of Property Command Handles that changed in this changelist. */
	TArray<uint16> Changed;

	/** Whether or not this Changelist should be resent due to a Nak. */
	bool Resend;
};

/**
 * Stores changelist history (that are used to know what properties have changed) for objects.
 *
 * Only a fixed number of history items are kept. Once that limit is reached, old entries are
 * merged into a single monolithic changelist (this happens incrementally each time a new entry
 * is added).
 */
class FRepChangelistState
{
public:

	FRepChangelistState() :
		HistoryStart(0),
		HistoryEnd(0),
		CompareIndex(0)
	{}

	~FRepChangelistState();

	TSharedPtr<FRepLayout> RepLayout;

	/** The maximum number of individual changelists allowed.*/
	static const int32 MAX_CHANGE_HISTORY = 64;

	/** Circular buffer of changelists. */
	FRepChangedHistory ChangeHistory[MAX_CHANGE_HISTORY];

	/** Index in the buffer where changelist history starts (i.e., the Oldest changelist). */
	int32 HistoryStart;

	/** Index in the buffer where changelist history ends (i.e., the Newest changelist). */
	int32 HistoryEnd;

	/** Number of times that properties have been compared */
	int32 CompareIndex;

	/** Latest state of all property data. Not used on Clients, only used on Servers if Shadow State is enabled. */
	FRepStateStaticBuffer StaticBuffer;

	/** Latest state of all shared serialization data. */
	FRepSerializationSharedInfo SharedSerialization;
};

class FGuidReferences;

typedef TMap<int32, FGuidReferences> FGuidReferencesMap;

/**
 * Helper class that tracks Replicated Properties that reference NetGUIDs (for Object Pointers / References).
 */
class FGuidReferences
{
public:
	FGuidReferences() : NumBufferBits(0), Array(NULL) {}

	FGuidReferences(
		FBitReader&					InReader,
		FBitReaderMark&				InMark,
		const TSet<FNetworkGUID>&	InUnmappedGUIDs,
		const TSet<FNetworkGUID>&	InMappedDynamicGUIDs,
		const int32					InParentIndex,
		const int32					InCmdIndex
	):
			UnmappedGUIDs(InUnmappedGUIDs),
			MappedDynamicGUIDs(InMappedDynamicGUIDs),
			Array(NULL),
			ParentIndex(InParentIndex),
			CmdIndex(InCmdIndex)
	{
		NumBufferBits = InReader.GetPosBits() - InMark.GetPos();
		InMark.Copy(InReader, Buffer);
	}

	FGuidReferences(
		FGuidReferencesMap*	InArray,
		const int32			InParentIndex,
		const int32			InCmdIndex
	):

			NumBufferBits(0),
			Array(InArray),
			ParentIndex(InParentIndex),
			CmdIndex(InCmdIndex)
	{}
		
	~FGuidReferences();

	void CountBytes(FArchive& Ar) const
	{
		UnmappedGUIDs.CountBytes(Ar);
		MappedDynamicGUIDs.CountBytes(Ar);
		Buffer.CountBytes(Ar);
	}


	/** GUIDs for objects that haven't been loaded / created yet. */
	TSet<FNetworkGUID> UnmappedGUIDs;


	/** GUIDs for dynamically spawned objects that have already been created. */
	TSet<FNetworkGUID> MappedDynamicGUIDs;
	TArray<uint8> Buffer;
	int32 NumBufferBits;

	FGuidReferencesMap* Array;

	/** The Property Command index of the top level property that references the GUID. */
	int32 ParentIndex;

	/** The Property Command index of the actual property that references the GUID. */
	int32 CmdIndex;
};

/** Replication State that is unique Per Object Per Net Connection. */
class FRepState
{
public:

	FRepState() : 
		HistoryStart(0), 
		HistoryEnd(0),
		NumNaks(0),
		OpenAckedCalled(false),
		AwakeFromDormancy(false),
		LastChangelistIndex(0),
		LastCompareIndex(0)
	{}

	~FRepState();

	void CountBytes(FArchive& Ar) const;

	/** Latest state of all property data. Used on Clients, or on Servers if Shadow State is disabled. */
	FRepStateStaticBuffer StaticBuffer;

	FGuidReferencesMap GuidReferencesMap;

	TSharedPtr<FRepLayout> RepLayout;

	/** Properties that have RepNotifies that we will need to call on Clients (and ListenServers). */
	TArray<UProperty *> RepNotifies;

	TSharedPtr<FRepChangedPropertyTracker> RepChangedPropertyTracker;

	/** The maximum number of individual changelists allowed.*/
	static const int32 MAX_CHANGE_HISTORY = 32;

	/** Circular buffer of changelists. */
	FRepChangedHistory ChangeHistory[MAX_CHANGE_HISTORY];

	/** Index in the buffer where changelist history starts (i.e., the Oldest changelist). */
	int32 HistoryStart;

	/** Index in the buffer where changelist history ends (i.e., the Newest changelist). */
	int32 HistoryEnd;

	/** Number of Changelist history entries that have outstanding Naks. */
	int32 NumNaks;

	/** List of changelists that were generated before the channel was fully opened.*/
	TArray<FRepChangedHistory> PreOpenAckHistory;

	/** Whether or not FRepLayout::OpenAcked has been called with this FRepState. */
	bool OpenAckedCalled;

	/** This property is no longer used. */
	bool AwakeFromDormancy;

	FReplicationFlags RepFlags;

	/** The unique list of properties that have changed since the channel was first opened */
	TArray<uint16> LifetimeChangelist;

	/**
	 * The last change list history item we replicated from FRepChangelistState.
	 * (If we are caught up to FRepChangelistState::HistoryEnd, there are no new changelists to replicate).
	 */
	int32 LastChangelistIndex;

	/**
	 * Tracks the last time this RepState actually replicated data.
	 * If this matches FRepChangelistState::CompareIndex, then there is definitely no new
	 * information since the last time we checked.
	 *
	 * @see FRepChangelistState::CompareIndex.
	 *
	 * Note, we can't solely rely on on LastChangelistIndex, since changelists are stored in circular buffers.
	 */
	int32 LastCompareIndex;

	/**
	 * A map tracking which replication conditions are currently active.
	 * @see ELifetimeCondition.
	 */
	bool ConditionMap[COND_Max];

	// Cache off the RemoteRole and Role per connection to avoid issues with
	// FScopedRoleDowngrade. See UE-66313 (among others).

	ENetRole SavedRemoteRole = ROLE_MAX;
	ENetRole SavedRole = ROLE_MAX;
};

/** Various types of Properties supported for Replication. */
enum class ERepLayoutCmdType : uint8
{
	DynamicArray			= 0,	//! Dynamic array
	Return					= 1,	//! Return from array, or end of stream
	Property				= 2,	//! Generic property

	PropertyBool			= 3,
	PropertyFloat			= 4,
	PropertyInt				= 5,
	PropertyByte			= 6,
	PropertyName			= 7,
	PropertyObject			= 8,
	PropertyUInt32			= 9,
	PropertyVector			= 10,
	PropertyRotator			= 11,
	PropertyPlane			= 12,
	PropertyVector100		= 13,
	PropertyNetId			= 14,
	RepMovement				= 15,
	PropertyVectorNormal	= 16,
	PropertyVector10		= 17,
	PropertyVectorQ			= 18,
	PropertyString			= 19,
	PropertyUInt64			= 20,
	PropertyNativeBool		= 21,
};

/** Various flags that describe how a Top Level Property should be handled. */
enum class ERepParentFlags : uint32
{
	None				= 0,
	IsLifetime			= (1 <<0),	//! This property is valid for the lifetime of the object (almost always set).
	IsConditional		= (1 <<1),	//! This property has a secondary condition to check
	IsConfig			= (1 <<2),	//! This property is defaulted from a config file
	IsCustomDelta		= (1 <<3)	//! This property uses custom delta compression
};

ENUM_CLASS_FLAGS(ERepParentFlags)

/**
 * A Top Level Property of a UClass, UStruct, or UFunction (arguments to a UFunction).
 *
 * @see FRepLayout
 */
class FRepParentCmd
{
public:

	FRepParentCmd(UProperty* InProperty, int32 InArrayIndex): 
		Property(InProperty), 
		ArrayIndex(InArrayIndex), 
		CmdStart(0), 
		CmdEnd(0), 
		RoleSwapIndex(-1), 
		Condition(COND_None),
		RepNotifyCondition(REPNOTIFY_OnChanged),
		Flags(ERepParentFlags::None)
	{}

	UProperty* Property;

	/**
	 * If the Property is a C-Style fixed size array, then a command will be created for every element in the array.
	 * This is the index of the element in the array for which the command represents.
	 *
	 * This will always be 0 for non array properties.
	 */
	int32 ArrayIndex;


	/**
	 * CmdStart and CmdEnd define the range of FRepLayoutCommands (by index in FRepLayouts Cmd array) of commands
	 * that are associated with this Parent Command.
	 *
	 * This is used to track and access nested Properties from the parent.
	 */
	uint16 CmdStart;

	/** @see CmdStart */
	uint16 CmdEnd;

	/**
	 * This value indicates whether or not this command needs to swapped, and what other
	 * command it should be swapped with.
	 *
	 * This is used for Role and RemoteRole which have inverted values on Servers and Clients.
	 */
	int32 RoleSwapIndex;

	ELifetimeCondition Condition;
	ELifetimeRepNotifyCondition	RepNotifyCondition;
	ERepParentFlags Flags;
};

/** Various flags that describe how a Property should be handled. */
enum class ERepLayoutFlags : uint8
{
	None					= 0,		//! No flags.
	IsSharedSerialization	= (1 <<0)	//! Indicates the property is eligible for shared serialization.
};

ENUM_CLASS_FLAGS(ERepLayoutFlags)

/**
 * Represents a single property, which could be either a Top Level Property, a Nested Struct Property,
 * or an element in a Dynamic Array.
 *
 * @see FRepLayout
 */
class FRepLayoutCmd
{
public:

	/** Pointer back to property, used for NetSerialize calls, etc. */
	UProperty* Property;

	/** For arrays, this is the cmd index to jump to, to skip this arrays inner elements. */
	uint16 EndCmd;

	/** For arrays, element size of data. */
	uint16 ElementSize;

	/** Absolute offset of property. */
	int32 Offset;

	/** Handle relative to start of array, or top list. */
	uint16 RelativeHandle;

	/** Index into Parents. */
	uint16 ParentIndex;

	/** Used to determine if property is still compatible */
	uint32 CompatibleChecksum;

	ERepLayoutCmdType Type;
	ERepLayoutFlags Flags;
};
	
/** Converts a relative handle to the appropriate index into the Cmds array */
class FHandleToCmdIndex
{
public:
	FHandleToCmdIndex():
		CmdIndex(INDEX_NONE)
	{
	}

	FHandleToCmdIndex(const int32 InHandleToCmdIndex):
		CmdIndex(InHandleToCmdIndex)
	{
	}

	FHandleToCmdIndex(FHandleToCmdIndex&& Other):
		CmdIndex(Other.CmdIndex),
		HandleToCmdIndex(MoveTemp(Other.HandleToCmdIndex))
	{
	}

	FHandleToCmdIndex& operator=(FHandleToCmdIndex&& Other)
	{
		if (this != &Other)
		{
			CmdIndex = Other.CmdIndex;
			HandleToCmdIndex = MoveTemp(Other.HandleToCmdIndex);
		}

		return *this;
	}

	int32 CmdIndex;
	TUniquePtr<TArray<FHandleToCmdIndex>> HandleToCmdIndex;
};

/**
 * Simple helper class to track state while iterating over changelists.
 * This class doesn't actually expose methods to do the iteration, or to retrieve
 * the current value.
 */
class FChangelistIterator
{
public:

	FChangelistIterator(const TArray<uint16>& InChanged, const int32 InChangedIndex):
		Changed(InChanged),
		ChangedIndex(InChangedIndex)
	{}

	/** Changelist that is being iterated. */
	const TArray<uint16>& Changed;

	/** Current index into the changelist. */
	int32 ChangedIndex;
};

/** Iterates over a changelist, taking each handle, and mapping to rep layout index, array index, etc. */
class FRepHandleIterator
{
public:

	FRepHandleIterator(
		FChangelistIterator&	InChangelistIterator,
		const TArray<FRepLayoutCmd>&		InCmds,
		const TArray<FHandleToCmdIndex>&	InHandleToCmdIndex,
		const int32							InElementSize,
		const int32							InMaxArrayIndex,
		const int32							InMinCmdIndex,
		const int32							InMaxCmdIndex
	):
		ChangelistIterator(InChangelistIterator),
		Cmds(InCmds),
		HandleToCmdIndex(InHandleToCmdIndex),
		NumHandlesPerElement(HandleToCmdIndex.Num()),
		ArrayElementSize(InElementSize),
		MaxArrayIndex(InMaxArrayIndex),
		MinCmdIndex(InMinCmdIndex),
		MaxCmdIndex(InMaxCmdIndex)
	{}

	/**
	 * Moves the iterator to the next available handle.
	 *
	 * @return True if the move was successful, false otherwise (e.g., end of the iteration range was reached).
	 */
	bool NextHandle();

	/**
	 * Skips all the handles associated with a dynamic array at the iterators current position.
	 *
	 * @return True if the move was successful, false otherwise (this will return True even if the next handle is the end).
	 */
	bool JumpOverArray();

	/**
	 * Gets the handle at the iterators current position without advancing it.
	 *
	 * @return The next Property Command handle.
	 */
	int32 PeekNextHandle() const;

	/** Used to track current state of the iteration. */
	FChangelistIterator& ChangelistIterator;

	/** List of all available Layout Commands. */
	const TArray<FRepLayoutCmd>& Cmds;

	/** Used to map Relative Handles to absolute Property Command Indices. */
	const TArray<FHandleToCmdIndex>& HandleToCmdIndex;

	/**
	 * The number of handles per Command.
	 * This should always be 1, except for Arrays.
	 */
	const int32 NumHandlesPerElement;

	/**
	 * Only used for Dynamic Arrays.
	 * @see FRepLayout ElementSize.
	 */
	const int32	ArrayElementSize;

	/**
	 * Number of elements in a Dynamic array.
	 * Should be 1 when iterating Top Level Properties or non-array properties.
	 */
	const int32	MaxArrayIndex;

	/** Lowest index in Cmds where the iterator can go. */
	const int32	MinCmdIndex;

	/** Highest index in Cmds where the iterator can go. */
	const int32	MaxCmdIndex;

	/** The current Relative Property Command handle. */
	int32 Handle;

	/** The current Property Command index. */
	int32 CmdIndex;

	/** The index of the current element in a dynamic array. */
	int32 ArrayIndex;

	/** The Byte offset of Serialized Property data for a dynamic array to the current element. */
	int32 ArrayOffset;
};

/**
 * This class holds all replicated properties for a given type (either a UClass, UStruct, or UFunction).
 * Helpers functions exist to read, write, and compare property state.
 *
 * There is only one FRepLayout for a given type, meaning all instances of the type share the FRepState.
 *
 * COMMANDS:
 *
 * All Properties in a RepLayout are represented as Layout Commands.
 * These commands dictate:
 *		- What the underlying data type is.
 *		- How the data is laid out in memory.
 *		- How the data should be serialized.
 *		- How the data should be compared (between instances of Objects, Structs, etc.).
 *		- Whether or not the data should trigger notifications on change (RepNotifies).
 *		- Whether or not the data is conditional (e.g. may be skipped when sending to some or all connections).
 *
 * Commands are split into 2 main types: Parent Commands (@see FRepParentCmd) and Child Commands (@see FRepLayoutCmd).
 *
 * A Parent Command represents a Top Level Property of the type represented by an FRepLayout.
 * A Child Command represents any Property (even nested properties).
 * Every Parent Command will have at least one associated Child Command.
 *
 * E.G.,
 *		Imagine an Object O, with 4 Properties, CA, DA, I, and S.
 *			CA is a fixed size C-Style array. This will generate 1 Parent Command and 1 Child Command for *each* element in the array.
 *			DA is a dynamic array (TArray). This will generate only 1 Parent Command and 1 Child Command, both referencing the array.
 *			S is a UStruct. This will generate 1 Parent Command and 1 Child Command for the Struct, and an additional Child Command for each property in the Struct.
 *			I is an integer (or other supported non-Struct type). This will generate 1 Parent Command and 1 Child Command for the int.
 *
 * CHANGELISTS
 *
 * Along with Layout Commands that describe the Properties in a type, RepLayout uses changelists to know
 * what Properties have changed between frames. @see FRepChangedHistory.
 *
 * Changelists are arrays of Property Handles that describe what Properties have changed, however they don't
 * track the actual values of the Properties.
 *
 * Property Handles are either Layout Command indices (in FRepLayout::Cmds), or array indices for Properties in
 * dynamic arrays.
 * Handles are 1-based, reserving 0 as a terminal case.
 *
 * In order to generate Changelists, Layout Commands are sequentially applied that compare the values
 * of an object's cached state to a object's current state. Any properties that are found to be different
 * will have their handle written into the changelist. This means handles within a changelists are
 * inherently ordered (with arrays inserted whose Handles are also ordered).
 *
 * When we want to replicate properties for an object, merge together any outstanding changelists
 * and then iterate over it using Layout Commands that serialize the necessary property data.
 *
 * Receiving is very similar, except the Handles are baked into the serialized data so no
 * explicit changelist is required. As each Handle is read, a Layout Command is applied
 * that serializes the data from the network bunch and applies it to an object.
 */
class FRepLayout : public FGCObject
{
	friend class FRepState;
	friend class FRepChangelistState;
	friend class UPackageMapClient;

public:
	FRepLayout():
		FirstNonCustomParent(0),
		RoleIndex(-1),
		RemoteRoleIndex(-1),
		Owner(NULL)
	{}

	/**
	 * Used to signal that the channel that owns a given object has been opened and acknowledged
	 * by a client.
	 *
	 * @param RepState	RepState for the Object whose channel was acked.
	 */
	void OpenAcked(FRepState * RepState) const;

	/**
	 * Used to initialize the given shadow data.
	 *
	 * Shadow Data / Shadow States are used to cache property data so that the Object's state can be
	 * compared between frames to see if any properties have changed. They are also used on clients
	 * to keep track of RepNotify state.
	 *
	 * This includes:
	 *		- Allocating memory for all Properties in the class.
	 *		- Constructing instances of each Property.
	 *		- Copying the values of the Properties from given object.
	 *
	 * @param ShadowData	The buffer where shadow data will be stored.
	 * @param Class			The class of the object represented by the input memory buffer.
	 * @param Src			Memory buffer storing object property data.
	 */
	void InitShadowData(
		TArray<uint8, TAlignedHeapAllocator<16>>&	ShadowData,
		UClass *									InObjectClass,
		const uint8 * const							Src) const;

	/**
	 * Used to initialize a FRepState.
	 *
	 * This includes:
	 *		- Initializing the ShadowData.
	 *		- Associating and validating the appropriate ChangedPropertyTracker.
	 *		- Building initial ConditionMap.
	 *
	 * @param RepState						The RepState to initialize.
	 * @param Class							The class of the object represented by the input memory.
	 * @param Src							Memory buffer storing object property data.
	 * @param InRepChangedPropertyTracker	The PropertyTracker we want to associate with the RepState.
	 */
	void InitRepState(
		FRepState *									RepState, 
		UClass *									InObjectClass, 
		const uint8 * const							Src, 
		TSharedPtr<FRepChangedPropertyTracker> &	InRepChangedPropertyTracker) const;

	void InitChangedTracker(FRepChangedPropertyTracker * ChangedTracker) const;

	/**
	 * Writes out any changed properties for an Object into the given data buffer,
	 * and does book keeping for the RepState of the object.
	 *
	 * Note, this does not compare properties or send them on the wire, it's only used
	 * to serialize properties.
	 *
	 * @param RepState				RepState for the object.
	 * @param RepChangelistState	RepChangelistState for the object.
	 * @param Data					Pointer to memory where property data is stored.
	 * @param ObjectClass			Class of the object.
	 * @param Writer				Writer used to store / write out the replicated properties.
	 * @param RepFlags				Flags used for replication.
	 */
	bool ReplicateProperties(
		FRepState* RESTRICT				RepState,
		FRepChangelistState* RESTRICT	RepChangelistState,
		const uint8* RESTRICT			Data,
		UClass*							ObjectClass,
		UActorChannel *					OwningChannel,
		FNetBitWriter&					Writer,
		const FReplicationFlags &		RepFlags) const;

	/**
	 * Writes all changed property values from the input owner data to the given buffer.
	 * This is used primarily by ReplicateProperties.
	 *
	 * @param RepState			RepState for the object.
	 * @param ChangedTracker	Used to indicate
	 * @param Data				Pointer to the object's memory.
	 * @param ObjectClass		Class of the object.
	 * @param Writer			Writer used to store / write out the replicated properties.
	 * @param Changed			Aggregate list of property handles that need to be written.
	 * @param SharedInfo		Shared Serialization state for properties.
	 */
	void SendProperties(
		FRepState*	RESTRICT				RepState,
		FRepChangedPropertyTracker*			ChangedTracker,
		const uint8* RESTRICT				Data,
		UClass*								ObjectClass,
		FNetBitWriter&						Writer,
		TArray<uint16>&						Changed,
		const FRepSerializationSharedInfo&	SharedInfo) const;

	ENGINE_API void InitFromObjectClass(UClass * InObjectClass, const UNetConnection* ServerConnection = nullptr);

	/**
	 * Reads all property values from the received buffer, and applies them to the
	 * property memory.
	 *
	 * @param OwningChannel			The channel of the Actor that owns the object whose properties we're reading.
	 * @param InObjectClass			Class of the object.
	 * @param RepState				RepState for the object.
	 * @param Data					Pointer to memory where read property data should be stored.
	 * @param InBunch				The data that should be read.
	 * @param bOutHasUnmapped		Whether or not unmapped GUIDs were read.
	 * @param bEnableRepNotifies	Whether or not RepNotifies will be fired due to changed properties.
	 * @param bOutGuidsChanged		Whether or not any GUIDs were changed.
	 */
	bool ReceiveProperties(
		UActorChannel*			OwningChannel,
		UClass*					InObjectClass,
		FRepState* RESTRICT		RepState,
		void* RESTRICT			Data,
		FNetBitReader&			InBunch,
		bool&					bOutHasUnmapped,
		const bool				bEnableRepNotifies,
		bool&					bOutGuidsChanged) const;

	void GatherGuidReferences(
		FRepState*			RepState,
		TSet<FNetworkGUID>&	OutReferencedGuids,
		int32&				OutTrackedGuidMemoryBytes) const;

	bool MoveMappedObjectToUnmapped(FRepState* RepState, const FNetworkGUID& GUID) const;

	void UpdateUnmappedObjects(
		FRepState*		RepState,
		UPackageMap*	PackageMap,
		UObject*		Object,
		bool&			bOutSomeObjectsWereMapped,
		bool&			bOutHasMoreUnmapped) const;

	void CallRepNotifies(FRepState * RepState, UObject* Object) const;

	void PostReplicate(
		FRepState*		RepState,
		FPacketIdRange&	PacketRange,
		bool			bReliable) const;

	void ReceivedNak(FRepState * RepState, int32 NakPacketId) const;
	bool AllAcked(FRepState * RepState) const;
	bool ReadyForDormancy(FRepState * RepState) const;

	void ValidateWithChecksum(const void* RESTRICT Data, FBitArchive & Ar) const;
	uint32 GenerateChecksum(const FRepState* RepState) const;

	/** Clamp the changelist so that it conforms to the current size of either the array, or arrays within structs/arrays */
	void PruneChangeList(
		FRepState*				RepState,
		const void* RESTRICT	Data,
		const TArray<uint16>&	Changed,
		TArray<uint16>&			PrunedChanged) const;

	/**
	 * Combines two changelists, ensuring that handles are in the correct order, and arrays are properly structured.
	 *
	 * @param Data			Property Data for the Object / RepState the changelists refer to.
	 * @param Dirty1		First changelist to merge. Must be non-empty, valid changelist.
	 * @param Dirty2		Second changelist to merge. May be empty, or otherwise valid changelist.
	 * @param MergedDirty	The combined changelist.
	 */
	void MergeChangeList(
		const uint8* RESTRICT	Data,
		const TArray<uint16>&	Dirty1,
		const TArray<uint16>&	Dirty2,
		TArray<uint16>&			MergedDirty) const;

	UE_DEPRECATED(4.20, "Use the DiffProperties overload with the EDiffPropertiesFlags parameter")
	bool DiffProperties(TArray<UProperty*>* RepNotifies, void* RESTRICT Destination, const void* RESTRICT Source, const bool bSync) const;

	/**
	 * Compare all properties between source and destination buffer, and optionally update the destination
	 * buffer to match the state of the source buffer if they don't match.
	 *
	 * @param RepNotifies	RepNotifies that should be fired if we're changing properties.
	 * @param Destination	Destination buffer that will be changed if we're changing properties.
	 * @param Source		Source buffer containing desired property values.
	 * @param Flags			Controls how DiffProperties behaves.
	 *
	 * @return True if there were any properties with different values.
	 */
	bool DiffProperties(
		TArray<UProperty*>*			RepNotifies,
		void* RESTRICT				Destination,
		const void* RESTRICT		Source,
		const EDiffPropertiesFlags	Flags) const;

	/**
	 * @see DiffProperties
	 *
	 * The main difference between this method and DiffProperties is that this method will skip
	 * any properties that are:
	 *
	 *	- Transient
	 *	- Point to Actors or ActorComponents
	 *	- Point to Objects that are non-stably named for networking.
	 *
	 * @param RepNotifies	RepNotifies that should be fired if we're changing properties.
	 * @param Destination	Destination buffer that will be changed if we're changing properties.
	 * @param Source		Source buffer containing desired property values.
	 * @param Flags			Controls how DiffProperties behaves.
	 *
	 * @return True if there were any properties with different values.
	 */
	bool DiffStableProperties(
		TArray<UProperty*>*		RepNotifies,
		TArray<UObject*>*		ObjReferences,
		void* RESTRICT			Destination,
		const void* RESTRICT	Source) const;

	void GetLifetimeCustomDeltaProperties(TArray<int32>& OutCustom, TArray<ELifetimeCondition>& OutConditions);

	// RPC support
	void InitFromFunction(UFunction* InFunction, const UNetConnection* ServerConnection = nullptr);

	/** @see SendProperties. */
	void ENGINE_API SendPropertiesForRPC(
		UFunction*		Function,
		UActorChannel*	Channel,
		FNetBitWriter&	Writer,
		void*			Data) const;

	/** @see ReceiveProperties. */
	void ReceivePropertiesForRPC(
		UObject*			Object,
		UFunction*			Function,
		UActorChannel*		Channel,
		FNetBitReader&		Reader,
		void*				Data,
		TSet<FNetworkGUID>&	UnmappedGuids) const;

	/** Builds shared serialization state for a multicast rpc */
	void ENGINE_API BuildSharedSerializationForRPC(void* Data);

	/** Clears shared serialization state for a multicast rpc */
	void ENGINE_API ClearSharedSerializationForRPC();

	// Struct support
	ENGINE_API void SerializePropertiesForStruct(
		UStruct*		Struct,
		FBitArchive&	Ar,
		UPackageMap*	Map,
		void*			Data,
		bool&			bHasUnmapped) const;

	ENGINE_API void InitFromStruct(UStruct * InStruct, const UNetConnection* ServerConnection = nullptr);

	/** Serializes all replicated properties of a UObject in or out of an archive (depending on what type of archive it is). */
	ENGINE_API void SerializeObjectReplicatedProperties(UObject* Object, FBitArchive & Ar) const;

	UObject* GetOwner() const { return Owner; }

	/** Currently only used for Replays / with the UDemoNetDriver. */
	void SendProperties_BackwardsCompatible(
		FRepState* RESTRICT			RepState,
		FRepChangedPropertyTracker* ChangedTracker,
		const uint8* RESTRICT		Data,
		UNetConnection*				Connection,
		FNetBitWriter&				Writer,
		TArray<uint16>&				Changed) const;

	/** Currently only used for Replays / with the UDemoNetDriver. */
	bool ReceiveProperties_BackwardsCompatible(
		UNetConnection*				Connection,
		FRepState* RESTRICT			RepState,
		void* RESTRICT				Data,
		FNetBitReader&				InBunch,
		bool&						bOutHasUnmapped,
		const bool					bEnableRepNotifies,
		bool&						bOutGuidsChanged) const;

	DEPRECATED(4.22, "Please use the version of CompareProperties that accepts a FRepState pointer.")
	bool CompareProperties(
		FRepChangelistState* RESTRICT	RepState,
		const uint8* RESTRICT			Data,
		const FReplicationFlags&		RepFlags) const
	{
		CompareProperties(nullptr, RepState, Data, RepFlags);
	}

	/**
	 * Compare Property Values currently stored in the Changelist State to the Property Values
	 * in the passed in data, generating a new changelist if necessary.
	 *
	 * @param RepState				RepState for the object.
	 * @param RepChangelistState	The FRepChangelistState that contains the last cached values and changelists.
	 * @param Data					The newest Property Data available.
	 * @param RepFlags				Flags that will be used if the object is replicated.
	 */
	bool CompareProperties(
		FRepState* RESTRICT				RepState,
		FRepChangelistState* RESTRICT	RepChangelistState,
		const uint8* RESTRICT			Data,
		const FReplicationFlags&		RepFlags) const;

	//~ Begin FGCObject Interface
	ENGINE_API virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	//~ End FGCObject Interface

private:
	void RebuildConditionalProperties(
		FRepState* RESTRICT					RepState,
		const FRepChangedPropertyTracker&	ChangedTracker,
		const FReplicationFlags&			RepFlags) const;

	void UpdateChangelistHistory(
		FRepState*				RepState,
		UClass*					ObjectClass,
		const uint8* RESTRICT	Data,
		UNetConnection*			Connection,
		TArray<uint16>*			OutMerged) const;

	void SendProperties_BackwardsCompatible_r(
		FRepState* RESTRICT					RepState,
		UPackageMapClient*					PackageMapClient,
		FNetFieldExportGroup*				NetFieldExportGroup,
		FRepChangedPropertyTracker*			ChangedTracker,
		FNetBitWriter&						Writer,
		const bool							bDoChecksum,
		FRepHandleIterator&					HandleIterator,
		const uint8* RESTRICT				SourceData) const;

	void SendAllProperties_BackwardsCompatible_r(
		FRepState* RESTRICT					RepState,
		FNetBitWriter&						Writer,
		const bool							bDoChecksum,
		UPackageMapClient*					PackageMapClient,
		FNetFieldExportGroup*				NetFieldExportGroup,
		const int32							CmdStart,
		const int32							CmdEnd,
		const uint8*						SourceData) const;

	void SendProperties_r(
		FRepState*	RESTRICT				RepState,
		FRepChangedPropertyTracker*			ChangedTracker,
		FNetBitWriter&						Writer,
		const bool							bDoChecksum,
		FRepHandleIterator&					HandleIterator,
		const uint8* RESTRICT				SourceData,
		const int32							ArrayDepth,
		const FRepSerializationSharedInfo&	SharedInfo) const;

	uint16 CompareProperties_r(
		FRepState* RESTRICT		RepState,
		const int32				CmdStart,
		const int32				CmdEnd,
		const uint8* RESTRICT	CompareData,
		const uint8* RESTRICT	Data,
		TArray<uint16>&		Changed,
		uint16					Handle,
		const bool				bIsInitial,
		const bool				bForceFail) const;

	void CompareProperties_Array_r(
		FRepState* RESTRICT		RepState,
		const uint8* RESTRICT	CompareData,
		const uint8* RESTRICT	Data,
		TArray<uint16>&			Changed,
		const uint16			CmdIndex,
		const uint16			Handle,
		const bool				bIsInitial,
		const bool				bForceFail) const;

	void BuildSharedSerialization(
		const uint8* RESTRICT			Data,
		TArray<uint16>&					Changed,
		const bool						bWriteHandle,
		FRepSerializationSharedInfo&	SharedInfo) const;

	void BuildSharedSerialization_r(
		FRepHandleIterator&				RepHandleIterator,
		const uint8* RESTRICT			SourceData,
		const bool						bWriteHandle,
		const bool						bDoChecksum,
		const int32						ArrayDepth,
		FRepSerializationSharedInfo&	SharedInfo) const;

	void BuildSharedSerializationForRPC_DynamicArray_r(
		const int32						CmdIndex,
		uint8*							Data,
		int32							AarayDepth,
		FRepSerializationSharedInfo&	SharedInfo);

	void BuildSharedSerializationForRPC_r(
		const int32						CmdStart,
		const int32						CmdEnd,
		void*							Data,
		int32							ArrayIndex,
		int32							ArrayDepth,
		FRepSerializationSharedInfo&	SharedInfo);

	TSharedPtr<FNetFieldExportGroup> CreateNetfieldExportGroup() const;

	int32 FindCompatibleProperty(
		const int32		CmdStart,
		const int32		CmdEnd,
		const uint32	Checksum) const;

	bool ReceiveProperties_BackwardsCompatible_r(
		FRepState * RESTRICT	RepState,
		FNetFieldExportGroup*	NetFieldExportGroup,
		FNetBitReader &			Reader,
		const int32				CmdStart,
		const int32				CmdEnd,
		uint8* RESTRICT			ShadowData,
		uint8* RESTRICT			OldData,
		uint8* RESTRICT			Data,
		FGuidReferencesMap*		GuidReferencesMap,
		bool&					bOutHasUnmapped,
		bool&					bOutGuidsChanged) const;

	void GatherGuidReferences_r(
		FGuidReferencesMap*	GuidReferencesMap,
		TSet<FNetworkGUID>&	OutReferencedGuids,
		int32&				OutTrackedGuidMemoryBytes) const;

	bool MoveMappedObjectToUnmapped_r(FGuidReferencesMap* GuidReferencesMap, const FNetworkGUID& GUID) const;

	void UpdateUnmappedObjects_r(
		FRepState*				RepState, 
		FGuidReferencesMap*		GuidReferencesMap,
		UObject*				OriginalObject,
		UPackageMap*			PackageMap, 
		uint8* RESTRICT			StoredData, 
		uint8* RESTRICT			Data, 
		const int32				MaxAbsOffset,
		bool&					bOutSomeObjectsWereMapped,
		bool&					bOutHasMoreUnmapped) const;

	void ValidateWithChecksum_DynamicArray_r(
		const FRepLayoutCmd&	Cmd,
		const int32				CmdIndex,
		const uint8* RESTRICT	Data,
		FBitArchive&			Ar) const;

	void ValidateWithChecksum_r(
		const int32				CmdStart,
		const int32				CmdEnd,
		const uint8* RESTRICT	Data,
		FBitArchive&			Ar) const;

	void SanityCheckChangeList_DynamicArray_r(
		const int32				CmdIndex, 
		const uint8* RESTRICT	Data, 
		TArray<uint16> &		Changed,
		int32 &					ChangedIndex) const;

	uint16 SanityCheckChangeList_r(
		const int32				CmdStart, 
		const int32				CmdEnd, 
		const uint8* RESTRICT	Data, 
		TArray<uint16> &		Changed,
		int32 &					ChangedIndex,
		uint16					Handle) const;

	void SanityCheckChangeList(const uint8* RESTRICT Data, TArray<uint16>& Changed) const;

	uint16 AddParentProperty(UProperty* Property, int32 ArrayIndex);

	int32 InitFromProperty_r(
		UProperty* Property,
		int32					Offset,
		int32					RelativeHandle,
		int32					ParentIndex,
		uint32					ParentChecksum,
		int32					StaticArrayIndex,
		const UNetConnection*	ServerConnection);

	uint32 AddPropertyCmd(
		UProperty* Property,
		int32					Offset,
		int32					RelativeHandle,
		int32					ParentIndex,
		uint32					ParentChecksum,
		int32					StaticArrayIndex,
		const UNetConnection*	ServerConnection);

	uint32 AddArrayCmd(
		UArrayProperty*			Property,
		int32					Offset,
		int32					RelativeHandle,
		int32					ParentIndex,
		uint32					ParentChecksum,
		int32					StaticArrayIndex,
		const UNetConnection*	ServerConnection);

	void AddReturnCmd();

	void SerializeProperties_DynamicArray_r(
		FBitArchive &						Ar, 
		UPackageMap*						Map,
		const int32							CmdIndex,
		uint8*								Data,
		bool &								bHasUnmapped,
		const int32							ArrayDepth,
		const FRepSerializationSharedInfo&	SharedInfo) const;

	void SerializeProperties_r(
		FBitArchive&						Ar, 
		UPackageMap*						Map,
		const int32							CmdStart, 
		const int32							CmdEnd, 
		void*								Data,
		bool&								bHasUnmapped,
		const int32							ArrayIndex,
		const int32							ArrayDepth,
		const FRepSerializationSharedInfo&	SharedInfo) const;

	void MergeChangeList_r(
		FRepHandleIterator&		RepHandleIterator1,
		FRepHandleIterator&		RepHandleIterator2,
		const uint8* RESTRICT	SourceData,
		TArray<uint16>&			OutChanged) const;

	void PruneChangeList_r(
		FRepHandleIterator&		RepHandleIterator,
		const uint8* RESTRICT	SourceData,
		TArray<uint16>&			OutChanged) const;
		
	void BuildChangeList_r(
		const TArray<FHandleToCmdIndex>&	HandleToCmdIndex,
		const int32							CmdStart,
		const int32							CmdEnd,
		uint8*								Data,
		const int32							HandleOffset,
		TArray<uint16>&						Changed) const;

	void BuildHandleToCmdIndexTable_r(
		const int32					CmdStart,
		const int32					CmdEnd,
		TArray<FHandleToCmdIndex>&	HandleToCmdIndex);

	void ConstructProperties(TArray<uint8, TAlignedHeapAllocator<16>>& ShadowData) const;
	void InitProperties(TArray<uint8, TAlignedHeapAllocator<16>>& ShadowData, const uint8* const Src) const;
	void DestructProperties(FRepStateStaticBuffer& RepStateStaticBuffer) const;

	/** Top level Layout Commands. */
	TArray<FRepParentCmd> Parents;

	/** All Layout Commands. */
	TArray<FRepLayoutCmd> Cmds;

	/** Converts a relative handle to the appropriate index into the Cmds array */
	TArray<FHandleToCmdIndex> BaseHandleToCmdIndex;

	int32 FirstNonCustomParent;

	/** Index of the Role property in the Parents list. May be INDEX_NONE if Owner doesn't have the property. */
	int32 RoleIndex;

	/** Index of the RemoteRole property in the Parents list. May be INDEX_NONE if Owner doesn't have the property. */
	int32 RemoteRoleIndex;

	/** UClass, UStruct, or UFunction that this FRepLayout represents.*/
	UObject* Owner;

	/** Shared serialization state for a multicast rpc */
	FRepSerializationSharedInfo SharedInfoRPC;

	/** Shared comparison to default state for multicast rpc */
	TBitArray<> SharedInfoRPCParentsChanged;
};
