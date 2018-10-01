// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterGameManager.h"

#include "Config/IPDisplayClusterConfigManager.h"

#include "DisplayClusterGameMode.h"
#include "DisplayClusterSettings.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Config/DisplayClusterConfigTypes.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "DisplayClusterCameraComponent.h"
#include "DisplayClusterSceneComponent.h"

#include "IPDisplayCluster.h"
#include "DisplayClusterGlobals.h"
#include "DisplayClusterStrings.h"


FDisplayClusterGameManager::FDisplayClusterGameManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
}

FDisplayClusterGameManager::~FDisplayClusterGameManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterGameManager::Init(EDisplayClusterOperationMode OperationMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	CurrentOperationMode = OperationMode;

	return true;
}

void FDisplayClusterGameManager::Release()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
}

bool FDisplayClusterGameManager::StartSession(const FString& configPath, const FString& nodeId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	ConfigPath = configPath;
	ClusterNodeId = nodeId;

	return true;
}

void FDisplayClusterGameManager::EndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
}

bool FDisplayClusterGameManager::StartScene(UWorld* pWorld)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	check(pWorld);
	CurrentWorld = pWorld;

	VRRootActor = nullptr;
	ActiveScreenComponent = nullptr;
	ActiveCameraComponent = nullptr;

	// Clean containers. We store only pointers so there is no need to do any additional
	// operations. All components will be destroyed by the engine.
	ScreenComponents.Reset();
	CameraComponents.Reset();
	SceneNodeComponents.Reset();

	if (IsDisplayClusterActive())
	{
		//@todo: move initialization to DisplayClusterRoot side
		if (!InitializeDisplayClusterActor())
		{
			UE_LOG(LogDisplayClusterGame, Error, TEXT("Couldn't initialize DisplayCluster hierarchy"));
			return false;
		}
	}

	return true;
}

void FDisplayClusterGameManager::EndScene()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);
	FScopeLock lock(&InternalsSyncScope);

	VRRootActor = nullptr;
	ActiveScreenComponent = nullptr;
	ActiveCameraComponent = nullptr;

	// Clean containers. We store only pointers so there is no need to do any additional
	// operations. All components will be destroyed by the engine.
	ScreenComponents.Reset();
	CameraComponents.Reset();
	SceneNodeComponents.Reset();
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterGameManager
//////////////////////////////////////////////////////////////////////////////////////////////
ADisplayClusterPawn* FDisplayClusterGameManager::GetRoot() const
{
	FScopeLock lock(&InternalsSyncScope);
	return VRRootActor;
}

TArray<UDisplayClusterScreenComponent*> FDisplayClusterGameManager::GetAllScreens() const
{
	FScopeLock lock(&InternalsSyncScope);
	return GetMapValues<UDisplayClusterScreenComponent>(ScreenComponents);
}

UDisplayClusterScreenComponent* FDisplayClusterGameManager::GetActiveScreen() const
{
	FScopeLock lock(&InternalsSyncScope);
	return ActiveScreenComponent;
}

UDisplayClusterScreenComponent* FDisplayClusterGameManager::GetScreenById(const FString& id) const
{
	FScopeLock lock(&InternalsSyncScope);
	return GetItem<UDisplayClusterScreenComponent>(ScreenComponents, id, FString("GetScreenById"));
}

int32 FDisplayClusterGameManager::GetScreensAmount() const
{
	FScopeLock lock(&InternalsSyncScope);
	return ScreenComponents.Num();
}

UDisplayClusterCameraComponent* FDisplayClusterGameManager::GetActiveCamera() const
{
	FScopeLock lock(&InternalsSyncScope);
	return ActiveCameraComponent;
}

UDisplayClusterCameraComponent* FDisplayClusterGameManager::GetCameraById(const FString& id) const
{
	FScopeLock lock(&InternalsSyncScope);
	return GetItem<UDisplayClusterCameraComponent>(CameraComponents, id, FString("GetCameraById"));
}

TArray<UDisplayClusterCameraComponent*> FDisplayClusterGameManager::GetAllCameras() const
{
	FScopeLock lock(&InternalsSyncScope);
	return GetMapValues<UDisplayClusterCameraComponent>(CameraComponents);
}

int32 FDisplayClusterGameManager::GetCamerasAmount() const
{
	FScopeLock lock(&InternalsSyncScope);
	return CameraComponents.Num();
}

void FDisplayClusterGameManager::SetActiveCamera(int32 idx)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!IsDisplayClusterActive())
	{
		return;
	}

	FDisplayClusterConfigCamera cam;
	if (!GDisplayCluster->GetPrivateConfigMgr()->GetCamera(idx, cam))
	{
		UE_LOG(LogDisplayClusterGame, Error, TEXT("Camera not found (idx=%d)"), idx);
		return;
	}

	return SetActiveCamera(cam.Id);
}

