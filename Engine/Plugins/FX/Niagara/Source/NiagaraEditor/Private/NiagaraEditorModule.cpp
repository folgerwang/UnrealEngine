// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraEditorModule.h"
#include "NiagaraModule.h"
#include "NiagaraEditorTickables.h"
#include "Modules/ModuleManager.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "Misc/ConfigCacheIni.h"
#include "ISequencerModule.h"
#include "ISettingsModule.h"
#include "SequencerChannelInterface.h"
#include "SequencerSettings.h"
#include "AssetRegistryModule.h"
#include "ThumbnailRendering/ThumbnailManager.h"

#include "AssetTypeActions/AssetTypeActions_NiagaraSystem.h"
#include "AssetTypeActions/AssetTypeActions_NiagaraEmitter.h"
#include "AssetTypeActions/AssetTypeActions_NiagaraScript.h"
#include "AssetTypeActions/AssetTypeActions_NiagaraParameterCollection.h"

#include "EdGraphUtilities.h"
#include "SGraphPin.h"
#include "KismetPins/SGraphPinVector4.h"
#include "KismetPins/SGraphPinNum.h"
#include "KismetPins/SGraphPinInteger.h"
#include "KismetPins/SGraphPinVector.h"
#include "KismetPins/SGraphPinVector2D.h"
#include "KismetPins/SGraphPinObject.h"
#include "KismetPins/SGraphPinColor.h"
#include "KismetPins/SGraphPinBool.h"
#include "Editor/GraphEditor/Private/KismetPins/SGraphPinEnum.h"
#include "SNiagaraGraphPinNumeric.h"
#include "SNiagaraGraphPinAdd.h"
#include "NiagaraNodeConvert.h"
#include "EdGraphSchema_Niagara.h"
#include "TypeEditorUtilities/NiagaraFloatTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraIntegerTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraEnumTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraBoolTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraFloatTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraVectorTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraColorTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraMatrixTypeEditorUtilities.h"
#include "TypeEditorUtilities/NiagaraDataInterfaceCurveTypeEditorUtilities.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorCommands.h"
#include "Sequencer/NiagaraSequence/NiagaraEmitterTrackEditor.h"
#include "Sequencer/LevelSequence/NiagaraSystemTrackEditor.h"
#include "PropertyEditorModule.h"
#include "NiagaraSettings.h"
#include "NiagaraModule.h"
#include "NiagaraShaderModule.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraDataInterfaceVector2DCurve.h"
#include "NiagaraDataInterfaceVectorCurve.h"
#include "NiagaraDataInterfaceVector4Curve.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraScriptViewModel.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "TNiagaraGraphPinEditableName.h"
#include "Sequencer/NiagaraSequence/Sections/MovieSceneNiagaraEmitterSection.h"
#include "UObject/Class.h"
#include "NiagaraScriptMergeManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraScriptSource.h"
#include "NiagaraTypes.h"

#include "MovieScene/Parameters/MovieSceneNiagaraBoolParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraFloatParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraIntegerParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraVectorParameterTrack.h"
#include "MovieScene/Parameters/MovieSceneNiagaraColorParameterTrack.h"

#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneIntegerSection.h"
#include "Sections/MovieSceneVectorSection.h"
#include "Sections/MovieSceneColorSection.h"

#include "ISequencerSection.h"
#include "Sections/BoolPropertySection.h"
#include "Sections/ColorPropertySection.h"

#include "Customizations/NiagaraComponentDetails.h"
#include "Customizations/NiagaraTypeCustomizations.h"
#include "Customizations/NiagaraEventScriptPropertiesCustomization.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraHlslTranslator.h"
#include "NiagaraThumbnailRenderer.h"


IMPLEMENT_MODULE( FNiagaraEditorModule, NiagaraEditor );

#define LOCTEXT_NAMESPACE "NiagaraEditorModule"

