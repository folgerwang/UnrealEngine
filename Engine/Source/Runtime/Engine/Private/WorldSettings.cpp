// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GameFramework/WorldSettings.h"
#include "Misc/MessageDialog.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineDefines.h"
#include "EngineStats.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "GameFramework/DefaultPhysicsVolume.h"
#include "EngineUtils.h"
#include "Engine/AssetUserData.h"
#include "Engine/WorldComposition.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameNetworkManager.h"
#include "AudioDevice.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/MapErrors.h"
#include "Particles/ParticleEventManager.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "UObject/ReleaseObjectVersion.h"
#include "UObject/EnterpriseObjectVersion.h"
#include "SceneManagement.h"
#include "AI/AISystemBase.h"
#include "AI/NavigationSystemConfig.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/BookmarkBase.h"
#include "Engine/BookMark.h"

#if WITH_EDITOR
#include "Editor.h"
#include "HierarchicalLOD.h"
#include "IMeshMergeUtilities.h"
#include "MeshMergeModule.h"
#endif 

#define LOCTEXT_NAMESPACE "ErrorChecking"

DEFINE_LOG_CATEGORY_STATIC(LogWorldSettings, Log, All);

// @todo vreditor urgent: Temporary hack to allow world-to-meters to be set before
// input is polled for motion controller devices each frame.
ENGINE_API float GNewWorldToMetersScale = 0.0f;

#if WITH_EDITOR
AWorldSettings::FOnBookmarkClassChanged AWorldSettings::OnBookmarkClassChanged;
AWorldSettings::FOnNumberOfBookmarksChanged AWorldSettings::OnNumberOfBoomarksChanged;
#endif

AWorldSettings::AWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(TEXT("Sprite")))
{
	// Structure to hold one-time initialization
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinder<UClass> DmgType_Environmental_Object;
		FConstructorStatics()
			: DmgType_Environmental_Object(TEXT("/Engine/EngineDamageTypes/DmgTypeBP_Environmental.DmgTypeBP_Environmental_C"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	bEnableWorldBoundsChecks = true;
	bEnableNavigationSystem = true;
	NavigationSystemConfig = nullptr;
	bEnableAISystem = true;
	bEnableWorldComposition = false;
	bEnableWorldOriginRebasing = false;
#if WITH_EDITORONLY_DATA	
	bEnableHierarchicalLODSystem = false;

 	FHierarchicalSimplification LODBaseSetup;
	HierarchicalLODSetup.Add(LODBaseSetup);
	NumHLODLevels = HierarchicalLODSetup.Num();
	bGenerateSingleClusterForLevel = false;
#endif

	KillZ = -HALF_WORLD_MAX1;
	KillZDamageType = ConstructorStatics.DmgType_Environmental_Object.Object;

	WorldToMeters = 100.f;
	MonoCullingDistance = 750.f;

	DefaultPhysicsVolumeClass = ADefaultPhysicsVolume::StaticClass();
	GameNetworkManagerClass = AGameNetworkManager::StaticClass();
	SetRemoteRoleForBackwardsCompat(ROLE_SimulatedProxy);
	bReplicates = true;
	bAlwaysRelevant = true;
	TimeDilation = 1.0f;
	MatineeTimeDilation = 1.0f;
	DemoPlayTimeDilation = 1.0f;
	PackedLightAndShadowMapTextureSize = 1024;
	bHidden = false;

	DefaultColorScale = FVector(1.0f, 1.0f, 1.0f);
	DefaultMaxDistanceFieldOcclusionDistance = 600;
	GlobalDistanceFieldViewDistance = 20000;
	DynamicIndirectShadowsSelfShadowingIntensity = .8f;
	bPlaceCellsOnlyAlongCameraTracks = false;
	VisibilityCellSize = 200;
	VisibilityAggressiveness = VIS_LeastAggressive;

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif // WITH_EDITORONLY_DATA

	bReplayRewindable = true;

	MaxNumberOfBookmarks = 10;

	DefaultBookmarkClass = UBookMark::StaticClass();
	LastBookmarkClass = DefaultBookmarkClass;
}

void AWorldSettings::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_NeedLoad|RF_WasLoaded|RF_ClassDefaultObject) == false)
	{
		TSubclassOf<UNavigationSystemConfig> NavSystemConfigClass = UNavigationSystemConfig::GetDefaultConfigClass();
		if (*NavSystemConfigClass)
		{
			NavigationSystemConfig = NewObject<UNavigationSystemConfig>(this, NavSystemConfigClass);
		}
	}

	if (MinGlobalTimeDilation < 0)
	{
		MinGlobalTimeDilation = 0;
	}

	if (MaxGlobalTimeDilation < 0)
	{
		MaxGlobalTimeDilation = 0;
	}

	if (MinUndilatedFrameTime < 0)
	{
		MinUndilatedFrameTime = 0;
	}

	if (MaxUndilatedFrameTime < 0)
	{
		MaxUndilatedFrameTime = 0;
	}

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UpdateNumberOfBookmarks();
		UpdateBookmarkClass();
	}
}

