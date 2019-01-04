// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MeshProxySettingsCustomizations.h"
#include "Modules/ModuleManager.h"
#include "GameFramework/WorldSettings.h"
#include "IDetailChildrenBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "MeshUtilities.h"
#include "IMeshReductionManagerModule.h"

#define LOCTEXT_NAMESPACE "MeshProxySettingsCustomizations"

void FMeshProxySettingsCustomizations::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}


TSharedRef<IPropertyTypeCustomization> FMeshProxySettingsCustomizations::MakeInstance()
{
	return MakeShareable(new FMeshProxySettingsCustomizations);
}


bool FMeshProxySettingsCustomizations::UseNativeProxyLODTool() const
{
	IMeshMerging* MergeModule = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface").GetMeshMergingInterface();
	return MergeModule && MergeModule->GetName().Equals("ProxyLODMeshMerging");
}

void FMeshProxySettingsCustomizations::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// Retrieve structure's child properties
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);
	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	// Determine if we are using our native module  If so, we will supress some of the options used by the current thirdparty tool (simplygon).

	IMeshReductionManagerModule& ModuleManager = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
	IMeshMerging* MergeModule = ModuleManager.GetMeshMergingInterface();

	IDetailGroup& MeshSettingsGroup = ChildBuilder.AddGroup(NAME_None, FText::FromString("Proxy Settings"));



	TSharedPtr< IPropertyHandle > HardAngleThresholdPropertyHandle        = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, HardAngleThreshold));
	TSharedPtr< IPropertyHandle > NormalCalcMethodPropertyHandle          = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, NormalCalculationMethod));
	TSharedPtr< IPropertyHandle > MaxRayCastDistdPropertyHandle           = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, MaxRayCastDist));
	TSharedPtr< IPropertyHandle > RecalculateNormalsPropertyHandle        = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, bRecalculateNormals));
	TSharedPtr< IPropertyHandle > UseLandscapeCullingPropertyHandle       = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, bUseLandscapeCulling));
	TSharedPtr< IPropertyHandle > LandscapeCullingPrecisionPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, LandscapeCullingPrecision));
	TSharedPtr< IPropertyHandle > MergeDistanceHandle                     = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, MergeDistance));
	TSharedPtr< IPropertyHandle > UnresolvedGeometryColorHandle           = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, UnresolvedGeometryColor));
	TSharedPtr< IPropertyHandle > VoxelSizeHandle                         = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FMeshProxySettings, VoxelSize));
	


	for (auto Iter(PropertyHandles.CreateConstIterator()); Iter; ++Iter)
	{
		// Handle special property cases (done inside the loop to maintain order according to the struct
		if (Iter.Value() == HardAngleThresholdPropertyHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.ToolTip(FText::FromString(FString("Angle at which a hard edge is introduced between faces.  Note: Increases vertex count and may introduce additional UV seams.  It is only recommended if not using normals maps")));
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsHardAngleThresholdVisible));
		}
		else if (Iter.Value() == NormalCalcMethodPropertyHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsNormalCalcMethodVisible));
		}
		else if (Iter.Value() == MaxRayCastDistdPropertyHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsSearchDistanceVisible));
		}
		else if (Iter.Value() == RecalculateNormalsPropertyHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsRecalculateNormalsVisible));
		}
		else if (Iter.Value() == UseLandscapeCullingPropertyHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.DisplayName(FText::FromString(FString("Enable Volume Culling")));
			MeshProxySettingsRow.ToolTip(FText::FromString(FString("Allow culling volumes to exclude geometry.")));
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsUseLandscapeCullingVisible));
		}
		else if (Iter.Value() == LandscapeCullingPrecisionPropertyHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsUseLandscapeCullingPrecisionVisible));
		}
		else if (Iter.Value() == MergeDistanceHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsMergeDistanceVisible));
		}
		else if (Iter.Value() == UnresolvedGeometryColorHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsUnresolvedGeometryColorVisible));
		}
		else if (Iter.Value() == VoxelSizeHandle)
		{
			IDetailPropertyRow& MeshProxySettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
			MeshProxySettingsRow.Visibility(TAttribute<EVisibility>(this, &FMeshProxySettingsCustomizations::IsVoxelSizeVisible));
		}
		else
		{
			IDetailPropertyRow& SettingsRow = MeshSettingsGroup.AddPropertyRow(Iter.Value().ToSharedRef());
		}
	}
}

EVisibility FMeshProxySettingsCustomizations::IsThirdPartySpecificVisible() const
{
	// Static assignment.  The tool can only change during editor restart.
	static bool bUseNativeTool = UseNativeProxyLODTool();

	if (bUseNativeTool)
	{
		return EVisibility::Hidden;
	}

	return EVisibility::Visible;
}

EVisibility FMeshProxySettingsCustomizations::IsProxyLODSpecificVisible() const
{
	// Static assignment.  The tool can only change during editor restart.
	static bool bUseNativeTool = UseNativeProxyLODTool();

	if (bUseNativeTool)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

EVisibility FMeshProxySettingsCustomizations::IsHardAngleThresholdVisible() const
{
	// Only proxyLOD actually uses this setting.  Historically, it has been exposed for
	// simplygon, but it was not actually connected!
	return IsProxyLODSpecificVisible();
}

EVisibility FMeshProxySettingsCustomizations::IsNormalCalcMethodVisible() const
{
	// Only ProxyLOD
	return IsProxyLODSpecificVisible();
}

EVisibility FMeshProxySettingsCustomizations::IsRecalculateNormalsVisible() const
{
	return IsThirdPartySpecificVisible();
}

EVisibility FMeshProxySettingsCustomizations::IsUseLandscapeCullingVisible() const
{
	return EVisibility::Visible;
}

EVisibility FMeshProxySettingsCustomizations::IsUseLandscapeCullingPrecisionVisible() const
{
	return IsThirdPartySpecificVisible();
}

EVisibility FMeshProxySettingsCustomizations::IsMergeDistanceVisible() const
{
	return EVisibility::Visible;
}
EVisibility FMeshProxySettingsCustomizations::IsUnresolvedGeometryColorVisible() const
{   // visible for proxylod but not third party tool (e.g. simplygon)
	return IsProxyLODSpecificVisible();
}
EVisibility FMeshProxySettingsCustomizations::IsSearchDistanceVisible() const
{
	return IsProxyLODSpecificVisible();
}
EVisibility FMeshProxySettingsCustomizations::IsVoxelSizeVisible() const
{
	return IsProxyLODSpecificVisible();
}



#undef LOCTEXT_NAMESPACE