const FName FNiagaraEditorModule::NiagaraEditorAppIdentifier( TEXT( "NiagaraEditorApp" ) );
const FLinearColor FNiagaraEditorModule::WorldCentricTabColorScale(0.0f, 0.0f, 0.2f, 0.5f);

const FName FNiagaraEditorModule::FInputMetaDataKeys::AdvancedDisplay = "AdvancedDisplay";
const FName FNiagaraEditorModule::FInputMetaDataKeys::EditCondition = "EditCondition";
const FName FNiagaraEditorModule::FInputMetaDataKeys::VisibleCondition = "VisibleCondition";
const FName FNiagaraEditorModule::FInputMetaDataKeys::InlineEditConditionToggle = "InlineEditConditionToggle";

EAssetTypeCategories::Type FNiagaraEditorModule::NiagaraAssetCategory;

//////////////////////////////////////////////////////////////////////////

class FNiagaraScriptGraphPanelPinFactory : public FGraphPanelPinFactory
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<class SGraphPin>, FCreateGraphPin, UEdGraphPin*);

	/** Registers a delegate for creating a pin for a specific type. */
	void RegisterTypePin(const UScriptStruct* Type, FCreateGraphPin CreateGraphPin)
	{
		TypeToCreatePinDelegateMap.Add(Type, CreateGraphPin);
	}

	/** Registers a delegate for creating a pin for for a specific miscellaneous sub category. */
	void RegisterMiscSubCategoryPin(FName SubCategory, FCreateGraphPin CreateGraphPin)
	{
		MiscSubCategoryToCreatePinDelegateMap.Add(SubCategory, CreateGraphPin);
	}

	//~ FGraphPanelPinFactory interface
	virtual TSharedPtr<class SGraphPin> CreatePin(class UEdGraphPin* InPin) const override
	{
		if (const UEdGraphSchema_Niagara* NSchema = Cast<UEdGraphSchema_Niagara>(InPin->GetSchema()))
		{
			if (InPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryType)
			{
				const UScriptStruct* Struct = CastChecked<const UScriptStruct>(InPin->PinType.PinSubCategoryObject.Get());
				const FCreateGraphPin* CreateGraphPin = TypeToCreatePinDelegateMap.Find(Struct);
				if (CreateGraphPin != nullptr)
				{
					return (*CreateGraphPin).Execute(InPin);
				}
			}
			else if (InPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryEnum)
			{
				const UEnum* Enum = Cast<const UEnum>(InPin->PinType.PinSubCategoryObject.Get());
				if (Enum == nullptr)
				{
					UE_LOG(LogNiagaraEditor, Error, TEXT("Pin states that it is of Enum type, but is missing its Enum! Pin Name '%s' Owning Node '%s'. Turning into standard int definition!"), *InPin->PinName.ToString(),
						*InPin->GetOwningNode()->GetName());
					InPin->PinType.PinCategory = UEdGraphSchema_Niagara::PinCategoryType;
					InPin->PinType.PinSubCategoryObject = MakeWeakObjectPtr(const_cast<UScriptStruct*>(FNiagaraTypeDefinition::GetIntStruct()));
					InPin->DefaultValue.Empty();
					return CreatePin(InPin);
				}
				return SNew(TNiagaraGraphPinEditableName<SGraphPinEnum>, InPin);
			}
			else if (InPin->PinType.PinCategory == UEdGraphSchema_Niagara::PinCategoryMisc)
			{
				const FCreateGraphPin* CreateGraphPin = MiscSubCategoryToCreatePinDelegateMap.Find(InPin->PinType.PinSubCategory);
				if (CreateGraphPin != nullptr)
				{
					return (*CreateGraphPin).Execute(InPin);
				}
			}

			return SNew(TNiagaraGraphPinEditableName<SGraphPin>, InPin);
		}
		return nullptr;
	}

private:
	TMap<const UScriptStruct*, FCreateGraphPin> TypeToCreatePinDelegateMap;
	TMap<FName, FCreateGraphPin> MiscSubCategoryToCreatePinDelegateMap;
};