void AWorldSettings::PreInitializeComponents()
{
	Super::PreInitializeComponents();

	// create the emitter pool
	// we only need to do this for the persistent level's WorldSettings as sublevel actors will have their WorldSettings set to it on association
	if (GetNetMode() != NM_DedicatedServer && IsInPersistentLevel())
	{
		UWorld* World = GetWorld();
		check(World);		

		// only create once - 
		if (World->MyParticleEventManager == NULL && !GEngine->ParticleEventManagerClassPath.IsEmpty())
		{
			UObject* Object = StaticLoadObject(UClass::StaticClass(), NULL, *GEngine->ParticleEventManagerClassPath, NULL, LOAD_NoWarn, NULL);
			if (Object != NULL)
			{
				TSubclassOf<AParticleEventManager> ParticleEventManagerClass = Cast<UClass>(Object);
				if (ParticleEventManagerClass != NULL)
				{
					FActorSpawnParameters SpawnParameters;
					SpawnParameters.Owner = this;
					SpawnParameters.Instigator = Instigator;
					SpawnParameters.ObjectFlags |= RF_Transient;	// We never want to save particle event managers into a map
					World->MyParticleEventManager = World->SpawnActor<AParticleEventManager>(ParticleEventManagerClass, SpawnParameters);
				}
			}
		}
	}
}

void AWorldSettings::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (GEngine->IsConsoleBuild())
	{
		GEngine->bUseConsoleInput = true;
	}
}

void AWorldSettings::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	UWorld* World = GetWorld();
	if (FAudioDevice* AudioDevice = World->GetAudioDevice())
	{
		AudioDevice->SetDefaultAudioSettings(World, DefaultReverbSettings, DefaultAmbientZoneSettings);
	}
}

float AWorldSettings::GetGravityZ() const
{
	if (!bWorldGravitySet)
	{
		// try to initialize cached value
		AWorldSettings* const MutableThis = const_cast<AWorldSettings*>(this);
		MutableThis->WorldGravityZ = bGlobalGravitySet ? GlobalGravityZ : UPhysicsSettings::Get()->DefaultGravityZ;	//allows us to override DefaultGravityZ
	}

	return WorldGravityZ;
}

void AWorldSettings::OnRep_WorldGravityZ()
{
	bWorldGravitySet = true;
}

float AWorldSettings::FixupDeltaSeconds(float DeltaSeconds, float RealDeltaSeconds)
{
	// DeltaSeconds is assumed to be fully dilated at this time, so we will dilate the clamp range as well
	float const Dilation = GetEffectiveTimeDilation();
	float const MinFrameTime = MinUndilatedFrameTime * Dilation;
	float const MaxFrameTime = MaxUndilatedFrameTime * Dilation;

	// clamp frame time according to desired limits
	return FMath::Clamp(DeltaSeconds, MinFrameTime, MaxFrameTime);	
}

