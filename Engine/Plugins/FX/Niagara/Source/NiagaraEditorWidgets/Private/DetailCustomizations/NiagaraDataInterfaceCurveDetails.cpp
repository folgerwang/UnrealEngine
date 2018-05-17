// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCurveDetails.h"
#include "NiagaraCurveOwner.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceVector2DCurve.h"
#include "NiagaraDataInterfaceVectorCurve.h"
#include "NiagaraDataInterfaceVector4Curve.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraEditorWidgetsModule.h"

#include "SCurveEditor.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/Optional.h"
#include "Brushes/SlateColorBrush.h"
#include "Modules/ModuleManager.h"

#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorWidgetsStyle.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/RichCurve.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceCurveDetails"

FRichCurve* GetCurveFromPropertyHandle(TSharedPtr<IPropertyHandle> Handle)
{
	TArray<void*> RawData;
	Handle->AccessRawData(RawData);
	return RawData.Num() == 1 ? static_cast<FRichCurve*>(RawData[0]) : nullptr;
}

class SNiagaraResizeBox : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnContentHeightChanged, float);

public:
	SLATE_BEGIN_ARGS(SNiagaraResizeBox)
		: _HandleHeight(5)
		, _ContentHeight(50)
		, _HandleColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f))
		, _HandleHighlightColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.5f))
	{}
		SLATE_ARGUMENT(float, HandleHeight)
		SLATE_ATTRIBUTE(float, ContentHeight)
		SLATE_ATTRIBUTE(FLinearColor, HandleColor)
		SLATE_ATTRIBUTE(FLinearColor, HandleHighlightColor)
		SLATE_EVENT(FOnContentHeightChanged, ContentHeightChanged)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		ContentHeight = InArgs._ContentHeight;
		HandleHeight = InArgs._HandleHeight;
		HandleColor = InArgs._HandleColor;
		HandleHighlightColor = InArgs._HandleHighlightColor;
		HandleBrush = FSlateColorBrush(FLinearColor::White);
		ContentHeightChanged = InArgs._ContentHeightChanged;

		ChildSlot
		[
			SNew(SBox)
			.HeightOverride(this, &SNiagaraResizeBox::GetHeightOverride)
			.Padding(FMargin(0, 0, 0, HandleHeight))
			[
				InArgs._Content.Widget
			]
		];
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FVector2D MouseLocation = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
			if (MyGeometry.GetLocalSize().Y - MouseLocation.Y < HandleHeight)
			{
				DragStartLocation = MouseLocation.Y;
				DragStartContentHeight = ContentHeight.Get();
				return FReply::Handled().CaptureMouse(SharedThis(this));
			}
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (this->HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FVector2D MouseLocation = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		LastMouseLocation = MouseLocation.Y;
		if (this->HasMouseCapture())
		{
			float NewContentHeight = DragStartContentHeight + (MouseLocation.Y - DragStartLocation);
			if (ContentHeight.IsBound() && ContentHeightChanged.IsBound())
			{
				ContentHeightChanged.Execute(NewContentHeight);
			}
			else
			{
				ContentHeight = NewContentHeight;
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
	{
		FLinearColor HandleBoxColor;
		int32 HandleLayerId = LayerId + 1;
		FVector2D LocalSize = AllottedGeometry.GetLocalSize();
		if (IsHovered() && LastMouseLocation.IsSet() && LastMouseLocation.GetValue() >= LocalSize.Y - HandleHeight && LastMouseLocation.GetValue() <= LocalSize.Y)
		{
			HandleBoxColor = HandleHighlightColor.Get();
		}
		else
		{
			HandleBoxColor = HandleColor.Get();
		}

		FVector2D HandleLocation(0, AllottedGeometry.GetLocalSize().Y - HandleHeight);
		FVector2D HandleSize(AllottedGeometry.GetLocalSize().X, HandleHeight);
		FSlateDrawElement::MakeBox
		(
			OutDrawElements,
			HandleLayerId,
			AllottedGeometry.ToPaintGeometry(HandleLocation, HandleSize),
			&HandleBrush,
			ESlateDrawEffect::None,
			HandleBoxColor
		);

		return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyClippingRect, OutDrawElements, HandleLayerId, InWidgetStyle, bParentEnabled);
	}

private:
	FOptionalSize GetHeightOverride() const
	{
		return ContentHeight.Get() + HandleHeight;
	}

private:
	TOptional<float> LastMouseLocation;

	TAttribute<float> ContentHeight;
	float HandleHeight;

	float DragStartLocation;
	float DragStartContentHeight;

	TAttribute<FLinearColor> HandleColor;
	TAttribute<FLinearColor> HandleHighlightColor;
	FSlateBrush HandleBrush;

	FOnContentHeightChanged ContentHeightChanged;
};

class SNiagaraDataInterfaceCurveEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraDataInterfaceCurveEditor) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		TArray<TSharedRef<IPropertyHandle>> InCurveProperties,
		bool bIsColorCurve,
		TSharedRef<FNiagaraStackCurveEditorOptions> InStackCurveEditorOptions)
	{
		CurveProperties = InCurveProperties;
		StackCurveEditorOptions = InStackCurveEditorOptions;

		TArray<UObject*> OuterObjects;
		CurveProperties[0]->GetOuterObjects(OuterObjects);
		UObject* CurveOwnerObject = OuterObjects[0];

		CurveOwner = MakeShared<FNiagaraCurveOwner>();
		if (bIsColorCurve)
		{
			CurveOwner->SetColorCurves(
				*GetCurveFromPropertyHandle(CurveProperties[0]),
				*GetCurveFromPropertyHandle(CurveProperties[1]),
				*GetCurveFromPropertyHandle(CurveProperties[2]),
				*GetCurveFromPropertyHandle(CurveProperties[3]),
				NAME_None,
				*CurveOwnerObject,
				FNiagaraCurveOwner::FNotifyCurveChanged::CreateRaw(this, &SNiagaraDataInterfaceCurveEditor::CurveChanged));
		}
		else
		{
			TArray<FLinearColor> CurveColors{ FLinearColor::Red, FLinearColor::Green, FLinearColor::Blue, FLinearColor::White };
			int32 ColorIndex = 0;
			for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
			{
				CurveOwner->AddCurve(
					*GetCurveFromPropertyHandle(CurveProperty),
					*CurveProperty->GetProperty()->GetDisplayNameText().ToString(),
					CurveColors[ColorIndex],
					*CurveOwnerObject,
					FNiagaraCurveOwner::FNotifyCurveChanged::CreateRaw(this, &SNiagaraDataInterfaceCurveEditor::CurveChanged));
				ColorIndex++;
			}
		}

		ViewMinInput = 0;
		ViewMaxOutput = 1;

		for (const FRichCurveEditInfo& CurveEditInfo : CurveOwner->GetCurves())
		{
			if (CurveEditInfo.CurveToEdit->GetNumKeys())
			{
				ViewMinInput = FMath::Min(ViewMinInput, CurveEditInfo.CurveToEdit->GetFirstKey().Time);
				ViewMaxOutput = FMath::Max(ViewMaxOutput, CurveEditInfo.CurveToEdit->GetLastKey().Time);
			}
		}

		ChildSlot
		[
			SAssignNew(CurveEditor, SCurveEditor)
			.HideUI(false)
			.ViewMinInput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMinInput)
			.ViewMaxInput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMaxInput)
			.ViewMinOutput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMinOutput)
			.ViewMaxOutput(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetViewMaxOutput)
			.AreCurvesVisible(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetAreCurvesVisible)
			.ZoomToFitVertical(false)
			.ZoomToFitHorizontal(false)
			.TimelineLength(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::GetTimelineLength)
			.OnSetInputViewRange(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::SetInputViewRange)
			.OnSetOutputViewRange(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::SetOutputViewRange)
			.OnSetAreCurvesVisible(StackCurveEditorOptions.ToSharedRef(), &FNiagaraStackCurveEditorOptions::SetAreCurvesVisible)
		];

		CurveEditor->SetCurveOwner(CurveOwner.Get());
		// Allow users to scroll over the widget in the editor using the scroll wheel (unless it has keyboard focus, in which case it will zoom in/out)
		CurveEditor->SetRequireFocusToZoom(true);
	}

