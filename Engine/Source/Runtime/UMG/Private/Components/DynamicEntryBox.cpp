// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Components/DynamicEntryBox.h"
#include "UMGPrivate.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "UMG"

//////////////////////////////////////////////////////////////////////////
// UDynamicEntryBox
//////////////////////////////////////////////////////////////////////////

UDynamicEntryBox::UDynamicEntryBox(const FObjectInitializer& Initializer)
	: Super(Initializer)
	, EntryWidgetPool(*this)
{
	bIsVariable = true;

	Visibility = ESlateVisibility::SelfHitTestInvisible;
	EntrySizeRule.SizeRule = ESlateSizeRule::Automatic;
}

void UDynamicEntryBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	EntryWidgetPool.ResetPool();
	MyPanelWidget.Reset();
}

void UDynamicEntryBox::Reset(bool bDeleteWidgets)
{
	EntryWidgetPool.ReleaseAll(bDeleteWidgets);

	if (MyPanelWidget.IsValid())
	{
		switch (EntryBoxType)
		{
		case EDynamicBoxType::Horizontal:
		case EDynamicBoxType::Vertical:
			StaticCastSharedPtr<SBoxPanel>(MyPanelWidget)->ClearChildren();
			break;
		case EDynamicBoxType::Wrap:
			StaticCastSharedPtr<SWrapBox>(MyPanelWidget)->ClearChildren();
			break;
		case EDynamicBoxType::Overlay:
			StaticCastSharedPtr<SOverlay>(MyPanelWidget)->ClearChildren();
			break;
		}
	}
}

const TArray<UUserWidget*>& UDynamicEntryBox::GetAllEntries() const
{
	return EntryWidgetPool.GetActiveWidgets();
}

int32 UDynamicEntryBox::GetNumEntries() const
{
	return MyPanelWidget.IsValid() ? MyPanelWidget->GetChildren()->Num() : 0;
}

void UDynamicEntryBox::RemoveEntry(UUserWidget* EntryWidget)
{
	if (EntryWidget)
	{
		if (MyPanelWidget.IsValid())
		{
			TSharedPtr<SWidget> CachedEntryWidget = EntryWidget->GetCachedWidget();
			if (CachedEntryWidget.IsValid())
			{
				switch (EntryBoxType)
				{
				case EDynamicBoxType::Horizontal:
				case EDynamicBoxType::Vertical:
					StaticCastSharedPtr<SBoxPanel>(MyPanelWidget)->RemoveSlot(CachedEntryWidget.ToSharedRef());
					break;
				case EDynamicBoxType::Wrap:
					StaticCastSharedPtr<SWrapBox>(MyPanelWidget)->RemoveSlot(CachedEntryWidget.ToSharedRef());
					break;
				case EDynamicBoxType::Overlay:
					StaticCastSharedPtr<SOverlay>(MyPanelWidget)->RemoveSlot(CachedEntryWidget.ToSharedRef());
					break;
				}
			}
		}
		EntryWidgetPool.Release(EntryWidget);
	}
}

void UDynamicEntryBox::SetEntrySpacing(const FVector2D& InEntrySpacing)
{
	EntrySpacing = InEntrySpacing;

	if (MyPanelWidget.IsValid())
	{
		if (EntryBoxType == EDynamicBoxType::Wrap)
		{
			// Wrap boxes can change their widget spacing on the fly
			StaticCastSharedPtr<SWrapBox>(MyPanelWidget)->SetInnerSlotPadding(EntrySpacing);
		}
		else if (EntryBoxType == EDynamicBoxType::Overlay)
		{
			TPanelChildren<SOverlay::FOverlaySlot>* OverlayChildren = static_cast<TPanelChildren<SOverlay::FOverlaySlot>*>(MyPanelWidget->GetChildren());
			for (int32 ChildIdx = 0; ChildIdx < OverlayChildren->Num(); ++ChildIdx)
			{
				FMargin Padding;
				if (SpacingPattern.Num() > 0)
				{
					FVector2D Spacing(0.f, 0.f);

					// First establish the starting location
					for (int32 CountIdx = 0; CountIdx < ChildIdx; ++CountIdx)
					{
						int32 PatternIdx = CountIdx % SpacingPattern.Num();
						Spacing += SpacingPattern[PatternIdx];
					}
					
					// Negative padding is no good, so negative spacing is expressed as positive spacing on the opposite side
					if (Spacing.X >= 0.f)
					{
						Padding.Left = Spacing.X;
					}
					else
					{
						Padding.Right = -Spacing.X;
					}
					if (Spacing.Y >= 0.f)
					{
						Padding.Top = Spacing.Y;
					}
					else
					{
						Padding.Bottom = -Spacing.Y;
					}
				}
				else
				{
					if (EntrySpacing.X >= 0.f)
					{
						Padding.Left = ChildIdx * EntrySpacing.X;
					}
					else
					{
						Padding.Right = ChildIdx * -EntrySpacing.X;
					}

					if (EntrySpacing.Y >= 0.f)
					{
						Padding.Top = ChildIdx * EntrySpacing.Y;
					}
					else
					{
						Padding.Bottom = ChildIdx * -EntrySpacing.Y;
					}
				}
				SOverlay::FOverlaySlot& OverlaySlot = (*OverlayChildren)[ChildIdx];
				OverlaySlot.SlotPadding = Padding;
			}
		}
		else
		{
			// Vertical & Horizontal have to manually update the padding on each slot
			const bool bIsHBox = EntryBoxType == EDynamicBoxType::Horizontal;
			TPanelChildren<SBoxPanel::FSlot>* BoxChildren = static_cast<TPanelChildren<SBoxPanel::FSlot>*>(MyPanelWidget->GetChildren());
			for (int32 ChildIdx = 0; ChildIdx < BoxChildren->Num(); ++ChildIdx)
			{
				const bool bIsFirstChild = ChildIdx == 0;

				FMargin Padding;
				Padding.Top = bIsHBox || bIsFirstChild ? 0.f : EntrySpacing.Y;
				Padding.Left = bIsHBox && !bIsFirstChild ? EntrySpacing.X : 0.f;

				SBoxPanel::FSlot& BoxSlot = (*BoxChildren)[ChildIdx];
				BoxSlot.SlotPadding = Padding;
			}
		}
	}
}