float AWorldSettings::SetTimeDilation(float NewTimeDilation)
{
	TimeDilation = FMath::Clamp(NewTimeDilation, MinGlobalTimeDilation, MaxGlobalTimeDilation);
	return TimeDilation;
}

void AWorldSettings::NotifyBeginPlay()
{
	UWorld* World = GetWorld();
	if (!World->bBegunPlay)
	{
		for (FActorIterator It(World); It; ++It)
		{
			SCOPE_CYCLE_COUNTER(STAT_ActorBeginPlay);
			It->DispatchBeginPlay();
		}
		World->bBegunPlay = true;
	}
}

void AWorldSettings::NotifyMatchStarted()
{
	UWorld* World = GetWorld();
	World->bMatchStarted = true;
}

void AWorldSettings::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps( OutLifetimeProps );

	DOREPLIFETIME( AWorldSettings, Pauser );
	DOREPLIFETIME( AWorldSettings, TimeDilation );
	DOREPLIFETIME( AWorldSettings, MatineeTimeDilation );
	DOREPLIFETIME( AWorldSettings, WorldGravityZ );
	DOREPLIFETIME( AWorldSettings, bHighPriorityLoading );
}

void AWorldSettings::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FEnterpriseObjectVersion::GUID);

	if (Ar.UE4Ver() < VER_UE4_ADD_OVERRIDE_GRAVITY_FLAG)
	{
		//before we had override flag we would use GlobalGravityZ != 0
		if(GlobalGravityZ != 0.0f)
		{
			bGlobalGravitySet = true;
		}
	}
#if WITH_EDITOR	
	if (Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::ConvertHLODScreenSize)
	{
		for (FHierarchicalSimplification& Setup : HierarchicalLODSetup)
		{
			const float OldScreenSize = Setup.TransitionScreenSize;

			const float HalfFOV = PI * 0.25f;
			const float ScreenWidth = 1920.0f;
			const float ScreenHeight = 1080.0f;
			const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);

			const float DummySphereRadius = 16.0f;
			const float ScreenArea = OldScreenSize * (ScreenWidth * ScreenHeight);
			const float ScreenRadius = FMath::Sqrt(ScreenArea / PI);
			const float ScreenDistance = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * DummySphereRadius / ScreenRadius;

			Setup.TransitionScreenSize = ComputeBoundsScreenSize(FVector::ZeroVector, DummySphereRadius, FVector(0.0f, 0.0f, ScreenDistance), ProjMatrix);
		}
	}
#endif

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Ar.IsLoading())
	{
		if (Ar.CustomVer(FEnterpriseObjectVersion::GUID) < FEnterpriseObjectVersion::BookmarkExtensibilityUpgrade)
		{
			UBookmarkBase** LocalBookmarks = reinterpret_cast<UBookmarkBase**>(static_cast<UBookMark**>(BookMarks)); //-V777
			const int32 NumBookmarks = sizeof(BookMarks) / sizeof(UBookMark*);
			BookmarkArray = TArray<UBookmarkBase*>(LocalBookmarks, NumBookmarks);
			AdjustNumberOfBookmarks();
		}
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void AWorldSettings::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != NULL)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != NULL)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* AWorldSettings::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return NULL;
}
#if WITH_EDITOR
const TArray<FHierarchicalSimplification>& AWorldSettings::GetHierarchicalLODSetup() const
{
	const UHierarchicalLODSettings* HLODSettings = GetDefault<UHierarchicalLODSettings>();

	// If we have a HLOD asset set use this
	if (HLODSetupAsset.LoadSynchronous())
	{
		return HLODSetupAsset->GetDefaultObject<UHierarchicalLODSetup>()->HierarchicalLODSetup;
	}
	else if (HLODSettings->bForceSettingsInAllMaps && HLODSettings->DefaultSetup.IsValid())
	{
		return HLODSettings->DefaultSetup->GetDefaultObject<UHierarchicalLODSetup>()->HierarchicalLODSetup;
	}
	
	return HierarchicalLODSetup;
}

