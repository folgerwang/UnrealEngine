// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetNavigation.h"
#include "Blueprint/WidgetTree.h"
#include "MovieScene.h"
#include "Animation/WidgetAnimation.h"
#include "Serialization/TextReferenceCollector.h"
#include "Engine/UserInterfaceSettings.h"
#include "UMGPrivate.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/ObjectResource.h"
#include "UObject/LinkerLoad.h"
#include "Engine/StreamableManager.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

#define LOCTEXT_NAMESPACE "UMG"

FAutoConsoleCommand GDumpTemplateSizesCommand(
	TEXT("Widget.DumpTemplateSizes"),
	TEXT("Dump the sizes of all widget class templates in memory"),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		struct FClassAndSize
		{
			FString ClassName;
			int32 TemplateSize = 0;
		};

		TArray<FClassAndSize> TemplateSizes;

		for (TObjectIterator<UWidgetBlueprintGeneratedClass> WidgetClassIt; WidgetClassIt; ++WidgetClassIt)
		{
			UWidgetBlueprintGeneratedClass* WidgetClass = *WidgetClassIt;

			if (WidgetClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}

#if WITH_EDITOR
			if (Cast<UBlueprint>(WidgetClass->ClassGeneratedBy)->SkeletonGeneratedClass == WidgetClass)
			{
				continue;
			}
#endif

			FClassAndSize Entry;
			Entry.ClassName = WidgetClass->GetName();

#if WITH_EDITOR
			if (WidgetClass->WillHaveTemplate())
#else
			if (WidgetClass->HasTemplate())
#endif
			{
				if (UUserWidget* TemplateWidget = WidgetClass->GetTemplate())
				{
					int32 TemplateSize = WidgetClass->GetStructureSize();
					TemplateWidget->WidgetTree->ForEachWidgetAndDescendants([&TemplateSize](UWidget* Widget) {
						TemplateSize += Widget->GetClass()->GetStructureSize();
					});

					Entry.TemplateSize = TemplateSize;
				}
			}

			TemplateSizes.Add(Entry);
		}

		TemplateSizes.StableSort([](const FClassAndSize& A, const FClassAndSize& B) {
			return A.TemplateSize > B.TemplateSize;
		});

		uint32 TotalSizeBytes = 0;
		UE_LOG(LogUMG, Display, TEXT("%-60s %-15s"), TEXT("Template Class"), TEXT("Size (bytes)"));
		for (const FClassAndSize& Entry : TemplateSizes)
		{
			TotalSizeBytes += Entry.TemplateSize;
			if (Entry.TemplateSize > 0)
			{
				UE_LOG(LogUMG, Display, TEXT("%-60s %-15d"), *Entry.ClassName, Entry.TemplateSize);
			}
			else
			{
				UE_LOG(LogUMG, Display, TEXT("%-60s %-15s"), *Entry.ClassName, TEXT("0 - (No Template)"));
			}
		}

		UE_LOG(LogUMG, Display, TEXT("Total size of templates %.3f MB"), TotalSizeBytes/(1024.f*1024.f));
	}), ECVF_Cheat);

#if WITH_EDITOR

int32 TemplatePreviewInEditor = 0;
static FAutoConsoleVariableRef CVarTemplatePreviewInEditor(TEXT("Widget.TemplatePreviewInEditor"), TemplatePreviewInEditor, TEXT("Should a dynamic template be generated at runtime for the editor for widgets?  Useful for debugging templates."), ECVF_Default);

#endif

#if WITH_EDITORONLY_DATA
namespace
{
	void CollectWidgetBlueprintGeneratedClassTextReferences(UObject* Object, FArchive& Ar)
	{
		// In an editor build, both UWidgetBlueprint and UWidgetBlueprintGeneratedClass reference an identical WidgetTree.
		// So we ignore the UWidgetBlueprintGeneratedClass when looking for persistent text references since it will be overwritten by the UWidgetBlueprint version.
	}
}
#endif

