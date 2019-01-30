// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AbcImporter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcCoreHDF5/All.h>
#include <Alembic/AbcCoreOgawa/All.h>
#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcCoreAbstract/TimeSampling.h>
#include <Alembic/AbcCoreHDF5/All.h>
THIRD_PARTY_INCLUDES_END

#include "Misc/Paths.h"
#include "Misc/FeedbackContext.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "RawIndexBuffer.h"
#include "Misc/ScopedSlowTask.h"

#include "PackageTools.h"
#include "MeshDescription.h"
#include "MeshAttributes.h"
#include "MeshAttributeArray.h"
#include "MeshDescriptionOperations.h"
#include "ObjectTools.h"

#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "SkelImport.h"
#include "Animation/AnimSequence.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Toolkits/AssetEditorManager.h"

#include "AbcImportUtilities.h"
#include "Utils.h"

#include "MeshUtilities.h"
#include "MaterialUtilities.h"
#include "Materials/MaterialInstance.h"

#include "Runtime/Engine/Classes/Materials/MaterialInterface.h"
#include "Runtime/Engine/Public/MaterialCompiler.h"

#include "Async/ParallelFor.h"

#include "EigenHelper.h"

#include "AbcAssetImportData.h"
#include "AbcFile.h"

#include "AssetRegistryModule.h"
#include "AnimationUtils.h"
#include "ComponentReregisterContext.h"
#include "GeometryCacheCodecV1.h"

#define LOCTEXT_NAMESPACE "AbcImporter"

DEFINE_LOG_CATEGORY_STATIC(LogAbcImporter, Verbose, All);

#define OBJECT_TYPE_SWITCH(a, b, c) if (AbcImporterUtilities::IsType<a>(ObjectMetaData)) { \
	a TypedObject = a(b, Alembic::Abc::kWrapExisting); \
	ParseAbcObject<a>(TypedObject, c); bHandled = true; }

#define PRINT_UNIQUE_VERTICES 0

FAbcImporter::FAbcImporter()
	: ImportSettings(nullptr), AbcFile(nullptr)
{

}

FAbcImporter::~FAbcImporter()
{
	delete AbcFile;
}

void FAbcImporter::UpdateAssetImportData(UAbcAssetImportData* AssetImportData)
{
	AssetImportData->TrackNames.Empty();
	const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
	for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		if (PolyMesh->bShouldImport)
		{
			AssetImportData->TrackNames.Add(PolyMesh->GetName());
		}
	}

	AssetImportData->SamplingSettings = ImportSettings->SamplingSettings;
}

void FAbcImporter::RetrieveAssetImportData(UAbcAssetImportData* AssetImportData)
{
	bool bAnySetForImport = false;

	for (FAbcPolyMesh* PolyMesh : AbcFile->GetPolyMeshes())
	{
		if (AssetImportData->TrackNames.Contains(PolyMesh->GetName()))
		{
			PolyMesh->bShouldImport = true;
			bAnySetForImport = true;
		}		
		else
		{
			PolyMesh->bShouldImport = false;
		}
	}

	// If none were set to import, set all of them to import (probably different scene/setup)
	if (!bAnySetForImport)
	{
		for (FAbcPolyMesh* PolyMesh : AbcFile->GetPolyMeshes())
		{
			PolyMesh->bShouldImport = true;
		}
	}
}

const EAbcImportError FAbcImporter::OpenAbcFileForImport(const FString InFilePath)
{
	AbcFile = new FAbcFile(InFilePath);
	return AbcFile->Open();
}

const EAbcImportError FAbcImporter::ImportTrackData(const int32 InNumThreads, UAbcImportSettings* InImportSettings)
{
	ImportSettings = InImportSettings;
	ImportSettings->NumThreads = InNumThreads;
	EAbcImportError Error = AbcFile->Import(ImportSettings);	

	return Error;
}

template<typename T>
T* FAbcImporter::CreateObjectInstance(UObject*& InParent, const FString& ObjectName, const EObjectFlags Flags)
{
	// Parent package to place new mesh
	UPackage* Package = nullptr;
	FString NewPackageName;

	// Setup package name and create one accordingly
	NewPackageName = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName() + TEXT("/") + ObjectName);
	NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);
	Package = CreatePackage(nullptr, *NewPackageName);

	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

	T* ExistingTypedObject = FindObject<T>(Package, *SanitizedObjectName);
	UObject* ExistingObject = FindObject<UObject>(Package, *SanitizedObjectName);

	if (ExistingTypedObject != nullptr)
	{
		ExistingTypedObject->PreEditChange(nullptr);
	}
	else if (ExistingObject != nullptr)
	{
		// Replacing an object.  Here we go!
		// Delete the existing object
		const bool bDeleteSucceeded = ObjectTools::DeleteSingleObject(ExistingObject);

		if (bDeleteSucceeded)
		{
			// Force GC so we can cleanly create a new asset (and not do an 'in place' replacement)
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

			// Create a package for each mesh
			Package = CreatePackage(nullptr, *NewPackageName);
			InParent = Package;
		}
		else
		{
			// failed to delete
			return nullptr;
		}
	}

	return NewObject<T>(Package, FName(*SanitizedObjectName), Flags | RF_Public);
}

UStaticMesh* FAbcImporter::CreateStaticMeshFromSample(UObject* InParent, const FString& Name, EObjectFlags Flags, const uint32 NumMaterials, const TArray<FString>& FaceSetNames, const FAbcMeshSample* Sample)
{
	UStaticMesh* StaticMesh = CreateObjectInstance<UStaticMesh>(InParent, Name, Flags);

	// Only import data if a valid object was created
	if (StaticMesh)
	{
		// Add the first LOD, we only support one
		int32 LODIndex = 0;
		StaticMesh->AddSourceModel();
		FMeshDescription* MeshDescription = StaticMesh->CreateMeshDescription(LODIndex);
		StaticMesh->RegisterMeshAttributes(*MeshDescription);
		// Generate a new lighting GUID (so its unique)
		StaticMesh->LightingGuid = FGuid::NewGuid();

		// Set it to use textured lightmaps. Note that Build Lighting will do the error-checking (texcoord index exists for all LODs, etc).
		StaticMesh->LightMapResolution = 64;
		StaticMesh->LightMapCoordinateIndex = 1;

		// Material setup, since there isn't much material information in the Alembic file, 
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);

		// Material list
		StaticMesh->StaticMaterials.Empty();
		// If there were FaceSets available in the Alembic file use the number of unique face sets as num material entries, otherwise default to one material for the whole mesh
		const uint32 FrameIndex = 0;
		uint32 NumFaceSets = FaceSetNames.Num();

		const bool bCreateMaterial = ImportSettings->MaterialSettings.bCreateMaterials;
		for (uint32 MaterialIndex = 0; MaterialIndex < ((NumMaterials != 0) ? NumMaterials : 1); ++MaterialIndex)
		{
			UMaterialInterface* Material = nullptr;
			if (FaceSetNames.IsValidIndex(MaterialIndex))
			{
				Material = RetrieveMaterial(FaceSetNames[MaterialIndex], InParent, Flags);

				if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
				{
					Material->PostEditChange();
				}
			}

			StaticMesh->StaticMaterials.Add((Material != nullptr) ? Material : DefaultMaterial);
		}

		GenerateMeshDescriptionFromSample(Sample, MeshDescription, StaticMesh);

		// Get the first LOD for filling it up with geometry, only support one LOD
		FStaticMeshSourceModel& SrcModel = StaticMesh->SourceModels[LODIndex];
		// Set build settings for the static mesh
		SrcModel.BuildSettings.bRecomputeNormals = false;
		SrcModel.BuildSettings.bRecomputeTangents = false;
		SrcModel.BuildSettings.bUseMikkTSpace = false;
		// Generate Lightmaps uvs (no support for importing right now)
		SrcModel.BuildSettings.bGenerateLightmapUVs = ImportSettings->StaticMeshSettings.bGenerateLightmapUVs;
		// Set lightmap UV index to 1 since we currently only import one set of UVs from the Alembic Data file
		SrcModel.BuildSettings.DstLightmapIndex = 1;

		// Store the mesh description
		StaticMesh->CommitMeshDescription(LODIndex);

		//Set the Imported version before calling the build
		StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;

		// Build the static mesh (using the build setting etc.) this generates correct tangents using the extracting smoothing group along with the imported Normals data
		StaticMesh->Build(false);

		// No collision generation for now
		StaticMesh->CreateBodySetup();
	}

	return StaticMesh;
}

