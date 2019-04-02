// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AbcFile.h"
#include "Misc/Paths.h"

#include "AbcImporter.h"
#include "AbcTransform.h"
#include "AbcPolyMesh.h"

#include "AbcImportUtilities.h"

#include "AssetRegistryModule.h"
#include "Materials/MaterialInterface.h"
#include "Logging/TokenizedMessage.h"
#include "AbcImportLogger.h"

#include "HAL/Platform.h"


#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

THIRD_PARTY_INCLUDES_START
#include <Alembic/AbcGeom/All.h>
#include "Materials/MaterialInstance.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#define LOCTEXT_NAMESPACE "AbcFile"

FAbcFile::FAbcFile(const FString& InFilePath)
	: FilePath(InFilePath)
	, RootObject(nullptr)
	, MinFrameIndex(TNumericLimits<int32>::Max())
	, MaxFrameIndex(TNumericLimits<int32>::Min())
	, ArchiveSecondsPerFrame(0.0)
	, NumFrames(0)	
	, FramesPerSecond(0)
	, SecondsPerFrame(0.0)
	, StartFrameIndex(0)
	, EndFrameIndex(0)
	, ArchiveBounds(EForceInit::ForceInitToZero)
	, MinTime(TNumericLimits<float>::Max())
	, MaxTime(TNumericLimits<float>::Min())
	, ImportTimeOffset(0.0f)
	, ImportLength(0.0f)
{
}

FAbcFile::~FAbcFile()
{
	for (FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		delete PolyMesh;
	}
	PolyMeshes.Empty();

	for (FAbcTransform* Transform : Transforms)
	{
		delete Transform;
	}
	Transforms.Empty();
}

EAbcImportError FAbcFile::Open()
{
	Factory.setPolicy(Alembic::Abc::ErrorHandler::kThrowPolicy);
	Factory.setOgawaNumStreams(12);
	
	// Extract Archive and compression type from file
	Archive = Factory.getArchive(TCHAR_TO_ANSI(*FPaths::ConvertRelativePathToFull(FilePath)), CompressionType);
	if (!Archive.valid())
	{
		return EAbcImportError::AbcImportError_InvalidArchive;
	}

	// Get Top/root object
	TopObject = Alembic::Abc::IObject(Archive, Alembic::Abc::kTop);
	if (!TopObject.valid())
	{
		return EAbcImportError::AbcImportError_NoValidTopObject;
	}

	TraverseAbcHierarchy(TopObject, nullptr);

	Alembic::Abc::ObjectHeader Header = TopObject.getHeader();

	// Determine top level archive bounding box
	const Alembic::Abc::MetaData ObjectMetaData = TopObject.getMetaData();
	Alembic::Abc::ICompoundProperty Properties = TopObject.getProperties();

	Alembic::Abc::IBox3dProperty ArchiveBoundsProperty = Alembic::AbcGeom::GetIArchiveBounds(Archive, Alembic::Abc::ErrorHandler::kQuietNoopPolicy);

	if (ArchiveBoundsProperty.valid())
	{
		ArchiveBounds = AbcImporterUtilities::ExtractBounds(ArchiveBoundsProperty);
	}

	const int32 TimeSamplingIndex = Archive.getNumTimeSamplings() > 1 ? 1 : 0;

	Alembic::Abc::TimeSamplingPtr TimeSampler = Archive.getTimeSampling(TimeSamplingIndex);
	if (TimeSampler)
	{
		ArchiveSecondsPerFrame = TimeSampler->getTimeSamplingType().getTimePerCycle();
	}

	MeshUtilities = FModuleManager::Get().LoadModulePtr<IMeshUtilities>("MeshUtilities");
	
	return EAbcImportError::AbcImportError_NoError;
}