#if WITH_EDITOR
void UDynamicEntryBox::ValidateCompiledDefaults(class FCompilerResultsLog& CompileLog) const
{
	if (!EntryWidgetClass)
	{
		CompileLog.Error(*FText::Format(LOCTEXT("Error_DynamicEntryBox_MissingEntryClass", "{0} has no EntryWidgetClass specified - required for any Dynamic Entry Box to function."), FText::FromString(GetName())).ToString());
	}
}
#endif

TSharedRef<SWidget> UDynamicEntryBox::RebuildWidget()
{
	TSharedPtr<SWidget> EntryBoxWidget;
	switch (EntryBoxType)
	{
	case EDynamicBoxType::Horizontal:
		EntryBoxWidget = SAssignNew(MyPanelWidget, SHorizontalBox);
		break;
	case EDynamicBoxType::Vertical:
		EntryBoxWidget = SAssignNew(MyPanelWidget, SVerticalBox);
		break;
	case EDynamicBoxType::Wrap:
		EntryBoxWidget = SAssignNew(MyPanelWidget, SWrapBox)
			.UseAllottedWidth(true)
			.InnerSlotPadding(EntrySpacing);
		break;
	case EDynamicBoxType::Overlay:
		EntryBoxWidget = SAssignNew(MyPanelWidget, SOverlay)
			.Clipping(EWidgetClipping::ClipToBounds);
		break;
	}

	return EntryBoxWidget.ToSharedRef();
}

#if WITH_EDITOR
void UDynamicEntryBox::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (MyPanelWidget.IsValid() && PropertyChangedEvent.GetPropertyName() == TEXT("EntryBoxType"))
	{
		MyPanelWidget.Reset();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UDynamicEntryBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	// At design-time, preview the desired number of entries
#if WITH_EDITORONLY_DATA
	if (IsDesignTime() && MyPanelWidget.IsValid())
	{
		if (!EntryWidgetClass)
		{
			// We have no entry class, so clear everything out
			Reset(true);
		}
		else if (MyPanelWidget->GetChildren()->Num() != NumDesignerPreviewEntries)
		{
			// When the number of entries to preview changes, the easiest thing to do is just soft-rebuild
			Reset();
			int32 StartingNumber = MyPanelWidget->GetChildren()->Num();
			while (StartingNumber < NumDesignerPreviewEntries)
			{
				UUserWidget* PreviewEntry = CreateEntryInternal(EntryWidgetClass);
				if (IsDesignTime() && OnPreviewEntryCreatedFunc)
				{
					OnPreviewEntryCreatedFunc(PreviewEntry);
				}
				StartingNumber++;
			}
		}
		else
		{
			// If we don't need to rebuild, update existing entries
			SetEntrySpacing(EntrySpacing);
			
			//@todo DanH: update alignment, spacing pattern, clipping, size rule, max element size
		}
	}
#endif
}

UUserWidget* UDynamicEntryBox::BP_CreateEntry()
{
	return CreateEntry();
}

UUserWidget* UDynamicEntryBox::BP_CreateEntryOfClass(TSubclassOf<UUserWidget> EntryClass)
{
	if (EntryClass)
	{
		return CreateEntryInternal(EntryClass);
	}

	return nullptr;
}