const TArray<UStaticMesh*> FAbcImporter::ImportAsStaticMesh(UObject* InParent, EObjectFlags Flags)
{
	checkf(AbcFile->GetNumPolyMeshes() > 0, TEXT("No poly meshes found"));

	TArray<UStaticMesh*> ImportedStaticMeshes;
	const FAbcStaticMeshSettings& StaticMeshSettings = ImportSettings->StaticMeshSettings;

	TFunction<void(int32, FAbcFile*)> Func = [this, &ImportedStaticMeshes, StaticMeshSettings, InParent, Flags](int32 FrameIndex, FAbcFile* InFile)
	{
		const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
		if (StaticMeshSettings.bMergeMeshes)
		{
			// If merging we merge all the raw mesh structures together and generate a static mesh asset from this
			TArray<FString> MergedFaceSetNames;
			TArray<FAbcMeshSample*> Samples;
			uint32 TotalNumMaterials = 0;

			TArray<const FAbcMeshSample*> SamplesToMerge;
			// Should merge all samples in the Alembic cache to one single static mesh
			for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
			{
				if (PolyMesh->bShouldImport)
				{
					const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
					SamplesToMerge.Add(Sample);
					TotalNumMaterials += (Sample->NumMaterials != 0) ? Sample->NumMaterials : 1;

					if (PolyMesh->FaceSetNames.Num() > 0)
					{
						MergedFaceSetNames.Append(PolyMesh->FaceSetNames);
					}
					else
					{
						// Default name
						static const FString DefaultName("NoFaceSetName");
						MergedFaceSetNames.Add(DefaultName);
					}
				}
			}

			// Only merged samples if there are any
			if (SamplesToMerge.Num())
			{
				FAbcMeshSample* MergedSample = AbcImporterUtilities::MergeMeshSamples(SamplesToMerge);


				UStaticMesh* StaticMesh = CreateStaticMeshFromSample(InParent, InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(AbcFile->GetFilePath()) + "_" + FGuid::NewGuid().ToString()), Flags, TotalNumMaterials, MergedFaceSetNames, MergedSample);
				if (StaticMesh)
				{
						ImportedStaticMeshes.Add(StaticMesh);
				}
			}
		}
		else
		{
			for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
			{
				const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
				if (PolyMesh->bShouldImport && Sample)
				{
					// Setup static mesh instance
					UStaticMesh* StaticMesh = CreateStaticMeshFromSample(InParent, InParent != GetTransientPackage() ? PolyMesh->GetName() : PolyMesh->GetName() + "_" + FGuid::NewGuid().ToString(), Flags, Sample->NumMaterials, PolyMesh->FaceSetNames, Sample);

					if (StaticMesh)
					{
						ImportedStaticMeshes.Add(StaticMesh);
					}
				}
			}
		}
	};
	

	EFrameReadFlags ReadFlags = ( ImportSettings->StaticMeshSettings.bMergeMeshes && ImportSettings->StaticMeshSettings.bPropagateMatrixTransformations ? EFrameReadFlags::ApplyMatrix : EFrameReadFlags::None ) | EFrameReadFlags::ForceSingleThreaded;
	AbcFile->ProcessFrames(Func, ReadFlags);
	
	return ImportedStaticMeshes;
}

UGeometryCache* FAbcImporter::ImportAsGeometryCache(UObject* InParent, EObjectFlags Flags)
{
	// Create a GeometryCache instance 
	UGeometryCache* GeometryCache = CreateObjectInstance<UGeometryCache>(InParent, InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(AbcFile->GetFilePath()) + "_" + FGuid::NewGuid().ToString()), Flags);

	// Only import data if a valid object was created
	if (GeometryCache)
	{
		TArray<TUniquePtr<FComponentReregisterContext>> ReregisterContexts;
		for (TObjectIterator<UGeometryCacheComponent> CacheIt; CacheIt; ++CacheIt)
		{
			if (CacheIt->GetGeometryCache() == GeometryCache)
			{	
				ReregisterContexts.Add(MakeUnique<FComponentReregisterContext>(*CacheIt));
			}
		}
		
		// In case this is a reimport operation
		GeometryCache->ClearForReimporting();

		// Load the default material for later usage
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		check(DefaultMaterial);
		uint32 MaterialOffset = 0;

		// Add tracks
		const int32 NumPolyMeshes = AbcFile->GetNumPolyMeshes();
		if (NumPolyMeshes != 0)
		{
			TArray<UGeometryCacheTrackStreamable*> Tracks;

			TArray<FAbcPolyMesh*> ImportPolyMeshes;
			TArray<int32> MaterialOffsets;

			const bool bContainsHeterogeneousMeshes = AbcFile->ContainsHeterogeneousMeshes();
			if (ImportSettings->GeometryCacheSettings.bApplyConstantTopologyOptimizations && bContainsHeterogeneousMeshes)
			{
				TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, LOCTEXT("HeterogeneousMeshesAndForceSingle", "Unable to enforce constant topology optimizations as the imported tracks contain topology varying data."));
				FAbcImportLogger::AddImportMessage(Message);
			}

			if (ImportSettings->GeometryCacheSettings.bFlattenTracks)
			{
				//UGeometryCacheCodecRaw* Codec = NewObject<UGeometryCacheCodecRaw>(GeometryCache, FName(*FString(TEXT("Flattened_Codec"))), RF_Public);
				UGeometryCacheCodecV1* Codec = NewObject<UGeometryCacheCodecV1>(GeometryCache, FName(*FString(TEXT("Flattened_Codec"))), RF_Public);
				Codec->InitializeEncoder(ImportSettings->GeometryCacheSettings.CompressedPositionPrecision, ImportSettings->GeometryCacheSettings.CompressedTextureCoordinatesNumberOfBits);
				UGeometryCacheTrackStreamable* Track = NewObject<UGeometryCacheTrackStreamable>(GeometryCache, FName(*FString(TEXT("Flattened_Track"))), RF_Public);
				Track->BeginCoding(Codec, ImportSettings->GeometryCacheSettings.bApplyConstantTopologyOptimizations && !bContainsHeterogeneousMeshes, ImportSettings->GeometryCacheSettings.bCalculateMotionVectorsDuringImport, ImportSettings->GeometryCacheSettings.bOptimizeIndexBuffers);
				Tracks.Add(Track);
				
				FScopedSlowTask SlowTask((ImportSettings->SamplingSettings.FrameEnd + 1) - ImportSettings->SamplingSettings.FrameStart, FText::FromString(FString(TEXT("Importing Frames"))));
				SlowTask.MakeDialog(true);

				// Need to get all face sets here -> material names?
				TArray<FString> UniqueFaceSetNames;

				const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
				bool bRequiresDefaultMaterial = false;
				for (FAbcPolyMesh* PolyMesh : PolyMeshes)
				{
					if (PolyMesh->bShouldImport)
					{
						for (const FString& FaceSetName : PolyMesh->FaceSetNames)
						{
							UniqueFaceSetNames.AddUnique(FaceSetName);
						}
					
						bRequiresDefaultMaterial |= PolyMesh->FaceSetNames.Num() == 0;
					}
				}

				if (bRequiresDefaultMaterial)
				{
					UniqueFaceSetNames.Insert(TEXT("DefaultMaterial"), 0 );
				}

				
				const int32 NumTracks = Tracks.Num();
				int32 PreviousNumVertices = 0;
				TFunction<void(int32, FAbcFile*)> Callback = [this, &Tracks, &SlowTask, &UniqueFaceSetNames, &PolyMeshes, &PreviousNumVertices](int32 FrameIndex, const FAbcFile* InAbcFile)
				{
					FAbcMeshSample MergedSample;
					bool bConstantTopology = true;

					for (FAbcPolyMesh* PolyMesh : PolyMeshes)
					{
						if (PolyMesh->bShouldImport)
						{
							const int32 Offset = MergedSample.MaterialIndices.Num();
							bConstantTopology = bConstantTopology && PolyMesh->bConstantTopology;
							if (PolyMesh->GetVisibility(FrameIndex))
							{
								const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
								AbcImporterUtilities::AppendMeshSample(&MergedSample, Sample);
								if (PolyMesh->FaceSetNames.Num() == 0)
								{
									FMemory::Memzero(MergedSample.MaterialIndices.GetData() + Offset, (MergedSample.MaterialIndices.Num() - Offset) * sizeof(int32));
								}
								else
								{
									for (int32 Index = Offset; Index < MergedSample.MaterialIndices.Num(); ++Index)
									{
										int32& MaterialIndex = MergedSample.MaterialIndices[Index];
										if (PolyMesh->FaceSetNames.IsValidIndex(MaterialIndex))
										{
											MaterialIndex = UniqueFaceSetNames.IndexOfByKey(PolyMesh->FaceSetNames[MaterialIndex]);
										}
										else
										{
											MaterialIndex = 0;
										}
									}
								}
							}
						}
					}
					
					if (FrameIndex > ImportSettings->SamplingSettings.FrameStart)						
					{
						bConstantTopology &= (PreviousNumVertices == MergedSample.Vertices.Num());
					}
					PreviousNumVertices = MergedSample.Vertices.Num();

					MergedSample.NumMaterials = UniqueFaceSetNames.Num();

					// Generate the mesh data for this sample
					FGeometryCacheMeshData MeshData;
					GeometryCacheDataForMeshSample(MeshData, &MergedSample, 0);
					
					Tracks[0]->AddMeshSample(MeshData, PolyMeshes[0]->GetTimeForFrameIndex(FrameIndex) - InAbcFile->GetImportTimeOffset(), bConstantTopology);
					
					if (IsInGameThread())
					{
						SlowTask.EnterProgressFrame(1.0f);
					}					
				};

				AbcFile->ProcessFrames(Callback, EFrameReadFlags::ApplyMatrix);

				// Now add materials for all the face set names
				for (const FString& FaceSetName : UniqueFaceSetNames)
				{
					UMaterialInterface* Material = RetrieveMaterial(FaceSetName, InParent, Flags);
					GeometryCache->Materials.Add((Material != nullptr) ? Material : DefaultMaterial);		

					if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
					{
						Material->PostEditChange();
					}
				}
			}
			else
			{
				const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
				for (FAbcPolyMesh* PolyMesh : PolyMeshes)
				{
					if (PolyMesh->bShouldImport)
					{
						//UGeometryCacheCodecRaw* Codec = NewObject<UGeometryCacheCodecRaw>(GeometryCache, FName(*(PolyMesh->GetName() + FString(TEXT("_Codec")))), RF_Public);
						UGeometryCacheCodecV1* Codec = NewObject<UGeometryCacheCodecV1>(GeometryCache, FName(*(PolyMesh->GetName() + FString(TEXT("_Codec")))), RF_Public);
						Codec->InitializeEncoder(ImportSettings->GeometryCacheSettings.CompressedPositionPrecision, ImportSettings->GeometryCacheSettings.CompressedTextureCoordinatesNumberOfBits);
						UGeometryCacheTrackStreamable* Track = NewObject<UGeometryCacheTrackStreamable>(GeometryCache, FName(*PolyMesh->GetName()), RF_Public);
						Track->BeginCoding(Codec, ImportSettings->GeometryCacheSettings.bApplyConstantTopologyOptimizations && !bContainsHeterogeneousMeshes, ImportSettings->GeometryCacheSettings.bCalculateMotionVectorsDuringImport, ImportSettings->GeometryCacheSettings.bOptimizeIndexBuffers);

						ImportPolyMeshes.Add(PolyMesh);
						Tracks.Add(Track);
						MaterialOffsets.Add(MaterialOffset);

						// Add materials for this Mesh Object
						const uint32 NumMaterials = (PolyMesh->FaceSetNames.Num() > 0) ? PolyMesh->FaceSetNames.Num() : 1;
						for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
						{
							UMaterialInterface* Material = nullptr;
							if (PolyMesh->FaceSetNames.IsValidIndex(MaterialIndex))
							{
								Material = RetrieveMaterial(PolyMesh->FaceSetNames[MaterialIndex], InParent, Flags);
								if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
								{
									Material->PostEditChange();
								}
							}

							GeometryCache->Materials.Add((Material != nullptr) ? Material : DefaultMaterial);
						}

						MaterialOffset += NumMaterials;
					}
				}

				const int32 NumTracks = Tracks.Num();
				TFunction<void(int32, FAbcFile*)> Callback = [this, NumTracks, &ImportPolyMeshes, &Tracks, &MaterialOffsets](int32 FrameIndex, const FAbcFile* InAbcFile)
				{
					for (int32 TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
					{
						const FAbcPolyMesh* PolyMesh = ImportPolyMeshes[TrackIndex];
						if (PolyMesh->bShouldImport)
						{
							UGeometryCacheTrackStreamable* Track = Tracks[TrackIndex];

							// Generate the mesh data for this sample
							const bool bVisible = PolyMesh->GetVisibility(FrameIndex);
							const float FrameTime = PolyMesh->GetTimeForFrameIndex(FrameIndex);
							if (bVisible)
							{
								const FAbcMeshSample* Sample = PolyMesh->GetSample(FrameIndex);
								FGeometryCacheMeshData MeshData;
								GeometryCacheDataForMeshSample(MeshData, Sample, MaterialOffsets[TrackIndex]);
								Track->AddMeshSample(MeshData, FrameTime, PolyMesh->bConstantTopology);
							}

							Track->AddVisibilitySample(bVisible, FrameTime);
						}
					}
				};

				AbcFile->ProcessFrames(Callback, EFrameReadFlags::ApplyMatrix);
			}

			TArray<FMatrix> Mats;
			Mats.Add(FMatrix::Identity);
			Mats.Add(FMatrix::Identity);

			for (UGeometryCacheTrackStreamable* Track : Tracks)
			{
				TArray<float> MatTimes;
				MatTimes.Add(0.0f);
				MatTimes.Add(AbcFile->GetImportLength() + AbcFile->GetImportTimeOffset());
				Track->SetMatrixSamples(Mats, MatTimes);

				Track->EndCoding();
				GeometryCache->AddTrack(Track);
			}
		}

		// For alembic, for now, we define the duration of the tracks as the duration of the longer track in the whole file so all tracks loop in union
		float MaxDuration = 0.0f;
		for (auto Track : GeometryCache->Tracks)
		{
			MaxDuration = FMath::Max(MaxDuration, Track->GetDuration());
		}
		for (auto Track : GeometryCache->Tracks)
		{
			Track->SetDuration(MaxDuration);
		}
		// Also store the number of frames in the cache
		GeometryCache->SetFrameStartEnd(ImportSettings->SamplingSettings.FrameStart, ImportSettings->SamplingSettings.FrameEnd);
		
		// Update all geometry cache components, TODO move render-data from component to GeometryCache and allow for DDC population
		for (TObjectIterator<UGeometryCacheComponent> CacheIt; CacheIt; ++CacheIt)
		{
			CacheIt->OnObjectReimported(GeometryCache);
		}
	}
	
	return GeometryCache;
}

