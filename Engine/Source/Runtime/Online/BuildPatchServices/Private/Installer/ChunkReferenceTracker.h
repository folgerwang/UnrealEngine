// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchManifest.h"

namespace BuildPatchServices
{
	/**
	 * An interface for tracking references to chunks used throughout an installation. It is used to share across systems the
	 * chunks that are still required and when.
	 */
	class IChunkReferenceTracker
	{
	public:
		enum class ESortDirection : uint8
		{
			Ascending = 0,
			Descending
		};

	public:
		virtual ~IChunkReferenceTracker() {}

		/**
		 * Gets a set of all chunks referenced by the installation this tracker refers to.
		 * @return set of GUID containing every chunk used.
		 */
		virtual TSet<FGuid> GetReferencedChunks() const = 0;

		/**
		 * Gets the number of times a specific chunk is still referenced for the associated installation.
		 * @param ChunkId           The id for the chunk in question.
		 * @return the number of references remaining.
		 */
		virtual int32 GetReferenceCount(const FGuid& ChunkId) const = 0;

		/**
		 * Sorts a given array of chunk ids by the order in which they are required for the installation.
		 * @param ChunkList         The array to be sorted.
		 * @param Direction         The direction of sort. Ascending places soonest required chunk first.
		 */
		virtual void SortByUseOrder(TArray<FGuid>& ChunkList, ESortDirection Direction) const = 0;

		/**
		 * Retrieve the array of next chunk references, using a predicate to select whether each chunk is considered.
		 * @param Count             The number of chunk entries that are desired.
		 * @param SelectPredicate   The predicate used to determine whether to count or ignore the given chunk.
		 * @return an array of unique chunk id entries, in the order in which they are required.
		 */
		virtual TArray<FGuid> GetNextReferences(int32 Count, const TFunction<bool(const FGuid&)>& SelectPredicate) const = 0;

		/**
		 * Pop the top reference from the tracker, indicating that operation has been performed.
		 * It is not valid to pop anything but the top guid, so it must be provided for verification of behavior.
		 * @param ChunkId           The id of the top chunk, this is used to verify behavior.
		 * @return true if the correct guid was provided and the reference was popped, false if the wrong guid
		 *         was provided and thus no change was made.
		 */
		virtual bool PopReference(const FGuid& ChunkId) = 0;
	};

	/**
	 * A factory for creating an IChunkReferenceTracker instance.
	 */
	class FChunkReferenceTrackerFactory
	{
	public:
		/**
		 * This implementation takes the install manifest and generates the internal data and chunk reference tracking based
		 * off of a set of files that will be constructed.
		 * @param InstallManifest   The install manifest to enumerate references from.
		 * @param FilesToConstruct  The set of files to be installed, other files will not be considered.
		 * @return the new IChunkReferenceTracker instance created.
		 */
		static IChunkReferenceTracker* Create(const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& FilesToConstruct);
		
		/**
		 * This implementation takes custom chunk references to track. The array should be every chunk reference, including duplicates, in order of use.
		 * See namespace CustomChunkReferencesHelpers for common setup examples to use.
		 * @param CustomChunkReferences    The custom chunk references to track.
		 * @return the new IChunkReferenceTracker instance created.
		 */
		static IChunkReferenceTracker* Create(TArray<FGuid> CustomChunkReferences);
	};


	/**
	 * Helpers for creating a custom chunk use stack for use with the equivalent FChunkReferenceTrackerFactory.
	 */
	namespace CustomChunkReferencesHelpers
	{
		/**
		 * This implementation takes the install manifest and generates the chunk use stack needed for a chunk reference tracker based
		 * on caching data and so using each chunk once in the order that would be required to install the build.
		 * @param InstallManifest   The install manifest to enumerate references from.
		 * @return the chunk use references for FChunkReferenceTrackerFactory::Create.
		 */
		FORCEINLINE TArray<FGuid> OrderedUniqueReferences(const FBuildPatchAppManifestRef& InstallManifest)
		{
			TArray<FGuid> ChunkReferences;
			// Create our full list of chunks, no dupes, just one reference per chunk in the correct order.
			TArray<FString> AllFiles;
			TSet<FGuid> AllChunks;
			InstallManifest->GetFileList(AllFiles);
			for (const FString& File : AllFiles)
			{
				const FFileManifest* NewFileManifest = InstallManifest->GetFileManifest(File);
				if (NewFileManifest != nullptr)
				{
					for (const FChunkPart& ChunkPart : NewFileManifest->FileChunkParts)
					{
						bool bWasAlreadyInSet = false;
						AllChunks.Add(ChunkPart.Guid, &bWasAlreadyInSet);
						if (!bWasAlreadyInSet)
						{
							ChunkReferences.Add(ChunkPart.Guid);
						}
					}
				}
			}
			return ChunkReferences;
		}
		
