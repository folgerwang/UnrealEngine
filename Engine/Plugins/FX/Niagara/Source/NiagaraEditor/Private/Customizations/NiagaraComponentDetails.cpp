// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraComponentDetails.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterface.h"
#include "DetailLayoutBuilder.h"
#include "IDetailPropertyRow.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SToolTip.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "EditorStyleSet.h"
#include "NiagaraSystemInstance.h"
#include "ViewModels/NiagaraParameterViewModel.h"
#include "NiagaraEditorUtilities.h"
#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraTypes.h"
#include "ScopedTransaction.h"
#include "SNiagaraParameterEditor.h"
#include "NiagaraEditorModule.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "IStructureDetailsView.h"
#include "IDetailsView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableText.h"
#include "Editor.h"
#include "NiagaraParameterCollectionViewModel.h"
#include "NiagaraScriptSource.h"
#include "NiagaraNodeInput.h"
#include "NiagaraGraph.h"
#include "GameDelegates.h"
#include "NiagaraEditorStyle.h"
#include "IDetailChildrenBuilder.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Images/SImage.h"
#include "NiagaraEditorModule.h"
#include "Modules/ModuleManager.h"
#include "INiagaraEditorTypeUtilities.h"
#include "Widgets/Layout/SBox.h"
#include "UObject/WeakObjectPtr.h"
#define LOCTEXT_NAMESPACE "NiagaraComponentDetails"

class FNiagaraComponentNodeBuilder : public IDetailCustomNodeBuilder
{
public:
	FNiagaraComponentNodeBuilder(UNiagaraComponent* InComponent, UNiagaraScript* SourceSpawn, UNiagaraScript* SourceUpdate)						   
	{
		Component = InComponent;
		Component->OnSynchronizedWithAssetParameters().AddRaw(this, &FNiagaraComponentNodeBuilder::ComponentSynchronizedWithAssetParameters);
		OriginalScripts.Add(SourceSpawn);
		OriginalScripts.Add(SourceUpdate);
		//UE_LOG(LogNiagaraEditor, Log, TEXT("FNiagaraComponentNodeBuilder %p Component %p"), this, Component.Get());
	}