FNiagaraEditorModule::FNiagaraEditorModule() 
	: SequencerSettings(nullptr)
	, TestCompileScriptCommand(nullptr)
{
}

void DumpParameterStore(const FNiagaraParameterStore& ParameterStore)
{
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::Get().GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	TArray<FNiagaraVariable> ParameterVariables;
	ParameterStore.GetParameters(ParameterVariables);
	for (const FNiagaraVariable& ParameterVariable : ParameterVariables)
	{
		FString Name = ParameterVariable.GetName().ToString();
		FString Type = ParameterVariable.GetType().GetName();
		FString Value;
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> ParameterTypeUtilities = NiagaraEditorModule.GetTypeUtilities(ParameterVariable.GetType());
		if (ParameterTypeUtilities.IsValid() && ParameterTypeUtilities->CanHandlePinDefaults())
		{
			FNiagaraVariable ParameterVariableWithValue = ParameterVariable;
			ParameterVariableWithValue.SetData(ParameterStore.GetParameterData(ParameterVariable));
			Value = ParameterTypeUtilities->GetPinDefaultStringFromValue(ParameterVariableWithValue);
		}
		else
		{
			Value = "(unsupported)";
		}
		UE_LOG(LogNiagaraEditor, Log, TEXT("%s\t%s\t%s"), *Name, *Type, *Value);
	}
}

void DumpRapidIterationParametersForScript(UNiagaraScript* Script, const FString& HeaderName)
{
	UEnum* NiagaraScriptUsageEnum = FindObjectChecked<UEnum>(ANY_PACKAGE, TEXT("ENiagaraScriptUsage"), true);
	FString UsageName = NiagaraScriptUsageEnum->GetNameByValue((int64)Script->GetUsage()).ToString();
	UE_LOG(LogNiagaraEditor, Log, TEXT("%s - %s - %s"), *Script->GetPathName(), *HeaderName, *UsageName);
	DumpParameterStore(Script->RapidIterationParameters);
}

void DumpRapidIterationParametersForEmitter(UNiagaraEmitter* Emitter, const FString& EmitterName)
{
	TArray<UNiagaraScript*> Scripts;
	Emitter->GetScripts(Scripts, false);
	for (UNiagaraScript* Script : Scripts)
	{
		DumpRapidIterationParametersForScript(Script, EmitterName);
	}
}

void DumpRapidIterationParamersForAsset(const TArray<FString>& Arguments)
{
	if (Arguments.Num() == 1)
	{
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		const FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*Arguments[0]);
		UObject* Asset = AssetData.GetAsset();
		if (Asset != nullptr)
		{
			UNiagaraSystem* SystemAsset = Cast<UNiagaraSystem>(Asset);
			if (SystemAsset != nullptr)
			{
				DumpRapidIterationParametersForScript(SystemAsset->GetSystemSpawnScript(), SystemAsset->GetName());
				DumpRapidIterationParametersForScript(SystemAsset->GetSystemUpdateScript(), SystemAsset->GetName());
				for (const FNiagaraEmitterHandle& EmitterHandle : SystemAsset->GetEmitterHandles())
				{
					DumpRapidIterationParametersForEmitter(EmitterHandle.GetInstance(), EmitterHandle.GetName().ToString());
				}
			}
			else
			{
				UNiagaraEmitter* EmitterAsset = Cast<UNiagaraEmitter>(Asset);
				if (EmitterAsset != nullptr)
				{
					DumpRapidIterationParametersForEmitter(EmitterAsset, EmitterAsset->GetName());
				}
				else
				{
					UE_LOG(LogNiagaraEditor, Warning, TEXT("DumpRapidIterationParameters - Only niagara system and niagara emitter assets are supported"));
				}
			}
		}
		else
		{
			UE_LOG(LogNiagaraEditor, Warning, TEXT("DumpRapidIterationParameters - Asset not found"));
		}
	}
	else
	{
		UE_LOG(LogNiagaraEditor, Warning, TEXT("DumpRapidIterationParameters - Must supply an asset path to dump"));
	}
}