private:
	void CurveChanged(FRichCurve* ChangedCurve, UObject* CurveOwnerObject)
	{
		UNiagaraDataInterfaceCurveBase* EditedCurve = Cast<UNiagaraDataInterfaceCurveBase>(CurveOwnerObject);
		EditedCurve->UpdateLUT(); // we need this done before notify change because of the internal copy methods
		for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
		{
			if (GetCurveFromPropertyHandle(CurveProperty) == ChangedCurve)
			{
				CurveProperty->NotifyPostChange();
				break;
			}
		}
	}

private:
	float ViewMinInput;
	float ViewMaxOutput;
	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	TSharedPtr<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions;
	TSharedPtr<FNiagaraCurveOwner> CurveOwner;
	TSharedPtr<SCurveEditor> CurveEditor;
};


// Curve Base
void FNiagaraDataInterfaceCurveDetailsBase::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	CustomDetailBuilder = &DetailBuilder;
	FNiagaraDataInterfaceDetailsBase::CustomizeDetails(DetailBuilder);
	// Only support single objects.
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	if (ObjectsBeingCustomized.Num() > 1)
	{
		return;
	}
	CustomizedCurveInterface = Cast<UNiagaraDataInterfaceCurveBase>(ObjectsBeingCustomized[0].Get());
	FNiagaraEditorWidgetsModule& NiagaraEditorWidgetsModule = FModuleManager::GetModuleChecked<FNiagaraEditorWidgetsModule>("NiagaraEditorWidgets");
	TSharedRef<FNiagaraStackCurveEditorOptions> StackCurveEditorOptions = NiagaraEditorWidgetsModule.GetOrCreateStackCurveEditorOptionsForObject(
		ObjectsBeingCustomized[0].Get(), GetDefaultAreCurvesVisible(), GetDefaultHeight());

	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	GetCurveProperties(DetailBuilder, CurveProperties);

	// Make sure all property handles are valid.
	for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
	{
		if (CurveProperty->IsValidHandle() == false)
		{
			return;
		}
	}

	for (TSharedRef<IPropertyHandle> CurveProperty : CurveProperties)
	{
		CurveProperty->MarkHiddenByCustomization();
	}

	IDetailCategoryBuilder& CurveCategory = DetailBuilder.EditCategory("Curve");
	TSharedRef<IPropertyHandle> ShowInCurveEditorHandle = CustomDetailBuilder->GetProperty(FName("ShowInCurveEditor"), UNiagaraDataInterfaceCurveBase::StaticClass());
	if (ShowInCurveEditorHandle->IsValidHandle())
	{
		ShowInCurveEditorHandle->MarkHiddenByCustomization();
	}
	CurveCategory.HeaderContent( 
		// Checkbox for showing in curve editor
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 5, 0)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.AutoWidth()
		[
			SNew(SButton)
			.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.ContentPadding(1)
			.ToolTipText(this, &FNiagaraDataInterfaceCurveDetailsBase::GetShowInCurveEditorTooltip)
			.OnClicked(this, &FNiagaraDataInterfaceCurveDetailsBase::OnToggleShowInCurveEditor)
			.Content()
			[
				SNew(SImage)
				.Image(FNiagaraEditorWidgetsStyle::Get().GetBrush("NiagaraEditor.ShowInCurveEditorIcon"))
				.ColorAndOpacity(this, &FNiagaraDataInterfaceCurveDetailsBase::GetShowInCurveEditorImageColor)
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(EHorizontalAlignment::HAlign_Right)
		[
			SNew(SComboButton)
			.HasDownArrow(true)
			.OnGetMenuContent(this, &FNiagaraDataInterfaceCurveDetailsBase::GetCurveToCopyMenu)
			.ContentPadding(2)
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Text(NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "Import", "Import"))
				.ToolTipText(NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "CopyCurveAsset", "Copy data from another Curve asset"))
			]
		]
	);
	CurveCategory.AddCustomRow(NSLOCTEXT("NiagaraDataInterfaceCurveDetails", "CurveFilterText", "Curve"))
		.WholeRowContent()
		[
			SNew(SNiagaraResizeBox)
			.ContentHeight(StackCurveEditorOptions, &FNiagaraStackCurveEditorOptions::GetHeight)
			.ContentHeightChanged(StackCurveEditorOptions, &FNiagaraStackCurveEditorOptions::SetHeight)
			.Content()
			[
				SNew(SNiagaraDataInterfaceCurveEditor, CurveProperties, GetIsColorCurve(), StackCurveEditorOptions)
			]
		];
}