TArray<FHierarchicalSimplification>& AWorldSettings::GetHierarchicalLODSetup()
{
	UHierarchicalLODSettings* HLODSettings = GetMutableDefault<UHierarchicalLODSettings>();
	
	// If we have a HLOD asset set use this
	if (HLODSetupAsset.LoadSynchronous())
	{
		return HLODSetupAsset->GetDefaultObject<UHierarchicalLODSetup>()->HierarchicalLODSetup;
	}
	else if (HLODSettings->bForceSettingsInAllMaps && HLODSettings->DefaultSetup.LoadSynchronous())
	{
		return HLODSettings->DefaultSetup->GetDefaultObject<UHierarchicalLODSetup>()->HierarchicalLODSetup;
	}

	return HierarchicalLODSetup;
}

int32 AWorldSettings::GetNumHierarchicalLODLevels() const
{
	const UHierarchicalLODSettings* HLODSettings = GetDefault<UHierarchicalLODSettings>();

	// If we have a HLOD asset set use this
	if (HLODSetupAsset.LoadSynchronous())
	{
		return HLODSetupAsset->GetDefaultObject<UHierarchicalLODSetup>()->HierarchicalLODSetup.Num();
	}
	else if (HLODSettings->bForceSettingsInAllMaps && HLODSettings->DefaultSetup.IsValid())
	{
		return HLODSettings->DefaultSetup->GetDefaultObject<UHierarchicalLODSetup>()->HierarchicalLODSetup.Num();
	}

	return  HierarchicalLODSetup.Num();
}

UMaterialInterface* AWorldSettings::GetHierarchicalLODBaseMaterial() const
{
	UMaterialInterface* Material = GetDefault<UHierarchicalLODSettings>()->BaseMaterial.LoadSynchronous();

	if (!OverrideBaseMaterial.IsNull())
	{
		Material = OverrideBaseMaterial.LoadSynchronous();
	}

	if (HLODSetupAsset.LoadSynchronous())
	{
		if (!HLODSetupAsset->GetDefaultObject<UHierarchicalLODSetup>()->OverrideBaseMaterial.IsNull())
		{
			Material = HLODSetupAsset->GetDefaultObject<UHierarchicalLODSetup>()->OverrideBaseMaterial.LoadSynchronous();
		}
	}

	return Material;
}

#endif // WITH_EDITOR

void AWorldSettings::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != NULL && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

void AWorldSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	for (FHierarchicalSimplification& Entry : HierarchicalLODSetup)
	{
		Entry.ProxySetting.PostLoadDeprecated();
		Entry.MergeSetting.LODSelectionType = EMeshLODSelectionType::CalculateLOD;
	}

#endif// WITH_EDITOR

	if (bEnableNavigationSystem && NavigationSystemConfig == nullptr)
	{
		ULevel* Level = GetLevel();
		if (Level && Level->IsPersistentLevel())
		{
			TSubclassOf<UNavigationSystemConfig> NavSystemConfigClass = UNavigationSystemConfig::GetDefaultConfigClass();
			if (*NavSystemConfigClass)
			{
				NavigationSystemConfig = NewObject<UNavigationSystemConfig>(this, NavSystemConfigClass);
			}
			bEnableNavigationSystem = false;
		}
	}
}

bool AWorldSettings::IsNavigationSystemEnabled() const
{
	return NavigationSystemConfig && NavigationSystemConfig->NavigationSystemClass.IsValid();
}


#if WITH_EDITOR

void AWorldSettings::CheckForErrors()
{
	Super::CheckForErrors();

	UWorld* World = GetWorld();
	if ( World->GetWorldSettings() != this )
	{
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_DuplicateLevelInfo", "Duplicate level info" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::DuplicateLevelInfo));
	}

	int32 NumLightingScenariosEnabled = 0;

	for (int32 LevelIndex = 0; LevelIndex < World->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = World->GetLevels()[LevelIndex];

		if (Level->bIsLightingScenario && Level->bIsVisible)
		{
			NumLightingScenariosEnabled++;
		}
	}

	if( World->NumLightingUnbuiltObjects > 0 && NumLightingScenariosEnabled <= 1 )
	{
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_RebuildLighting", "Maps need lighting rebuilt" ) ))
			->AddToken(FMapErrorToken::Create(FMapErrors::RebuildLighting));
	}
}