TArray<UObject*> FAbcImporter::ImportAsSkeletalMesh(UObject* InParent, EObjectFlags Flags)
{
	// First compress the animation data
	const bool bCompressionResult = CompressAnimationDataUsingPCA(ImportSettings->CompressionSettings, true);

	TArray<UObject*> GeneratedObjects;

	if (!bCompressionResult)
	{
		return GeneratedObjects;
	}

	// Enforce to compute normals and tangents for the average sample which forms the base of the skeletal mesh
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	for (const FCompressedAbcData& CompressedData : CompressedMeshData)
	{
		FAbcMeshSample* AverageSample = CompressedData.AverageSample;
		if (ImportSettings->NormalGenerationSettings.bForceOneSmoothingGroupPerObject)
		{
			// Set smoothing group indices and calculate smooth normals
			AverageSample->SmoothingGroupIndices.Empty(AverageSample->Indices.Num() / 3);
			AverageSample->SmoothingGroupIndices.AddZeroed(AverageSample->Indices.Num() / 3);
			AverageSample->NumSmoothingGroups = 1;
			AbcImporterUtilities::CalculateSmoothNormals(AverageSample);
		}
		else
		{
			AbcImporterUtilities::CalculateNormals(AverageSample);
			AbcImporterUtilities::GenerateSmoothingGroupsIndices(AverageSample, ImportSettings->NormalGenerationSettings.HardEdgeAngleThreshold);
			AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(AverageSample, AverageSample->SmoothingGroupIndices, AverageSample->NumSmoothingGroups);
		}
	}

	// Create a Skeletal mesh instance 
	
	const FString& ObjectName = InParent != GetTransientPackage() ? FPaths::GetBaseFilename(InParent->GetName()) : (FPaths::GetBaseFilename(AbcFile->GetFilePath()) + "_" + FGuid::NewGuid().ToString());
	const FString SanitizedObjectName = ObjectTools::SanitizeObjectName(ObjectName);

	USkeletalMesh* ExistingSkeletalMesh = FindObject<USkeletalMesh>(InParent, *SanitizedObjectName);	
	FSkinnedMeshComponentRecreateRenderStateContext* RecreateExistingRenderStateContext = ExistingSkeletalMesh ? new FSkinnedMeshComponentRecreateRenderStateContext(ExistingSkeletalMesh, false) : nullptr;
	
	USkeletalMesh* SkeletalMesh = CreateObjectInstance<USkeletalMesh>(InParent, ObjectName, Flags);

	// Only import data if a valid object was created
	if (SkeletalMesh)
	{
		// Touch pre edit change
		SkeletalMesh->PreEditChange(NULL);

		// Retrieve the imported resource structure and allocate a new LOD model
		FSkeletalMeshModel* ImportedModel = SkeletalMesh->GetImportedModel();
		check(ImportedModel->LODModels.Num() == 0);
		ImportedModel->LODModels.Empty();
		ImportedModel->EmptyOriginalReductionSourceMeshData();
		ImportedModel->LODModels.Add(new FSkeletalMeshLODModel());
		SkeletalMesh->ResetLODInfo();
		SkeletalMesh->AddLODInfo();
		FSkeletalMeshLODModel& LODModel = ImportedModel->LODModels[0];

		const FMeshBoneInfo BoneInfo(FName(TEXT("RootBone"), FNAME_Add), TEXT("RootBone_Export"), INDEX_NONE);
		const FTransform BoneTransform;
		{
			FReferenceSkeletonModifier RefSkelModifier(SkeletalMesh->RefSkeleton, SkeletalMesh->Skeleton);
			RefSkelModifier.Add(BoneInfo, BoneTransform);
		}


		FAbcMeshSample* MergedMeshSample = new FAbcMeshSample();
		for (const FCompressedAbcData& Data : CompressedMeshData)
		{
			AbcImporterUtilities::AppendMeshSample(MergedMeshSample, Data.AverageSample);
		}
		
		// Forced to 1
		LODModel.NumTexCoords = MergedMeshSample->NumUVSets;
		SkeletalMesh->bHasVertexColors = true;
		SkeletalMesh->VertexColorGuid = FGuid::NewGuid();

		/* Bounding box according to animation */
		SkeletalMesh->SetImportedBounds(AbcFile->GetArchiveBounds().GetBox());

		bool bBuildSuccess = false;
		TArray<int32> MorphTargetVertexRemapping;
		TArray<int32> UsedVertexIndicesForMorphs;
		MergedMeshSample->TangentX.Empty();
		MergedMeshSample->TangentY.Empty();
		bBuildSuccess = BuildSkeletalMesh(LODModel, SkeletalMesh->RefSkeleton, MergedMeshSample, MorphTargetVertexRemapping, UsedVertexIndicesForMorphs);

		if (!bBuildSuccess)
		{
			SkeletalMesh->MarkPendingKill();
			return GeneratedObjects;
		}

		// Create the skeleton object
		FString SkeletonName = FString::Printf(TEXT("%s_Skeleton"), *SkeletalMesh->GetName());
		USkeleton* Skeleton = CreateObjectInstance<USkeleton>(InParent, SkeletonName, Flags);

		// Merge bones to the selected skeleton
		check(Skeleton->MergeAllBonesToBoneTree(SkeletalMesh));
		Skeleton->MarkPackageDirty();
		if (SkeletalMesh->Skeleton != Skeleton)
		{
			SkeletalMesh->Skeleton = Skeleton;
			SkeletalMesh->MarkPackageDirty();
		}

		// Create animation sequence for the skeleton
		UAnimSequence* Sequence = CreateObjectInstance<UAnimSequence>(InParent, FString::Printf(TEXT("%s_Animation"), *SkeletalMesh->GetName()), Flags);
		Sequence->SetSkeleton(Skeleton);
		Sequence->SequenceLength = AbcFile->GetImportLength();
		Sequence->ImportFileFramerate = AbcFile->GetFramerate();
		Sequence->ImportResampleFramerate = AbcFile->GetFramerate();
		int32 ObjectIndex = 0;
		uint32 TriangleOffset = 0;
		uint32 WedgeOffset = 0;
		uint32 VertexOffset = 0;

		for (FCompressedAbcData& CompressedData : CompressedMeshData)
		{
			FAbcMeshSample* AverageSample = CompressedData.AverageSample;

			if (CompressedData.BaseSamples.Num() > 0)
			{
				const int32 NumBases = CompressedData.BaseSamples.Num();
				int32 NumUsedBases = 0;

				const int32 NumIndices = CompressedData.AverageSample->Indices.Num();

				for (int32 BaseIndex = 0; BaseIndex < NumBases; ++BaseIndex)
				{
					FAbcMeshSample* BaseSample = CompressedData.BaseSamples[BaseIndex];

					//AbcImporterUtilities::CalculateNormalsWithSmoothingGroups(BaseSample, AverageSample->SmoothingGroupIndices, AverageSample->NumSmoothingGroups);

					// Create new morph target with name based on object and base index
					UMorphTarget* MorphTarget = NewObject<UMorphTarget>(SkeletalMesh, FName(*FString::Printf(TEXT("Base_%i_%i"), BaseIndex, ObjectIndex)));

					// Setup morph target vertices directly
					TArray<FMorphTargetDelta> MorphDeltas;
					GenerateMorphTargetVertices(BaseSample, MorphDeltas, AverageSample, WedgeOffset, MorphTargetVertexRemapping, UsedVertexIndicesForMorphs, VertexOffset, WedgeOffset);
					MorphTarget->PopulateDeltas(MorphDeltas, 0, LODModel.Sections);

					const float PercentageOfVerticesInfluences = ((float)MorphTarget->MorphLODModels[0].Vertices.Num() / (float)NumIndices) * 100.0f;
					if (PercentageOfVerticesInfluences > ImportSettings->CompressionSettings.MinimumNumberOfVertexInfluencePercentage)
					{
						SkeletalMesh->RegisterMorphTarget(MorphTarget);
						MorphTarget->MarkPackageDirty();

						// Set up curves
						const TArray<float>& CurveValues = CompressedData.CurveValues[BaseIndex];
						const TArray<float>& TimeValues = CompressedData.TimeValues[BaseIndex];
						// Morph target stuffies
						FString CurveName = FString::Printf(TEXT("Base_%i_%i"), BaseIndex, ObjectIndex);
						FName ConstCurveName = *CurveName;

						// Sets up the morph target curves with the sample values and time keys
						SetupMorphTargetCurves(Skeleton, ConstCurveName, Sequence, CurveValues, TimeValues);
					}
					else
					{
						MorphTarget->MarkPendingKill();
					}
				}
			}

			Sequence->RawCurveData.RemoveRedundantKeys();

			WedgeOffset += CompressedData.AverageSample->Indices.Num();
			VertexOffset += CompressedData.AverageSample->Vertices.Num();

			const uint32 NumMaterials = CompressedData.MaterialNames.Num();
			for (uint32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
			{
				const FString& MaterialName = CompressedData.MaterialNames[MaterialIndex];
				UMaterialInterface* Material = RetrieveMaterial(MaterialName, InParent, Flags);
				SkeletalMesh->Materials.Add(FSkeletalMaterial(Material, true));
				if (Material != UMaterial::GetDefaultMaterial(MD_Surface))
				{
					Material->PostEditChange();
				}
			}

			++ObjectIndex;
		}

		// Set recompute tangent flag on skeletal mesh sections
		for (FSkelMeshSection& Section : LODModel.Sections)
		{
			Section.bRecomputeTangent = true;
		}

		SkeletalMesh->CalculateInvRefMatrices();
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();

		// Retrieve the name mapping container
		const FSmartNameMapping* NameMapping = Skeleton->GetSmartNameContainer(USkeleton::AnimCurveMappingName);
		Sequence->RawCurveData.RefreshName(NameMapping);
		Sequence->MarkRawDataAsModified();
		Sequence->PostEditChange();
		Sequence->SetPreviewMesh(SkeletalMesh);
		Sequence->MarkPackageDirty();

		Skeleton->SetPreviewMesh(SkeletalMesh);
		Skeleton->PostEditChange();

		GeneratedObjects.Add(SkeletalMesh);
		GeneratedObjects.Add(Skeleton);
		GeneratedObjects.Add(Sequence);

		FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
		AssetEditorManager.CloseAllEditorsForAsset(Skeleton);
		AssetEditorManager.CloseAllEditorsForAsset(SkeletalMesh);
		AssetEditorManager.CloseAllEditorsForAsset(Sequence);
	}

	if (RecreateExistingRenderStateContext)
	{
		delete RecreateExistingRenderStateContext;
	}

	return GeneratedObjects;
}

void FAbcImporter::SetupMorphTargetCurves(USkeleton* Skeleton, FName ConstCurveName, UAnimSequence* Sequence, const TArray<float> &CurveValues, const TArray<float>& TimeValues)
{
	FSmartName NewName;
	Skeleton->AddSmartNameAndModify(USkeleton::AnimCurveMappingName, ConstCurveName, NewName);

	check(Sequence->RawCurveData.AddCurveData(NewName));
	FFloatCurve * NewCurve = static_cast<FFloatCurve *> (Sequence->RawCurveData.GetCurveData(NewName.UID, ERawCurveTrackTypes::RCT_Float));

	for (int32 KeyIndex = 0; KeyIndex < CurveValues.Num(); ++KeyIndex)
	{
		const float& CurveValue = CurveValues[KeyIndex];
		const float& TimeValue = TimeValues[KeyIndex];

		FKeyHandle NewKeyHandle = NewCurve->FloatCurve.AddKey(TimeValue, CurveValue, false);

		ERichCurveInterpMode NewInterpMode = RCIM_Linear;
		ERichCurveTangentMode NewTangentMode = RCTM_Auto;
		ERichCurveTangentWeightMode NewTangentWeightMode = RCTWM_WeightedNone;

		float LeaveTangent = 0.f;
		float ArriveTangent = 0.f;
		float LeaveTangentWeight = 0.f;
		float ArriveTangentWeight = 0.f;

		NewCurve->FloatCurve.SetKeyInterpMode(NewKeyHandle, NewInterpMode);
		NewCurve->FloatCurve.SetKeyTangentMode(NewKeyHandle, NewTangentMode);
		NewCurve->FloatCurve.SetKeyTangentWeightMode(NewKeyHandle, NewTangentWeightMode);
	}
}

const bool FAbcImporter::CompressAnimationDataUsingPCA(const FAbcCompressionSettings& InCompressionSettings, const bool bRunComparison /*= false*/)
{
	const TArray<FAbcPolyMesh*>& PolyMeshes = AbcFile->GetPolyMeshes();
	
	// Split up poly mesh objects into constant and animated objects to process
	TArray<FAbcPolyMesh*> PolyMeshesToCompress;
	TArray<FAbcPolyMesh*> ConstantPolyMeshObjects;
	for (FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		if (PolyMesh->bShouldImport && PolyMesh->bConstantTopology)
		{
			if (PolyMesh->IsConstant() && PolyMesh->bConstantTransformation)
			{
				ConstantPolyMeshObjects.Add(PolyMesh);
			}
			else if (!PolyMesh->IsConstant() || (InCompressionSettings.bBakeMatrixAnimation && !PolyMesh->bConstantTransformation))
			{
				PolyMeshesToCompress.Add(PolyMesh);
			}
		}
	}

	bool bResult = true;
	const int32 NumPolyMeshesToCompress = PolyMeshesToCompress.Num();
	if (NumPolyMeshesToCompress)
	{
		if (InCompressionSettings.bMergeMeshes)
		{
			// Merged path
			const uint32 FrameZeroIndex = 0;
			TArray<FVector> AverageVertexData;

			float MinTime = FLT_MAX;
			float MaxTime = -FLT_MAX;
			int32 NumSamples = 0;

			TArray<uint32> ObjectVertexOffsets;
			TFunction<void(int32, FAbcFile*)> MergedMeshesFunc = [this, PolyMeshesToCompress, &MinTime, &MaxTime, &NumSamples, &ObjectVertexOffsets, &AverageVertexData, NumPolyMeshesToCompress](int32 FrameIndex, FAbcFile* InFile)
			{
				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];

					MinTime = FMath::Min(MinTime, (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());
					MaxTime = FMath::Max(MaxTime, (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());

					if (ObjectVertexOffsets.Num() != NumPolyMeshesToCompress)
					{
						ObjectVertexOffsets.Add(AverageVertexData.Num());
						AverageVertexData.Append(PolyMesh->GetSample(FrameIndex)->Vertices);
					}
					else
					{
						for (int32 VertexIndex = 0; VertexIndex < PolyMesh->GetSample(FrameIndex)->Vertices.Num(); ++VertexIndex)
						{
							AverageVertexData[VertexIndex + ObjectVertexOffsets[MeshIndex]] += PolyMesh->GetSample(FrameIndex)->Vertices[VertexIndex];
						}
					}
				}

				++NumSamples;
			};

			EFrameReadFlags Flags = EFrameReadFlags::PositionOnly;
			if (ImportSettings->CompressionSettings.bBakeMatrixAnimation)
			{
				Flags |= EFrameReadFlags::ApplyMatrix;
			}

			AbcFile->ProcessFrames(MergedMeshesFunc, Flags);

			// Average out vertex data
			const float Multiplier = 1.0f / FMath::Max(NumSamples, 1);
			for (FVector& Vertex : AverageVertexData)
			{
				Vertex *= Multiplier;
			}


			// Allocate compressed mesh data object
			CompressedMeshData.AddDefaulted();
			FCompressedAbcData& CompressedData = CompressedMeshData.Last();

			FAbcMeshSample MergedZeroFrameSample;
			for (FAbcPolyMesh* PolyMesh : PolyMeshesToCompress)
			{
				AbcImporterUtilities::AppendMeshSample(&MergedZeroFrameSample, PolyMesh->GetTransformedFirstSample());

				// QQ FUNCTIONALIZE
				// Add material names from this mesh object
				if (PolyMesh->FaceSetNames.Num() > 0)
				{
					CompressedData.MaterialNames.Append(PolyMesh->FaceSetNames);
				}
				else
				{
					static const FString DefaultName("NoFaceSetName");
					CompressedData.MaterialNames.Add(DefaultName);
				}
			}

			const uint32 NumVertices = AverageVertexData.Num();
			const uint32 NumMatrixRows = NumVertices * 3;

			TArray<float> OriginalMatrix;
			OriginalMatrix.AddZeroed(NumMatrixRows * NumSamples);

			uint32 SampleIndex = 0;
			TFunction<void(int32, FAbcFile*)> GenerateMatrixFunc = [this, PolyMeshesToCompress, NumPolyMeshesToCompress, &OriginalMatrix, &AverageVertexData, &NumSamples, &ObjectVertexOffsets, &SampleIndex, NumMatrixRows](int32 FrameIndex, FAbcFile* InFile)
			{
				// For each object generate the delta frame data for the PCA compression
				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
					AbcImporterUtilities::GenerateDeltaFrameDataMatrix(PolyMesh->GetSample(FrameIndex)->Vertices, AverageVertexData, SampleIndex * NumMatrixRows, ObjectVertexOffsets[MeshIndex], OriginalMatrix);
				}

					++SampleIndex;
			};

			AbcFile->ProcessFrames(GenerateMatrixFunc, Flags);

			// Perform compression
			TArray<float> OutU, OutV, OutMatrix;
			const uint32 NumUsedSingularValues = PerformSVDCompression(OriginalMatrix, NumMatrixRows, NumSamples, OutU, OutV, InCompressionSettings.BaseCalculationType == EBaseCalculationType::PercentageBased ? InCompressionSettings.PercentageOfTotalBases / 100.0f : 100.0f, InCompressionSettings.BaseCalculationType == EBaseCalculationType::FixedNumber ? InCompressionSettings.MaxNumberOfBases : 0);

			// Set up average frame 
			CompressedData.AverageSample = new FAbcMeshSample(MergedZeroFrameSample);
			FMemory::Memcpy(CompressedData.AverageSample->Vertices.GetData(), AverageVertexData.GetData(), sizeof(FVector) * NumVertices);

			const float FrameStep = (MaxTime - MinTime) / (float)(NumSamples - 1);
			AbcImporterUtilities::GenerateCompressedMeshData(CompressedData, NumUsedSingularValues, NumSamples, OutU, OutV, FrameStep, FMath::Max(MinTime, 0.0f));

			if (bRunComparison)
			{
				CompareCompressionResult(OriginalMatrix, NumSamples, NumMatrixRows, NumUsedSingularValues, NumVertices, OutU, OutV, AverageVertexData);
			}
		}
		else
		{
			TArray<float> MinTimes;
			TArray<float> MaxTimes;
			TArray<TArray<FVector>> AverageVertexData;

			AverageVertexData.AddDefaulted(NumPolyMeshesToCompress);
			MinTimes.AddZeroed(NumPolyMeshesToCompress);
			MaxTimes.AddZeroed(NumPolyMeshesToCompress);
			
			int32 NumSamples = 0;
			TFunction<void(int32, FAbcFile*)> IndividualMeshesFunc = [this, NumPolyMeshesToCompress, &PolyMeshesToCompress, &MinTimes, &MaxTimes, &NumSamples, &AverageVertexData](int32 FrameIndex, FAbcFile* InFile)
			{
			// Each individual object creates a compressed data object
				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
					TArray<FVector>& AverageVertices = AverageVertexData[MeshIndex];

					if (AverageVertices.Num() == 0)
					{
						MinTimes[MeshIndex] = FLT_MAX;
						MaxTimes[MeshIndex] = -FLT_MAX;
						AverageVertices.Append(PolyMesh->GetSample(FrameIndex)->Vertices);
					}
					else
					{
						const TArray<FVector>& CurrentVertices = PolyMesh->GetSample(FrameIndex)->Vertices;
						for (int32 VertexIndex = 0; VertexIndex < AverageVertices.Num(); ++VertexIndex)
						{
							AverageVertices[VertexIndex] += CurrentVertices[VertexIndex];
						}
					}

					MinTimes[MeshIndex] = FMath::Min(MinTimes[MeshIndex], (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());
					MaxTimes[MeshIndex] = FMath::Max(MaxTimes[MeshIndex], (float)PolyMesh->GetTimeForFrameIndex(FrameIndex) - AbcFile->GetImportTimeOffset());
				}

				++NumSamples;
			};

			EFrameReadFlags Flags = EFrameReadFlags::PositionOnly;
			if (ImportSettings->CompressionSettings.bBakeMatrixAnimation)
			{
				Flags |= EFrameReadFlags::ApplyMatrix;
			}

			AbcFile->ProcessFrames(IndividualMeshesFunc, Flags);

			// Average out vertex data
			const float Multiplier = 1.0f / FMath::Max(NumSamples, 1);
			for (TArray<FVector>& VertexData : AverageVertexData)
			{
				for (FVector& Vertex : VertexData)
				{
					Vertex *= Multiplier;
				}
			}

			TArray<TArray<float>> Matrices;
			for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
			{
				Matrices.AddDefaulted();
				Matrices[MeshIndex].AddZeroed(AverageVertexData[MeshIndex].Num() * 3 * NumSamples);
			}

			uint32 SampleIndex = 0;
			TFunction<void(int32, FAbcFile*)> GenerateMatrixFunc = [this, NumPolyMeshesToCompress, &Matrices, &SampleIndex, &PolyMeshesToCompress, &AverageVertexData](int32 FrameIndex, FAbcFile* InFile)
			{
				// For each object generate the delta frame data for the PCA compression
				for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
				{
					FAbcPolyMesh* PolyMesh = PolyMeshesToCompress[MeshIndex];
					const uint32 NumMatrixRows = AverageVertexData[MeshIndex].Num() * 3;
					AbcImporterUtilities::GenerateDeltaFrameDataMatrix(PolyMesh->GetSample(FrameIndex)->Vertices, AverageVertexData[MeshIndex], SampleIndex * NumMatrixRows, 0, Matrices[MeshIndex]);
				}

				++SampleIndex;
			};

			AbcFile->ProcessFrames(GenerateMatrixFunc, Flags);

			for (int32 MeshIndex = 0; MeshIndex < NumPolyMeshesToCompress; ++MeshIndex)
			{
				// Perform compression
				TArray<float> OutU, OutV, OutMatrix;
				const int32 NumVertices = AverageVertexData[MeshIndex].Num();
				const int32 NumMatrixRows = NumVertices * 3;
				const uint32 NumUsedSingularValues = PerformSVDCompression(Matrices[MeshIndex], NumMatrixRows, NumSamples, OutU, OutV, InCompressionSettings.BaseCalculationType == EBaseCalculationType::PercentageBased ? InCompressionSettings.PercentageOfTotalBases / 100.0f : 100.0f, InCompressionSettings.BaseCalculationType == EBaseCalculationType::FixedNumber ? InCompressionSettings.MaxNumberOfBases : 0);

				// Allocate compressed mesh data object
				CompressedMeshData.AddDefaulted();
				FCompressedAbcData& CompressedData = CompressedMeshData.Last();
				CompressedData.AverageSample = new FAbcMeshSample(*PolyMeshesToCompress[MeshIndex]->GetTransformedFirstSample());
				FMemory::Memcpy(CompressedData.AverageSample->Vertices.GetData(), AverageVertexData[MeshIndex].GetData(), sizeof(FVector) * NumVertices);

				const float FrameStep = (MaxTimes[MeshIndex] - MinTimes[MeshIndex]) / (float)(NumSamples);
				AbcImporterUtilities::GenerateCompressedMeshData(CompressedData, NumUsedSingularValues, NumSamples, OutU, OutV, FrameStep, FMath::Max(MinTimes[MeshIndex], 0.0f));

				// QQ FUNCTIONALIZE
				// Add material names from this mesh object
				if (PolyMeshesToCompress[MeshIndex]->FaceSetNames.Num() > 0)
				{
					CompressedData.MaterialNames.Append(PolyMeshesToCompress[MeshIndex]->FaceSetNames);
				}
				else
				{
					static const FString DefaultName("NoFaceSetName");
					CompressedData.MaterialNames.Add(DefaultName);
				}

				if (bRunComparison)
				{
					CompareCompressionResult(Matrices[MeshIndex], NumSamples, NumMatrixRows, NumUsedSingularValues, NumVertices, OutU, OutV, AverageVertexData[MeshIndex]);
				}
			}
		}
		
	}
	else
	{
		bResult = ConstantPolyMeshObjects.Num() > 0;
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(bResult ? EMessageSeverity::Warning : EMessageSeverity::Error, LOCTEXT("NoMeshesToProcess", "Unable to compress animation data, no meshes (with constant topology) found with Vertex Animation and baked Matrix Animation is turned off."));
		FAbcImportLogger::AddImportMessage(Message);
		
	}

	// Process the constant meshes by only adding them as average samples (without any bases/morphtargets to add as well)
	for (const FAbcPolyMesh* ConstantPolyMesh : ConstantPolyMeshObjects)
	{
		// Allocate compressed mesh data object
		CompressedMeshData.AddDefaulted();
		FCompressedAbcData& CompressedData = CompressedMeshData.Last();

		if (ImportSettings->CompressionSettings.bBakeMatrixAnimation)
		{
			CompressedData.AverageSample = new FAbcMeshSample(*ConstantPolyMesh->GetTransformedFirstSample());
		}
		else
		{
			CompressedData.AverageSample = new FAbcMeshSample(*ConstantPolyMesh->GetFirstSample());
		}

		// QQ FUNCTIONALIZE
		// Add material names from this mesh object
		if (ConstantPolyMesh->FaceSetNames.Num() > 0)
		{
			CompressedData.MaterialNames.Append(ConstantPolyMesh->FaceSetNames);
		}
		else
		{
			static const FString DefaultName("NoFaceSetName");
			CompressedData.MaterialNames.Add(DefaultName);
		}
	}
		
	return bResult;
}