/////////////////////////////////////////////////////
// UWidgetBlueprintGeneratedClass

UWidgetBlueprintGeneratedClass::UWidgetBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bAllowDynamicCreation(true)
	, bTemplateInitialized(false)
{
#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterTextReferenceCollectorCallback AutomaticRegistrationOfTextReferenceCollector(UWidgetBlueprintGeneratedClass::StaticClass(), &CollectWidgetBlueprintGeneratedClassTextReferences); }
	bCanCallPreConstruct = true;
#endif
}

void UWidgetBlueprintGeneratedClass::InitializeBindingsStatic(UUserWidget* UserWidget, const TArray< FDelegateRuntimeBinding >& InBindings)
{
	ensure(!UserWidget->HasAnyFlags(RF_ArchetypeObject));

	// Note: It's not safe to assume here that the UserWidget class type is a UWidgetBlueprintGeneratedClass!
	// - @see InitializeWidgetStatic()

	// For each property binding that we're given, find the corresponding field, and setup the delegate binding on the widget.
	for (const FDelegateRuntimeBinding& Binding : InBindings)
	{
		// If the binding came from a parent class, this will still find it - FindField() searches the super class hierarchy by default.
		UObjectProperty* WidgetProperty = FindField<UObjectProperty>(UserWidget->GetClass(), *Binding.ObjectName);
		if (WidgetProperty == nullptr)
		{
			continue;
		}

		UWidget* Widget = Cast<UWidget>(WidgetProperty->GetObjectPropertyValue_InContainer(UserWidget));

		if (Widget)
		{
			UDelegateProperty* DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), FName(*(Binding.PropertyName.ToString() + TEXT("Delegate"))));
			if (!DelegateProperty)
			{
				DelegateProperty = FindField<UDelegateProperty>(Widget->GetClass(), Binding.PropertyName);
			}

			if (DelegateProperty)
			{
				bool bSourcePathBound = false;

				if (Binding.SourcePath.IsValid())
				{
					bSourcePathBound = Widget->AddBinding(DelegateProperty, UserWidget, Binding.SourcePath);
				}

				// If no native binder is found then the only possibility is that the binding is for
				// a delegate that doesn't match the known native binders available and so we
				// fallback to just attempting to bind to the function directly.
				if (bSourcePathBound == false)
				{
					FScriptDelegate* ScriptDelegate = DelegateProperty->GetPropertyValuePtr_InContainer(Widget);
					if (ScriptDelegate)
					{
						ScriptDelegate->BindUFunction(UserWidget, Binding.FunctionName);
					}
				}
			}
		}
	}
}