bool AWorldSettings::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (InProperty->GetOuter()
			&& InProperty->GetOuter()->GetName() == TEXT("LightmassWorldInfoSettings"))
		{
			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, bGenerateAmbientOcclusionMaterialMask)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, DirectIlluminationOcclusionFraction)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, IndirectIlluminationOcclusionFraction)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, OcclusionExponent)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, FullyOccludedSamplesFraction)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, MaxOcclusionDistance)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, bVisualizeAmbientOcclusion))
			{
				return LightmassSettings.bUseAmbientOcclusion;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumetricLightmapDetailCellSize)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumetricLightmapMaximumBrickMemoryMb)
				|| PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumetricLightmapSphericalHarmonicSmoothing))
			{
				return LightmassSettings.VolumeLightingMethod == VLM_VolumetricLightmap;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, VolumeLightSamplePlacementScale))
			{
				return LightmassSettings.VolumeLightingMethod == VLM_SparseVolumeLightingSamples;
			}

			if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FLightmassWorldInfoSettings, EnvironmentColor))
			{
				return LightmassSettings.EnvironmentIntensity > 0;
			}
		}
	}

	return Super::CanEditChange(InProperty);
}

void AWorldSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FName MemberPropertyName = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue()->GetFName();

	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultReverbSettings) || MemberPropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultAmbientZoneSettings))
	{
		UWorld* World = GetWorld();
		if (FAudioDevice* AudioDevice = World->GetAudioDevice())
		{
			AudioDevice->SetDefaultAudioSettings(World, DefaultReverbSettings, DefaultAmbientZoneSettings);
		}
	}
}

void AWorldSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		const FName PropertyName = PropertyThatChanged->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings,bForceNoPrecomputedLighting) && bForceNoPrecomputedLighting)
		{
			FMessageDialog::Open( EAppMsgType::Ok, LOCTEXT("bForceNoPrecomputedLightingIsEnabled", "bForceNoPrecomputedLighting is now enabled, build lighting once to propagate the change (will remove existing precomputed lighting data)."));
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings,bEnableWorldComposition))
		{
			if (UWorldComposition::EnableWorldCompositionEvent.IsBound())
			{
				bEnableWorldComposition = UWorldComposition::EnableWorldCompositionEvent.Execute(GetWorld(), bEnableWorldComposition);
			}
			else
			{
				bEnableWorldComposition = false;
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, NavigationSystemConfig))
		{
			UWorld* World = GetWorld();
			if (World)
			{
				World->SetNavigationSystem(nullptr);
				if (NavigationSystemConfig)
				{
					FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
				}
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, MaxNumberOfBookmarks))
		{
			UpdateNumberOfBookmarks();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultBookmarkClass))
		{
			UpdateBookmarkClass();
		}
	}

	LightmassSettings.NumIndirectLightingBounces = FMath::Clamp(LightmassSettings.NumIndirectLightingBounces, 0, 100);
	LightmassSettings.NumSkyLightingBounces = FMath::Clamp(LightmassSettings.NumSkyLightingBounces, 0, 100);
	LightmassSettings.IndirectLightingSmoothness = FMath::Clamp(LightmassSettings.IndirectLightingSmoothness, .25f, 10.0f);
	LightmassSettings.VolumeLightSamplePlacementScale = FMath::Clamp(LightmassSettings.VolumeLightSamplePlacementScale, .1f, 100.0f);
	LightmassSettings.VolumetricLightmapDetailCellSize = FMath::Clamp(LightmassSettings.VolumetricLightmapDetailCellSize, 1.0f, 10000.0f);
	LightmassSettings.IndirectLightingQuality = FMath::Clamp(LightmassSettings.IndirectLightingQuality, .1f, 100.0f);
	LightmassSettings.StaticLightingLevelScale = FMath::Clamp(LightmassSettings.StaticLightingLevelScale, .001f, 1000.0f);
	LightmassSettings.EmissiveBoost = FMath::Max(LightmassSettings.EmissiveBoost, 0.0f);
	LightmassSettings.DiffuseBoost = FMath::Max(LightmassSettings.DiffuseBoost, 0.0f);
	LightmassSettings.DirectIlluminationOcclusionFraction = FMath::Clamp(LightmassSettings.DirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.IndirectIlluminationOcclusionFraction = FMath::Clamp(LightmassSettings.IndirectIlluminationOcclusionFraction, 0.0f, 1.0f);
	LightmassSettings.OcclusionExponent = FMath::Max(LightmassSettings.OcclusionExponent, 0.0f);
	LightmassSettings.FullyOccludedSamplesFraction = FMath::Clamp(LightmassSettings.FullyOccludedSamplesFraction, 0.0f, 1.0f);
	LightmassSettings.MaxOcclusionDistance = FMath::Max(LightmassSettings.MaxOcclusionDistance, 0.0f);
	LightmassSettings.EnvironmentIntensity = FMath::Max(LightmassSettings.EnvironmentIntensity, 0.0f);

	// Ensure texture size is power of two between 512 and 4096.
	PackedLightAndShadowMapTextureSize = FMath::Clamp<uint32>( FMath::RoundUpToPowerOfTwo( PackedLightAndShadowMapTextureSize ), 512, 4096 );

	if (PropertyThatChanged != nullptr && GetWorld() != nullptr && GetWorld()->PersistentLevel->GetWorldSettings() == this)
	{
		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification,TransitionScreenSize))
		{
			GEditor->BroadcastHLODTransitionScreenSizeChanged();
		}

		else if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(AWorldSettings,HierarchicalLODSetup))
		{
			GEditor->BroadcastHLODLevelsArrayChanged();
			NumHLODLevels = HierarchicalLODSetup.Num();			
		}
		else if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(AWorldSettings, OverrideBaseMaterial))
		{
			if (!OverrideBaseMaterial.IsNull())
			{
				const IMeshMergeUtilities& Module = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
				if (!Module.IsValidBaseMaterial(OverrideBaseMaterial.LoadSynchronous(), true))
				{
					OverrideBaseMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EngineMaterials/BaseFlattenMaterial.BaseFlattenMaterial"), NULL, LOAD_None, NULL);
				}
			}
		}
	}

	if (PropertyThatChanged != nullptr && GetWorld() != nullptr && GetWorld()->Scene)
	{
		GetWorld()->Scene->UpdateSceneSettings(this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UHierarchicalLODSetup::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHierarchicalLODSetup, OverrideBaseMaterial))
	{
		if (!OverrideBaseMaterial.IsNull())
		{
			const IMeshMergeUtilities& Module = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
			if (!Module.IsValidBaseMaterial(OverrideBaseMaterial.LoadSynchronous(), true))
			{
				OverrideBaseMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("/Engine/EngineMaterials/BaseFlattenMaterial.BaseFlattenMaterial"), NULL, LOAD_None, NULL);
			}
		}
	}
}
#endif // WITH_EDITOR

void AWorldSettings::CompactBookmarks()
{
	int32 LowIndex = 0;
	int32 HighIndex = NumMappedBookmarks;

	while (true)
	{
		// Find the next available spot.
		while (true)
		{
			if (BookmarkArray.IsValidIndex(LowIndex))
			{
				// Found an empty spot, so we can move on.
				if (BookmarkArray[LowIndex] == nullptr)
				{
					break;
				}

				++LowIndex;
			}
			else
			{
				// There's no more spots to check, so we're done.
				return;
			}
		}

		// Find the next filled spot.
		HighIndex = FMath::Max(HighIndex, LowIndex + 1);
		while (true)
		{
			if (BookmarkArray.IsValidIndex(HighIndex))
			{
				// Found a valid filled spot, so we can move on.
				if (BookmarkArray[HighIndex] != nullptr)
				{
					break;
				}

				++HighIndex;
			}
			else
			{
				// There's no more spots to check, so we're done.
				return;
			}
		}

		// Swap the filled slot element into the empty slot.
		BookmarkArray.Swap(LowIndex, HighIndex);
		++LowIndex;
		++HighIndex;
	}
}