void FAbcImporter::CompareCompressionResult(const TArray<float>& OriginalMatrix, const uint32 NumSamples, const uint32 NumRows, const uint32 NumUsedSingularValues, const uint32 NumVertices, const TArray<float>& OutU, const TArray<float>& OutV, const TArray<FVector>& AverageFrame)
{
	// TODO NEED FEEDBACK FOR USER ON COMPRESSION RESULTS
#if 0
	TArray<float> ComparisonMatrix;
	ComparisonMatrix.AddZeroed(OriginalMatrix.Num());
	for (uint32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		const int32 SampleOffset = (SampleIndex * NumRows);
		const int32 CurveOffset = (SampleIndex * NumUsedSingularValues);
		for (uint32 BaseIndex = 0; BaseIndex < NumUsedSingularValues; ++BaseIndex)
		{
			const int32 BaseOffset = (BaseIndex * NumVertices * 3);
			for (uint32 VertexIndex = 0; VertexIndex < NumVertices; VertexIndex++)
			{
				const int32 VertexOffset = (VertexIndex * 3);
				ComparisonMatrix[VertexOffset + SampleOffset + 0] += OutU[BaseOffset + VertexOffset + 0] * OutV[BaseIndex + CurveOffset];
				ComparisonMatrix[VertexOffset + SampleOffset + 1] += OutU[BaseOffset + VertexOffset + 1] * OutV[BaseIndex + CurveOffset];
				ComparisonMatrix[VertexOffset + SampleOffset + 2] += OutU[BaseOffset + VertexOffset + 2] * OutV[BaseIndex + CurveOffset];
			}
		}
	}

	Eigen::MatrixXf EigenOriginalMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(OriginalMatrix, NumRows, NumSamples, EigenOriginalMatrix);

	Eigen::MatrixXf EigenComparisonMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(ComparisonMatrix, NumRows, NumSamples, EigenComparisonMatrix);

	TArray<float> AverageMatrix;
	AverageMatrix.AddZeroed(NumRows * NumSamples);
	
	
	for (int32 Index = 0; Index < AverageFrame.Num(); Index++ )
	{
		const FVector& Vector = AverageFrame[Index];
		for (uint32 i = 0; i < NumSamples; ++i)
		{
			const uint32 IndexOffset = (Index * 3) + (i * NumRows);
			AverageMatrix[IndexOffset + 0] = Vector.X;
			AverageMatrix[IndexOffset + 1] = Vector.Y;
			AverageMatrix[IndexOffset + 2] = Vector.Z;
		}		
	}

	Eigen::MatrixXf EigenAverageMatrix;
	EigenHelpers::ConvertArrayToEigenMatrix(AverageMatrix, NumRows, NumSamples, EigenAverageMatrix);

	Eigen::MatrixXf One = (EigenOriginalMatrix - EigenComparisonMatrix);
	Eigen::MatrixXf Two = (EigenOriginalMatrix - EigenAverageMatrix);

	const float LengthOne = One.squaredNorm();
	const float LengthTwo = Two.squaredNorm();
	const float Distortion = (LengthOne / LengthTwo) * 100.0f;

	// Compare arrays
	for (int32 i = 0; i < ComparisonMatrix.Num(); ++i)
	{
		ensureMsgf(FMath::IsNearlyEqual(OriginalMatrix[i], ComparisonMatrix[i]), TEXT("Difference of %2.10f found"), FMath::Abs(OriginalMatrix[i] - ComparisonMatrix[i]));
	}
#endif 
}