class FNiagaraSystemBoolParameterTrackEditor : public FNiagaraSystemParameterTrackEditor<UMovieSceneNiagaraBoolParameterTrack, UMovieSceneBoolSection>
{
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override
	{
		checkf(SectionObject.GetClass()->IsChildOf<UMovieSceneBoolSection>(), TEXT("Unsupported section."));
		return MakeShareable(new FBoolPropertySection(SectionObject));
	}
};

class FNiagaraSystemColorParameterTrackEditor : public FNiagaraSystemParameterTrackEditor<UMovieSceneNiagaraColorParameterTrack, UMovieSceneColorSection>
{
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override
	{
		checkf(SectionObject.GetClass()->IsChildOf<UMovieSceneColorSection>(), TEXT("Unsupported section."));
		return MakeShareable(new FColorPropertySection(*Cast<UMovieSceneColorSection>(&SectionObject), ObjectBinding, GetSequencer()));
	}
};

void FNiagaraEditorModule::StartupModule()
{
	FHlslNiagaraTranslator::Init();
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	NiagaraAssetCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("FX")), LOCTEXT("NiagaraAssetsCategory", "FX"));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraSystem()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraEmitter()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraScriptFunctions()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraScriptModules()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraScriptDynamicInputs()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraParameterCollection()));
	RegisterAssetTypeAction(AssetTools, MakeShareable(new FAssetTypeActions_NiagaraParameterCollectionInstance()));

	UNiagaraSettings::OnSettingsChanged().AddRaw(this, &FNiagaraEditorModule::OnNiagaraSettingsChangedEvent);
	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(this, &FNiagaraEditorModule::OnPreGarbageCollection);

	// register details customization
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("NiagaraComponent", FOnGetDetailCustomizationInstance::CreateStatic(&FNiagaraComponentDetails::MakeInstance));
	
	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraFloat",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraInt32",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraNumeric",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraParameterMap",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraNumericCustomization::MakeInstance)
	);


	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraBool",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraBoolCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraMatrix",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraMatrixCustomization::MakeInstance)
	);

	PropertyModule.RegisterCustomPropertyTypeLayout("NiagaraVariableAttributeBinding",
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FNiagaraVariableAttributeBindingCustomization::MakeInstance)
	);

	FNiagaraEditorStyle::Initialize();
	FNiagaraEditorCommands::Register();

	TSharedPtr<FNiagaraScriptGraphPanelPinFactory> GraphPanelPinFactory = MakeShareable(new FNiagaraScriptGraphPanelPinFactory());

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetFloatStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinNum<float>>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetIntStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinInteger>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetVec2Struct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinVector2D>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetVec3Struct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinVector>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetVec4Struct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinVector4>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetColorStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinColor>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetBoolStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SGraphPinBool>, GraphPin); }));

	GraphPanelPinFactory->RegisterTypePin(FNiagaraTypeDefinition::GetGenericNumericStruct(), FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(TNiagaraGraphPinEditableName<SNiagaraGraphPinNumeric>, GraphPin); }));

	// TODO: Don't register this here.
	GraphPanelPinFactory->RegisterMiscSubCategoryPin(UNiagaraNodeWithDynamicPins::AddPinSubCategory, FNiagaraScriptGraphPanelPinFactory::FCreateGraphPin::CreateLambda(
		[](UEdGraphPin* GraphPin) -> TSharedRef<SGraphPin> { return SNew(SNiagaraGraphPinAdd, GraphPin); }));

	EnumTypeUtilities = MakeShareable(new FNiagaraEditorEnumTypeUtilities());
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetFloatDef(), MakeShareable(new FNiagaraEditorFloatTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetIntDef(), MakeShareable(new FNiagaraEditorIntegerTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetBoolDef(), MakeShareable(new FNiagaraEditorBoolTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetVec2Def(), MakeShareable(new FNiagaraEditorVector2TypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetVec3Def(), MakeShareable(new FNiagaraEditorVector3TypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetVec4Def(), MakeShareable(new FNiagaraEditorVector4TypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetQuatDef(), MakeShareable(new FNiagaraEditorQuatTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetColorDef(), MakeShareable(new FNiagaraEditorColorTypeUtilities()));
	RegisterTypeUtilities(FNiagaraTypeDefinition::GetMatrix4Def(), MakeShareable(new FNiagaraEditorMatrixTypeUtilities()));

	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceCurveTypeEditorUtilities, ESPMode::ThreadSafe>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceVector2DCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceCurveTypeEditorUtilities, ESPMode::ThreadSafe>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceVectorCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceVectorCurveTypeEditorUtilities, ESPMode::ThreadSafe>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceVector4Curve::StaticClass()), MakeShared<FNiagaraDataInterfaceVectorCurveTypeEditorUtilities, ESPMode::ThreadSafe>());
	RegisterTypeUtilities(FNiagaraTypeDefinition(UNiagaraDataInterfaceColorCurve::StaticClass()), MakeShared<FNiagaraDataInterfaceColorCurveTypeEditorUtilities, ESPMode::ThreadSafe>());

	FEdGraphUtilities::RegisterVisualPinFactory(GraphPanelPinFactory);

	FNiagaraOpInfo::Init();

	RegisterSettings();

	// Register sequencer track editors
	ISequencerModule &SequencerModule = FModuleManager::LoadModuleChecked<ISequencerModule>("Sequencer");
	CreateEmitterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FNiagaraEmitterTrackEditor::CreateTrackEditor));
	CreateSystemTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FNiagaraSystemTrackEditor::CreateTrackEditor));

	SequencerModule.RegisterChannelInterface<FMovieSceneNiagaraEmitterChannel>();

	CreateBoolParameterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(
		&FNiagaraSystemBoolParameterTrackEditor::CreateTrackEditor));
	CreateFloatParameterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(
		&FNiagaraSystemParameterTrackEditor<UMovieSceneNiagaraFloatParameterTrack, UMovieSceneFloatSection>::CreateTrackEditor));
	CreateIntegerParameterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(
		&FNiagaraSystemParameterTrackEditor<UMovieSceneNiagaraIntegerParameterTrack, UMovieSceneIntegerSection>::CreateTrackEditor));
	CreateVectorParameterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(
		&FNiagaraSystemParameterTrackEditor<UMovieSceneNiagaraVectorParameterTrack, UMovieSceneVectorSection>::CreateTrackEditor));
	CreateColorParameterTrackEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(
		&FNiagaraSystemColorParameterTrackEditor::CreateTrackEditor));

	RegisterParameterTrackCreatorForType(*FNiagaraBool::StaticStruct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter) {
		return NewObject<UMovieSceneNiagaraBoolParameterTrack>(); }));
	RegisterParameterTrackCreatorForType(*FNiagaraFloat::StaticStruct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter) {
		return NewObject<UMovieSceneNiagaraFloatParameterTrack>(); }));
	RegisterParameterTrackCreatorForType(*FNiagaraInt32::StaticStruct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter) {
		return NewObject<UMovieSceneNiagaraIntegerParameterTrack>(); }));
	RegisterParameterTrackCreatorForType(*FNiagaraTypeDefinition::GetVec2Struct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter) 
	{
		UMovieSceneNiagaraVectorParameterTrack* VectorTrack = NewObject<UMovieSceneNiagaraVectorParameterTrack>();
		VectorTrack->SetChannelsUsed(2);
		return VectorTrack;
	}));
	RegisterParameterTrackCreatorForType(*FNiagaraTypeDefinition::GetVec3Struct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter)
	{
		UMovieSceneNiagaraVectorParameterTrack* VectorTrack = NewObject<UMovieSceneNiagaraVectorParameterTrack>();
		VectorTrack->SetChannelsUsed(3);
		return VectorTrack;
	}));
	RegisterParameterTrackCreatorForType(*FNiagaraTypeDefinition::GetVec4Struct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter)
	{
		UMovieSceneNiagaraVectorParameterTrack* VectorTrack = NewObject<UMovieSceneNiagaraVectorParameterTrack>();
		VectorTrack->SetChannelsUsed(4);
		return VectorTrack;
	}));
	RegisterParameterTrackCreatorForType(*FNiagaraTypeDefinition::GetColorStruct(), FOnCreateMovieSceneTrackForParameter::CreateLambda([](FNiagaraVariable InParameter) {
		return NewObject<UMovieSceneNiagaraColorParameterTrack>(); }));

	// Register the shader queue processor (for cooking)
	INiagaraModule& NiagaraModule = FModuleManager::LoadModuleChecked<INiagaraModule>("Niagara");
	NiagaraModule.SetOnProcessShaderCompilationQueue(INiagaraModule::FOnProcessQueue::CreateLambda([]()
	{
		FNiagaraShaderQueueTickable::ProcessQueue();
	}));

	INiagaraShaderModule& NiagaraShaderModule = FModuleManager::LoadModuleChecked<INiagaraShaderModule>("NiagaraShader");
	NiagaraShaderModule.SetOnProcessShaderCompilationQueue(INiagaraShaderModule::FOnProcessQueue::CreateLambda([]()
	{
		FNiagaraShaderQueueTickable::ProcessQueue();
	}));

	// Register the emitter merge handler.
	ScriptMergeManager = MakeShared<FNiagaraScriptMergeManager>();
	MergeEmitterHandle = NiagaraModule.RegisterOnMergeEmitter(INiagaraModule::FOnMergeEmitter::CreateSP(ScriptMergeManager.ToSharedRef(), &FNiagaraScriptMergeManager::MergeEmitter));

	// Register the script compiler
	ScriptCompilerHandle = NiagaraModule.RegisterScriptCompiler(INiagaraModule::FScriptCompiler::CreateLambda([this](const FNiagaraCompileRequestDataBase* CompileRequest, const FNiagaraCompileOptions& Options)
	{
		return CompileScript(CompileRequest, Options);
	}));

	PrecompilerHandle = NiagaraModule.RegisterPrecompiler(INiagaraModule::FOnPrecompile::CreateLambda([this](UObject* InObj)
	{
		return Precompile(InObj);
	}));

	// Register the create default script source handler.
	CreateDefaultScriptSourceHandle = NiagaraModule.RegisterOnCreateDefaultScriptSource(
		INiagaraModule::FOnCreateDefaultScriptSource::CreateLambda([](UObject* Outer) { return NewObject<UNiagaraScriptSource>(Outer); }));


	TestCompileScriptCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("fx.TestCompileNiagaraScript"),
		TEXT("Compiles the specified script on disk for the niagara vector vm"),
		FConsoleCommandWithArgsDelegate::CreateRaw(this, &FNiagaraEditorModule::TestCompileScriptFromConsole));

	DumpRapidIterationParametersForAsset = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("fx.DumpRapidIterationParametersForAsset"),
		TEXT("Dumps the values of the rapid iteration parameters for the specified asset by path."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpRapidIterationParamersForAsset));

	UThumbnailManager::Get().RegisterCustomRenderer(UNiagaraEmitter::StaticClass(), UNiagaraEmitterThumbnailRenderer::StaticClass());
	UThumbnailManager::Get().RegisterCustomRenderer(UNiagaraSystem::StaticClass(), UNiagaraSystemThumbnailRenderer::StaticClass());
}