void UWidgetBlueprintGeneratedClass::InitializeWidgetStatic(UUserWidget* UserWidget
	, const UClass* InClass
	, bool InHasTemplate
	, bool InAllowDynamicCreation
	, UWidgetTree* InWidgetTree
	, const TArray< UWidgetAnimation* >& InAnimations
	, const TArray< FDelegateRuntimeBinding >& InBindings)
{
	check(InClass);

	// Note: It's not safe to assume here that the UserWidget class type is a UWidgetBlueprintGeneratedClass! In the case of a nativized widget
	// blueprint class, it will be a UDynamicClass instead, and this API will be invoked by the blueprint's C++ code that's generated at cook time.
	// - @see FBackendHelperUMG::EmitWidgetInitializationFunctions()

	if ( UserWidget->HasAllFlags(RF_ArchetypeObject) )
	{
		UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Running Initialize On Archetype, %s."), *InClass->GetName(), *UserWidget->GetName());
		return;
	}

	UWidgetTree* ClonedTree = UserWidget->WidgetTree;

	if ( UserWidget->bCookedWidgetTree )
	{
#if WITH_EDITOR
		// TODO This can get called at editor time when PostLoad runs and we attempt to initialize the tree.
		// Perhaps we shouldn't call init in post load if it's a cooked tree?

		//UE_LOG(LogUMG, Fatal, TEXT("Initializing a cooked widget tree at editor time! %s."), *InClass->GetName());
#else
		// If we can be templated, we need to go ahead and initialize all the user widgets under us, since we're
		// an already expanded tree.
		check(ClonedTree);
		// Either we have a template and permit fast creation, or we don't have a template and don't allow dynamic creation
		// and this is some widget with a cooked widget tree nested inside some other template.
		check((InHasTemplate && InAllowDynamicCreation) || (!InHasTemplate && !InAllowDynamicCreation))

		// TODO NDarnell This initialization can be made faster if part of storing the template data is some kind of
		// acceleration structure that could be the all the userwidgets we need to initialize bindings for...etc.

		// If there's an existing widget tree, then we need to initialize all userwidgets in the tree.
		ClonedTree->ForEachWidget([&] (UWidget* Widget) {
			check(Widget);

			if ( UUserWidget* SubUserWidget = Cast<UUserWidget>(Widget) )
			{
				SubUserWidget->Initialize();
			}
		});

		BindAnimations(UserWidget, InAnimations);

		InitializeBindingsStatic(UserWidget, InBindings);

		UBlueprintGeneratedClass::BindDynamicDelegates(InClass, UserWidget);

#endif
		// We don't need any more initialization for template widgets.
		return;
	}
	else
	{
		// Normally the ClonedTree should be null - we do in the case of design time with the widget, actually
		// clone the widget tree directly from the WidgetBlueprint so that the rebuilt preview matches the newest
		// widget tree, without a full blueprint compile being required.  In that case, the WidgetTree on the UserWidget
		// will have already been initialized to some value.  When that's the case, we'll avoid duplicating it from the class
		// similar to how we use to use the DesignerWidgetTree.
		if ( ClonedTree == nullptr )
		{
			UserWidget->DuplicateAndInitializeFromWidgetTree(InWidgetTree);
			ClonedTree = UserWidget->WidgetTree;
		}
	}

#if !WITH_EDITOR && UE_BUILD_DEBUG
	UE_LOG(LogUMG, Warning, TEXT("Widget Class %s - Slow Static Duplicate Object."), *InClass->GetName());
#endif

	UserWidget->WidgetGeneratedByClass = MakeWeakObjectPtr(const_cast<UClass*>(InClass));

#if WITH_EDITOR
	UserWidget->WidgetGeneratedBy = InClass->ClassGeneratedBy;
#endif

	if (ClonedTree)
	{
		BindAnimations(UserWidget, InAnimations);

		UClass* WidgetBlueprintClass = UserWidget->GetClass();

		ClonedTree->ForEachWidget([&](UWidget* Widget) {
			// Not fatal if NULL, but shouldn't happen
			if (!ensure(Widget != nullptr))
			{
				return;
			}

			Widget->WidgetGeneratedByClass = MakeWeakObjectPtr(const_cast<UClass*>(InClass));

#if WITH_EDITOR
			Widget->WidgetGeneratedBy = InClass->ClassGeneratedBy;
#endif

			// TODO UMG Make this an FName
			FString VariableName = Widget->GetName();

			// Find property with the same name as the template and assign the new widget to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(WidgetBlueprintClass, *VariableName);
			if (Prop)
			{
				Prop->SetObjectPropertyValue_InContainer(UserWidget, Widget);
				UObject* Value = Prop->GetObjectPropertyValue_InContainer(UserWidget);
				check(Value == Widget);
			}

			// Initialize Navigation Data
			if (Widget->Navigation)
			{
				Widget->Navigation->ResolveRules(UserWidget, ClonedTree);
			}

#if WITH_EDITOR
			Widget->ConnectEditorData();
#endif
		});

		InitializeBindingsStatic(UserWidget, InBindings);

		// Bind any delegates on widgets
		UBlueprintGeneratedClass::BindDynamicDelegates(InClass, UserWidget);

		//TODO UMG Add OnWidgetInitialized?
	}
}