const int32 FAbcImporter::PerformSVDCompression(TArray<float>& OriginalMatrix, const uint32 NumRows, const uint32 NumSamples, TArray<float>& OutU, TArray<float>& OutV, const float InPercentage, const int32 InFixedNumValue)
{
	TArray<float> OutS;
	EigenHelpers::PerformSVD(OriginalMatrix, NumRows, NumSamples, OutU, OutV, OutS);

	// Now we have the new basis data we have to construct the correct morph target data and curves
	const float PercentageBasesUsed = InPercentage;
	const int32 NumNonZeroSingularValues = OutS.Num();
	const int32 NumUsedSingularValues = (InFixedNumValue != 0) ? FMath::Min(InFixedNumValue, (int32)OutS.Num()) : (int32)((float)NumNonZeroSingularValues * PercentageBasesUsed);

	// Pre-multiply the bases with it's singular values
	ParallelFor(NumUsedSingularValues, [&](int32 ValueIndex)
	{
		const float Multiplier = OutS[ValueIndex];
		const int32 ValueOffset = ValueIndex * NumRows;

		for (uint32 RowIndex = 0; RowIndex < NumRows; ++RowIndex)
		{
			OutU[ValueOffset + RowIndex] *= Multiplier;
		}
	});

	UE_LOG(LogAbcImporter, Log, TEXT("Decomposed animation and reconstructed with %i number of bases (full %i, percentage %f, calculated %i)"), NumUsedSingularValues, OutS.Num(), PercentageBasesUsed * 100.0f, NumUsedSingularValues);	
	
	return NumUsedSingularValues;
}