EAbcImportError FAbcFile::Import(UAbcImportSettings* InImportSettings)
{
	ImportSettings = InImportSettings;	

	FAbcSamplingSettings& SamplingSettings = ImportSettings->SamplingSettings;

	StartFrameIndex = SamplingSettings.bSkipEmpty ? (SamplingSettings.FrameStart > MinFrameIndex ? SamplingSettings.FrameStart : MinFrameIndex) : SamplingSettings.FrameStart;
	EndFrameIndex = SamplingSettings.FrameEnd;

	if (ImportSettings->ImportType == EAlembicImportType::StaticMesh)
	{
		EndFrameIndex = StartFrameIndex + 1;
	}

	int32 FrameSpan = EndFrameIndex - StartFrameIndex;
	// If Start==End or Start > End output error message due to invalid frame span
	if (FrameSpan <= 0)
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error, FText::Format(LOCTEXT("NoFramesForMeshObject", "Invalid frame range specified {0} - {1}."), FText::FromString(FString::FromInt(StartFrameIndex)), FText::FromString(FString::FromInt(EndFrameIndex))));
		FAbcImportLogger::AddImportMessage(Message);
		return AbcImportError_FailedToImportData;
	}

	// Calculate time step and min/max frame indices according to user sampling settings
	float TimeStep = 0.0f;
	float CacheLength = MaxTime - MinTime;
	EAlembicSamplingType SamplingType = SamplingSettings.SamplingType;
	switch (SamplingType)
	{
	case EAlembicSamplingType::PerFrame:
	{
		// Calculates the time step required to get the number of frames
		TimeStep = !FMath::IsNearlyZero(ArchiveSecondsPerFrame) ? ArchiveSecondsPerFrame : CacheLength / (float)(MaxFrameIndex - MinFrameIndex);
		break;
	}

	case EAlembicSamplingType::PerTimeStep:
	{
		// Calculates the original time step and the ratio between it and the user specified time step
		const float OriginalTimeStep = CacheLength / (float)(MaxFrameIndex - MinFrameIndex);
		const float FrameStepRatio = OriginalTimeStep / SamplingSettings.TimeSteps;
		TimeStep = SamplingSettings.TimeSteps;

		AbcImporterUtilities::CalculateNewStartAndEndFrameIndices(FrameStepRatio, StartFrameIndex, EndFrameIndex);
		FrameSpan = EndFrameIndex - StartFrameIndex;
		break;
	}
	case EAlembicSamplingType::PerXFrames:
	{
		// Calculates the original time step and the ratio between it and the user specified time step
		const float OriginalTimeStep = CacheLength / (float)(MaxFrameIndex - MinFrameIndex);
		const float FrameStepRatio = OriginalTimeStep / ((float)SamplingSettings.FrameSteps * OriginalTimeStep);
		TimeStep = ((float)SamplingSettings.FrameSteps * OriginalTimeStep);

		AbcImporterUtilities::CalculateNewStartAndEndFrameIndices(FrameStepRatio, StartFrameIndex, EndFrameIndex);
		FrameSpan = EndFrameIndex - StartFrameIndex;
		break;
	}

	default:
		checkf(false, TEXT("Incorrect sampling type found in import settings (%i)"), (uint8)SamplingType);
	}

	SecondsPerFrame = TimeStep;
	ImportLength = FrameSpan * TimeStep;

	// Calculate time offset from start of import animation range
	ImportTimeOffset = StartFrameIndex * SecondsPerFrame;

	// Read first-frames for both the transforms and poly meshes

	bool bValidFirstFrames = true;
	for (FAbcTransform* Transform : Transforms)
	{
		bValidFirstFrames &= Transform->ReadFirstFrame(StartFrameIndex * SecondsPerFrame, StartFrameIndex);
	}

	for (FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		if (PolyMesh->bShouldImport)
		{
			bValidFirstFrames &= PolyMesh->ReadFirstFrame(StartFrameIndex * SecondsPerFrame, StartFrameIndex);
		}
	}	

	if (!bValidFirstFrames)
	{
		return AbcImportError_FailedToImportData;
	}

	// Add up all poly mesh bounds
	FBoxSphereBounds MeshBounds(EForceInit::ForceInitToZero);
	for (const FAbcPolyMesh* PolyMesh : PolyMeshes)
	{
		if (PolyMesh->bShouldImport)
		{
			MeshBounds = MeshBounds + PolyMesh->SelfBounds + PolyMesh->ChildBounds;
		}
	}

	// If there were not bounds available at the archive level or the mesh bounds are larger than the archive bounds use those
	if (FMath::IsNearlyZero(ArchiveBounds.SphereRadius) || MeshBounds.SphereRadius >ArchiveBounds.SphereRadius )
	{
		ArchiveBounds = MeshBounds;
	}
	AbcImporterUtilities::ApplyConversion(ArchiveBounds, ImportSettings->ConversionSettings);

	// If the users opted to try and find materials in the project whos names match one of the face sets
	if (ImportSettings->MaterialSettings.bFindMaterials)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> AssetData;
		const UClass* Class = UMaterialInterface::StaticClass();
		AssetRegistryModule.Get().GetAssetsByClass(Class->GetFName(), AssetData, true);
		for (FAbcPolyMesh* PolyMesh : PolyMeshes)
		{
			for (const FString& FaceSetName : PolyMesh->FaceSetNames)
			{
				UMaterialInterface** ExistingMaterial = MaterialMap.Find(*FaceSetName);
				if (!ExistingMaterial)
				{
					FAssetData* MaterialAsset = AssetData.FindByPredicate([=](const FAssetData& Asset)
					{
						return Asset.AssetName.ToString() == FaceSetName;
					});

					if (MaterialAsset)
					{
						UMaterialInterface* FoundMaterialInterface = Cast<UMaterialInterface>(MaterialAsset->GetAsset());
						if (FoundMaterialInterface)
						{							
							MaterialMap.Add(FaceSetName, FoundMaterialInterface);
							UMaterial* BaseMaterial = Cast<UMaterial>(FoundMaterialInterface);
							if ( !BaseMaterial )
							{
								if (UMaterialInstance* FoundInstance = Cast<UMaterialInstance>(FoundMaterialInterface))
								{
									BaseMaterial = FoundInstance->GetMaterial();
								}
							}

							if (BaseMaterial)
							{
								BaseMaterial->bUsedWithSkeletalMesh |= ImportSettings->ImportType == EAlembicImportType::Skeletal;
								BaseMaterial->bUsedWithMorphTargets |= ImportSettings->ImportType == EAlembicImportType::Skeletal;
								BaseMaterial->bUsedWithGeometryCache |= ImportSettings->ImportType == EAlembicImportType::GeometryCache;
							}							
						}
					}
					else
					{
						TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning, FText::Format(LOCTEXT("NoMaterialForFaceSet", "Unable to find matching Material for Face Set {0}, using default material instead."), FText::FromString(FaceSetName)));
						FAbcImportLogger::AddImportMessage(Message);
					}
				}
			}
		}
	}
	// Or the user opted to create materials with and for the faceset names in this ABC file
	else if (ImportSettings->MaterialSettings.bCreateMaterials)
	{
		// Creates materials according to the face set names that were found in the Alembic file
		for (FAbcPolyMesh* PolyMesh : PolyMeshes)
		{
			for (const FString& FaceSetName : PolyMesh->FaceSetNames)
			{
				// Preventing duplicate material creation
				UMaterialInterface** ExistingMaterial = MaterialMap.Find(*FaceSetName);
				if (!ExistingMaterial)
				{ 
					UMaterial* Material = NewObject<UMaterial>((UObject*)GetTransientPackage(), *FaceSetName);
					Material->bUsedWithMorphTargets = true;
					MaterialMap.Add(FaceSetName, Material);
				}
			}
		}
	}

	return AbcImportError_NoError;
}