void UWidgetBlueprintGeneratedClass::BindAnimations(UUserWidget* Instance, const TArray< UWidgetAnimation* >& InAnimations)
{
	// Note: It's not safe to assume here that the UserWidget class type is a UWidgetBlueprintGeneratedClass!
	// - @see InitializeWidgetStatic()

	for (UWidgetAnimation* Animation : InAnimations)
	{
		if (Animation->GetMovieScene())
		{
			// Find property with the same name as the animation and assign the animation to it.
			UObjectPropertyBase* Prop = FindField<UObjectPropertyBase>(Instance->GetClass(), Animation->GetMovieScene()->GetFName());
			if (Prop)
			{
				Prop->SetObjectPropertyValue_InContainer(Instance, Animation);
			}
		}
	}
}

#if WITH_EDITOR
void UWidgetBlueprintGeneratedClass::SetClassRequiresNativeTick(bool InClassRequiresNativeTick)
{
	bClassRequiresNativeTick = InClassRequiresNativeTick;
}
#endif

void UWidgetBlueprintGeneratedClass::InitializeWidget(UUserWidget* UserWidget) const
{
	TArray<UWidgetAnimation*> AllAnims;
	TArray<FDelegateRuntimeBinding> AllBindings;

	// Include current class animations.
	AllAnims.Append(Animations);

	// Include current class bindings.
	AllBindings.Append(Bindings);

	// Iterate all generated classes in the widget's parent class hierarchy and include animations and bindings found on each one.
	UClass* SuperClass = GetSuperClass();
	while (UWidgetBlueprintGeneratedClass* WBPGC = Cast<UWidgetBlueprintGeneratedClass>(SuperClass))
	{
		AllAnims.Append(WBPGC->Animations);
		AllBindings.Append(WBPGC->Bindings);

		SuperClass = SuperClass->GetSuperClass();
	}

	InitializeWidgetStatic(UserWidget, this, HasTemplate(), bAllowDynamicCreation, WidgetTree, AllAnims, AllBindings);
}

void UWidgetBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

	// Clear CDO flag on tree
	if (WidgetTree)
	{
		WidgetTree->ClearFlags(RF_DefaultSubObject);
	}

	if ( GetLinkerUE4Version() < VER_UE4_RENAME_WIDGET_VISIBILITY )
	{
		static const FName Visiblity(TEXT("Visiblity"));
		static const FName Visibility(TEXT("Visibility"));

		for ( FDelegateRuntimeBinding& Binding : Bindings )
		{
			if ( Binding.PropertyName == Visiblity )
			{
				Binding.PropertyName = Visibility;
			}
		}
	}
}

void UWidgetBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);

	const ERenameFlags RenFlags = REN_DontCreateRedirectors | ( ( bRecompilingOnLoad ) ? REN_ForceNoResetLoaders : 0 ) | REN_NonTransactional | REN_DoNotDirty;

	// Remove the old widdget tree.
	if ( WidgetTree )
	{
		WidgetTree->Rename(nullptr, GetTransientPackage(), RenFlags);
		FLinkerLoad::InvalidateExport(WidgetTree);
		WidgetTree = nullptr;
	}

	// Remove all animations.
	for ( UWidgetAnimation* Animation : Animations )
	{
		Animation->Rename(nullptr, GetTransientPackage(), RenFlags);
		FLinkerLoad::InvalidateExport(Animation);
	}
	Animations.Empty();

	bValidTemplate = false;

	Template = nullptr;
	TemplateAsset.Reset();

#if WITH_EDITOR
	EditorTemplate = nullptr;
#endif

	Bindings.Empty();
}

bool UWidgetBlueprintGeneratedClass::NeedsLoadForServer() const
{
	const UUserInterfaceSettings* UISettings = GetDefault<UUserInterfaceSettings>(UUserInterfaceSettings::StaticClass());
	check(UISettings);
	return UISettings->bLoadWidgetsOnDedicatedServer;
}