const TArray<UStaticMesh*> FAbcImporter::ReimportAsStaticMesh(UStaticMesh* Mesh)
{
	const FString StaticMeshName = Mesh->GetName();
	const TArray<UStaticMesh*> StaticMeshes = ImportAsStaticMesh(Mesh->GetOuter(), RF_Public | RF_Standalone);

	return StaticMeshes;
}

UGeometryCache* FAbcImporter::ReimportAsGeometryCache(UGeometryCache* GeometryCache)
{
	UGeometryCache* ReimportedCache = ImportAsGeometryCache(GeometryCache->GetOuter(), RF_Public | RF_Standalone);
	return ReimportedCache;
}

TArray<UObject*> FAbcImporter::ReimportAsSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	TArray<UObject*> ReimportedObjects = ImportAsSkeletalMesh(SkeletalMesh->GetOuter(), RF_Public | RF_Standalone);
	return ReimportedObjects;
}

const TArray<FAbcPolyMesh*>& FAbcImporter::GetPolyMeshes() const
{
	return AbcFile->GetPolyMeshes();
}

const uint32 FAbcImporter::GetStartFrameIndex() const
{
	return (AbcFile != nullptr) ? AbcFile->GetMinFrameIndex() : 0;
}

const uint32 FAbcImporter::GetEndFrameIndex() const
{
	return (AbcFile != nullptr) ? FMath::Max(AbcFile->GetMaxFrameIndex() - 1, 1) : 1;
}

const uint32 FAbcImporter::GetNumMeshTracks() const
{
	return (AbcFile != nullptr) ? AbcFile->GetNumPolyMeshes() : 0;
}

