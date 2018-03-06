// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LODActorBase.cpp: Static mesh actor base class implementation.
=============================================================================*/

#include "Engine/LODActor.h"
#include "UObject/UObjectIterator.h"
#include "Engine/CollisionProfile.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/MapErrors.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "EngineUtils.h"
#include "UObject/FrameworkObjectVersion.h"
#include "UObject/AthenaObjectVersion.h"

#if WITH_EDITOR
#include "Editor.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "ObjectTools.h"
#include "HierarchicalLOD.h"
#endif

#define LOCTEXT_NAMESPACE "LODActor"

int32 GMaximumAllowedHLODLevel = -1;

static FAutoConsoleVariableRef CVarMaximumAllowedHLODLevel(
	TEXT("r.HLOD.MaximumLevel"),
	GMaximumAllowedHLODLevel,
	TEXT("How far down the LOD hierarchy to allow showing (can be used to limit quality loss and streaming texture memory usage on high scalability settings)\n")
	TEXT("-1: No maximum level (default)\n")
	TEXT("0: Prevent ever showing a HLOD cluster instead of individual meshes\n")
	TEXT("1: Allow only the first level of HLOD clusters to be shown\n")
	TEXT("2+: Allow up to the Nth level of HLOD clusters to be shown"),
	ECVF_Scalability);