	~FNiagaraComponentNodeBuilder()
	{
		if (Component.IsValid())
		{
			Component->OnSynchronizedWithAssetParameters().RemoveAll(this);
		}
		//UE_LOG(LogNiagaraEditor, Log, TEXT("~FNiagaraComponentNodeBuilder %p Component %p"), this, Component.Get());
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
		static const FName NiagaraComponentNodeBuilder("FNiagaraComponentNodeBuilder");
		return NiagaraComponentNodeBuilder;
	}

	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		check(Component.IsValid());
		TArray<FNiagaraVariable> Parameters;
		FNiagaraParameterStore& ParamStore = Component->GetOverrideParameters();
		ParamStore.GetParameters(Parameters);

		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		
		for (const FNiagaraVariable& Parameter : Parameters)
		{
			TSharedPtr<SWidget> NameWidget;

			NameWidget =
				SNew(STextBlock)
				.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
				.Text(FText::FromName(Parameter.GetName()));
			
			IDetailPropertyRow* Row = nullptr;

			TSharedPtr<SWidget> CustomValueWidget;

			if (!Parameter.IsDataInterface())
			{
				TSharedPtr<FStructOnScope> StructOnScope = MakeShareable(new FStructOnScope(Parameter.GetType().GetStruct(), (uint8*)ParamStore.GetParameterData(Parameter)));
				Row = ChildrenBuilder.AddExternalStructureProperty(StructOnScope.ToSharedRef(), NAME_None, Parameter.GetName());

			}
			else 
			{
				int32 DataInterfaceOffset = ParamStore.IndexOf(Parameter);
				UObject* DefaultValueObject = ParamStore.GetDataInterfaces()[DataInterfaceOffset];

				TArray<UObject*> Objects;
				Objects.Add(DefaultValueObject);

				TOptional<bool> bAllowChildrenOverride(true);
				TOptional<bool> bCreateCategoryNodesOverride(false);
				Row = ChildrenBuilder.AddExternalObjectProperty(Objects, NAME_None, Parameter.GetName(), bAllowChildrenOverride, bCreateCategoryNodesOverride); 

				CustomValueWidget =
					SNew(STextBlock)
					.TextStyle(FNiagaraEditorStyle::Get(), "NiagaraEditor.ParameterText")
					.Text(FText::FromString(FName::NameToDisplayString(DefaultValueObject->GetClass()->GetName(), false)));
			}

			if (Row)
			{
				TSharedPtr<SWidget> DefaultNameWidget;
				TSharedPtr<SWidget> DefaultValueWidget;

				TSharedPtr<IPropertyHandle> PropertyHandle = Row->GetPropertyHandle();

				FDetailWidgetRow& CustomWidget = Row->CustomWidget(true);

				TArray<UObject*> Objects;
				Row->GetPropertyHandle()->GetOuterObjects(Objects);

				Row->GetDefaultWidgets(DefaultNameWidget, DefaultValueWidget, CustomWidget);

				if (Parameter.IsDataInterface())
				{
					PropertyHandle->SetOnPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfacePreChange, Parameter));
					PropertyHandle->SetOnChildPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfacePreChange, Parameter));
					PropertyHandle->SetOnPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfaceChanged, Parameter));
					PropertyHandle->SetOnChildPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnDataInterfaceChanged, Parameter));
				}
				else
				{
					PropertyHandle->SetOnPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterPreChange, Parameter));
					PropertyHandle->SetOnChildPropertyValuePreChange(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterPreChange, Parameter));
					PropertyHandle->SetOnPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterChanged, Parameter));
					PropertyHandle->SetOnChildPropertyValueChanged(
						FSimpleDelegate::CreateRaw(this, &FNiagaraComponentNodeBuilder::OnParameterChanged, Parameter));
				}

				CustomWidget
					.NameContent()
					[
						SNew(SBox)
						.Padding(FMargin(0.0f, 2.0f))
						[
							NameWidget.ToSharedRef()
						]
					];

				TSharedPtr<SWidget> ValueWidget = DefaultValueWidget;
				if (CustomValueWidget.IsValid())
				{
					ValueWidget = CustomValueWidget;
				}

				CustomWidget
					.ValueContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.HAlign(HAlign_Fill)
						.Padding(4.0f)
						[
							// Add in the parameter editor factoried above.
							ValueWidget.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.AutoWidth()
						[
							// Add in the "reset to default" buttons
							SNew(SButton)
							.OnClicked_Raw(this, &FNiagaraComponentNodeBuilder::OnLocationResetClicked, Parameter)
							.Visibility_Raw(this, &FNiagaraComponentNodeBuilder::GetLocationResetVisibility, Parameter)
							.ContentPadding(FMargin(5.f, 0.f))
							.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.Content()
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
							]
						]
					];
			}
		}
	}

private:
	void OnParameterPreChange(FNiagaraVariable Var)
	{
		check(Component.IsValid());
		Component->Modify();
	}

	void OnDataInterfacePreChange(FNiagaraVariable Var)
	{
		check(Component.IsValid());
		Component->Modify();
	}

	void OnParameterChanged(FNiagaraVariable Var)
	{
		check(Component.IsValid());
		Component->GetOverrideParameters().OnParameterChange();
		Component->SetParameterValueOverriddenLocally(Var, true, false);
	}

	void OnDataInterfaceChanged(FNiagaraVariable Var)
	{
		Component->GetOverrideParameters().OnInterfaceChange();
		check(Component.IsValid());
		Component->SetParameterValueOverriddenLocally(Var, true, true);
	}

	bool DoesParameterDifferFromDefault(const FNiagaraVariable& Var)
	{
		check(Component.IsValid());
		return Component->IsParameterValueOverriddenLocally(Var.GetName());
	}

	FReply OnLocationResetClicked(FNiagaraVariable Parameter)
	{
		check(Component.IsValid());
		FScopedTransaction ScopedTransaction(LOCTEXT("ResetParameterValue", "Reset parameter value to system defaults."));
		Component->Modify();
		Component->SetParameterValueOverriddenLocally(Parameter, false, false);
		return FReply::Handled();
	}

	EVisibility GetLocationResetVisibility(FNiagaraVariable Parameter) const
	{
		return Component.IsValid() && Component->IsParameterValueOverriddenLocally(Parameter.GetName()) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	void ComponentSynchronizedWithAssetParameters()
	{
		OnRebuildChildren.ExecuteIfBound();
	}

private:
	TWeakObjectPtr<UNiagaraComponent> Component;
	FSimpleDelegate OnRebuildChildren;
	TArray<TSharedRef<FStructOnScope>> CreatedStructOnScopes;
	TArray<UNiagaraScript*> OriginalScripts;
};