class UBookmarkBase* AWorldSettings::GetOrAddBookmark(const uint32 BookmarkIndex, const bool bRecreateOnClassMismatch)
{
	if (BookmarkArray.IsValidIndex(BookmarkIndex))
	{
		UBookmarkBase*& Bookmark = BookmarkArray[BookmarkIndex];

		if (Bookmark == nullptr || (bRecreateOnClassMismatch && Bookmark->GetClass() != GetDefaultBookmarkClass()))
		{
			Bookmark = NewObject<UBookmarkBase>(this, GetDefaultBookmarkClass());
		}

		return Bookmark;
	}

	return nullptr;
}

void AWorldSettings::AdjustNumberOfBookmarks()
{
	if (MaxNumberOfBookmarks < 0)
	{
		UE_LOG(LogWorldSettings, Warning, TEXT("%s: MaxNumberOfBookmarks cannot be below 0 (Value=%d). Defaulting to 10"), *GetPathName(this), MaxNumberOfBookmarks);
		MaxNumberOfBookmarks = NumMappedBookmarks;
	}

	if (MaxNumberOfBookmarks < BookmarkArray.Num())
	{
		UE_LOG(LogWorldSettings, Warning, TEXT("%s: MaxNumberOfBookmarks set below current number of bookmarks. Clearing %d bookmarks."), *GetPathNameSafe(this), BookmarkArray.Num() - MaxNumberOfBookmarks);
	}

	BookmarkArray.SetNumZeroed(MaxNumberOfBookmarks);
}

void AWorldSettings::UpdateNumberOfBookmarks()
{
	if (MaxNumberOfBookmarks != BookmarkArray.Num())
	{
		AdjustNumberOfBookmarks();

#if WITH_EDITOR
		OnNumberOfBoomarksChanged.Broadcast(this);
#endif
	}
}

void AWorldSettings::SanitizeBookmarkClasses()
{
	if (UClass* ExpectedClass = GetDefaultBookmarkClass().Get())
	{
		bool bFoundInvalidBookmarks = false;
		for (int32 i = 0; i < BookmarkArray.Num(); ++i)
		{
			if (UBookmarkBase* Bookmark = BookmarkArray[i])
			{
				if (Bookmark->GetClass() != ExpectedClass)
				{
					// Just clear the reference, this bookmark should get cleaned up next GC cycle.
					BookmarkArray[i] = nullptr;
					bFoundInvalidBookmarks = true;
				}
			}
		}

		if (bFoundInvalidBookmarks)
		{
			UE_LOG(LogWorldSettings, Warning, TEXT("%s: Bookmarks found with invalid classes"), *GetPathName(this));
		}
	}
	else
	{
		UE_LOG(LogWorldSettings, Warning, TEXT("%s: Invalid bookmark class, clearing existing bookmarks."), *GetPathName(this));
		DefaultBookmarkClass = UBookMark::StaticClass();
		SanitizeBookmarkClasses();
	}
}

void AWorldSettings::UpdateBookmarkClass()
{
	if (LastBookmarkClass != DefaultBookmarkClass)
	{

#if WITH_EDITOR
		OnBookmarkClassChanged.Broadcast(this);
#endif

		// Explicitly done after OnBookmarkClassChanged, in case there's any upgrade work
		// that can be done.
		SanitizeBookmarkClasses();
		
		LastBookmarkClass = DefaultBookmarkClass;
	}
}

FSoftClassPath AWorldSettings::GetAISystemClassName() const
{
	return bEnableAISystem ? UAISystemBase::GetAISystemClassName() : FSoftClassPath();
}

#undef LOCTEXT_NAMESPACE