void FNiagaraEditorModule::ShutdownModule()
{
	// Ensure that we don't have any lingering compiles laying around that will explode after this module shuts down.
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* Sys = *It;
		if (Sys)
		{
			Sys->WaitForCompilationComplete();
		}
	}

	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
	
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto CreatedAssetTypeAction : CreatedAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(CreatedAssetTypeAction.ToSharedRef());
		}
	}
	CreatedAssetTypeActions.Empty();

	UNiagaraSettings::OnSettingsChanged().RemoveAll(this);

	FCoreUObjectDelegates::GetPreGarbageCollectDelegate().RemoveAll(this);

	
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout("NiagaraComponent");
	}

	FNiagaraEditorStyle::Shutdown();

	UnregisterSettings();

	ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModule != nullptr)
	{
		SequencerModule->UnRegisterTrackEditor(CreateEmitterTrackEditorHandle);
		SequencerModule->UnRegisterTrackEditor(CreateSystemTrackEditorHandle);
		SequencerModule->UnRegisterTrackEditor(CreateBoolParameterTrackEditorHandle);
		SequencerModule->UnRegisterTrackEditor(CreateFloatParameterTrackEditorHandle);
		SequencerModule->UnRegisterTrackEditor(CreateIntegerParameterTrackEditorHandle);
		SequencerModule->UnRegisterTrackEditor(CreateVectorParameterTrackEditorHandle);
		SequencerModule->UnRegisterTrackEditor(CreateColorParameterTrackEditorHandle);
	}

	INiagaraModule* NiagaraModule = FModuleManager::GetModulePtr<INiagaraModule>("Niagara");
	if (NiagaraModule != nullptr)
	{
		NiagaraModule->UnregisterOnMergeEmitter(MergeEmitterHandle);
		NiagaraModule->UnregisterOnCreateDefaultScriptSource(CreateDefaultScriptSourceHandle);
		NiagaraModule->UnregisterScriptCompiler(ScriptCompilerHandle);
		NiagaraModule->UnregisterPrecompiler(PrecompilerHandle);
	}

	// Verify that we've cleaned up all the view models in the world.
	FNiagaraSystemViewModel::CleanAll();
	FNiagaraEmitterViewModel::CleanAll();
	FNiagaraScriptViewModel::CleanAll();

	if (TestCompileScriptCommand != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(TestCompileScriptCommand);
	}

	if (DumpRapidIterationParametersForAsset != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(DumpRapidIterationParametersForAsset);
	}

	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UNiagaraEmitter::StaticClass());
		UThumbnailManager::Get().UnregisterCustomRenderer(UNiagaraSystem::StaticClass());
	}
}

