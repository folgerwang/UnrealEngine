// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcCoreFactory/IFactory.h>
#include <Alembic/Abc/IArchive.h>
#include <Alembic/Abc/IObject.h>

THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class IAbcObject;
class FAbcPolyMesh;
class FAbcTransform;
class UAbcImportSettings;
class UMaterialInterface;
class IMeshUtilities;

enum EAbcImportError : uint32;

/** Read flags for the abc file */
enum class EFrameReadFlags : uint8
{
	None = 0,
	/** Will only read position data for the objects */
	PositionOnly = 1 << 1,
	/** Will pre-multiply the world matrix with the read sample positions */
	ApplyMatrix = 1 << 2,
	/** Will force single thread processing */
	ForceSingleThreaded = 1 << 4,
};

ENUM_CLASS_FLAGS(EFrameReadFlags);

class FAbcFile
{
public:
	FAbcFile(const FString& InFilePath);
	~FAbcFile();

	/** Opens the ABC file and reads initial data for import options  */
	EAbcImportError Open();
	/** Sets up import data and propagates import settings to read objects */
	EAbcImportError Import(UAbcImportSettings* InImportSettings);

	/** Get file path for currently opened ABC file*/
	const FString GetFilePath() const;

	/** Process all to-import frames for each poly mesh inside of the ABC file */
	void ProcessFrames(TFunctionRef<void(int32, FAbcFile*)> InCallback, const EFrameReadFlags InFlags);
	
	/** Returns ABC specific frame and timing information */
	const int32 GetMinFrameIndex() const;
	const int32 GetMaxFrameIndex() const;
	const float GetImportTimeOffset() const;
	const float GetImportLength() const;
	const int32 GetFramerate() const;
	const FBoxSphereBounds& GetArchiveBounds() const;
	const bool ContainsHeterogeneousMeshes() const;

	/** Returns user specified import settings */
	const UAbcImportSettings* GetImportSettings() const;	

	/** Return array of PolyMeshes inside of the ABC file */
	const TArray<FAbcPolyMesh*>& GetPolyMeshes() const;
	/** Return number of PolyMeshes inside of the ABC file */
	const int32 GetNumPolyMeshes() const;
	/** Return array of Transform Objects inside of the ABC file */
	const TArray<FAbcTransform*>& GetTransforms() const;

	/** Return mesh utilities module ptr (used in non-gamethread jobs)*/
	IMeshUtilities* GetMeshUtilities() const;
	/** Returns a material for the specified material(faceset) name or null if it wasn't created or found */
	UMaterialInterface** GetMaterialByName(const FString& InMaterialName);		
protected:
	void TraverseAbcHierarchy(const Alembic::Abc::IObject& InObject, IAbcObject* InParent);
	void ReadFrame(int32 FrameIndex, const EFrameReadFlags InFlags, const int32 ReadIndex = INDEX_NONE);
	void CleanupFrameData(const int32 ReadIndex);
protected:
	/** File path for the ABC file */
	const FString FilePath;
	/** Cached user set import settings */
	UAbcImportSettings* ImportSettings;

	/** Factory used to generate objects*/
	Alembic::AbcCoreFactory::IFactory Factory;
	Alembic::AbcCoreFactory::IFactory::CoreType CompressionType;
	/** Archive-typed ABC file */
	Alembic::Abc::IArchive Archive;
	/** Alembic typed root (top) object*/
	Alembic::Abc::IObject TopObject;

	/** Abstract and typed objects part of this ABC file*/
	TArray<IAbcObject*> Objects;
	TArray<FAbcPolyMesh*> PolyMeshes;
	TArray<FAbcTransform*> Transforms;	
	/** Root (top) object in the ABC file */
	IAbcObject* RootObject;

	/** Min and maximum frame index which contain actual data in the Alembic file*/
	int32 MinFrameIndex;
	int32 MaxFrameIndex;
	
	/** FPS stored inside of the ABC file */
	float ArchiveSecondsPerFrame;
	
	/** Map of material created for the imported alembic file identified by material names */
	TMap<FString, UMaterialInterface*> MaterialMap;
	
	/** Total (max) number of frames in the Alembic file */
	int32 NumFrames;
	/** Frames per second (retrieved and specified in top Alembic object) */
	int32 FramesPerSecond;
	/** Seconds per frame (calculated according to FPS) */
	float SecondsPerFrame;

	/** Frame indices at which to start and stop importing */
	int32 StartFrameIndex;
	int32 EndFrameIndex;
	
	/** Entire bounds of the archive over time */
	FBoxSphereBounds ArchiveBounds;

	/** Min and maximum time found in the Alembic file*/
	float MinTime;
	float MaxTime;
	float ImportTimeOffset;
	/** Final length (in seconds)_of sequence we are importing */
	float ImportLength;

	/** Cached Mesh utilities ptr for normal calculations */
	IMeshUtilities* MeshUtilities;
};