FText FNiagaraDataInterfaceCurveDetailsBase::GetShowInCurveEditorTooltip() const
{
	return LOCTEXT("ShowInCurveEditorToolTip", "Show this curve in the curves tab.");
}

FSlateColor FNiagaraDataInterfaceCurveDetailsBase::GetShowInCurveEditorImageColor() const
{
	return CustomizedCurveInterface->ShowInCurveEditor
		? FEditorStyle::GetSlateColor("SelectionColor")
		: FLinearColor::Gray;
}

FReply FNiagaraDataInterfaceCurveDetailsBase::OnToggleShowInCurveEditor() const
{
	TSharedRef<IPropertyHandle> ShowInCurveEditorHandle = CustomDetailBuilder->GetProperty(FName("ShowInCurveEditor"), UNiagaraDataInterfaceCurveBase::StaticClass());
	if (ShowInCurveEditorHandle->IsValidHandle())
	{
		bool bShowInCurveEditor;
		ShowInCurveEditorHandle->GetValue(bShowInCurveEditor);
		ShowInCurveEditorHandle->SetValue(!bShowInCurveEditor);
	}
	return FReply::Handled();
}

void FNiagaraDataInterfaceCurveDetailsBase::ImportSelectedAsset(UObject* SelectedAsset)
{
	TArray<FRichCurve> FloatCurves;
	GetFloatCurvesFromAsset(SelectedAsset, FloatCurves);
	TArray<TSharedRef<IPropertyHandle>> CurveProperties;
	GetCurveProperties(*CustomDetailBuilder, CurveProperties);
	if (FloatCurves.Num() == CurveProperties.Num())
	{
		FScopedTransaction ImportTransaction(LOCTEXT("ImportCurveTransaction", "Import curve"));
		CustomizedCurveInterface->Modify();
		for (int i = 0; i < CurveProperties.Num(); i++)
		{
			if (CurveProperties[i]->IsValidHandle())
			{
				*GetCurveFromPropertyHandle(CurveProperties[i]) = FloatCurves[i];
			}
		}
		CustomizedCurveInterface->UpdateLUT(); // we need this done before notify change because of the internal copy methods
		for (auto CurveProperty : CurveProperties)
		{
			CurveProperty->NotifyPostChange();
		}
	}
}