void FAbcFile::TraverseAbcHierarchy(const Alembic::Abc::IObject& InObject, IAbcObject* InParent)
{
	// Get Header and MetaData info from current Alembic Object
	Alembic::AbcCoreAbstract::ObjectHeader Header = InObject.getHeader();
	const Alembic::Abc::MetaData ObjectMetaData = InObject.getMetaData();
	const uint32 NumChildren = InObject.getNumChildren();

	bool bHandled = false;

	IAbcObject* CreatedObject = nullptr;

	if (AbcImporterUtilities::IsType<Alembic::AbcGeom::IPolyMesh>(ObjectMetaData))
	{
		Alembic::AbcGeom::IPolyMesh Mesh = Alembic::AbcGeom::IPolyMesh(InObject, Alembic::Abc::kWrapExisting);
		
		FAbcPolyMesh* PolyMesh = new FAbcPolyMesh(Mesh, this, InParent);
		PolyMeshes.Add(PolyMesh);
		CreatedObject = PolyMesh;
		Objects.Add(CreatedObject);

		MinTime = FMath::Min(MinTime, PolyMesh->GetTimeForFirstData());
		MaxTime = FMath::Max(MaxTime, PolyMesh->GetTimeForLastData());
		NumFrames = FMath::Max(NumFrames, PolyMesh->GetNumberOfSamples());
		MinFrameIndex = FMath::Min(MinFrameIndex, PolyMesh->GetFrameIndexForFirstData());
		MaxFrameIndex = FMath::Max(MaxFrameIndex, PolyMesh->GetFrameIndexForFirstData() + PolyMesh->GetNumberOfSamples());

		bHandled = true;
	}
	else if (AbcImporterUtilities::IsType<Alembic::AbcGeom::IXform>(ObjectMetaData))
	{
		Alembic::AbcGeom::IXform Xform = Alembic::AbcGeom::IXform(InObject, Alembic::Abc::kWrapExisting);
		FAbcTransform* Transform = new FAbcTransform(Xform, this, InParent);
		Transforms.Add(Transform);
		CreatedObject = Transform;
		Objects.Add(CreatedObject);
		
		MinTime = FMath::Min(MinTime, Transform->GetTimeForFirstData());
		MaxTime = FMath::Max(MaxTime, Transform->GetTimeForLastData());
		NumFrames = FMath::Max(NumFrames, Transform->GetNumberOfSamples());
		MinFrameIndex = FMath::Min(MinFrameIndex, Transform->GetFrameIndexForFirstData());
		MaxFrameIndex = FMath::Max(MaxFrameIndex, Transform->GetFrameIndexForFirstData() + Transform->GetNumberOfSamples());

		bHandled = true;
	}

	if (RootObject == nullptr && CreatedObject != nullptr)
	{
		RootObject = CreatedObject;
	}

	// Recursive traversal of child objects
	if (NumChildren > 0)
	{
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			const Alembic::Abc::IObject& AbcChildObject = InObject.getChild(ChildIndex);
			TraverseAbcHierarchy(AbcChildObject, CreatedObject);
		}
	}
}