		/**
		 * This implementation takes a new install manifest and a current manifest. It generates the chunk use stack needed for a chunk reference tracker based
		 * on caching data for a patch only, and so using the chunks in InstallManifest, which are not in CurrentManifest, once each in the order that they
		 * would be required to patch the build.
		 * @param InstallManifest   The install manifest to enumerate chunk references from.
		 * @param CurrentManifest   The current manifest to exclude chunk references from.
		 * @return the chunk use references for FChunkReferenceTrackerFactory::Create.
		 */
		FORCEINLINE TArray<FGuid> OrderedUniquePatchReferences(const FBuildPatchAppManifestRef& InstallManifest, const FBuildPatchAppManifestRef& CurrentManifest)
		{
			TArray<FGuid> ChunkReferences;
			// Create our list of chunks, no dupes, just one reference per chunk which appears only in InstallManifest, and in the correct order of use.
			TArray<FString> AllFiles;
			TSet<FGuid> OldChunks;
			TSet<FGuid> NewChunks;
			CurrentManifest->GetDataList(OldChunks);
			InstallManifest->GetFileList(AllFiles);
			for (const FString& File : AllFiles)
			{
				const FFileManifest* NewFileManifest = InstallManifest->GetFileManifest(File);
				if (NewFileManifest != nullptr)
				{
					for (const FChunkPart& ChunkPart : NewFileManifest->FileChunkParts)
					{
						const bool bIsNewChunk = !OldChunks.Contains(ChunkPart.Guid);
						if (bIsNewChunk)
						{
							bool bWasAlreadyInSet = false;
							NewChunks.Add(ChunkPart.Guid, &bWasAlreadyInSet);
							if (!bWasAlreadyInSet)
							{
								ChunkReferences.Add(ChunkPart.Guid);
							}
						}
					}
				}
			}
			return ChunkReferences;
		}
		
		/**
		 * This implementation takes the install manifest and a tagset. It generates the chunk use stack needed for a chunk reference tracker based
		 * on caching data and so using each chunk once in the order that would be required to install the build when using the same tagset provided.
		 * @param InstallManifest   The install manifest to enumerate references from.
		 * @param TagSet            The tagset that would be used to install the build, which will filter down required file list and thus required chunk list.
		 * @return the chunk use references for FChunkReferenceTrackerFactory::Create.
		 */
		FORCEINLINE TArray<FGuid> OrderedUniqueReferencesTagged(const FBuildPatchAppManifestRef& InstallManifest, const TSet<FString>& TagSet)
		{
			TArray<FGuid> ChunkReferences;
			// Create our full list of chunks, no dupes, just one reference per chunk in the correct order.
			TArray<FString> TaggedFiles;
			TSet<FGuid> TaggedChunks;
			InstallManifest->GetTaggedFileList(TagSet, TaggedFiles);
			for (const FString& File : TaggedFiles)
			{
				const FFileManifest* NewFileManifest = InstallManifest->GetFileManifest(File);
				if (NewFileManifest != nullptr)
				{
					for (const FChunkPart& ChunkPart : NewFileManifest->FileChunkParts)
					{
						bool bWasAlreadyInSet = false;
						TaggedChunks.Add(ChunkPart.Guid, &bWasAlreadyInSet);
						if (!bWasAlreadyInSet)
						{
							ChunkReferences.Add(ChunkPart.Guid);
						}
					}
				}
			}
			return ChunkReferences;
		}
		
		/**
		 * This implementation takes a new install manifest, a current manifest, and a tagset. It generates the chunk use stack needed for a chunk reference tracker based
		 * on caching data for a patch only, and so using the chunks in InstallManifest, which are not in CurrentManifest, once each in the order that would be required
         * to patch the build when using the same tagset provided.
		 * @param InstallManifest   The install manifest to enumerate references from.
		 * @param CurrentManifest   The current manifest to exclude chunk references from.
		 * @param TagSet            The tagset that would be used to patch the build, which will filter down required file list and thus required chunk list.
		 * @return the chunk use references for FChunkReferenceTrackerFactory::Create.
		 */
		FORCEINLINE TArray<FGuid> OrderedUniquePatchReferencesTagged(const FBuildPatchAppManifestRef& InstallManifest, const FBuildPatchAppManifestRef& CurrentManifest, const TSet<FString>& TagSet)
		{
			TArray<FGuid> ChunkReferences;
			// Create our list of chunks, no dupes, just one reference per chunk which appears only in InstallManifest, and in the correct order of use.
			TSet<FGuid> OldChunks;
			CurrentManifest->GetDataList(OldChunks);
			TArray<FString> TaggedFiles;
			TSet<FGuid> TaggedNewChunks;
			InstallManifest->GetTaggedFileList(TagSet, TaggedFiles);
			for (const FString& File : TaggedFiles)
			{
				const FFileManifest* NewFileManifest = InstallManifest->GetFileManifest(File);
				if (NewFileManifest != nullptr)
				{
					for (const FChunkPart& ChunkPart : NewFileManifest->FileChunkParts)
					{
						const bool bIsNewChunk = !OldChunks.Contains(ChunkPart.Guid);
						if (bIsNewChunk)
						{
							bool bWasAlreadyInSet = false;
							TaggedNewChunks.Add(ChunkPart.Guid, &bWasAlreadyInSet);
							if (!bWasAlreadyInSet)
							{
								ChunkReferences.Add(ChunkPart.Guid);
							}
						}
					}
				}
			}
			return ChunkReferences;
		}
	}
}