TSharedRef<SWidget> FNiagaraDataInterfaceCurveDetailsBase::GetCurveToCopyMenu()
{
	FName ClassName = GetSupportedAssetClassName();
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &FNiagaraDataInterfaceCurveDetailsBase::CurveToCopySelected);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassNames.Add(ClassName);
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	return SNew(SBox)
		.WidthOverride(300.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
	
	return SNullWidget::NullWidget;
}

void FNiagaraDataInterfaceCurveDetailsBase::CurveToCopySelected(const FAssetData& AssetData)
{
	ImportSelectedAsset(AssetData.GetAsset());
	FSlateApplication::Get().DismissAllMenus();
}

// Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceCurveDetails>();
}

void FNiagaraDataInterfaceCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& CurveProperties) const
{
	CurveProperties.Add(DetailBuilder.GetProperty(FName("Curve"), UNiagaraDataInterfaceCurve::StaticClass()));
}

FName FNiagaraDataInterfaceCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveFloat::StaticClass()->GetFName();
}

void FNiagaraDataInterfaceCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveFloat* CurveAsset = Cast<UCurveFloat>(SelectedAsset);
	FloatCurves.Add(CurveAsset->FloatCurve);
}

// Vector 2D Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVector2DCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVector2DCurveDetails>();
}