static TAutoConsoleVariable<float> CVarHLODDitherPauseTime(
	TEXT("r.HLOD.DitherPauseTime"),
	0.5f,
	TEXT("HLOD dither pause time in seconds\n"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

ENGINE_API TAutoConsoleVariable<float> CVarHLODDistanceScale(
	TEXT("r.HLOD.DistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing discrete HLOD for transition for static meshes. (defaults to 1)\n")
	TEXT("(higher values make HLODs transition farther away, e.g., 2 is twice the distance)"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

ENGINE_API TAutoConsoleVariable<FString> CVarHLODDistanceOverride(
	TEXT("r.HLOD.DistanceOverride"),
	"0.0",
	TEXT("If non-zero, overrides the distance that HLOD transitions will take place for all objects at the HLOD level index, formatting is as follows:\n\tr.HLOD.DistanceOverride 5000, 10000, 20000 this would result in HLOD levels 0, 1 and 2 transitioning at respectively 5000, 1000 and 20000.\n"),
	ECVF_Scalability);

ENGINE_API TArray<float> ALODActor::HLODDistances;

#if !(UE_BUILD_SHIPPING)
static void HLODConsoleCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() == 1)
	{
		const int32 State = FCString::Atoi(*Args[0]);

		if (State == 0 || State == 1)
		{
			const bool bHLODEnabled = (State == 1) ? true : false;
			FlushRenderingCommands();
			const TArray<ULevel*>& Levels = World->GetLevels();
			for (ULevel* Level : Levels)
			{
				for (AActor* Actor : Level->Actors)
				{
					ALODActor* LODActor = Cast<ALODActor>(Actor);
					if (LODActor)
					{
						LODActor->SetActorHiddenInGame(!bHLODEnabled);
#if WITH_EDITOR
						LODActor->SetIsTemporarilyHiddenInEditor(!bHLODEnabled);
#endif // WITH_EDITOR
						LODActor->MarkComponentsRenderStateDirty();
					}
				}
			}
		}
	}
	else if (Args.Num() == 2)
	{
#if WITH_EDITOR
		if (Args[0] == "force")
		{
			const int32 ForcedLevel = FCString::Atoi(*Args[1]);

			if (ForcedLevel >= -1 && ForcedLevel < World->GetWorldSettings()->GetNumHierarchicalLODLevels())
			{
				const TArray<ULevel*>& Levels = World->GetLevels();
				for (ULevel* Level : Levels)
				{
					for (AActor* Actor : Level->Actors)
					{
						ALODActor* LODActor = Cast<ALODActor>(Actor);

						if (LODActor)
						{
							if (ForcedLevel != -1)
							{
								if (LODActor->LODLevel == ForcedLevel + 1)
								{
									LODActor->SetForcedView(true);
								}
								else
								{
									LODActor->SetHiddenFromEditorView(true, ForcedLevel + 1);
								}
							}
							else
							{
								LODActor->SetForcedView(false);
								LODActor->SetIsTemporarilyHiddenInEditor(false);
							}
						}
					}
				}
			}
		}		
#endif // WITH_EDITOR
	}
}

static FAutoConsoleCommandWithWorldAndArgs GHLODCmd(
	TEXT("r.HLOD"),
	TEXT("Single argument: 0 or 1 to Disable/Enable HLOD System\nMultiple arguments: force X where X is the HLOD level that should be forced into view"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(HLODConsoleCommand)
	);

static void ListUnbuiltHLODActors(const TArray<FString>& Args, UWorld* World)
{
	int32 NumUnbuilt = 0;
	for (TActorIterator<ALODActor> HLODIt(World); HLODIt; ++HLODIt)
	{
		ALODActor* Actor = *HLODIt;
		if (!Actor->IsBuilt())
		{
			++NumUnbuilt;
			FString ActorPathName = Actor->GetPathName(World);
			UE_LOG(LogInit, Warning, TEXT("HLOD %s is unbuilt"), *ActorPathName);
		}
	}

	UE_LOG(LogInit, Warning, TEXT("%d HLOD actor(s) were unbuilt"), NumUnbuilt);
}

static FAutoConsoleCommandWithWorldAndArgs GHLODListUnbuiltCmd(
	TEXT("r.HLOD.ListUnbuilt"),
	TEXT("Lists all unbuilt HLOD actors in the world"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(ListUnbuiltHLODActors)
);

#endif // !(UE_BUILD_SHIPPING)

//////////////////////////////////////////////////////////////////////////
// ALODActor

FAutoConsoleVariableSink ALODActor::CVarSink(FConsoleCommandDelegate::CreateStatic(&ALODActor::OnCVarsChanged));

ALODActor::ALODActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LODDrawDistance(5000)
	, bHasActorTriedToRegisterComponents(false)
{
	bCanBeDamaged = false;

	// Cast shadows if any sub-actors do
	bool bCastsShadow = false;
	bool bCastsStaticShadow = false;
	bool bCastsDynamicShadow = false;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;

#if WITH_EDITORONLY_DATA
	
	bListedInSceneOutliner = false;

	// Always dirty when created
	bDirty = true;

	NumTrianglesInSubActors = 0;
	NumTrianglesInMergedMesh = 0;
	
#endif // WITH_EDITORONLY_DATA

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent0"));
	StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	StaticMeshComponent->Mobility = EComponentMobility::Static;
	StaticMeshComponent->SetGenerateOverlapEvents(false);
	StaticMeshComponent->CastShadow = bCastsShadow;
	StaticMeshComponent->bCastStaticShadow = bCastsStaticShadow;
	StaticMeshComponent->bCastDynamicShadow = bCastsDynamicShadow;
	StaticMeshComponent->bAllowCullDistanceVolume = false;
	StaticMeshComponent->bNeverDistanceCull = true;
	bNeedsDrawDistanceReset = false;
	ResetDrawDistanceTime = 0.0f;
	RootComponent = StaticMeshComponent;	
	CachedNumHLODLevels = 1;
}

FString ALODActor::GetDetailedInfoInternal() const
{
	return StaticMeshComponent ? StaticMeshComponent->GetDetailedInfoInternal() : TEXT("No_StaticMeshComponent");
}

void ALODActor::PostLoad()
{
	Super::PostLoad();
	StaticMeshComponent->MinDrawDistance = LODDrawDistance;
	StaticMeshComponent->bCastDynamicShadow = false;	
	UpdateRegistrationToMatchMaximumLODLevel();

#if WITH_EDITOR
	if (bRequiresLODScreenSizeConversion)
	{
		if (TransitionScreenSize == 0.0f)
		{
			TransitionScreenSize = 1.0f;
		}
		else
		{
			const float HalfFOV = PI * 0.25f;
			const float ScreenWidth = 1920.0f;
			const float ScreenHeight = 1080.0f;
			const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);
			FBoxSphereBounds Bounds = GetStaticMeshComponent()->CalcBounds(FTransform());

			// legacy transition screen size was previously a screen AREA fraction using resolution-scaled values, so we need to convert to distance first to correctly calculate the threshold
			const float ScreenArea = TransitionScreenSize * (ScreenWidth * ScreenHeight);
			const float ScreenRadius = FMath::Sqrt(ScreenArea / PI);
			const float ScreenDistance = FMath::Max(ScreenWidth / 2.0f * ProjMatrix.M[0][0], ScreenHeight / 2.0f * ProjMatrix.M[1][1]) * Bounds.SphereRadius / ScreenRadius;

			// Now convert using the query function
			TransitionScreenSize = ComputeBoundsScreenSize(FVector::ZeroVector, Bounds.SphereRadius, FVector(0.0f, 0.0f, ScreenDistance), ProjMatrix);
		}
	}

	CachedNumHLODLevels = GetLevel()->GetWorldSettings()->GetNumHierarchicalLODLevels();

	if (bDirty)
	{
		// Temporarily disabling this warning while it is being worked on as it is spamming cook logs and happens even for minor changes.
		/*
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("ALODActor_PostRegisterAllComponents", "HLOD Cluster {ActorName} is out of date."), Arguments)));

		// Show MapCheck window
		FMessageLog("MapCheck").Open(EMessageSeverity::Warning);
		*/
	}
#endif

	ParseOverrideDistancesCVar();
	UpdateOverrideTransitionDistance();
}

void ALODActor::UpdateOverrideTransitionDistance()
{
	const int32 NumDistances = ALODActor::HLODDistances.Num();
	// Determine correct distance index to apply to ensure combinations of different levels will work			
	const int32 DistanceIndex = [&]()
	{
		if (CachedNumHLODLevels == NumDistances)
		{
			return LODLevel - 1;
		}
		else if (CachedNumHLODLevels < NumDistances)
		{
			return (LODLevel + (NumDistances - CachedNumHLODLevels)) - 1;
		}
		else
		{
			// We've reached the end of the array, change nothing
			return (int32)INDEX_NONE;
		}
	}();

	if (DistanceIndex != INDEX_NONE)
	{
		StaticMeshComponent->MinDrawDistance = (!HLODDistances.IsValidIndex(DistanceIndex) || FMath::IsNearlyZero(HLODDistances[DistanceIndex])) ? LODDrawDistance : ALODActor::HLODDistances[DistanceIndex];
		StaticMeshComponent->MarkRenderStateDirty();
	}
}

void ALODActor::ParseOverrideDistancesCVar()
{
	// Parse HLOD override distance cvar into array
	const FString DistanceOverrideValues = CVarHLODDistanceOverride.GetValueOnAnyThread();
	TArray<FString> Distances;
	DistanceOverrideValues.ParseIntoArray(/*out*/ Distances, TEXT(","), /*bCullEmpty=*/ false);
	HLODDistances.Empty(Distances.Num());

	for (const FString& DistanceString : Distances)
	{
		const float DistanceForThisLevel = FCString::Atof(*DistanceString);
		HLODDistances.Add(DistanceForThisLevel);
	}	
}

void ALODActor::Tick(float DeltaSeconds)
{
	AActor::Tick(DeltaSeconds);
	if (bNeedsDrawDistanceReset)
	{		
		if (ResetDrawDistanceTime > CVarHLODDitherPauseTime.GetValueOnAnyThread())
		{
			const int32 NumDistances = ALODActor::HLODDistances.Num();
			const int32 DistanceIndex = [&]()
	        {
		        if (CachedNumHLODLevels <= NumDistances)
		        {
			    	return (LODLevel + (NumDistances - CachedNumHLODLevels)) - 1;
		        }
		        else
		        {
			        // We've reached the end of the array, change nothing
			        return (int32)INDEX_NONE;
		        }
	        }();


			const float HLODDistanceOverride = (!ALODActor::HLODDistances.IsValidIndex(DistanceIndex)) ? 0.0f : ALODActor::HLODDistances[DistanceIndex];
			// Determine desired HLOD state
			float MinDrawDistance = LODDrawDistance;
			const bool bIsOverridingHLODDistance = HLODDistanceOverride != 0.0f;
			if (bIsOverridingHLODDistance)
			{
				MinDrawDistance = HLODDistanceOverride;
			}
			const float AdjustedMinDrawDist = MinDrawDistance * CVarHLODDistanceScale.GetValueOnAnyThread();

			StaticMeshComponent->MinDrawDistance = AdjustedMinDrawDist;
			StaticMeshComponent->MarkRenderStateDirty();
			bNeedsDrawDistanceReset = false;
			ResetDrawDistanceTime = 0.0f;
			PrimaryActorTick.SetTickFunctionEnable(false);
		}
		else
        {
			ResetDrawDistanceTime += DeltaSeconds;
        }
	}
}

void ALODActor::PauseDitherTransition()
{
	StaticMeshComponent->MinDrawDistance = 0.0f;
	StaticMeshComponent->MarkRenderStateDirty();
	bNeedsDrawDistanceReset = true;
	ResetDrawDistanceTime = 0.0f;
}

void ALODActor::StartDitherTransition()
{
	PrimaryActorTick.SetTickFunctionEnable(true);
}

void ALODActor::UpdateRegistrationToMatchMaximumLODLevel()
{
	// Determine if we can show this HLOD level and allow or prevent the SMC from being registered
	// This doesn't save the memory of the static mesh or lowest mip levels, but it prevents the proxy from being created
	// or high mip textures from being streamed in
	const int32 MaximumAllowedHLODLevel = GMaximumAllowedHLODLevel;
	const bool bAllowShowingThisLevel = (MaximumAllowedHLODLevel < 0) || (LODLevel <= MaximumAllowedHLODLevel);

	check(StaticMeshComponent);
	if (StaticMeshComponent->bAutoRegister != bAllowShowingThisLevel)
	{
		StaticMeshComponent->bAutoRegister = bAllowShowingThisLevel;

		if (!bAllowShowingThisLevel && StaticMeshComponent->IsRegistered())
		{
			ensure(bHasActorTriedToRegisterComponents);
			StaticMeshComponent->UnregisterComponent();
		}
		else if (bAllowShowingThisLevel && !StaticMeshComponent->IsRegistered())
		{
			// We should only register components if the actor had already tried to register before (otherwise it'll be taken care of in the normal flow)
			if (bHasActorTriedToRegisterComponents)
			{
				StaticMeshComponent->RegisterComponent();
			}
		}
	}
}

void ALODActor::PostRegisterAllComponents() 
{
	Super::PostRegisterAllComponents();

	bHasActorTriedToRegisterComponents = true;

#if WITH_EDITOR
	if(!GetWorld()->IsPlayInEditor())
	{
		// Clean up sub actor if assets were delete manually
		CleanSubActorArray();

		// Clean up sub objects if assets were delete manually
		CleanSubObjectsArray();

		UpdateSubActorLODParents();
	}
#endif
}

void ALODActor::SetDrawDistance(float InDistance)
{
	LODDrawDistance = InDistance;
	StaticMeshComponent->MinDrawDistance = LODDrawDistance;
}

#if WITH_EDITOR

void ALODActor::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	if (PropertyThatWillChange)
	{
		const FName PropertyName = PropertyThatWillChange->GetFName();

		// If the Sub Objects array is changed, in case of asset deletion make sure me flag as dirty since the cluster will be invalid
		if (PropertyName == TEXT("SubObjects"))
		{
			SetIsDirty(true);
		}
	}

	// Flush all pending rendering commands.
	FlushRenderingCommands();
}

void ALODActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	FName PropertyName = PropertyThatChanged != NULL ? PropertyThatChanged->GetFName() : NAME_None;
	
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, bOverrideTransitionScreenSize) || PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, TransitionScreenSize))
	{
		float CalculateSreenSize = 0.0f;

		if (bOverrideTransitionScreenSize)
		{
			CalculateSreenSize = TransitionScreenSize;
		}
		else
		{
			UWorld* World = GetWorld();
			check(World != nullptr);
			const TArray<struct FHierarchicalSimplification>& HierarchicalLODSetups = World->GetWorldSettings()->GetHierarchicalLODSetup();
			checkf(HierarchicalLODSetups.IsValidIndex(LODLevel - 1), TEXT("Out of range HLOD level (%i) found in LODActor (%s)"), LODLevel - 1, *GetName());
			CalculateSreenSize = HierarchicalLODSetups[LODLevel - 1].TransitionScreenSize;
		}

		RecalculateDrawingDistance(CalculateSreenSize);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, bOverrideScreenSize) || PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, ScreenSize)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, bOverrideMaterialMergeSettings) || PropertyName == GET_MEMBER_NAME_CHECKED(ALODActor, MaterialSettings))
	{
		// If we change override settings dirty the actor
		SetIsDirty(true);
	}

	UpdateRegistrationToMatchMaximumLODLevel();

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

