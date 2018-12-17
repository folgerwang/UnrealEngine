// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceDetails.h"
#include "IDetailCustomization.h"
#include "NiagaraDataInterface.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorUtilities.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Widgets/Images/SImage.h"
#include "PropertyHandle.h"
#include "IPropertyUtilities.h"
#include "Misc/NotifyHook.h"
#include "IDetailChildrenBuilder.h"

#define LOCTEXT_NAMESPACE "FNiagaraDataInterfaceDetailsBase"
#define ErrorsCategoryName  TEXT("Errors")

class SNiagaraDataInterfaceError : public SCompoundWidget
{
public:
	DECLARE_DELEGATE(FOnFixTriggered);

public:
	SLATE_BEGIN_ARGS(SNiagaraDataInterfaceError){}
		SLATE_EVENT(FOnFixTriggered, OnFixTriggered)
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UNiagaraDataInterface* InDataInterface, FNiagaraDataInterfaceError InError)
	{
		OnFixTriggered = InArgs._OnFixTriggered;
		
		Error = InError;
		DataInterface = InDataInterface;

		TSharedRef<SHorizontalBox> ErrorBox = SNew(SHorizontalBox);
		
		ErrorBox->AddSlot()
			.AutoWidth()
			[
				SNew(SHorizontalBox)
				.ToolTipText(GetErrorTextTooltip())
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Icons.Error"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
					.Clipping(EWidgetClipping::ClipToBounds)
					.Text(GetErrorSummary())
				]
			];
		if (Error.GetErrorFixable())
		{
			ErrorBox->AddSlot()
			.VAlign(VAlign_Top)
			.Padding(5, 0, 0, 0)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked(this, &SNiagaraDataInterfaceError::OnFixNowClicked)
				.ToolTipText(NSLOCTEXT("NiagaraDataInterfaceError", "FixButtonLabelToolTip", "Fix the data linked to this interface."))
				.Content()
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("NiagaraDataInterfaceError", "FixButtonLabel", "Fix Now"))
				]
			];
		}
		ChildSlot
		[
			ErrorBox
		];
	}

private:
	FText GetErrorSummary() const
	{
		return Error.GetErrorSummaryText();
	}

	FText GetErrorTextTooltip() const
	{
		return Error.GetErrorText();
	}

	FReply OnFixNowClicked()
	{
		FScopedTransaction ScopedTransaction(NSLOCTEXT("NiagaraDataInterfaceDetails", "FixDataIntefraceTransaction", "Fix asset for data interface"));
		OnFixTriggered.ExecuteIfBound();
		Error.TryFixError();
		DataInterface->PostEditChange();
		return FReply::Handled();
	}

private:
	FNiagaraDataInterfaceError Error;
	IDetailLayoutBuilder* DetailBuilder;
	UNiagaraDataInterface* DataInterface;
	FSimpleDelegate OnFixTriggered;
};

class FNiagaraDataInterfaceCustomNodeBuilder : public IDetailCustomNodeBuilder
											 , public TSharedFromThis<FNiagaraDataInterfaceCustomNodeBuilder>
{
public:
	FNiagaraDataInterfaceCustomNodeBuilder(IDetailLayoutBuilder* InDetailBuilder)
		: DetailBuilder(InDetailBuilder)
	{
	}

	void Initialize(UNiagaraDataInterface& InDataInterface)
	{
		DataInterface = &InDataInterface;
		DataInterface->OnChanged().AddSP(this, &FNiagaraDataInterfaceCustomNodeBuilder::OnDataInterfaceChanged);
	}

	~FNiagaraDataInterfaceCustomNodeBuilder()
	{
		if (DataInterface.IsValid())
		{
			DataInterface->OnChanged().RemoveAll(this);
		}
	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override
	{
		OnRebuildChildren = InOnRegenerateChildren;
	}

	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) {}
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const { return false; }

	virtual FName GetName() const  override
	{
		static const FName NiagaraDataInterfaceCustomNodeBuilder("NiagaraDataInterfaceCustomNodeBuilder");
		return NiagaraDataInterfaceCustomNodeBuilder;
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		for (FNiagaraDataInterfaceError Error : DataInterface->GetErrors())
		{
			FDetailWidgetRow& Row = ChildrenBuilder.AddCustomRow(NSLOCTEXT("NiagaraDataInterfaceDetails", "DataError", "Data Error"));
			Row.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SNiagaraDataInterfaceError, DataInterface.Get(), Error)
					.OnFixTriggered(this, &FNiagaraDataInterfaceCustomNodeBuilder::OnErrorFixTriggered)
			
				]
			];
		}
	}
private:
	void OnDataInterfaceChanged()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

	void OnErrorFixTriggered()
	{
		UProperty* PropertyPlaceholder = nullptr;  // we don't need to specify the property, all we need is to trigger the restart of the emitter
		FPropertyChangedEvent ChangeEvent(PropertyPlaceholder, EPropertyChangeType::Unspecified);
		if (DetailBuilder->GetPropertyUtilities()->GetNotifyHook() != nullptr)
		{
			DetailBuilder->GetPropertyUtilities()->GetNotifyHook()->NotifyPostChange(ChangeEvent, PropertyPlaceholder);
		}
	}


private:
	TWeakObjectPtr<UNiagaraDataInterface> DataInterface;
	IDetailLayoutBuilder* DetailBuilder;
	FSimpleDelegate OnRebuildChildren;
};

void FNiagaraDataInterfaceDetailsBase::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Builder = &DetailBuilder;
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	check(SelectedObjects.Num() == 1);
	DataInterface = Cast<UNiagaraDataInterface>(SelectedObjects[0].Get());
	check(DataInterface.IsValid());
	DataInterface->OnChanged().AddSP(this, &FNiagaraDataInterfaceDetailsBase::OnDataChanged);
	IDetailCategoryBuilder& ErrorsBuilderRef = DetailBuilder.EditCategory(ErrorsCategoryName, LOCTEXT("Errors", "Errors"), ECategoryPriority::Important);
	ErrorsCategoryBuilder = &ErrorsBuilderRef;
	CustomBuilder = MakeShared<FNiagaraDataInterfaceCustomNodeBuilder>(&DetailBuilder);
	CustomBuilder->Initialize(*DataInterface);
	ErrorsCategoryBuilder->AddCustomBuilder(CustomBuilder.ToSharedRef());
	OnDataChanged();
}

void FNiagaraDataInterfaceDetailsBase::OnDataChanged() // need to only refresh errors, and all will be good
{
	if (Builder != nullptr)
	{
		int CurrentErrorCount = DataInterface->GetErrors().Num();
		
		if (CurrentErrorCount == 0)
		{
			ErrorsCategoryBuilder->SetCategoryVisibility(false);
		}
		else
		{
			ErrorsCategoryBuilder->SetCategoryVisibility(true);
		}
	}
}

FNiagaraDataInterfaceDetailsBase::~FNiagaraDataInterfaceDetailsBase()
{
	if (DataInterface.IsValid())
	{
		DataInterface->OnChanged().RemoveAll(this);
	}
}

TSharedRef<IDetailCustomization> FNiagaraDataInterfaceDetailsBase::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceDetailsBase>();
}


#undef LOCTEXT_NAMESPACE