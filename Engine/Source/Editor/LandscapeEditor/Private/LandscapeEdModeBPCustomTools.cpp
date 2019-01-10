// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInterface.h"
#include "AI/NavigationSystemBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UnrealWidget.h"
#include "EditorModeManager.h"
#include "EditorViewportClient.h"
#include "LandscapeToolInterface.h"
#include "LandscapeProxy.h"
#include "LandscapeEdMode.h"
#include "Containers/ArrayView.h"
#include "LandscapeEditorObject.h"
#include "ScopedTransaction.h"
#include "LandscapeEdit.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeEdModeTools.h"
#include "LandscapeBPCustomBrush.h"
#include "LandscapeInfo.h"
#include "Landscape.h"
//#include "LandscapeDataAccess.h"

#define LOCTEXT_NAMESPACE "Landscape"

template<class ToolTarget>
class FLandscapeToolBPCustom : public FLandscapeTool
{
protected:
	FEdModeLandscape* EdMode;

public:
	FLandscapeToolBPCustom(FEdModeLandscape* InEdMode)
		: EdMode(InEdMode)
	{
	}

	virtual bool UsesTransformWidget() const { return true; }
	virtual bool OverrideWidgetLocation() const { return false; }
	virtual bool OverrideWidgetRotation() const { return false; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
	}

	virtual const TCHAR* GetToolName() override { return TEXT("BPCustom"); }
	virtual FText GetDisplayName() override { return FText(); };

	virtual void SetEditRenderType() override { GLandscapeEditRenderMode = ELandscapeEditRenderMode::None | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual bool SupportsMask() override { return false; }

	virtual ELandscapeToolTargetTypeMask::Type GetSupportedTargetTypes() override
	{
		return ELandscapeToolTargetTypeMask::FromType(ToolTarget::TargetType);
	}

	virtual void EnterTool() override
	{
	}

	virtual void ExitTool() override
	{
	}

	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) 
	{
	}

	virtual bool BeginTool(FEditorViewportClient* ViewportClient, const FLandscapeToolTarget& Target, const FVector& InHitLocation) override
	{
		if (EdMode->UISettings->BlueprintCustomBrush == nullptr)
		{
			return false;
		}

		ALandscapeBlueprintCustomBrush* DefaultObject = Cast<ALandscapeBlueprintCustomBrush>(EdMode->UISettings->BlueprintCustomBrush->GetDefaultObject(false));

		if (DefaultObject == nullptr)
		{
			return false;
		}

		// Only allow placing brushes that would affect our target type
		if ((DefaultObject->IsAffectingHeightmap() && Target.TargetType == ELandscapeToolTargetType::Heightmap) || (DefaultObject->IsAffectingWeightmap() && Target.TargetType == ELandscapeToolTargetType::Weightmap))
		{
			ULandscapeInfo* Info = EdMode->CurrentToolTarget.LandscapeInfo.Get();
			check(Info);

			FVector SpawnLocation = Info->GetLandscapeProxy()->LandscapeActorToWorld().TransformPosition(InHitLocation);

			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.bNoFail = true;
			SpawnInfo.OverrideLevel = Info->LandscapeActor.Get()->GetTypedOuter<ULevel>(); // always spawn in the same level as the one containing the ALandscape

			ALandscapeBlueprintCustomBrush* Brush = ViewportClient->GetWorld()->SpawnActor<ALandscapeBlueprintCustomBrush>(EdMode->UISettings->BlueprintCustomBrush, SpawnLocation, FRotator(0.0f), SpawnInfo);
			EdMode->UISettings->BlueprintCustomBrush = nullptr;

			GEditor->SelectNone(true, true);
			GEditor->SelectActor(Brush, true, true);

			EdMode->RefreshDetailPanel();
		}		
		
		return true;
	}

	virtual void EndTool(FEditorViewportClient* ViewportClient) override
	{
	}

	virtual bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y) override
	{
		return false;
	}

	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override
	{
		if (InKey == EKeys::Enter && InEvent == IE_Pressed)
		{
		}

		return false;
	}

	virtual bool InputDelta(FEditorViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale) override
	{
		return false;
	}

	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override
	{
		// The editor can try to render the tool before the UpdateLandscapeEditorData command runs and the landscape editor realizes that the landscape has been hidden/deleted
		const ULandscapeInfo* const LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo.Get();
		const ALandscapeProxy* const LandscapeProxy = LandscapeInfo->GetLandscapeProxy();
		if (LandscapeProxy)
		{
			const FTransform LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();

			int32 MinX, MinY, MaxX, MaxY;
			if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
			{
				// TODO if required
			}
		}
	}

	
protected:
/*	float GetLocalZAtPoint(const ULandscapeInfo* LandscapeInfo, int32 x, int32 y) const
	{
		// try to find Z location
		TSet<ULandscapeComponent*> Components;
		LandscapeInfo->GetComponentsInRegion(x, y, x, y, Components);
		for (ULandscapeComponent* Component : Components)
		{
			FLandscapeComponentDataInterface DataInterface(Component);
			return LandscapeDataAccess::GetLocalHeight(DataInterface.GetHeight(x - Component->SectionBaseX, y - Component->SectionBaseY));
		}
		return 0.0f;
	}
*/

public:
};
/*
void FEdModeLandscape::ApplyMirrorTool()
{
	if (CurrentTool->GetToolName() == FName("Mirror"))
	{
		FLandscapeToolMirror* MirrorTool = (FLandscapeToolMirror*)CurrentTool;
		MirrorTool->ApplyMirror();
		GEditor->RedrawLevelEditingViewports();
	}
}

void FEdModeLandscape::CenterMirrorTool()
{
	if (CurrentTool->GetToolName() == FName("Mirror"))
	{
		FLandscapeToolMirror* MirrorTool = (FLandscapeToolMirror*)CurrentTool;
		MirrorTool->CenterMirrorPoint();
		GEditor->RedrawLevelEditingViewports();
	}
}
*/



//
// Toolset initialization
//
void FEdModeLandscape::InitializeTool_BPCustom()
{
	auto Sculpt_Tool_BPCustom = MakeUnique<FLandscapeToolBPCustom<FHeightmapToolTarget>>(this);
	Sculpt_Tool_BPCustom->ValidBrushes.Add("BrushSet_Dummy");
	LandscapeTools.Add(MoveTemp(Sculpt_Tool_BPCustom));

	auto Paint_Tool_BPCustom = MakeUnique<FLandscapeToolBPCustom<FWeightmapToolTarget>>(this);
	Paint_Tool_BPCustom->ValidBrushes.Add("BrushSet_Dummy");
	LandscapeTools.Add(MoveTemp(Paint_Tool_BPCustom));
}

#undef LOCTEXT_NAMESPACE