bool ALODActor::GetReferencedContentObjects( TArray<UObject*>& Objects ) const
{
	Super::GetReferencedContentObjects(Objects);
	Objects.Append(SubObjects);
	
	// Retrieve referenced objects for sub actors as well
	for (AActor* SubActor : SubActors)
	{
		if (SubActor)
		{
			SubActor->GetReferencedContentObjects(Objects);
		}
	}
	return true;
}

void ALODActor::CheckForErrors()
{
	FMessageLog MapCheck("MapCheck");

	// Only check when this is not a preview actor and actually has a static mesh	
	Super::CheckForErrors();
	if (!StaticMeshComponent)
	{
		MapCheck.Warning()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(LOCTEXT("MapCheck_Message_StaticMeshComponent", "Static mesh actor has NULL StaticMeshComponent property - please delete")))
			->AddToken(FMapErrorToken::Create(FMapErrors::StaticMeshComponent));
	}

	if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh() == nullptr)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidLODActorMissingMesh", "{ActorName} : Static mesh is missing for the built LODActor.  Did you remove the asset? Please delete it and build LOD again. "), Arguments)))
			->AddToken(FMapErrorToken::Create(FMapErrors::LODActorMissingStaticMesh));
	}
	
	if (SubActors.Num() == 0)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
		FMessageLog("MapCheck").Error()
			->AddToken(FUObjectToken::Create(this))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidLODActorEmptyActor", "{ActorName} : NoActor is assigned. We recommend you to delete this actor. "), Arguments)))
			->AddToken(FMapErrorToken::Create(FMapErrors::LODActorNoActorFound));
	}
	else
	{
		for (AActor* Actor : SubActors)
		{
			// see if it's null, if so it is not good
			if(Actor == nullptr)
			{
				FFormatNamedArguments Arguments;
				Arguments.Add(TEXT("ActorName"), FText::FromString(GetName()));
				FMessageLog("MapCheck").Error()
					->AddToken(FUObjectToken::Create(this))
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("MapCheck_Message_InvalidLODActorNullActor", "{ActorName} : Actor is missing. The actor might have been removed. We recommend you to build LOD again. "), Arguments)))
					->AddToken(FMapErrorToken::Create(FMapErrors::LODActorMissingActor));
			}
		}
	}
}