bool UWidgetBlueprintGeneratedClass::HasTemplate() const
{
	return bValidTemplate && bAllowDynamicCreation;
}

void UWidgetBlueprintGeneratedClass::SetTemplate(UUserWidget* InTemplate)
{
	Template = InTemplate;
	TemplateAsset = InTemplate;
	
	if (Template)
	{
		Template->AddToCluster(this, true);
	}

	bValidTemplate = TemplateAsset.IsNull() ? false : true;
}

UUserWidget* UWidgetBlueprintGeneratedClass::GetTemplate()
{
#if WITH_EDITOR

	if ( TemplatePreviewInEditor )
	{
		if ( EditorTemplate == nullptr && bAllowTemplate && bAllowDynamicCreation )
		{
			EditorTemplate = NewObject<UUserWidget>(this, this, NAME_None, EObjectFlags(RF_ArchetypeObject | RF_Transient));
			EditorTemplate->TemplateInit();

#if UE_BUILD_DEBUG
			TArray<FText> OutErrors;
			if ( EditorTemplate->VerifyTemplateIntegrity(OutErrors) == false )
			{
				UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Template Failed Verification"), *GetName());
			}
#endif
		}

		return EditorTemplate;
	}
	else
	{
		return nullptr;
	}

#else

	if ( bTemplateInitialized == false && HasTemplate() )
	{
		// This shouldn't be possible with the EDL loader, so only attempt to do it then.
		if (GEventDrivenLoaderEnabled == false && Template == nullptr)
		{
			Template = TemplateAsset.LoadSynchronous();
		}

		// If you hit this ensure, it's possible there's a problem with the loader, or the cooker and the template
		// widget did not end up in the cooked package.
		if ( ensureMsgf(Template, TEXT("No Template Found!  Could not load a Widget Archetype for %s."), *GetName()) )
		{
			bTemplateInitialized = true;

			// This should only ever happen if the EDL is disabled, where you're not guaranteed every object in the package
			// has been loaded at this point.
			if (GEventDrivenLoaderEnabled == false)
			{
				if (Template->HasAllFlags(RF_NeedLoad))
				{
					if (FLinkerLoad* Linker = Template->GetLinker())
					{
						Linker->Preload(Template);
					}
				}
			}

#if !UE_BUILD_SHIPPING
			UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Loaded Fast Template."), *GetName());
#endif

#if UE_BUILD_DEBUG
			TArray<FText> OutErrors;
			if ( Template->VerifyTemplateIntegrity(OutErrors) == false )
			{
				UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Template Failed Verification"), *GetName());
			}
#endif
		}
		else
		{
#if !UE_BUILD_SHIPPING
			UE_LOG(LogUMG, Error, TEXT("Widget Class %s - Failed To Load Template."), *GetName());
#endif
		}
	}

#endif

	return Template;
}

void UWidgetBlueprintGeneratedClass::PreSave(const class ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR
	if ( TargetPlatform && TargetPlatform->RequiresCookedData() )
	{
		if ( WidgetTree )
		{
			if (bCookSlowConstructionWidgetTree)
			{
				WidgetTree->ClearFlags(RF_Transient);
			}
			else
			{
				WidgetTree->SetFlags(RF_Transient);
			}
		}

		InitializeTemplate(TargetPlatform);
	}
	else
	{
		// If we're saving the generated class in the editor, should we allow it to preserve a shadow copy of the one in the
		// blueprint?  Seems dangerous to have this potentially stale copy around, when really it should be the latest version
		// that's compiled on load.
		if ( WidgetTree )
		{
			WidgetTree->SetFlags(RF_Transient);
		}
	}
#endif

	Super::PreSave(TargetPlatform);
}

void UWidgetBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
}