FNiagaraEditorModule& FNiagaraEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
}

void FNiagaraEditorModule::OnNiagaraSettingsChangedEvent(const FString& PropertyName, const UNiagaraSettings* Settings)
{
	if (PropertyName == "AdditionalParameterTypes" || PropertyName == "AdditionalPayloadTypes")
	{
		FNiagaraTypeDefinition::RecreateUserDefinedTypeRegistry();
	}
}

void FNiagaraEditorModule::RegisterTypeUtilities(FNiagaraTypeDefinition Type, TSharedRef<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> EditorUtilities)
{
	TypeEditorsCS.Lock();
	TypeToEditorUtilitiesMap.Add(Type, EditorUtilities);
	TypeEditorsCS.Unlock();
}


TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> FNiagaraEditorModule::GetTypeUtilities(const FNiagaraTypeDefinition& Type)
{
	TypeEditorsCS.Lock();
	TSharedRef<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe>* EditorUtilities = TypeToEditorUtilitiesMap.Find(Type);
	TypeEditorsCS.Unlock();

	if(EditorUtilities != nullptr)
	{
		return *EditorUtilities;
	}

	if (Type.IsEnum())
	{
		return EnumTypeUtilities;
	}

	return TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe>();
}

TSharedRef<SWidget> FNiagaraEditorModule::CreateStackWidget(UNiagaraStackViewModel* StackViewModel) const
{
	checkf(OnCreateStackWidget.IsBound(), TEXT("Can not create stack widget.  Stack creation delegate was never set."));
	return OnCreateStackWidget.Execute(StackViewModel);
}