void ALODActor::EditorApplyTranslation(const FVector& DeltaTranslation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
}

void ALODActor::EditorApplyRotation(const FRotator& DeltaRotation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
}

void ALODActor::EditorApplyScale(const FVector& DeltaScale, const FVector* PivotLocation, bool bAltDown, bool bShiftDown, bool bCtrlDown)
{
}

void ALODActor::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
}

void ALODActor::AddSubActor(AActor* InActor)
{
	SubActors.Add(InActor);
	InActor->SetLODParent(StaticMeshComponent, LODDrawDistance);
	SetIsDirty(true);

	// Adding number of triangles
	if (!InActor->IsA<ALODActor>())
	{
		TArray<UStaticMeshComponent*> StaticMeshComponents;
		InActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
		for (UStaticMeshComponent* Component : StaticMeshComponents)
		{
			const UStaticMesh* StaticMesh = (Component) ? Component->GetStaticMesh() : nullptr;
			if (StaticMesh && StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
			{
				NumTrianglesInSubActors += StaticMesh->RenderData->LODResources[0].GetNumTriangles();
			}
			Component->MarkRenderStateDirty();
		}
	}
	else
	{
		ALODActor* LODActor = Cast<ALODActor>(InActor);
		NumTrianglesInSubActors += LODActor->GetNumTrianglesInSubActors();
		
	}
	
	// Reset the shadowing flags and determine them according to our current sub actors
	DetermineShadowingFlags();
}

const bool ALODActor::RemoveSubActor(AActor* InActor)
{
	if ((InActor != nullptr) && SubActors.Contains(InActor))
	{
		SubActors.Remove(InActor);
		InActor->SetLODParent(nullptr, 0);
		SetIsDirty(true);

		// Deducting number of triangles
		if (!InActor->IsA<ALODActor>())
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			InActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for (UStaticMeshComponent* Component : StaticMeshComponents)
			{
				const UStaticMesh* StaticMesh = (Component) ? Component->GetStaticMesh() : nullptr;
				if (StaticMesh && StaticMesh->RenderData && StaticMesh->RenderData->LODResources.Num() > 0)
				{
					NumTrianglesInSubActors -= StaticMesh->RenderData->LODResources[0].GetNumTriangles();
				}

				Component->MarkRenderStateDirty();
			}
		}
		else
		{
			ALODActor* LODActor = Cast<ALODActor>(InActor);
			NumTrianglesInSubActors -= LODActor->GetNumTrianglesInSubActors();
		}

		if (StaticMeshComponent)
		{
			StaticMeshComponent->MarkRenderStateDirty();
		}	
				
		// In case the user removes an actor while the HLOD system is force viewing one LOD level
		InActor->SetIsTemporarilyHiddenInEditor(false);

		// Reset the shadowing flags and determine them according to our current sub actors
		DetermineShadowingFlags();
				
		return true;
	}

	return false;
}