void UWidgetBlueprintGeneratedClass::InitializeTemplate(const ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR

	if ( TargetPlatform && TargetPlatform->RequiresCookedData() )
	{
		bool bCanTemplate = bAllowTemplate && bAllowDynamicCreation;

		if ( bCanTemplate )
		{
			UUserWidget* WidgetTemplate = NewObject<UUserWidget>(GetTransientPackage(), this);
			WidgetTemplate->TemplateInit();

			// Determine if we can generate a template for this widget to speed up CreateWidget time.
			TArray<FText> OutErrors;
			bCanTemplate = WidgetTemplate->VerifyTemplateIntegrity(OutErrors);
			for ( FText Error : OutErrors )
			{
				UE_LOG(LogUMG, Warning, TEXT("Widget Class %s Template Error - %s."), *GetName(), *Error.ToString());
			}
		}

		UPackage* WidgetTemplatePackage = GetOutermost();

		// Remove the old archetype.
		{
			UUserWidget* OldArchetype = FindObject<UUserWidget>(WidgetTemplatePackage, TEXT("WidgetArchetype"));
			if ( OldArchetype )
			{
				const ERenameFlags RenFlags = REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty | REN_ForceNoResetLoaders;

				FString TransientArchetypeString = FString::Printf(TEXT("OLD_TEMPLATE_%s"), *OldArchetype->GetName());
				FName TransientArchetypeName = MakeUniqueObjectName(GetTransientPackage(), OldArchetype->GetClass(), FName(*TransientArchetypeString));
				OldArchetype->Rename(*TransientArchetypeName.ToString(), GetTransientPackage(), RenFlags);
				OldArchetype->SetFlags(RF_Transient);
				OldArchetype->ClearFlags(RF_Public | RF_Standalone | RF_ArchetypeObject);
			}
		}

		if ( bCanTemplate )
		{
			UUserWidget* WidgetTemplate = NewObject<UUserWidget>(WidgetTemplatePackage, this, TEXT("WidgetArchetype"), EObjectFlags(RF_Public | RF_Standalone | RF_ArchetypeObject));
			WidgetTemplate->TemplateInit();

			SetTemplate(WidgetTemplate);

			UE_LOG(LogUMG, Verbose, TEXT("Widget Class %s - Template Initialized."), *GetName());
		}
		else if (bAllowDynamicCreation == false)
		{
			UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Not Allowed To Create Template"), *GetName());

			SetTemplate(nullptr);
		}
		else if ( bAllowTemplate == false )
		{
			UE_LOG(LogUMG, Display, TEXT("Widget Class %s - Not Allowed To Create Template"), *GetName());

			SetTemplate(nullptr);
		}
		else
		{
			UE_LOG(LogUMG, Warning, TEXT("Widget Class %s - Failed To Create Template"), *GetName());

			SetTemplate(nullptr);
		}
	}
#endif
}

UWidgetBlueprintGeneratedClass* UWidgetBlueprintGeneratedClass::FindWidgetTreeOwningClass()
{
	UWidgetBlueprintGeneratedClass* RootBGClass = this;
	UWidgetBlueprintGeneratedClass* BGClass = RootBGClass;

	while (BGClass)
	{
		//TODO NickD: This conditional post load shouldn't be needed any more once the Fast Widget creation path is the only path!
		// Force post load on the generated class so all subobjects are done (specifically the widget tree).
		BGClass->ConditionalPostLoad();

		const bool bNoRootWidget = (nullptr == BGClass->WidgetTree) || (nullptr == BGClass->WidgetTree->RootWidget);

		if (bNoRootWidget)
		{
			UWidgetBlueprintGeneratedClass* SuperBGClass = Cast<UWidgetBlueprintGeneratedClass>(BGClass->GetSuperClass());
			if (SuperBGClass)
			{
				BGClass = SuperBGClass;
				continue;
			}
			else
			{
				// If we reach a super class that isn't a UWidgetBlueprintGeneratedClass, return the root class.
				return RootBGClass;
			}
		}

		return BGClass;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