void FDisplayClusterGameManager::SetActiveCamera(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!IsDisplayClusterActive())
	{
		return;
	}

	FScopeLock lock(&InternalsSyncScope);

	if (!CameraComponents.Contains(id))
	{
		UE_LOG(LogDisplayClusterGame, Error, TEXT("Couldn't switch camera. No such node id: %s"), *id);
		return;
	}

	ActiveCameraComponent = CameraComponents[id];
	VRRootActor->GetCameraComponent()->AttachToComponent(ActiveCameraComponent, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
	VRRootActor->GetCameraComponent()->SetRelativeLocation(FVector::ZeroVector);
	VRRootActor->GetCameraComponent()->SetRelativeRotation(FRotator::ZeroRotator);

	// Update 'rotate around' component
	SetRotateAroundComponent(ActiveCameraComponent);

	UE_LOG(LogDisplayClusterGame, Log, TEXT("Camera %s activated"), *ActiveCameraComponent->GetId());
}

UDisplayClusterSceneComponent* FDisplayClusterGameManager::GetNodeById(const FString& id) const
{
	FScopeLock lock(&InternalsSyncScope);
	return GetItem<UDisplayClusterSceneComponent>(SceneNodeComponents, id, FString("GetNodeById"));
}

TArray<UDisplayClusterSceneComponent*> FDisplayClusterGameManager::GetAllNodes() const
{
	FScopeLock lock(&InternalsSyncScope);
	return GetMapValues<UDisplayClusterSceneComponent>(SceneNodeComponents);
}

USceneComponent* FDisplayClusterGameManager::GetTranslationDirectionComponent() const
{
	if (!IsDisplayClusterActive())
	{
		return nullptr;
	}

	if (VRRootActor == nullptr)
	{
		return nullptr;
	}

	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("GetTranslationDirectionComponent: %s"), (VRRootActor->TranslationDirection ? *VRRootActor->TranslationDirection->GetName() : TEXT("nullptr")));
	return VRRootActor->TranslationDirection;
}

void FDisplayClusterGameManager::SetTranslationDirectionComponent(USceneComponent* pComp)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!IsDisplayClusterActive())
	{
		return;
	}

	if (VRRootActor == nullptr)
	{
		return;
	}

	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterGame, Log, TEXT("New translation direction component set: %s"), (pComp ? *pComp->GetName() : TEXT("nullptr")));
	VRRootActor->TranslationDirection = pComp;
}

void FDisplayClusterGameManager::SetTranslationDirectionComponent(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!IsDisplayClusterActive())
	{
		return;
	}

	UE_LOG(LogDisplayClusterGame, Log, TEXT("New translation direction node id requested: %s"), *id);
	SetTranslationDirectionComponent(GetNodeById(id));
}

USceneComponent* FDisplayClusterGameManager::GetRotateAroundComponent() const
{
	if (!IsDisplayClusterActive())
	{
		return nullptr;
	}

	if (VRRootActor == nullptr)
	{
		return nullptr;
	}

	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("GetRotateAroundComponent: %s"), (VRRootActor->RotationAround ? *VRRootActor->RotationAround->GetName() : TEXT("nullptr")));
	return VRRootActor->RotationAround;
}

void FDisplayClusterGameManager::SetRotateAroundComponent(USceneComponent* pComp)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!IsDisplayClusterActive())
	{
		return;
	}

	if (VRRootActor == nullptr)
	{
		return;
	}

	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterGame, Log, TEXT("New rotate around component set: %s"), (pComp ? *pComp->GetName() : TEXT("nullptr")));
	VRRootActor->RotationAround = pComp;
}

void FDisplayClusterGameManager::SetRotateAroundComponent(const FString& id)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	if (!IsDisplayClusterActive())
	{
		return;
	}

	if (VRRootActor == nullptr)
	{
		return;
	}

	FScopeLock lock(&InternalsSyncScope);
	UE_LOG(LogDisplayClusterGame, Log, TEXT("New rotate around node id requested: %s"), *id);
	VRRootActor->RotationAround = GetNodeById(id);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterGameManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterGameManager::InitializeDisplayClusterActor()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterGame);

	APlayerController* pController = UGameplayStatics::GetPlayerController(CurrentWorld, 0);
	check(pController);
	
	VRRootActor = StaticCast<ADisplayClusterPawn*>(pController->GetPawn());
	if (!VRRootActor)
	{
		// Seems the DisplayCluster features has been disabled
		UE_LOG(LogDisplayClusterGame, Warning, TEXT("No DisplayCluster root found"));
		return false;
	}

	if (!(CreateCameras() && CreateScreens() && CreateNodes()))
	{
		UE_LOG(LogDisplayClusterGame, Error, TEXT("An error occurred during DisplayCluster root initialization"));
		return false;
	}

	// Let DisplayCluster nodes initialize ourselves
	for (auto it = SceneNodeComponents.CreateIterator(); it; ++it)
	{
		if (it->Value->ApplySettings() == false)
		{
			UE_LOG(LogDisplayClusterGame, Warning, TEXT("Coulnd't initialize DisplayCluster node: ID=%s"), *it->Key);
		}
	}

	// Set the first camera active by default
	SetActiveCamera(ActiveCameraComponent->GetId());

	// Check if default camera was specified in command line arguments
	FString camId;
	if (FParse::Value(FCommandLine::Get(), DisplayClusterStrings::args::Camera, camId))
	{
		DisplayClusterHelpers::str::DustCommandLineValue(camId);
		UE_LOG(LogDisplayClusterGame, Log, TEXT("Default camera from command line arguments: %s"), *camId);
		if (CameraComponents.Contains(camId))
		{
			SetActiveCamera(camId);
		}
	}

	return true;
}

