// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OculusAssetManager.h"
#include "OculusHMDPrivate.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "OculusAssetDirectory.h"
#include "UObject/GCObject.h"

/* FOculusAssetDirectory
 *****************************************************************************/

FSoftObjectPath FOculusAssetDirectory::AssetListing[] =
{
	FString(TEXT("/OculusVR/Meshes/RiftHMD.RiftHMD")),
	FString(TEXT("/OculusVR/Meshes/GearVRController.GearVRController")),
	FString(TEXT("/OculusVR/Meshes/LeftTouchController.LeftTouchController")),
	FString(TEXT("/OculusVR/Meshes/RightTouchController.RightTouchController")),

	FString(TEXT("/OculusVR/Materials/PokeAHoleMaterial.PokeAHoleMaterial"))
};

#if WITH_EDITORONLY_DATA
class FOculusAssetRepo : public FGCObject, public TArray<UObject*>
{
public:
	// made an on-demand singleton rather than a static global, to avoid issues with FGCObject initialization
	static FOculusAssetRepo& Get()
	{
		static FOculusAssetRepo AssetRepository;
		return AssetRepository;
	}

	UObject* LoadAndAdd(const FSoftObjectPath& AssetPath)
	{
		UObject* AssetObj = AssetPath.TryLoad();
		if (AssetObj != nullptr)
		{
			AddUnique(AssetObj);
		}
		return AssetObj;
	}

public:
	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObjects(*this);
	}
};

void FOculusAssetDirectory::LoadForCook()
{
	FOculusAssetRepo& AssetRepro = FOculusAssetRepo::Get();
	for (int32 AssetIndex = 0; AssetIndex < ARRAY_COUNT(FOculusAssetDirectory::AssetListing); ++AssetIndex)
	{
		AssetRepro.LoadAndAdd(FOculusAssetDirectory::AssetListing[AssetIndex]);
	}
}

void FOculusAssetDirectory::ReleaseAll()
{
	FOculusAssetRepo::Get().Empty();
}
#endif // WITH_EDITORONLY_DATA


/* OculusAssetManager_Impl
 *****************************************************************************/

namespace OculusAssetManager_Impl
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	struct FRenderableDevice
	{
		ovrpNode OVRNode;
		FSoftObjectPath MeshAssetRef;
	};

	static FRenderableDevice RenderableDevices[] =
	{
		{ ovrpNode_Head,      FOculusAssetDirectory::AssetListing[0] },
#if PLATFORM_ANDROID
		{ ovrpNode_HandLeft,  FOculusAssetDirectory::AssetListing[1] },
		{ ovrpNode_HandRight, FOculusAssetDirectory::AssetListing[1] },
#else 
		{ ovrpNode_HandLeft,  FOculusAssetDirectory::AssetListing[2] },
		{ ovrpNode_HandRight, FOculusAssetDirectory::AssetListing[3] },
#endif
	};

	static uint32 RenderableDeviceCount = sizeof(RenderableDevices) / sizeof(RenderableDevices[0]);
#endif // #if OCULUS_HMD_SUPPORTED_PLATFORMS

	static UObject* FindDeviceMesh(const int32 DeviceID);
};


static UObject* OculusAssetManager_Impl::FindDeviceMesh(const int32 DeviceID)
{
	UObject* DeviceMesh = nullptr;
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	const ovrpNode DeviceOVRNode = OculusHMD::ToOvrpNode(DeviceID);

	if (DeviceOVRNode != ovrpNode_None)
	{
		for (uint32 DeviceIndex = 0; DeviceIndex < RenderableDeviceCount; ++DeviceIndex)
		{
			const FRenderableDevice& RenderableDevice = RenderableDevices[DeviceIndex];
			if (RenderableDevice.OVRNode == DeviceOVRNode)
			{
				DeviceMesh = RenderableDevice.MeshAssetRef.TryLoad();
				break;
			}
		}
	}
#endif
	return DeviceMesh;
}

/* FOculusAssetManager
*****************************************************************************/