TSharedRef<IDetailCustomization> FNiagaraComponentDetails::MakeInstance()
{
	return MakeShareable(new FNiagaraComponentDetails);
}

FNiagaraComponentDetails::FNiagaraComponentDetails() : Builder(nullptr)
{
}

FNiagaraComponentDetails::~FNiagaraComponentDetails()
{
	if (GEngine)
	{
		GEngine->OnWorldDestroyed().RemoveAll(this);
	}

	FGameDelegates::Get().GetEndPlayMapDelegate().RemoveAll(this);
}

void FNiagaraComponentDetails::OnPiEEnd()
{
	UE_LOG(LogNiagaraEditor, Log, TEXT("onPieEnd"));
	if (Component.IsValid())
	{
		if (Component->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("onPieEnd - has package flags"));
			UWorld* TheWorld = UWorld::FindWorldInPackage(Component->GetOutermost());
			if (TheWorld)
			{
				OnWorldDestroyed(TheWorld);
			}
		}
	}
}

void FNiagaraComponentDetails::OnWorldDestroyed(class UWorld* InWorld)
{
	// We have to clear out any temp data interfaces that were bound to the component's package when the world goes away or otherwise
	// we'll report GC leaks..
	if (Component.IsValid())
	{
		if (Component->GetWorld() == InWorld)
		{
			UE_LOG(LogNiagaraEditor, Log, TEXT("OnWorldDestroyed - matched up"));
			Builder = nullptr;
		}
	}
}

void FNiagaraComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Builder = &DetailBuilder;

	static const FName ParamCategoryName = TEXT("NiagaraComponent_Parameters");
	static const FName ScriptCategoryName = TEXT("Parameters");

	TSharedPtr<IPropertyHandle> LocalOverridesPropertyHandle = DetailBuilder.GetProperty(TEXT("OverrideParameters"));
	if (LocalOverridesPropertyHandle.IsValid())
	{
		LocalOverridesPropertyHandle->MarkHiddenByCustomization();
	}

	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	if (ObjectsCustomized.Num() == 1 && ObjectsCustomized[0]->IsA<UNiagaraComponent>())
	{
		Component = CastChecked<UNiagaraComponent>(ObjectsCustomized[0].Get());

		if (GEngine)
		{
			GEngine->OnWorldDestroyed().AddRaw(this, &FNiagaraComponentDetails::OnWorldDestroyed);
		}

		FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FNiagaraComponentDetails::OnPiEEnd);

		if (Component->GetAsset())
		{
			UNiagaraScript* ScriptSpawn = Component->GetAsset()->GetSystemSpawnScript();
			UNiagaraScript* ScriptUpdate = Component->GetAsset()->GetSystemUpdateScript();

			IDetailCategoryBuilder& InputParamCategory = DetailBuilder.EditCategory(ParamCategoryName, LOCTEXT("ParamCategoryName", "Override Parameters"));
			InputParamCategory.AddCustomBuilder(MakeShared<FNiagaraComponentNodeBuilder>(Component.Get(), ScriptSpawn, ScriptUpdate));
		}
	}
}

#undef LOCTEXT_NAMESPACE