void ALODActor::DetermineShadowingFlags()
{
	// Cast shadows if any sub-actors do
	bool bCastsShadow = false;
	bool bCastsStaticShadow = false;
	bool bCastsDynamicShadow = false;
	bool bCastFarShadow = false;
	for (AActor* Actor : SubActors)
	{
		if (Actor)
		{
			TArray<UStaticMeshComponent*> StaticMeshComponents;
			Actor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
			for (UStaticMeshComponent* Component : StaticMeshComponents)
			{
				bCastsShadow |= Component->CastShadow;
				bCastsStaticShadow |= Component->bCastStaticShadow;
				bCastsDynamicShadow |= Component->bCastDynamicShadow;
				bCastFarShadow |= Component->bCastFarShadow;
			}
		}
	}

	StaticMeshComponent->CastShadow = bCastsShadow;
	StaticMeshComponent->bCastStaticShadow = bCastsStaticShadow;
	StaticMeshComponent->bCastDynamicShadow = bCastsDynamicShadow;
	StaticMeshComponent->bCastFarShadow = bCastFarShadow;
	StaticMeshComponent->MarkRenderStateDirty();
}

void ALODActor::SetIsDirty(const bool bNewState)
{
	bDirty = bNewState;

	// Set parent LODActor dirty as well if bNewState = true
	if (IsDirty())
	{
		// If this LODActor is a SubActor at a higher LOD level mark parent dirty as well
		UPrimitiveComponent* LODParentComponent = StaticMeshComponent->GetLODParentPrimitive();
		if (LODParentComponent)
		{
			ALODActor* LODParentActor = Cast<ALODActor>(LODParentComponent->GetOwner());
			if (LODParentActor)
			{
				LODParentActor->Modify();
				LODParentActor->SetIsDirty(true);
			}
		}
		if (GetDefault<UHierarchicalLODSettings>()->bInvalidateHLODClusters)
		{
			// Set static mesh to null
			StaticMeshComponent->SetStaticMesh(nullptr);
			// Broadcast actor marked dirty event
			if (GEditor)
			{
				GEditor->BroadcastHLODActorMarkedDirty(this);
			}
			PreviousSubObjects.Append(SubObjects);
			SubObjects.Empty();
		}
	}	
	else
	{
		UpdateSubActorLODParents();
		// Deal with the case where the UObjects are being reused
		PreviousSubObjects.RemoveAll([this](UObject* InObject) -> bool { return SubObjects.Contains(InObject); });
	}
}

