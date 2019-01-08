// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Types/ReflectionMetadata.h"

FString FReflectionMetaData::GetWidgetDebugInfo(const SWidget* InWidget)
{
	if (!InWidget)
	{
		return TEXT("None");
	}

	// UMG widgets have meta-data to help track them
	TSharedPtr<FReflectionMetaData> MetaData = InWidget->GetMetaData<FReflectionMetaData>();
	if (MetaData.IsValid())
	{
		if (const UObject* AssetPtr = MetaData->Asset.Get())
		{
			const FName AssetName = AssetPtr->GetFName();
			const FName WidgetName = MetaData->Name;

			return FString::Printf(TEXT("%s [%s]"), *AssetName.ToString(), *WidgetName.ToString());
		}
	}

	TSharedPtr<FReflectionMetaData> ParentMetadata = GetWidgetOrParentMetaData(InWidget);
	if (ParentMetadata.IsValid())
	{
		if (const UObject* AssetPtr = ParentMetadata->Asset.Get())
		{
			const FName AssetName = AssetPtr->GetFName();
			const FName WidgetName = ParentMetadata->Name;

			return FString::Printf(TEXT("%s [%s(%s)]"), *AssetName.ToString(), *WidgetName.ToString(), *InWidget->GetReadableLocation());
		}
	}

	return InWidget->ToString();
}

TSharedPtr<FReflectionMetaData> FReflectionMetaData::GetWidgetOrParentMetaData(const SWidget* InWidget)
{
	const SWidget* CurrentWidget = InWidget;
	while (CurrentWidget != nullptr)
	{
		// UMG widgets have meta-data to help track them
		TSharedPtr<FReflectionMetaData> MetaData = CurrentWidget->GetMetaData<FReflectionMetaData>();
		if (MetaData.IsValid() && MetaData->Asset.Get())
		{
			return MetaData;
		}

		// If the widget we're on doesn't have metadata or asset information, try the parent widgets,
		// sometimes complex widgets create many internal widgets, they should still belong to the
		// corresponding asset/class.
		CurrentWidget = CurrentWidget->GetParentWidget().Get();
	}

	return TSharedPtr<FReflectionMetaData>();
}