FOculusAssetManager::FOculusAssetManager()
{
	IModularFeatures::Get().RegisterModularFeature(IXRSystemAssets::GetModularFeatureName(), this);
}

FOculusAssetManager::~FOculusAssetManager()
{
	IModularFeatures::Get().UnregisterModularFeature(IXRSystemAssets::GetModularFeatureName(), this);
}

bool FOculusAssetManager::EnumerateRenderableDevices(TArray<int32>& DeviceListOut)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	using namespace OculusAssetManager_Impl;
	DeviceListOut.Empty(RenderableDeviceCount);

	for (uint32 DeviceIndex = 0; DeviceIndex < RenderableDeviceCount; ++DeviceIndex)
	{
		const FRenderableDevice& RenderableDevice = RenderableDevices[DeviceIndex];

		const int32 ExternalDeviceId = OculusHMD::ToExternalDeviceId(RenderableDevice.OVRNode);
		DeviceListOut.Add(ExternalDeviceId);
	}

	return true;
#else 
	return false;
#endif
}

int32 FOculusAssetManager::GetDeviceId(EControllerHand ControllerHand)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	ovrpNode ControllerOVRNode = ovrpNode_None;

	switch (ControllerHand)
	{
	case EControllerHand::AnyHand:
		// @TODO: maybe check if the right is tracking, if not choose left (if tracking)?
	case EControllerHand::Right:
		ControllerOVRNode = ovrpNode_HandRight;
		break;
	case EControllerHand::Left:
		ControllerOVRNode = ovrpNode_HandLeft;
		break;

	case EControllerHand::ExternalCamera:
		ControllerOVRNode = ovrpNode_TrackerZero;
		break;
// 	case EControllerHand::Special_1:
// 		ControllerOVRNode = ovrpNode_TrackerOne;
// 		break;
// 	case EControllerHand::Special_2:
// 		ControllerOVRNode = ovrpNode_TrackerTwo;
// 		break;
// 	case EControllerHand::Special_3:
// 		ControllerOVRNode = ovrpNode_TrackerThree;
// 		break;

// 	case EControllerHand::Special_4:
// 		ControllerOVRNode = ovrpNode_DeviceObjectZero;
// 		break;

	default:
		// ControllerOVRNode = ovrpNode_None => returns -1
		break;
	}
	return OculusHMD::ToExternalDeviceId(ControllerOVRNode);
#else 
	return INDEX_NONE;
#endif
}

UPrimitiveComponent* FOculusAssetManager::CreateRenderComponent(const int32 DeviceId, AActor* Owner, EObjectFlags Flags, const bool /*bForceSynchronous*/, const FXRComponentLoadComplete& OnLoadComplete)
{
	UPrimitiveComponent* NewRenderComponent = nullptr;
	if (UObject* DeviceMesh = OculusAssetManager_Impl::FindDeviceMesh(DeviceId))
	{
		if (UStaticMesh* AsStaticMesh = Cast<UStaticMesh>(DeviceMesh))
		{
			const FName ComponentName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), *FString::Printf(TEXT("%s_Device%d"), TEXT("Oculus"), DeviceId));
			UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(Owner, ComponentName, Flags);

			MeshComponent->SetStaticMesh(AsStaticMesh);
			NewRenderComponent = MeshComponent;
		}
		else if (USkeletalMesh* AsSkeletalMesh = Cast<USkeletalMesh>(DeviceMesh))
		{
			const FName ComponentName = MakeUniqueObjectName(Owner, USkeletalMeshComponent::StaticClass(), *FString::Printf(TEXT("%s_Device%d"), TEXT("Oculus"), DeviceId));
			USkeletalMeshComponent* SkelMeshComponent = NewObject<USkeletalMeshComponent>(Owner, ComponentName, Flags);

			SkelMeshComponent->SetSkeletalMesh(AsSkeletalMesh);
			NewRenderComponent = SkelMeshComponent;
		}
		NewRenderComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	OnLoadComplete.ExecuteIfBound(NewRenderComponent);
	return NewRenderComponent;
}