UUserWidget* UDynamicEntryBox::CreateEntryInternal(TSubclassOf<UUserWidget> InEntryClass)
{
	if (MyPanelWidget.IsValid())
	{
		UUserWidget* NewEntryWidget = EntryWidgetPool.GetOrCreateInstance(InEntryClass);
		AddEntryChild(*NewEntryWidget);
		return NewEntryWidget;
	}

	UE_LOG(LogUMG, Warning, TEXT("UDynamicEntryBox::CreateEntryInternal(): Failed to create an entry."));
	return nullptr;
}

FMargin UDynamicEntryBox::BuildEntryPadding(const FVector2D& DesiredSpacing)
{
	FMargin EntryPadding;
	if (DesiredSpacing.X >= 0.f)
	{
		EntryPadding.Left = DesiredSpacing.X;
	}
	else
	{
		EntryPadding.Right = -DesiredSpacing.X;
	}

	if (DesiredSpacing.Y >= 0.f)
	{
		EntryPadding.Top = DesiredSpacing.Y;
	}
	else
	{
		EntryPadding.Bottom = -DesiredSpacing.Y;
	}

	return EntryPadding;
}

void UDynamicEntryBox::AddEntryChild(UUserWidget& ChildWidget)
{
	FSlotBase* NewSlot = nullptr;
	if (EntryBoxType == EDynamicBoxType::Wrap)
	{
		NewSlot = &StaticCastSharedPtr<SWrapBox>(MyPanelWidget)->AddSlot()
			.FillEmptySpace(false)
			.HAlign(EntryHorizontalAlignment)
			.VAlign(EntryVerticalAlignment);
	}
	else if (EntryBoxType == EDynamicBoxType::Overlay)
	{
		const int32 ChildIdx = MyPanelWidget->GetChildren()->Num();
		SOverlay::FOverlaySlot& OverlaySlot = (SOverlay::FOverlaySlot&)StaticCastSharedPtr<SOverlay>(MyPanelWidget)->AddSlot();

		EHorizontalAlignment HAlign = EntryHorizontalAlignment;
		EVerticalAlignment VAlign = EntryVerticalAlignment;

		FVector2D TargetSpacing = FVector2D::ZeroVector;
		if (SpacingPattern.Num() > 0)
		{
			for (int32 CountIdx = 0; CountIdx < ChildIdx; ++CountIdx)
			{
				const int32 PatternIdx = CountIdx % SpacingPattern.Num();
				TargetSpacing += SpacingPattern[PatternIdx];
			}
		}
		else
		{
			TargetSpacing = EntrySpacing * ChildIdx;
			HAlign = EntrySpacing.X >= 0.f ? EHorizontalAlignment::HAlign_Left : EHorizontalAlignment::HAlign_Right;
			VAlign = EntrySpacing.Y >= 0.f ? EVerticalAlignment::VAlign_Top : EVerticalAlignment::VAlign_Bottom;
		}
		
		OverlaySlot.HAlignment = HAlign;
		OverlaySlot.VAlignment = VAlign;
		OverlaySlot.SlotPadding = BuildEntryPadding(TargetSpacing);

		NewSlot = &OverlaySlot;
	}
	else
	{
		const bool bIsHBox = EntryBoxType == EDynamicBoxType::Horizontal;
		const bool bIsFirstChild = MyPanelWidget->GetChildren()->Num() == 0;

		SBoxPanel::FSlot& BoxPanelSlot = bIsHBox ? (SBoxPanel::FSlot&)StaticCastSharedPtr<SHorizontalBox>(MyPanelWidget)->AddSlot().MaxWidth(MaxElementSize) : (SBoxPanel::FSlot&)StaticCastSharedPtr<SVerticalBox>(MyPanelWidget)->AddSlot().MaxHeight(MaxElementSize);
		BoxPanelSlot.HAlignment = EntryHorizontalAlignment;
		BoxPanelSlot.VAlignment = EntryVerticalAlignment;
		BoxPanelSlot.SizeParam = UWidget::ConvertSerializedSizeParamToRuntime(EntrySizeRule);

		FMargin Padding;
		Padding.Top = bIsHBox || bIsFirstChild ? 0.f : EntrySpacing.Y;
		Padding.Left = bIsHBox && !bIsFirstChild ? EntrySpacing.X : 0.f;
		BoxPanelSlot.SlotPadding = Padding;

		NewSlot = &BoxPanelSlot;
	}

	if (ensure(NewSlot))
	{
		NewSlot->AttachWidget(ChildWidget.TakeWidget());
	}
}

#undef LOCTEXT_NAMESPACE