void FAbcFile::ReadFrame(int32 FrameIndex, const EFrameReadFlags InFlags, const int32 ReadIndex /*= INDEX_NONE*/)
{
	for (IAbcObject* Object : Objects)
	{
		Object->SetFrameAndTime(FrameIndex * SecondsPerFrame, FrameIndex, InFlags, ReadIndex);
	}
}

void FAbcFile::CleanupFrameData(const int32 ReadIndex)
{
	for (IAbcObject* Object : Objects)
	{
		Object->PurgeFrameData(ReadIndex);
	}
}

void FAbcFile::ProcessFrames(TFunctionRef<void(int32, FAbcFile*)> InCallback, const EFrameReadFlags InFlags)
{
	const int32 NumWorkerThreads = FMath::Min(FTaskGraphInterface::Get().GetNumWorkerThreads(), MaxNumberOfResidentSamples);
	const bool bSingleThreaded = (CompressionType == Alembic::AbcCoreFactory::IFactory::kHDF5) || ImportSettings->NumThreads == 1 || EnumHasAnyFlags(InFlags, EFrameReadFlags::ForceSingleThreaded);
	int32 ProcessedFrameIndex = StartFrameIndex - 1;

	if (bSingleThreaded)
	{
		for (int32 FrameIndex = StartFrameIndex; FrameIndex <= EndFrameIndex; ++FrameIndex)
		{
			ReadFrame(FrameIndex, InFlags, 0);
			InCallback(FrameIndex, this);
			CleanupFrameData(0);
		}
	}
	else
	{
		ParallelFor(NumWorkerThreads, [this, InFlags, InCallback, bSingleThreaded, NumWorkerThreads, &ProcessedFrameIndex](int32 ThreadIndex)
		{			
			int32 FrameIndex = INDEX_NONE;
				
			int32 StepIndex = 0;
			FrameIndex = (NumWorkerThreads * StepIndex + ThreadIndex) + StartFrameIndex;

			while (FrameIndex <= EndFrameIndex)
			{
				// Read frame data into memory
				ReadFrame(FrameIndex, InFlags, ThreadIndex);

				// Spin until we can process our frame
				while (ProcessedFrameIndex < (FrameIndex - 1))
				{	
					FPlatformProcess::Sleep(0.1f);
				}

				// Call user defined callback and mark this frame index as processed
				InCallback(FrameIndex, this);					
				ProcessedFrameIndex = FrameIndex;

				// Now cleanup the frame data
				CleanupFrameData(ThreadIndex);

				// Get new frame index to read for next run cycle
				++StepIndex;
				FrameIndex = (NumWorkerThreads * StepIndex + ThreadIndex) + StartFrameIndex;
			};
		});
	}
}

const int32 FAbcFile::GetMinFrameIndex() const
{
	return MinFrameIndex;
}

const int32 FAbcFile::GetMaxFrameIndex() const
{
	return MaxFrameIndex;
}

const UAbcImportSettings* FAbcFile::GetImportSettings() const 
{
	return ImportSettings;
}

const TArray<FAbcPolyMesh*>& FAbcFile::GetPolyMeshes() const
{
	return PolyMeshes;
}

const TArray<FAbcTransform*>& FAbcFile::GetTransforms() const
{
	return Transforms;
}

const int32 FAbcFile::GetNumPolyMeshes() const
{
	return PolyMeshes.Num();
}

const FString FAbcFile::GetFilePath() const
{
	return FilePath;
}

const float FAbcFile::GetImportTimeOffset() const
{
	return ImportTimeOffset;
}

const float FAbcFile::GetImportLength() const
{
	return ImportLength;
}

const int32 FAbcFile::GetFramerate() const
{
	return FramesPerSecond;
}

const FBoxSphereBounds& FAbcFile::GetArchiveBounds() const
{
	return ArchiveBounds;
}

const bool FAbcFile::ContainsHeterogeneousMeshes() const
{
	bool bHomogeneous = true;

	for (const FAbcPolyMesh* Mesh : PolyMeshes)
	{
		bHomogeneous &= ( Mesh->bConstantTopology || !Mesh->bShouldImport );
	}

	return !bHomogeneous;
}

IMeshUtilities* FAbcFile::GetMeshUtilities() const
{
	return MeshUtilities;
}

UMaterialInterface** FAbcFile::GetMaterialByName(const FString& InMaterialName)
{
	return MaterialMap.Find(InMaterialName);
}

#undef LOCTEXT_NAMESPACE // "AbcFile"