const bool ALODActor::HasValidSubActors() const
{
	int32 NumMeshes = 0;

	// Make sure there is at least one mesh in the subactors
	for (AActor* SubActor : SubActors)
	{
		if (SubActor)
		{
			TInlineComponentArray<UStaticMeshComponent*> Components;
			SubActor->GetComponents(/*out*/ Components);

#if WITH_EDITOR
			FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
			IHierarchicalLODUtilities* Utilities = Module.GetUtilities();

			for (UStaticMeshComponent* Component : Components)
			{
				if (!Component->bHiddenInGame && Component->ShouldGenerateAutoLOD(LODLevel - 1))
				{
					++NumMeshes;
				}
			}
#else
			NumMeshes += Components.Num();
#endif

			if (NumMeshes > 0)
			{
				break;
			}
		}
	}

	return NumMeshes > 0;
}

const bool ALODActor::HasAnySubActors() const
{
	return (SubActors.Num() != 0);
}

void ALODActor::ToggleForceView()
{
	// Toggle the forced viewing of this LODActor, set drawing distance to 0.0f or LODDrawDistance
	StaticMeshComponent->MinDrawDistance = (StaticMeshComponent->MinDrawDistance == 0.0f) ? LODDrawDistance : 0.0f;
	StaticMeshComponent->MarkRenderStateDirty();
}