FDelegateHandle FNiagaraEditorModule::SetOnCreateStackWidget(FOnCreateStackWidget InOnCreateStackWidget)
{
	checkf(OnCreateStackWidget.IsBound() == false, TEXT("Stack creation delegate already set."));
	OnCreateStackWidget = InOnCreateStackWidget;
	return OnCreateStackWidget.GetHandle();
}

void FNiagaraEditorModule::ResetOnCreateStackWidget(FDelegateHandle Handle)
{
	checkf(OnCreateStackWidget.GetHandle() == Handle, TEXT("Can only reset the stack creation module with the handle it was created with."));
	OnCreateStackWidget.Unbind();
}

TSharedRef<FNiagaraScriptMergeManager> FNiagaraEditorModule::GetScriptMergeManager() const
{
	return ScriptMergeManager.ToSharedRef();
}

void FNiagaraEditorModule::RegisterParameterTrackCreatorForType(const UScriptStruct& StructType, FOnCreateMovieSceneTrackForParameter CreateTrack)
{
	checkf(TypeToParameterTrackCreatorMap.Contains(&StructType) == false, TEXT("Type already registered"));
	TypeToParameterTrackCreatorMap.Add(&StructType, CreateTrack);
}

void FNiagaraEditorModule::UnregisterParameterTrackCreatorForType(const UScriptStruct& StructType)
{
	TypeToParameterTrackCreatorMap.Remove(&StructType);
}