void FNiagaraDataInterfaceVector2DCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("XCurve"), UNiagaraDataInterfaceVector2DCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("YCurve"), UNiagaraDataInterfaceVector2DCurve::StaticClass()));
}

FName FNiagaraDataInterfaceVector2DCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveVector::StaticClass()->GetFName();
}

void FNiagaraDataInterfaceVector2DCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveVector* CurveAsset = Cast<UCurveVector>(SelectedAsset);
	for (int i = 0; i < 2; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}


// Vector Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVectorCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVectorCurveDetails>();
}

void FNiagaraDataInterfaceVectorCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("XCurve"), UNiagaraDataInterfaceVectorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("YCurve"), UNiagaraDataInterfaceVectorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("ZCurve"), UNiagaraDataInterfaceVectorCurve::StaticClass()));
}

FName FNiagaraDataInterfaceVectorCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveVector::StaticClass()->GetFName();
}

void FNiagaraDataInterfaceVectorCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveVector* CurveAsset = Cast<UCurveVector>(SelectedAsset);
	for (int i = 0; i < 3; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}


// Vector 4 Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceVector4CurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceVector4CurveDetails>();
}

void FNiagaraDataInterfaceVector4CurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("XCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("YCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("ZCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("WCurve"), UNiagaraDataInterfaceVector4Curve::StaticClass()));
}

FName FNiagaraDataInterfaceVector4CurveDetails::GetSupportedAssetClassName() const
{
	return UCurveLinearColor::StaticClass()->GetFName();
}

void FNiagaraDataInterfaceVector4CurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveLinearColor* CurveAsset = Cast<UCurveLinearColor>(SelectedAsset);
	for (int i = 0; i < 4; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}

// Color Curve
TSharedRef<IDetailCustomization> FNiagaraDataInterfaceColorCurveDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceColorCurveDetails>();
}

void FNiagaraDataInterfaceColorCurveDetails::GetCurveProperties(IDetailLayoutBuilder& DetailBuilder, TArray<TSharedRef<IPropertyHandle>>& OutCurveProperties) const
{
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("RedCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("GreenCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("BlueCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
	OutCurveProperties.Add(DetailBuilder.GetProperty(FName("AlphaCurve"), UNiagaraDataInterfaceColorCurve::StaticClass()));
}

FName FNiagaraDataInterfaceColorCurveDetails::GetSupportedAssetClassName() const
{
	return UCurveLinearColor::StaticClass()->GetFName();
}

void FNiagaraDataInterfaceColorCurveDetails::GetFloatCurvesFromAsset(UObject* SelectedAsset, TArray<FRichCurve>& FloatCurves) const
{
	UCurveLinearColor* CurveAsset = Cast<UCurveLinearColor>(SelectedAsset);
	for (int i = 0; i < 4; i++)
	{
		FloatCurves.Add(CurveAsset->FloatCurves[i]);
	}
}
#undef LOCTEXT_NAMESPACE