void ALODActor::SetForcedView(const bool InState)
{
	// Set forced viewing state of this LODActor, set drawing distance to 0.0f or LODDrawDistance
	StaticMeshComponent->MinDrawDistance = (InState) ? 0.0f : LODDrawDistance;
	StaticMeshComponent->MarkRenderStateDirty();
}

void ALODActor::SetHiddenFromEditorView(const bool InState, const int32 ForceLODLevel )
{
	// If we are also subactor for a higher LOD level or this actor belongs to a higher HLOD level than is being forced hide the actor
	if (GetStaticMeshComponent()->GetLODParentPrimitive() || LODLevel > ForceLODLevel )
	{
		SetIsTemporarilyHiddenInEditor(InState);			

		for (AActor* Actor : SubActors)
		{
			if (Actor)
			{
				// If this actor belongs to a lower HLOD level that is being forced hide the sub-actors
				if (LODLevel < ForceLODLevel)
				{
					Actor->SetIsTemporarilyHiddenInEditor(InState);
				}

				// Toggle/set the LOD parent to nullptr or this
				Actor->SetLODParent((InState) ? nullptr : StaticMeshComponent, (InState) ? 0.0f : LODDrawDistance);
			}
		}
	}

	StaticMeshComponent->MarkRenderStateDirty();
}

const uint32 ALODActor::GetNumTrianglesInSubActors()
{
	return NumTrianglesInSubActors;
}

const uint32 ALODActor::GetNumTrianglesInMergedMesh()
{
	return NumTrianglesInMergedMesh;
}

void ALODActor::SetStaticMesh(class UStaticMesh* InStaticMesh)
{
	if (StaticMeshComponent)
	{
		StaticMeshComponent->SetStaticMesh(InStaticMesh);
		SetIsDirty(false);

		ensure(StaticMeshComponent->GetStaticMesh() == InStaticMesh);
		if (InStaticMesh && InStaticMesh->RenderData && InStaticMesh->RenderData->LODResources.Num() > 0)
		{
			NumTrianglesInMergedMesh = InStaticMesh->RenderData->LODResources[0].GetNumTriangles();
		}
	}
}

void ALODActor::UpdateSubActorLODParents()
{
	for (AActor* Actor : SubActors)
	{	
		if (Actor)
		{
			Actor->SetLODParent(StaticMeshComponent, StaticMeshComponent->MinDrawDistance);
		}
	}
}

void ALODActor::CleanSubActorArray()
{
	bool bIsDirty = false;
	for (int32 SubActorIndex = 0; SubActorIndex < SubActors.Num(); ++SubActorIndex)
	{
		AActor* Actor = SubActors[SubActorIndex];
		if (Actor == nullptr)
		{
			SubActors.RemoveAtSwap(SubActorIndex);
			SubActorIndex--;
			bIsDirty = true;
		}
	}

	if (bIsDirty)
	{
		SetIsDirty(true);
	}
}

void ALODActor::CleanSubObjectsArray()
{
	bool bIsDirty = false;
	for (int32 SubObjectIndex = 0; SubObjectIndex < SubObjects.Num(); ++SubObjectIndex)
	{
		UObject* Object = SubObjects[SubObjectIndex];
		if (Object == nullptr)
		{
			SubObjects.RemoveAtSwap(SubObjectIndex);
			SubObjectIndex--;
			bIsDirty = true;
		}
	}

	if (bIsDirty)
	{
		SetIsDirty(true);		
	}
}

void ALODActor::RecalculateDrawingDistance(const float InTransitionScreenSize)
{
	// At the moment this assumes a fixed field of view of 90 degrees (horizontal and vertical axes)
	static const float FOVRad = 90.0f * (float)PI / 360.0f;
	static const FMatrix ProjectionMatrix = FPerspectiveMatrix(FOVRad, 1920, 1080, 0.01f);
	FBoxSphereBounds Bounds = GetStaticMeshComponent()->CalcBounds(FTransform());
	LODDrawDistance = ComputeBoundsDrawDistance(InTransitionScreenSize, Bounds.SphereRadius, ProjectionMatrix);

	StaticMeshComponent->MinDrawDistance = LODDrawDistance;	

	UpdateSubActorLODParents();
}


#endif // WITH_EDITOR