void FAbcImporter::GenerateMeshDescriptionFromSample(const FAbcMeshSample* Sample, FMeshDescription* MeshDescription, UStaticMesh* StaticMesh)
{
	TVertexAttributesRef<FVector> VertexPositions = MeshDescription->VertexAttributes().GetAttributesRef<FVector>(MeshAttribute::Vertex::Position);
	TEdgeAttributesRef<bool> EdgeHardnesses = MeshDescription->EdgeAttributes().GetAttributesRef<bool>(MeshAttribute::Edge::IsHard);
	TEdgeAttributesRef<float> EdgeCreaseSharpnesses = MeshDescription->EdgeAttributes().GetAttributesRef<float>(MeshAttribute::Edge::CreaseSharpness);
	TPolygonGroupAttributesRef<FName> PolygonGroupImportedMaterialSlotNames = MeshDescription->PolygonGroupAttributes().GetAttributesRef<FName>(MeshAttribute::PolygonGroup::ImportedMaterialSlotName);
	TVertexInstanceAttributesRef<FVector> VertexInstanceNormals = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Normal);
	TVertexInstanceAttributesRef<FVector> VertexInstanceTangents = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector>(MeshAttribute::VertexInstance::Tangent);
	TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = MeshDescription->VertexInstanceAttributes().GetAttributesRef<float>(MeshAttribute::VertexInstance::BinormalSign);
	TVertexInstanceAttributesRef<FVector4> VertexInstanceColors = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector4>(MeshAttribute::VertexInstance::Color);
	TVertexInstanceAttributesRef<FVector2D> VertexInstanceUVs = MeshDescription->VertexInstanceAttributes().GetAttributesRef<FVector2D>(MeshAttribute::VertexInstance::TextureCoordinate);

	//Speedtree use UVs to store is data
	VertexInstanceUVs.SetNumIndices(Sample->NumUVSets);
	
	for (int32 MatIndex = 0; MatIndex < StaticMesh->StaticMaterials.Num(); ++MatIndex)
	{
		const FPolygonGroupID& PolygonGroupID = MeshDescription->CreatePolygonGroup();
		PolygonGroupImportedMaterialSlotNames[PolygonGroupID] = StaticMesh->StaticMaterials[MatIndex].ImportedMaterialSlotName;
	}

	// position
	for (int32 VertexIndex = 0; VertexIndex < Sample->Vertices.Num(); ++VertexIndex)
	{
		FVector Position = Sample->Vertices[VertexIndex];

		FVertexID VertexID = MeshDescription->CreateVertex();
		VertexPositions[VertexID] = FVector(Position);
	}

	uint32 TriangleCount = Sample->Indices.Num() / 3;
	for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		TArray<FVertexInstanceID> CornerVertexInstanceIDs;
		CornerVertexInstanceIDs.SetNum(3);
		FVertexID CornerVertexIDs[3];
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			uint32 IndiceIndex = (TriangleIndex * 3) + Corner;
			uint32 VertexIndex = Sample->Indices[IndiceIndex];
			const FVertexID VertexID(VertexIndex);
			const FVertexInstanceID& VertexInstanceID = MeshDescription->CreateVertexInstance(VertexID);

			// tangents
			FVector TangentX = Sample->TangentX[IndiceIndex];
			FVector TangentY = Sample->TangentY[IndiceIndex];
			FVector TangentZ = Sample->Normals[IndiceIndex];

			VertexInstanceTangents[VertexInstanceID] = TangentX;
			VertexInstanceNormals[VertexInstanceID] = TangentZ;
			VertexInstanceBinormalSigns[VertexInstanceID] = GetBasisDeterminantSign(TangentX.GetSafeNormal(), TangentY.GetSafeNormal(), TangentZ.GetSafeNormal());

			if (Sample->Colors.Num())
			{
				VertexInstanceColors[VertexInstanceID] = FVector4(Sample->Colors[IndiceIndex]);
			}
			else
			{
				VertexInstanceColors[VertexInstanceID] = FVector4(FLinearColor::White);
			}

			for (uint32 UVIndex = 0; UVIndex < Sample->NumUVSets; ++UVIndex)
			{
				VertexInstanceUVs.Set(VertexInstanceID, UVIndex, Sample->UVs[UVIndex][IndiceIndex]);
			}
			CornerVertexInstanceIDs[Corner] = VertexInstanceID;
			CornerVertexIDs[Corner] = VertexID;
		}

		const FPolygonGroupID PolygonGroupID(Sample->MaterialIndices[TriangleIndex]);
		// Insert a polygon into the mesh
		const FPolygonID NewPolygonID = MeshDescription->CreatePolygon(PolygonGroupID, CornerVertexInstanceIDs);
		//Triangulate the polygon
		FMeshPolygon& Polygon = MeshDescription->GetPolygon(NewPolygonID);
		MeshDescription->ComputePolygonTriangulation(NewPolygonID, Polygon.Triangles);
	}
	//Set the edge hardness from the smooth group
	FMeshDescriptionOperations::ConvertSmoothGroupToHardEdges(Sample->SmoothingGroupIndices, *MeshDescription);
}

void FAbcImporter::GeometryCacheDataForMeshSample(FGeometryCacheMeshData &OutMeshData, const FAbcMeshSample* MeshSample, const uint32 MaterialOffset)
{
	OutMeshData.BoundingBox = FBox(MeshSample->Vertices);

	// We currently always have everything except motion vectors
	// TODO: Make this user configurable
	OutMeshData.VertexInfo.bHasColor0 = true;
	OutMeshData.VertexInfo.bHasTangentX = true;
	OutMeshData.VertexInfo.bHasTangentZ = true;
	OutMeshData.VertexInfo.bHasUV0 = true;
	OutMeshData.VertexInfo.bHasMotionVectors = false;

	uint32 NumMaterials = MaterialOffset;

	const int32 NumTriangles = MeshSample->Indices.Num() / 3;
	const uint32 NumSections = MeshSample->NumMaterials ? MeshSample->NumMaterials : 1;

	TArray<TArray<uint32>> SectionIndices;
	SectionIndices.AddDefaulted(NumSections);

	OutMeshData.Positions.AddZeroed(MeshSample->Normals.Num());
	OutMeshData.TangentsX.AddZeroed(MeshSample->Normals.Num());
	OutMeshData.TangentsZ.AddZeroed(MeshSample->Normals.Num());
	OutMeshData.TextureCoordinates.AddZeroed(MeshSample->Normals.Num());
	OutMeshData.Colors.AddZeroed(MeshSample->Normals.Num());
	
	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		const int32 SectionIndex = MeshSample->MaterialIndices[TriangleIndex];
		TArray<uint32>& Section = SectionIndices[SectionIndex];

		for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			const int32 CornerIndex = (TriangleIndex * 3) + VertexIndex;
			const int32 Index = MeshSample->Indices[CornerIndex];

			OutMeshData.Positions[CornerIndex] = MeshSample->Vertices[Index];
			OutMeshData.TangentsX[CornerIndex] = MeshSample->TangentX[CornerIndex];
			OutMeshData.TangentsZ[CornerIndex] = MeshSample->Normals[CornerIndex];
			// store determinant of basis in w component of normal vector
			OutMeshData.TangentsZ[CornerIndex].Vector.W = GetBasisDeterminantSignByte(MeshSample->TangentX[CornerIndex], MeshSample->TangentY[CornerIndex], MeshSample->Normals[CornerIndex]);
			OutMeshData.TextureCoordinates[CornerIndex] = MeshSample->UVs[0][CornerIndex];
			OutMeshData.Colors[CornerIndex] = MeshSample->Colors[CornerIndex].ToFColor(false);

			Section.Add(CornerIndex);
		}
	}

	TArray<uint32>& Indices = OutMeshData.Indices;
	for (uint32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
	{
		// Sometimes empty sections seem to be in the file, filter these out
		// as empty batches are not allowed by the geometry cache (They ultimately trigger checks in the renderer)
		// and it seems pretty nasty to filter them out post decode in-game
		if (!SectionIndices[SectionIndex].Num())
		{
			continue;
		}

		FGeometryCacheMeshBatchInfo BatchInfo;
		BatchInfo.StartIndex = Indices.Num();
		BatchInfo.MaterialIndex = NumMaterials;
		NumMaterials++;


		BatchInfo.NumTriangles = SectionIndices[SectionIndex].Num() / 3;
		Indices.Append(SectionIndices[SectionIndex]);
		OutMeshData.BatchesInfo.Add(BatchInfo);
	}
}

