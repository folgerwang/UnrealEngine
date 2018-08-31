// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Templates/WidgetTemplateClass.h"

#if WITH_EDITOR
	#include "Editor.h"
#endif // WITH_EDITOR
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"

#include "Blueprint/WidgetTree.h"
#include "Styling/SlateIconFinder.h"

#define LOCTEXT_NAMESPACE "UMGEditor"

FWidgetTemplateClass::FWidgetTemplateClass()
	: WidgetClass(nullptr)
{
	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &FWidgetTemplateClass::OnObjectsReplaced);
}

FWidgetTemplateClass::FWidgetTemplateClass(TSubclassOf<UWidget> InWidgetClass)
	: WidgetClass(InWidgetClass.Get())
{
	Name = WidgetClass->GetDisplayNameText();

	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &FWidgetTemplateClass::OnObjectsReplaced);
}

FWidgetTemplateClass::FWidgetTemplateClass(const FAssetData& InWidgetAssetData, TSubclassOf<UWidget> InWidgetClass)
	: WidgetAssetData(InWidgetAssetData)
{
	if (InWidgetClass)
	{
		WidgetClass = *InWidgetClass;
		Name = WidgetClass->GetDisplayNameText();
	}
	else
	{
		Name = FText::FromString(FName::NameToDisplayString(WidgetAssetData.AssetName.ToString(), false));
	}
}

FWidgetTemplateClass::~FWidgetTemplateClass()
{
	GEditor->OnObjectsReplaced().RemoveAll(this);
}

FText FWidgetTemplateClass::GetCategory() const
{
	if (WidgetClass.Get())
	{
		auto DefaultWidget = WidgetClass->GetDefaultObject<UWidget>();
		return DefaultWidget->GetPaletteCategory();
	}
	else
	{
		auto DefaultWidget = UWidget::StaticClass()->GetDefaultObject<UWidget>();
		return DefaultWidget->GetPaletteCategory();
	}
}

UWidget* FWidgetTemplateClass::Create(UWidgetTree* Tree)
{
	// Load the blueprint asset if needed
	if (!WidgetClass.Get())
	{
		FString AssetPath = WidgetAssetData.ObjectPath.ToString();
		UBlueprint* LoadedBP = LoadObject<UBlueprint>(nullptr, *AssetPath);
		WidgetClass = *LoadedBP->GeneratedClass;
	}

	return CreateNamed(Tree, NAME_None);
}

const FSlateBrush* FWidgetTemplateClass::GetIcon() const
{
	if (WidgetClass.IsValid())
	{
		return FSlateIconFinder::FindIconBrushForClass(WidgetClass.Get());
	}
	else
	{
		return FSlateIconFinder::FindIconBrushForClass(UWidget::StaticClass());
	}
	return nullptr;
}

TSharedRef<IToolTip> FWidgetTemplateClass::GetToolTip() const
{
	if (WidgetClass.IsValid())
	{
		return IDocumentation::Get()->CreateToolTip(WidgetClass->GetToolTipText(), nullptr, FString(TEXT("Shared/Types/")) + WidgetClass->GetName(), TEXT("Class"));
	}
	else
	{
		FText Description;

		FString DescriptionStr = WidgetAssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UBlueprint, BlueprintDescription));
		if (!DescriptionStr.IsEmpty())
		{
			DescriptionStr.ReplaceInline(TEXT("\\n"), TEXT("\n"));
			Description = FText::FromString(MoveTemp(DescriptionStr));
		}
		else
		{
			Description = Name;
		}

		return IDocumentation::Get()->CreateToolTip(Description, nullptr, FString(TEXT("Shared/Types/")) + Name.ToString(), TEXT("Class"));
	}
}

void FWidgetTemplateClass::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	UObject* const* NewObject = ReplacementMap.Find(WidgetClass.Get());
	if (NewObject)
	{
		WidgetClass = CastChecked<UClass>(*NewObject);
	}
}

UWidget* FWidgetTemplateClass::CreateNamed(class UWidgetTree* Tree, FName NameOverride)
{
	if (NameOverride != NAME_None)
	{
		UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), Tree, *NameOverride.ToString());
		if (ExistingObject != nullptr)
		{
			NameOverride = MakeUniqueObjectName(Tree, WidgetClass.Get(), NameOverride);
		}
	}

	UWidget* NewWidget = Tree->ConstructWidget<UWidget>(WidgetClass.Get(), NameOverride);
	NewWidget->OnCreationFromPalette();

	return NewWidget;
}

#undef LOCTEXT_NAMESPACE