bool FNiagaraEditorModule::CanCreateParameterTrackForType(const UScriptStruct& StructType)
{
	return TypeToParameterTrackCreatorMap.Contains(&StructType);
}

UMovieSceneNiagaraParameterTrack* FNiagaraEditorModule::CreateParameterTrackForType(const UScriptStruct& StructType, FNiagaraVariable Parameter)
{
	FOnCreateMovieSceneTrackForParameter* CreateTrack = TypeToParameterTrackCreatorMap.Find(&StructType);
	checkf(CreateTrack != nullptr, TEXT("Type not supported"));
	UMovieSceneNiagaraParameterTrack* ParameterTrack = CreateTrack->Execute(Parameter);
	ParameterTrack->SetParameter(Parameter);
	return ParameterTrack;
}

const FNiagaraEditorCommands& FNiagaraEditorModule::Commands()
{
	return FNiagaraEditorCommands::Get();
}

void FNiagaraEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action)
{
	AssetTools.RegisterAssetTypeActions(Action);
	CreatedAssetTypeActions.Add(Action);
}

void FNiagaraEditorModule::RegisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SequencerSettings = USequencerSettingsContainer::GetOrCreate<USequencerSettings>(TEXT("NiagaraSequenceEditor"));

		SettingsModule->RegisterSettings("Editor", "ContentEditors", "NiagaraSequenceEditor",
			LOCTEXT("NiagaraSequenceEditorSettingsName", "Niagara Sequence Editor"),
			LOCTEXT("NiagaraSequenceEditorSettingsDescription", "Configure the look and feel of the Niagara Sequence Editor."),
			SequencerSettings);	
	}
}

void FNiagaraEditorModule::UnregisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "ContentEditors", "NiagaraSequenceEditor");
	}
}

void FNiagaraEditorModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SequencerSettings)
	{
		Collector.AddReferencedObject(SequencerSettings);
	}
}

void FNiagaraEditorModule::OnPreGarbageCollection()
{
	// For commandlets like GenerateDistillFileSetsCommandlet, they just load the package and do some hierarchy navigation within it 
	// tracking sub-assets, then they garbage collect. Since nothing is holding onto the system at the root level, it will be summarily
	// killed and any of references will also be killed. To thwart this for now, we are forcing the compilations to complete BEFORE
	// garbage collection kicks in. To do otherwise for now has too many loose ends (a system may be left around after the level has been
	// unloaded, leaving behind weird external references, etc). This should be revisited when more time is available (i.e. not days before a 
	// release is due to go out).
	for (TObjectIterator<UNiagaraSystem> It; It; ++It)
	{
		UNiagaraSystem* System = *It;
		if (System && System->HasOutstandingCompilationRequests())
		{
			System->WaitForCompilationComplete();
		}
	}
}

#undef LOCTEXT_NAMESPACE