bool FAbcImporter::BuildSkeletalMesh( FSkeletalMeshLODModel& LODModel, const FReferenceSkeleton& RefSkeleton, FAbcMeshSample* Sample, TArray<int32>& OutMorphTargetVertexRemapping, TArray<int32>& OutUsedVertexIndicesForMorphs)
{
	// Module manager is not thread safe, so need to prefetch before parallelfor
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");

	const bool bComputeNormals = (Sample->Normals.Num() == 0);
	const bool bComputeTangents = (Sample->TangentX.Num() == 0) || (Sample->TangentY.Num() == 0);

	// Compute normals/tangents if needed
	if (bComputeNormals || bComputeTangents)
	{
		uint32 TangentOptions = 0;
		MeshUtilities.CalculateTangents(Sample->Vertices, Sample->Indices, Sample->UVs[0], Sample->SmoothingGroupIndices, TangentOptions, Sample->TangentX, Sample->TangentY, Sample->Normals);
	}

	// Populate faces
	const uint32 NumFaces = Sample->Indices.Num() / 3;
	TArray<SkeletalMeshImportData::FMeshFace> Faces;
	Faces.AddZeroed(NumFaces);

	TArray<FMeshSection> MeshSections;
	MeshSections.AddDefaulted(Sample->NumMaterials);

	// Process all the faces and add to their respective mesh section
	for (uint32 FaceIndex = 0; FaceIndex < NumFaces; ++FaceIndex)
	{
		const uint32 FaceOffset = FaceIndex * 3;
		const int32 MaterialIndex = Sample->MaterialIndices[FaceIndex];

		check(MeshSections.IsValidIndex(MaterialIndex));

		FMeshSection& Section = MeshSections[MaterialIndex];
		Section.MaterialIndex = MaterialIndex;
		Section.NumUVSets = Sample->NumUVSets;
	
		for (uint32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
		{
			LODModel.MaxImportVertex = FMath::Max<int32>(LODModel.MaxImportVertex, Sample->Indices[FaceOffset + VertexIndex]);

			Section.OriginalIndices.Add(FaceOffset + VertexIndex);
			Section.Indices.Add(Sample->Indices[FaceOffset + VertexIndex]);
			Section.TangentX.Add(Sample->TangentX[FaceOffset + VertexIndex]);
			Section.TangentY.Add(Sample->TangentY[FaceOffset + VertexIndex]);
			Section.TangentZ.Add(Sample->Normals[FaceOffset + VertexIndex]);

			for (uint32 UVIndex = 0; UVIndex < Sample->NumUVSets; ++UVIndex)
			{
				Section.UVs[UVIndex].Add(Sample->UVs[UVIndex][FaceOffset + VertexIndex]);
			}		
			
			Section.Colors.Add(Sample->Colors[FaceOffset + VertexIndex].ToFColor(false));
		}

		++Section.NumFaces;
	}

	// Sort the vertices by z value
	MeshSections.Sort([](const FMeshSection& A, const FMeshSection& B) { return A.MaterialIndex < B.MaterialIndex; });

	// Create Skeletal mesh LOD sections
	LODModel.Sections.Empty(MeshSections.Num());
	LODModel.NumVertices = 0;
	LODModel.IndexBuffer.Empty();

	TArray<uint32> RawPointIndices;
	TArray< TArray<uint32> > VertexIndexRemap;
	VertexIndexRemap.Empty(MeshSections.Num());

	// Create actual skeletal mesh sections
	for (int32 SectionIndex = 0; SectionIndex < MeshSections.Num(); ++SectionIndex)
	{
		const FMeshSection& SourceSection = MeshSections[SectionIndex];
		FSkelMeshSection& TargetSection = *new(LODModel.Sections) FSkelMeshSection();
		TargetSection.MaterialIndex = (uint16)SourceSection.MaterialIndex;
		TargetSection.NumTriangles = SourceSection.NumFaces;
		TargetSection.BaseVertexIndex = LODModel.NumVertices;

		// Separate the section's vertices into rigid and soft vertices.
		TArray<uint32>& ChunkVertexIndexRemap = *new(VertexIndexRemap)TArray<uint32>();
		ChunkVertexIndexRemap.AddUninitialized(SourceSection.NumFaces * 3);

		TMultiMap<uint32, uint32> FinalVertices;
		TMap<FSoftSkinVertex*, uint32> VertexMapping;
		
		// Reused soft vertex
		FSoftSkinVertex NewVertex;

		uint32 VertexOffset = 0;
		// Generate Soft Skin vertices (used by the skeletal mesh)
		for (uint32 FaceIndex = 0; FaceIndex < SourceSection.NumFaces; ++FaceIndex)
		{
			const uint32 FaceOffset = FaceIndex * 3;
			const int32 MaterialIndex = Sample->MaterialIndices[FaceIndex];

			for (uint32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
			{
				const uint32 Index = SourceSection.Indices[FaceOffset + VertexIndex];

				TArray<uint32> DuplicateVertexIndices;
				FinalVertices.MultiFind(Index, DuplicateVertexIndices);
				
				// Populate vertex data
				NewVertex.Position = Sample->Vertices[Index];
				NewVertex.TangentX = SourceSection.TangentX[FaceOffset + VertexIndex];
				NewVertex.TangentY = SourceSection.TangentY[FaceOffset + VertexIndex];
				NewVertex.TangentZ = SourceSection.TangentZ[FaceOffset + VertexIndex];
				for (uint32 UVIndex = 0; UVIndex < SourceSection.NumUVSets; ++UVIndex)
				{
					NewVertex.UVs[UVIndex] = SourceSection.UVs[UVIndex][FaceOffset + VertexIndex];
				}
				
				NewVertex.Color = SourceSection.Colors[FaceOffset + VertexIndex];

				// Set up bone influence (only using one bone so maxed out weight)
				FMemory::Memzero(NewVertex.InfluenceBones);
				FMemory::Memzero(NewVertex.InfluenceWeights);
				NewVertex.InfluenceWeights[0] = 255;
				
				int32 FinalVertexIndex = INDEX_NONE;
				if (DuplicateVertexIndices.Num())
				{
					for (const uint32 DuplicateVertexIndex : DuplicateVertexIndices)
					{
						if (AbcImporterUtilities::AreVerticesEqual(TargetSection.SoftVertices[DuplicateVertexIndex], NewVertex))
						{
							// Use the existing vertex
							FinalVertexIndex = DuplicateVertexIndex;
							break;
						}						
					}
				}

				if (FinalVertexIndex == INDEX_NONE)
				{
					FinalVertexIndex = TargetSection.SoftVertices.Add(NewVertex);
#if PRINT_UNIQUE_VERTICES
					FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Vert - P(%.2f, %.2f,%.2f) N(%.2f, %.2f,%.2f) TX(%.2f, %.2f,%.2f) TY(%.2f, %.2f,%.2f) UV(%.2f, %.2f)\n"), NewVertex.Position.X, NewVertex.Position.Y, NewVertex.Position.Z, SourceSection.TangentX[FaceOffset + VertexIndex].X, 
						SourceSection.TangentZ[FaceOffset + VertexIndex].X, SourceSection.TangentZ[FaceOffset + VertexIndex].Y, SourceSection.TangentZ[FaceOffset + VertexIndex].Z, SourceSection.TangentX[FaceOffset + VertexIndex].Y, SourceSection.TangentX[FaceOffset + VertexIndex].Z, SourceSection.TangentY[FaceOffset + VertexIndex].X, SourceSection.TangentY[FaceOffset + VertexIndex].Y, SourceSection.TangentY[FaceOffset + VertexIndex].Z, NewVertex.UVs[0].X, NewVertex.UVs[0].Y);
#endif

					FinalVertices.Add(Index, FinalVertexIndex);
					OutUsedVertexIndicesForMorphs.Add(Index);
					OutMorphTargetVertexRemapping.Add(SourceSection.OriginalIndices[FaceOffset + VertexIndex]);
				}

				RawPointIndices.Add(FinalVertexIndex);
				ChunkVertexIndexRemap[VertexOffset] = TargetSection.BaseVertexIndex + FinalVertexIndex;
				++VertexOffset;
			}
		}

		LODModel.NumVertices += TargetSection.SoftVertices.Num();
		TargetSection.NumVertices = TargetSection.SoftVertices.Num();

		// Only need first bone from active bone indices
		TargetSection.BoneMap.Add(0);

		TargetSection.CalcMaxBoneInfluences();
	}

	// Only using bone zero
	LODModel.ActiveBoneIndices.Add(0);

	// Copy raw point indices to LOD model.
	LODModel.RawPointIndices.RemoveBulkData();
	if (RawPointIndices.Num())
	{
		LODModel.RawPointIndices.Lock(LOCK_READ_WRITE);
		void* Dest = LODModel.RawPointIndices.Realloc(RawPointIndices.Num());
		FMemory::Memcpy(Dest, RawPointIndices.GetData(), LODModel.RawPointIndices.GetBulkDataSize());
		LODModel.RawPointIndices.Unlock();
	}

	// Finish building the sections.
	for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
	{
		FSkelMeshSection& Section = LODModel.Sections[SectionIndex];

		const TArray<uint32>& SectionIndices = MeshSections[SectionIndex].Indices;
		Section.BaseIndex = LODModel.IndexBuffer.Num();
		const int32 NumIndices = SectionIndices.Num();
		const TArray<uint32>& SectionVertexIndexRemap = VertexIndexRemap[SectionIndex];
		for (int32 Index = 0; Index < NumIndices; Index++)
		{
			uint32 VertexIndex = SectionVertexIndexRemap[Index];
			LODModel.IndexBuffer.Add(VertexIndex);
		}
	}

	// Compute the required bones for this model.
	USkeletalMesh::CalculateRequiredBones(LODModel, RefSkeleton, NULL);

	return true;
}

void FAbcImporter::GenerateMorphTargetVertices(FAbcMeshSample* BaseSample, TArray<FMorphTargetDelta> &MorphDeltas, FAbcMeshSample* AverageSample, uint32 WedgeOffset, const TArray<int32>& RemapIndices, const TArray<int32>& UsedVertexIndicesForMorphs, const uint32 VertexOffset, const uint32 IndexOffset)
{
	FMorphTargetDelta MorphVertex;
	const uint32 NumberOfUsedVertices = UsedVertexIndicesForMorphs.Num();	
	for (uint32 VertIndex = 0; VertIndex < NumberOfUsedVertices; ++VertIndex)
	{
		const int32 UsedVertexIndex = UsedVertexIndicesForMorphs[VertIndex] - VertexOffset;
		const uint32 UsedNormalIndex = RemapIndices[VertIndex] - IndexOffset;

		if (UsedVertexIndex >= 0 && UsedVertexIndex < BaseSample->Vertices.Num())
		{			
			// Position delta
			MorphVertex.PositionDelta = BaseSample->Vertices[UsedVertexIndex] - AverageSample->Vertices[UsedVertexIndex];
			// Tangent delta
			MorphVertex.TangentZDelta = BaseSample->Normals[UsedNormalIndex] - AverageSample->Normals[UsedNormalIndex];
			// Index of base mesh vert this entry is to modify
			MorphVertex.SourceIdx = VertIndex;
			MorphDeltas.Add(MorphVertex);
		}
	}
}

UMaterialInterface* FAbcImporter::RetrieveMaterial(const FString& MaterialName, UObject* InParent, EObjectFlags Flags)
{
	UMaterialInterface* Material = nullptr;
	UMaterialInterface** CachedMaterial = AbcFile->GetMaterialByName(MaterialName);
	if (CachedMaterial)
	{
		Material = *CachedMaterial;
		// Material could have been deleted if we're overriding/reimporting an asset
		if (Material->IsValidLowLevel())
		{
			if (Material->GetOuter() == GetTransientPackage())
			{
				UMaterial* ExistingTypedObject = FindObject<UMaterial>(InParent, *MaterialName);
				if (!ExistingTypedObject)
				{
					// This is in for safety, as we do not expect this to happen
					UObject* ExistingObject = FindObject<UObject>(InParent, *MaterialName);
					if (ExistingObject)
					{
						return nullptr;
					}

					Material->Rename(*MaterialName, InParent);				
					Material->SetFlags(Flags);
					FAssetRegistryModule::AssetCreated(Material);
				}
				else
				{
					ExistingTypedObject->PreEditChange(nullptr);
					Material = ExistingTypedObject;
				}
			}
		}
		else
		{
			// In this case recreate the material
			Material = NewObject<UMaterial>(InParent, *MaterialName);
			Material->SetFlags(Flags);
			FAssetRegistryModule::AssetCreated(Material);
		}
	}
	else
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
		check(Material);
	}

	return Material;
}

FCompressedAbcData::~FCompressedAbcData()
{
	delete AverageSample;
	for (FAbcMeshSample* Sample : BaseSamples)
	{
		delete Sample;
	}
}

#undef LOCTEXT_NAMESPACE