FBox ALODActor::GetComponentsBoundingBox(bool bNonColliding) const 
{
	FBox BoundBox = Super::GetComponentsBoundingBox(bNonColliding);

	// If BoundBox ends up to nothing create a new invalid one
	if (BoundBox.GetVolume() == 0.0f)
	{
		BoundBox = FBox(ForceInit);
	}

	if (bNonColliding)
	{
		if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
		{
			FBoxSphereBounds StaticBound = StaticMeshComponent->GetStaticMesh()->GetBounds();
			FBox StaticBoundBox(BoundBox.GetCenter()-StaticBound.BoxExtent, BoundBox.GetCenter()+StaticBound.BoxExtent);
			BoundBox += StaticBoundBox;
		}
		else
		{
			for (AActor* Actor : SubActors)
			{
				if (Actor)
				{
					BoundBox += Actor->GetComponentsBoundingBox(bNonColliding);
				}				
			}
		}
	}

	return BoundBox;	
}

void ALODActor::OnCVarsChanged()
{
	// Initialized to MIN_int32 to make sure that we run this once at startup regardless of the CVar value (assuming it is valid)
	static int32 CachedMaximumAllowedHLODLevel = MIN_int32;
	const int32 MaximumAllowedHLODLevel = GMaximumAllowedHLODLevel;

	if (MaximumAllowedHLODLevel != CachedMaximumAllowedHLODLevel)
	{
		CachedMaximumAllowedHLODLevel = MaximumAllowedHLODLevel;

		for (ALODActor* Actor : TObjectRange<ALODActor>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::PendingKill))
		{
			Actor->UpdateRegistrationToMatchMaximumLODLevel();
		}
	}
	
	static TArray<float> CachedDistances = HLODDistances;
	ParseOverrideDistancesCVar();

	const bool bInvalidatedCachedValues = [&]() -> bool
	{
		for (int32 Index = 0; Index < CachedDistances.Num(); ++Index)
		{
			const float CachedDistance = CachedDistances[Index];
			if (HLODDistances.IsValidIndex(Index))
			{
				const float NewDistance = HLODDistances[Index];
				if (NewDistance != CachedDistance)
				{
					return true;
				}
			}
			else
			{
				return true;
			}
		}

		return CachedDistances.Num() != HLODDistances.Num();
	}();

	if (bInvalidatedCachedValues)
	{
		CachedDistances = HLODDistances;
		const int32 NumDistances = CachedDistances.Num();
		for (ALODActor* Actor : TObjectRange<ALODActor>(RF_ClassDefaultObject | RF_ArchetypeObject, true, EInternalObjectFlags::PendingKill))
		{
			Actor->UpdateOverrideTransitionDistance();
		}
	}
}

#if WITH_EDITOR
void ALODActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFrameworkObjectVersion::GUID);
	Ar.UsingCustomVersion(FAthenaObjectVersion::GUID);

	bRequiresLODScreenSizeConversion = Ar.CustomVer(FFrameworkObjectVersion::GUID) < FFrameworkObjectVersion::LODsUseResolutionIndependentScreenSize;

	if (Ar.CustomVer(FAthenaObjectVersion::GUID) < FAthenaObjectVersion::CullDistanceRefactor_NeverCullALODActorsByDefault)
	{
		if (UStaticMeshComponent* SMComponent = GetStaticMeshComponent())
		{	
			SMComponent->LDMaxDrawDistance = 0.f;
			SMComponent->bNeverDistanceCull = true;
		}
	}
}

void ALODActor::PreSave(const class ITargetPlatform* TargetPlatform)
{
	AActor::PreSave(TargetPlatform);	
	if (PreviousSubObjects.Num() && GetDefault<UHierarchicalLODSettings>()->bDeleteHLODAssets)
	{
		PreviousSubObjects.RemoveAll([](const UObject* Object) -> bool { return Object == nullptr; });
		ObjectTools::DeleteObjectsUnchecked(PreviousSubObjects);
		PreviousSubObjects.Empty();
	}
}

void ALODActor::BeginDestroy()
{
	AActor::BeginDestroy();
	if (PreviousSubObjects.Num())
	{
		for (UObject* Object : PreviousSubObjects)
		{
			if (Object)
			{
				Object->MarkPendingKill();
			}
		}
		PreviousSubObjects.Empty();
	}
}
#endif
#undef LOCTEXT_NAMESPACE