bool FDisplayClusterGameManager::CreateScreens()
{
	// Get local screen settings
	FDisplayClusterConfigScreen localScreen;
	if (GDisplayCluster->GetPrivateConfigMgr()->GetLocalScreen(localScreen) == false)
	{
		UE_LOG(LogDisplayClusterGame, Error, TEXT("Couldn't get projection screen settings"));
		return false;
	}

	// Create screens
	const TArray<FDisplayClusterConfigScreen> screens = GDisplayCluster->GetPrivateConfigMgr()->GetScreens();
	for (const auto& screen : screens)
	{
		// Create screen
		UDisplayClusterScreenComponent* pScreen = NewObject<UDisplayClusterScreenComponent>(VRRootActor, FName(*screen.Id), RF_Transient);
		check(pScreen);

		pScreen->AttachToComponent(VRRootActor->GetCollisionOffsetComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		pScreen->RegisterComponent();

		// Pass settings
		pScreen->SetSettings(&screen);

		// Is this active screen (for this node)?
		if (screen.Id == localScreen.Id)
			ActiveScreenComponent = pScreen;

		// Store the screen
		ScreenComponents.Add(screen.Id, pScreen);
		SceneNodeComponents.Add(screen.Id, pScreen);
	}

	// Check if local screen was found
	if (!ActiveScreenComponent)
	{
		UE_LOG(LogDisplayClusterGame, Error, TEXT("Local screen not found"));
		return false;
	}

	return true;
}

bool FDisplayClusterGameManager::CreateNodes()
{
	// Create other nodes
	const TArray<FDisplayClusterConfigSceneNode> nodes = GDisplayCluster->GetPrivateConfigMgr()->GetSceneNodes();
	for (const auto& node : nodes)
	{
		UDisplayClusterSceneComponent* pNode = NewObject<UDisplayClusterSceneComponent>(VRRootActor, FName(*node.Id), RF_Transient);
		check(pNode);

		pNode->AttachToComponent(VRRootActor->GetCollisionOffsetComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		pNode->RegisterComponent();

		pNode->SetSettings(&node);
		SceneNodeComponents.Add(node.Id, pNode);
	}

	return true;
}

bool FDisplayClusterGameManager::CreateCameras()
{
	const TArray<FDisplayClusterConfigCamera> cams = GDisplayCluster->GetPrivateConfigMgr()->GetCameras();
	for (const auto& cam : cams)
	{
		UDisplayClusterCameraComponent* pCam = NewObject<UDisplayClusterCameraComponent>(VRRootActor, FName(*cam.Id), RF_Transient);
		check(pCam);

		pCam->AttachToComponent(VRRootActor->GetCollisionOffsetComponent(), FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		pCam->RegisterComponent();

		pCam->SetSettings(&cam);
		
		CameraComponents.Add(cam.Id, pCam);
		SceneNodeComponents.Add(cam.Id, pCam);

		if (ActiveCameraComponent == nullptr)
		{
			ActiveCameraComponent = pCam;
		}
	}

	// At least one camera must be set up
	if (!ActiveCameraComponent)
	{
		UE_LOG(LogDisplayClusterGame, Warning, TEXT("No camera found"));
		return false;
	}

	return CameraComponents.Num() > 0;
}

// Extracts array of values from a map
template <typename ObjType>
TArray<ObjType*> FDisplayClusterGameManager::GetMapValues(const TMap<FString, ObjType*>& container) const
{
	TArray<ObjType*> items;
	container.GenerateValueArray(items);
	return items;
}

// Gets item by id. Performs checks and logging.
template <typename DataType>
DataType* FDisplayClusterGameManager::GetItem(const TMap<FString, DataType*>& container, const FString& id, const FString& logHeader) const
{
	if (container.Contains(id))
	{
		return container[id];
	}

	UE_LOG(LogDisplayClusterGame, Warning, TEXT("%s: ID not found <%s>. Return nullptr."), *logHeader, *id);
	return nullptr;
}
