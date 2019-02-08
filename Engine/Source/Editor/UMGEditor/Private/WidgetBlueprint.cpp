// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "WidgetBlueprint.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "MovieScene.h"

#include "Engine/UserDefinedStruct.h"
#include "EdGraph/EdGraph.h"
#include "Blueprint/WidgetTree.h"
#include "Animation/WidgetAnimation.h"

#include "Kismet2/StructureEditorUtils.h"

#include "Kismet2/CompilerResultsLog.h"
#include "Binding/PropertyBinding.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "UObject/PropertyTag.h"
#include "WidgetBlueprintCompiler.h"
#include "UObject/EditorObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "WidgetGraphSchema.h"
#include "UMGEditorProjectSettings.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "Modules/ModuleManager.h"
#endif
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_CallFunction.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Composite.h"
#include "Blueprint/WidgetNavigation.h"

#define LOCTEXT_NAMESPACE "UMG"

FWidgetBlueprintDelegates::FGetAssetTags FWidgetBlueprintDelegates::GetAssetTags;

FEditorPropertyPathSegment::FEditorPropertyPathSegment()
	: Struct(nullptr)
	, MemberName(NAME_None)
	, MemberGuid()
	, IsProperty(true)
{
}

FEditorPropertyPathSegment::FEditorPropertyPathSegment(const UProperty* InProperty)
{
	IsProperty = true;
	MemberName = InProperty->GetFName();
	if ( InProperty->GetOwnerStruct() )
	{
		Struct = InProperty->GetOwnerStruct();
		MemberGuid = FStructureEditorUtils::GetGuidForProperty(InProperty);
	}
	else if ( InProperty->GetOwnerClass() )
	{
		Struct = InProperty->GetOwnerClass();
		UBlueprint::GetGuidFromClassByFieldName<UProperty>(InProperty->GetOwnerClass(), InProperty->GetFName(), MemberGuid);
	}
	else
	{
		// Should not be possible to hit.
		check(false);
	}
}

FEditorPropertyPathSegment::FEditorPropertyPathSegment(const UFunction* InFunction)
{
	IsProperty = false;
	MemberName = InFunction->GetFName();
	if ( InFunction->GetOwnerClass() )
	{
		Struct = InFunction->GetOwnerClass();
		UBlueprint::GetGuidFromClassByFieldName<UFunction>(InFunction->GetOwnerClass(), InFunction->GetFName(), MemberGuid);
	}
	else
	{
		// Should not be possible to hit.
		check(false);
	}
}

FEditorPropertyPathSegment::FEditorPropertyPathSegment(const UEdGraph* InFunctionGraph)
{
	IsProperty = false;
	MemberName = InFunctionGraph->GetFName();
	UBlueprint* Blueprint = CastChecked<UBlueprint>(InFunctionGraph->GetOuter());
	Struct = Blueprint->GeneratedClass;
	check(Struct);
	MemberGuid = InFunctionGraph->GraphGuid;
}

void FEditorPropertyPathSegment::Rebase(UBlueprint* SegmentBase)
{
	Struct = SegmentBase->GeneratedClass;
}

bool FEditorPropertyPathSegment::ValidateMember(UDelegateProperty* DelegateProperty, FText& OutError) const
{
	// We may be binding to a function that doesn't have a explicit binder system that can handle it.  In that case
	// check to see if the function signatures are compatible, if it is, even if we don't have a binder we can just
	// directly bind the function to the delegate.
	if ( UFunction* Function = Cast<UFunction>(GetMember()) )
	{
		// Check the signatures to ensure these functions match.
		if ( Function->IsSignatureCompatibleWith(DelegateProperty->SignatureFunction, UFunction::GetDefaultIgnoredSignatureCompatibilityFlags() | CPF_ReturnParm) )
		{
			return true;
		}
	}

	// Next check to see if we have a binder suitable for handling this case.
	if ( DelegateProperty->SignatureFunction->NumParms == 1 )
	{
		if ( UProperty* ReturnProperty = DelegateProperty->SignatureFunction->GetReturnProperty() )
		{
			// TODO I don't like having the path segment system needing to have knowledge of the binding layer.
			// think about divorcing the two.

			// Find the binder that can handle the delegate return type.
			TSubclassOf<UPropertyBinding> Binder = UWidget::FindBinderClassForDestination(ReturnProperty);
			if ( Binder == nullptr )
			{
				OutError = FText::Format(LOCTEXT("Binding_Binder_NotFound", "Member:{0}: No binding exists for {1}."),
					GetMemberDisplayText(),
					ReturnProperty->GetClass()->GetDisplayNameText());
				return false;
			}

			if ( UField* Field = GetMember() )
			{
				if ( UProperty* Property = Cast<UProperty>(Field) )
				{
					// Ensure that the binder also can handle binding from the property we care about.
					if ( Binder->GetDefaultObject<UPropertyBinding>()->IsSupportedSource(Property) )
					{
						return true;
					}
					else
					{
						OutError = FText::Format(LOCTEXT("Binding_UnsupportedType_Property", "Member:{0} Unable to bind {1}, unsupported type."),
							GetMemberDisplayText(),
							Property->GetClass()->GetDisplayNameText());
						return false;
					}
				}
				else if ( UFunction* Function = Cast<UFunction>(Field) )
				{
					if ( Function->NumParms == 1 )
					{
						if ( Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) )
						{
							if ( UProperty* MemberReturn = Function->GetReturnProperty() )
							{
								// Ensure that the binder also can handle binding from the property we care about.
								if ( Binder->GetDefaultObject<UPropertyBinding>()->IsSupportedSource(MemberReturn) )
								{
									return true;
								}
								else
								{
									OutError = FText::Format(LOCTEXT("Binding_UnsupportedType_Function", "Member:{0} Unable to bind {1}, unsupported type."),
										GetMemberDisplayText(),
										MemberReturn->GetClass()->GetDisplayNameText());
									return false;
								}
							}
							else
							{
								OutError = FText::Format(LOCTEXT("Binding_NoReturn", "Member:{0} Has no return value, unable to bind."),
									GetMemberDisplayText());
								return false;
							}
						}
						else
						{
							OutError = FText::Format(LOCTEXT("Binding_Pure", "Member:{0} Unable to bind, the function is not marked as pure."),
								GetMemberDisplayText());
							return false;
						}
					}
					else
					{
						OutError = FText::Format(LOCTEXT("Binding_NumArgs", "Member:{0} Has the wrong number of arguments, it needs to return 1 value and take no parameters."),
							GetMemberDisplayText());
						return false;
					}
				}
			}
		}
	}

	OutError = LOCTEXT("Binding_UnknownError", "Unknown Error");

	return false;
}

UField* FEditorPropertyPathSegment::GetMember() const
{
	FName FieldName = GetMemberName();
	if ( FieldName != NAME_None )
	{
		UField* Field = FindField<UField>(Struct, FieldName);
		//if ( Field == nullptr )
		//{
		//	if ( UClass* Class = Cast<UClass>(Struct) )
		//	{
		//		if ( UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy) )
		//		{
		//			if ( UClass* SkeletonClass = Blueprint->SkeletonGeneratedClass )
		//			{
		//				Field = FindField<UField>(SkeletonClass, FieldName);
		//			}
		//		}
		//	}
		//}

		return Field;
	}

	return nullptr;
}

FName FEditorPropertyPathSegment::GetMemberName() const
{
	if ( MemberGuid.IsValid() )
	{
		FName NameFromGuid = NAME_None;

		if ( UClass* Class = Cast<UClass>(Struct) )
		{
			if ( UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy) )
			{
				if ( IsProperty )
				{
					NameFromGuid = UBlueprint::GetFieldNameFromClassByGuid<UProperty>(Class, MemberGuid);
				}
				else
				{
					NameFromGuid = UBlueprint::GetFieldNameFromClassByGuid<UFunction>(Class, MemberGuid);
				}
			}
		}
		else if ( UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct) )
		{
			if ( UProperty* Property = FStructureEditorUtils::GetPropertyByGuid(UserStruct, MemberGuid) )
			{
				NameFromGuid = Property->GetFName();
			}
		}

		//check(NameFromGuid != NAME_None);
		return NameFromGuid;
	}
	
	//check(MemberName != NAME_None);
	return MemberName;
}

FText FEditorPropertyPathSegment::GetMemberDisplayText() const
{
	if ( MemberGuid.IsValid() )
	{
		if ( UClass* Class = Cast<UClass>(Struct) )
		{
			if ( UBlueprint* Blueprint = Cast<UBlueprint>(Class->ClassGeneratedBy) )
			{
				if ( IsProperty )
				{
					return FText::FromName(UBlueprint::GetFieldNameFromClassByGuid<UProperty>(Class, MemberGuid));
				}
				else
				{
					return FText::FromName(UBlueprint::GetFieldNameFromClassByGuid<UFunction>(Class, MemberGuid));
				}
			}
		}
		else if ( UUserDefinedStruct* UserStruct = Cast<UUserDefinedStruct>(Struct) )
		{
			if ( UProperty* Property = FStructureEditorUtils::GetPropertyByGuid(UserStruct, MemberGuid) )
			{
				return Property->GetDisplayNameText();
			}
		}
	}

	return FText::FromName(MemberName);
}

FGuid FEditorPropertyPathSegment::GetMemberGuid() const
{
	return MemberGuid;
}

FEditorPropertyPath::FEditorPropertyPath()
{
}

FEditorPropertyPath::FEditorPropertyPath(const TArray<UField*>& BindingChain)
{
	for ( const UField* Field : BindingChain )
	{
		if ( const UProperty* Property = Cast<UProperty>(Field) )
		{
			Segments.Add(FEditorPropertyPathSegment(Property));
		}
		else if ( const UFunction* Function = Cast<UFunction>(Field) )
		{
			Segments.Add(FEditorPropertyPathSegment(Function));
		}
		else
		{
			// Should never happen
			check(false);
		}
	}
}

bool FEditorPropertyPath::Rebase(UBlueprint* SegmentBase)
{
	if ( !IsEmpty() )
	{
		Segments[0].Rebase(SegmentBase);
		return true;
	}

	return false;
}

bool FEditorPropertyPath::Validate(UDelegateProperty* Destination, FText& OutError) const
{
	if ( IsEmpty() )
	{
		OutError = LOCTEXT("Binding_Empty", "The binding is empty.");
		return false;
	}

	for ( int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); SegmentIndex++ )
	{
		const FEditorPropertyPathSegment& Segment = Segments[SegmentIndex];
		if ( UStruct* OwnerStruct = Segment.GetStruct() )
		{
			if ( Segment.GetMember() == nullptr )
			{
				OutError = FText::Format(LOCTEXT("Binding_MemberNotFound", "Binding: '{0}' : '{1}' was not found on '{2}'."),
					GetDisplayText(),
					Segment.GetMemberDisplayText(),
					OwnerStruct->GetDisplayNameText());

				return false;
			}
		}
		else
		{
			OutError = FText::Format(LOCTEXT("Binding_StructNotFound", "Binding: '{0}' : Unable to locate owner class or struct for '{1}'"),
				GetDisplayText(),
				Segment.GetMemberDisplayText());

			return false;
		}
	}

	// Validate the last member in the segment
	const FEditorPropertyPathSegment& LastSegment = Segments[Segments.Num() - 1];
	return LastSegment.ValidateMember(Destination, OutError);
}

FText FEditorPropertyPath::GetDisplayText() const
{
	FString DisplayText;

	for ( int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); SegmentIndex++ )
	{
		const FEditorPropertyPathSegment& Segment = Segments[SegmentIndex];
		DisplayText.Append(Segment.GetMemberDisplayText().ToString());
		if ( SegmentIndex < ( Segments.Num() - 1 ) )
		{
			DisplayText.Append(TEXT("."));
		}
	}

	return FText::FromString(DisplayText);
}

FDynamicPropertyPath FEditorPropertyPath::ToPropertyPath() const
{
	TArray<FString> PropertyChain;

	for ( const FEditorPropertyPathSegment& Segment : Segments )
	{
		FName SegmentName = Segment.GetMemberName();

		if ( SegmentName != NAME_None )
		{
			PropertyChain.Add(SegmentName.ToString());
		}
		else
		{
			return FDynamicPropertyPath();
		}
	}

	return FDynamicPropertyPath(PropertyChain);
}

bool FDelegateEditorBinding::IsAttributePropertyBinding(UWidgetBlueprint* Blueprint) const
{
	// First find the target widget we'll be attaching the binding to.
	if (UWidget* TargetWidget = Blueprint->WidgetTree->FindWidget(FName(*ObjectName)))
	{
		// Next find the underlying delegate we're actually binding to, if it's an event the name will be the same,
		// for properties we need to lookup the delegate property we're actually going to be binding to.
		UDelegateProperty* BindableProperty = FindField<UDelegateProperty>(TargetWidget->GetClass(), FName(*(PropertyName.ToString() + TEXT("Delegate"))));
		return BindableProperty != nullptr;
	}

	return false;
}

bool FDelegateEditorBinding::DoesBindingTargetExist(UWidgetBlueprint* Blueprint) const
{
	// First find the target widget we'll be attaching the binding to.
	if (UWidget* TargetWidget = Blueprint->WidgetTree->FindWidget(FName(*ObjectName)))
	{
		return true;
	}

	return false;
}

bool FDelegateEditorBinding::IsBindingValid(UClass* BlueprintGeneratedClass, UWidgetBlueprint* Blueprint, FCompilerResultsLog& MessageLog) const
{
	FDelegateRuntimeBinding RuntimeBinding = ToRuntimeBinding(Blueprint);

	// First find the target widget we'll be attaching the binding to.
	if ( UWidget* TargetWidget = Blueprint->WidgetTree->FindWidget(FName(*ObjectName)) )
	{
		// Next find the underlying delegate we're actually binding to, if it's an event the name will be the same,
		// for properties we need to lookup the delegate property we're actually going to be binding to.
		UDelegateProperty* BindableProperty = FindField<UDelegateProperty>(TargetWidget->GetClass(), FName(*( PropertyName.ToString() + TEXT("Delegate") )));
		UDelegateProperty* EventProperty = FindField<UDelegateProperty>(TargetWidget->GetClass(), PropertyName);

		bool bNeedsToBePure = BindableProperty ? true : false;
		UDelegateProperty* DelegateProperty = BindableProperty ? BindableProperty : EventProperty;

		// Locate the delegate property on the widget that's a delegate for a property we want to bind.
		if ( DelegateProperty )
		{
			if ( !SourcePath.IsEmpty() )
			{
				FText ValidationError;
				if ( SourcePath.Validate(DelegateProperty, ValidationError) == false )
				{
					MessageLog.Error(
						*FText::Format(
							LOCTEXT("BindingErrorFmt", "Binding: Property '@@' on Widget '@@': {0}"),
							ValidationError
						).ToString(),
						DelegateProperty,
						TargetWidget
					);

					return false;
				}

				return true;
			}
			else
			{
				// On our incoming blueprint generated class, try and find the function we claim exists that users
				// are binding their property too.
				if ( UFunction* Function = BlueprintGeneratedClass->FindFunctionByName(RuntimeBinding.FunctionName, EIncludeSuperFlag::IncludeSuper) )
				{
					// Check the signatures to ensure these functions match.
					if ( Function->IsSignatureCompatibleWith(DelegateProperty->SignatureFunction, UFunction::GetDefaultIgnoredSignatureCompatibilityFlags() | CPF_ReturnParm) )
					{
						// Only allow binding pure functions to property bindings.
						if ( bNeedsToBePure && !Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) )
						{
							FText const ErrorFormat = LOCTEXT("BindingNotBoundToPure", "Binding: property '@@' on widget '@@' needs to be bound to a pure function, '@@' is not pure.");
							MessageLog.Error(*ErrorFormat.ToString(), DelegateProperty, TargetWidget, Function);

							return false;
						}

						return true;
					}
					else
					{
						FText const ErrorFormat = LOCTEXT("BindingFunctionSigDontMatch", "Binding: property '@@' on widget '@@' bound to function '@@', but the sigatnures don't match.  The function must return the same type as the property and have no parameters.");
						MessageLog.Error(*ErrorFormat.ToString(), DelegateProperty, TargetWidget, Function);
					}
				}
				else
				{
					// Bindable property removed.
				}
			}
		}
		else
		{
			// Bindable Property Removed
		}
	}
	else
	{
		// Ignore missing widgets
	}

	return false;
}

FDelegateRuntimeBinding FDelegateEditorBinding::ToRuntimeBinding(UWidgetBlueprint* Blueprint) const
{
	FDelegateRuntimeBinding Binding;
	Binding.ObjectName = ObjectName;
	Binding.PropertyName = PropertyName;
	if ( Kind == EBindingKind::Function )
	{
		Binding.FunctionName = ( MemberGuid.IsValid() ) ? Blueprint->GetFieldNameFromClassByGuid<UFunction>(Blueprint->SkeletonGeneratedClass, MemberGuid) : FunctionName;
	}
	else
	{
		Binding.FunctionName = FunctionName;
	}
	Binding.Kind = Kind;
	Binding.SourcePath = SourcePath.ToPropertyPath();

	return Binding;
}

bool FWidgetAnimation_DEPRECATED::SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot)
{
	static FName AnimationDataName("AnimationData");
	if(Tag.Type == NAME_StructProperty && Tag.Name == AnimationDataName)
	{
		FStructuredArchive::FRecord Record = Slot.EnterRecord();
		Record << NAMED_FIELD(MovieScene);
		Record << NAMED_FIELD(AnimationBindings);
		return true;
	}

	return false;
}
/////////////////////////////////////////////////////
// UWidgetBlueprint

UWidgetBlueprint::UWidgetBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SupportDynamicCreation(EWidgetSupportsDynamicCreation::Default)
	, TickFrequency(EWidgetTickFrequency::Auto)
{
}

void UWidgetBlueprint::ReplaceDeprecatedNodes()
{
	if (GetLinkerCustomVersion(FFortniteMainBranchObjectVersion::GUID) < FFortniteMainBranchObjectVersion::WidgetStopDuplicatingAnimations)
	{
		// Update old graphs to all use the widget graph schema.
		TArray<UEdGraph*> Graphs;
		GetAllGraphs(Graphs);

		for (UEdGraph* Graph : Graphs)
		{
			Graph->Schema = UWidgetGraphSchema::StaticClass();
		}
	}

	Super::ReplaceDeprecatedNodes();
}

#if WITH_EDITORONLY_DATA
void UWidgetBlueprint::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);
}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA

void UWidgetBlueprint::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FWidgetBlueprintDelegates::GetAssetTags.Broadcast(this, OutTags);
}

void UWidgetBlueprint::NotifyGraphRenamed(class UEdGraph* Graph, FName OldName, FName NewName)
{
	Super::NotifyGraphRenamed(Graph, OldName, NewName);
	
	// Update any explicit widget bindings.
	WidgetTree->ForEachWidget([OldName, NewName](UWidget* Widget) {
		if (Widget->Navigation)
		{
			Widget->Navigation->SetFlags(RF_Transactional);
			Widget->Navigation->Modify();
			Widget->Navigation->TryToRenameBinding(OldName, NewName);
		}
	});
}

#endif

void UWidgetBlueprint::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
}

void UWidgetBlueprint::PostLoad()
{
	Super::PostLoad();

	WidgetTree->ForEachWidget([&] (UWidget* Widget) {
		Widget->ConnectEditorData();
	});

	if( GetLinkerUE4Version() < VER_UE4_FIXUP_WIDGET_ANIMATION_CLASS )
	{
		// Fixup widget animations.
		for( auto& OldAnim : AnimationData_DEPRECATED )
		{
			FName AnimName = OldAnim.MovieScene->GetFName();

			// Rename the old movie scene so we can reuse the name
			OldAnim.MovieScene->Rename( *MakeUniqueObjectName( this, UMovieScene::StaticClass(), "MovieScene").ToString(), nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);

			UWidgetAnimation* NewAnimation = NewObject<UWidgetAnimation>(this, AnimName, RF_Transactional);

			OldAnim.MovieScene->Rename(*AnimName.ToString(), NewAnimation, REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional );

			NewAnimation->MovieScene = OldAnim.MovieScene;
			NewAnimation->AnimationBindings = OldAnim.AnimationBindings;
			
			Animations.Add( NewAnimation );
		}	

		AnimationData_DEPRECATED.Empty();
	}

	if ( GetLinkerUE4Version() < VER_UE4_RENAME_WIDGET_VISIBILITY )
	{
		static const FName Visiblity(TEXT("Visiblity"));
		static const FName Visibility(TEXT("Visibility"));

		for ( FDelegateEditorBinding& Binding : Bindings )
		{
			if ( Binding.PropertyName == Visiblity )
			{
				Binding.PropertyName = Visibility;
			}
		}
	}

	if ( GetLinkerCustomVersion(FEditorObjectVersion::GUID) < FEditorObjectVersion::WidgetGraphSchema )
	{
		// Update old graphs to all use the widget graph schema.
		TArray<UEdGraph*> Graphs;
		GetAllGraphs(Graphs);

		for ( UEdGraph* Graph : Graphs )
		{
			Graph->Schema = UWidgetGraphSchema::StaticClass();
		}
	}
}

void UWidgetBlueprint::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if ( !bDuplicatingReadOnly )
	{
		// We need to update all the bindings and change each bindings first segment in the path
		// to be the new class this blueprint generates, as all bindings must first originate on 
		// the widget blueprint, the first segment is always a reference to 'self'.
		for ( FDelegateEditorBinding& Binding : Bindings )
		{
			Binding.SourcePath.Rebase(this);
		}
	}
}

UClass* UWidgetBlueprint::GetBlueprintClass() const
{
	return UWidgetBlueprintGeneratedClass::StaticClass();
}

bool UWidgetBlueprint::AllowsDynamicBinding() const
{
	return true;
}

void UWidgetBlueprint::GatherDependencies(TSet<TWeakObjectPtr<UBlueprint>>& InDependencies) const
{
	Super::GatherDependencies(InDependencies);

	if ( WidgetTree )
	{
		WidgetTree->ForEachWidget([&] (UWidget* Widget) {
			if ( UBlueprint* WidgetBlueprint = UBlueprint::GetBlueprintFromClass(Widget->GetClass()) )
			{
				bool bWasAlreadyInSet;
				InDependencies.Add(WidgetBlueprint, &bWasAlreadyInSet);

				if ( !bWasAlreadyInSet )
				{
					WidgetBlueprint->GatherDependencies(InDependencies);
				}
			}
		});
	}
}

bool UWidgetBlueprint::ValidateGeneratedClass(const UClass* InClass)
{
	const UWidgetBlueprintGeneratedClass* GeneratedClass = Cast<const UWidgetBlueprintGeneratedClass>(InClass);
	if ( !ensure(GeneratedClass) )
	{
		return false;
	}
	const UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(GetBlueprintFromClass(GeneratedClass));
	if ( !ensure(Blueprint) )
	{
		return false;
	}

	if ( !ensure(Blueprint->WidgetTree && ( Blueprint->WidgetTree->GetOuter() == Blueprint )) )
	{
		return false;
	}
	else
	{
		TArray < UWidget* > AllWidgets;
		Blueprint->WidgetTree->GetAllWidgets(AllWidgets);
		for ( UWidget* Widget : AllWidgets )
		{
			if ( !ensure(Widget->GetOuter() == Blueprint->WidgetTree) )
			{
				return false;
			}
		}
	}

	if ( !ensure(GeneratedClass->WidgetTree && ( GeneratedClass->WidgetTree->GetOuter() == GeneratedClass )) )
	{
		return false;
	}
	else
	{
		TArray < UWidget* > AllWidgets;
		GeneratedClass->WidgetTree->GetAllWidgets(AllWidgets);
		for ( UWidget* Widget : AllWidgets )
		{
			if ( !ensure(Widget->GetOuter() == GeneratedClass->WidgetTree) )
			{
				return false;
			}
		}
	}

	return true;
}

TSharedPtr<FKismetCompilerContext> UWidgetBlueprint::GetCompilerForWidgetBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FWidgetBlueprintCompilerContext(CastChecked<UWidgetBlueprint>(BP), InMessageLog, InCompileOptions));
}

void UWidgetBlueprint::GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Add( UUserWidget::StaticClass() );
}

bool UWidgetBlueprint::IsWidgetFreeFromCircularReferences(UUserWidget* UserWidget) const
{
	if (UserWidget != nullptr)
	{
		if (UserWidget->GetClass() == GeneratedClass)
		{
			// If this user widget is the same as the blueprint's generated class, we should reject it because it
			// will cause a circular reference within the blueprint.
			return false;
		}
		else if (UWidgetBlueprint* GeneratedByBlueprint = Cast<UWidgetBlueprint>(UserWidget->WidgetGeneratedBy))
		{
			// Check the generated by blueprints - this will catch even cases where one has the other in the widget tree but hasn't compiled yet
			if (GeneratedByBlueprint->WidgetTree && GeneratedByBlueprint->WidgetTree->RootWidget)
			{
				TArray<UWidget*> ChildWidgets;
				GeneratedByBlueprint->WidgetTree->GetChildWidgets(GeneratedByBlueprint->WidgetTree->RootWidget, ChildWidgets);
				for (UWidget* ChildWidget : ChildWidgets)
				{
					if (UWidgetBlueprint* ChildGeneratedBlueprint = Cast<UWidgetBlueprint>(ChildWidget->WidgetGeneratedBy))
					{
						if (this == ChildGeneratedBlueprint)
						{
							return false;
						}
					}
				}
			}
		}
		else if (UserWidget->WidgetTree)
		{
			// This loop checks for references that existed in the compiled blueprint, in case it's changed since then
			TArray<UWidget*> ChildWidgets;
			UserWidget->WidgetTree->GetAllWidgets(ChildWidgets);

			for (UWidget* Widget : ChildWidgets)
			{
				if (Cast<UUserWidget>(Widget) != nullptr)
				{
					if ( !IsWidgetFreeFromCircularReferences(Cast<UUserWidget>(Widget)) )
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

UPackage* UWidgetBlueprint::GetWidgetTemplatePackage() const
{
	return GetOutermost();
}

static bool HasLatentActions(UEdGraph* Graph)
{
	for (const UEdGraphNode* Node : Graph->Nodes)
	{
		if (const UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node))
		{
			// Check any function call nodes to see if they are latent.
			UFunction* TargetFunction = CallFunctionNode->GetTargetFunction();
			if (TargetFunction && TargetFunction->HasMetaData(FBlueprintMetadata::MD_Latent))
			{
				return true;
			}
		}

		else if (const UK2Node_MacroInstance* MacroInstanceNode = Cast<UK2Node_MacroInstance>(Node))
		{
			// Any macro graphs that haven't already been checked need to be checked for latent function calls
			//if (InspectedGraphList.Find(MacroInstanceNode->GetMacroGraph()) == INDEX_NONE)
			{
				if (HasLatentActions(MacroInstanceNode->GetMacroGraph()))
				{
					return true;
				}
			}
		}
		else if (const UK2Node_Composite* CompositeNode = Cast<UK2Node_Composite>(Node))
		{
			// Any collapsed graphs that haven't already been checked need to be checked for latent function calls
			//if (InspectedGraphList.Find(CompositeNode->BoundGraph) == INDEX_NONE)
			{
				if (HasLatentActions(CompositeNode->BoundGraph))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void UWidgetBlueprint::UpdateTickabilityStats(bool& OutHasLatentActions, bool& OutHasAnimations, bool& OutClassRequiresNativeTick)
{
	if (GeneratedClass && GeneratedClass->ClassConstructor)
	{
		UWidgetBlueprintGeneratedClass* WidgetBPGeneratedClass = CastChecked<UWidgetBlueprintGeneratedClass>(GeneratedClass);
		UUserWidget* DefaultWidget = WidgetBPGeneratedClass->GetDefaultObject<UUserWidget>();

		TArray<UBlueprint*> BlueprintParents;
		UBlueprint::GetBlueprintHierarchyFromClass(WidgetBPGeneratedClass, BlueprintParents);

		bool bHasLatentActions = false;
		bool bHasAnimations = false;
		const bool bHasScriptImplementedTick = DefaultWidget->bHasScriptImplementedTick;

		for (UBlueprint* Blueprint : BlueprintParents)
		{
			UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(Blueprint);
			if (WidgetBP)
			{
				bHasAnimations |= WidgetBP->Animations.Num() > 0;

				if (!bHasLatentActions)
				{
					TArray<UEdGraph*> AllGraphs;
					WidgetBP->GetAllGraphs(AllGraphs);

					for (UEdGraph* Graph : AllGraphs)
					{
						if (HasLatentActions(Graph))
						{
							bHasLatentActions = true;
							break;
						}
					}
				}
			}
		}

		UClass* NativeParent = FBlueprintEditorUtils::GetNativeParent(this);
		static const FName DisableNativeTickMetaTag("DisableNativeTick");
		const bool bClassRequiresNativeTick = !NativeParent->HasMetaData(DisableNativeTickMetaTag);

		TickFrequency = DefaultWidget->GetDesiredTickFrequency();
		TickPredictionReason = TEXT("");
		TickPrediction = EWidgetCompileTimeTickPrediction::WontTick;
		switch (TickFrequency)
		{
		case EWidgetTickFrequency::Never:
			TickPrediction = EWidgetCompileTimeTickPrediction::WontTick;
			break;
		case EWidgetTickFrequency::Auto:
		{
			TArray<FString> Reasons;
			if (bHasScriptImplementedTick)
			{
				Reasons.Add(TEXT("Script"));
			}

			if (bClassRequiresNativeTick)
			{
				Reasons.Add(TEXT("Native"));
			}

			if (bHasAnimations)
			{
				Reasons.Add(TEXT("Anim"));
			}

			if (bHasLatentActions)
			{
				Reasons.Add(TEXT("Latent"));
			}

			for (int32 ReasonIdx = 0; ReasonIdx < Reasons.Num(); ++ReasonIdx)
			{
				TickPredictionReason += Reasons[ReasonIdx];
				if (ReasonIdx != Reasons.Num() - 1)
				{
					TickPredictionReason.AppendChar('|');
				}
			}

			if (bHasScriptImplementedTick || bClassRequiresNativeTick)
			{
				// Widget has an implemented tick or the generated class is not a direct child of UUserWidget (means it could have a native tick) then it will definitely tick
				TickPrediction = EWidgetCompileTimeTickPrediction::WillTick;
			}
			else if (bHasAnimations || bHasLatentActions)
			{
				// Widget has latent actions or animations and will tick if these are triggered
				TickPrediction = EWidgetCompileTimeTickPrediction::OnDemand;
			}
		}
		break;
		}

		OutHasLatentActions = bHasLatentActions;
		OutHasAnimations = bHasAnimations;
		OutClassRequiresNativeTick = bClassRequiresNativeTick;
	}
}

bool UWidgetBlueprint::WidgetSupportsDynamicCreation() const
{
	switch (SupportDynamicCreation)
	{
	case EWidgetSupportsDynamicCreation::Yes:
		return true;
	case EWidgetSupportsDynamicCreation::No:
		return false;
	case EWidgetSupportsDynamicCreation::Default:
	default:
		return GetDefault<UUMGEditorProjectSettings>()->CompilerOption_SupportsDynamicCreation(this);
	}
}

bool UWidgetBlueprint::ArePropertyBindingsAllowed() const
{
	return GetDefault<UUMGEditorProjectSettings>()->CompilerOption_PropertyBindingRule(this) == EPropertyBindingPermissionLevel::Allow;
}

#if WITH_EDITOR
void UWidgetBlueprint::LoadModulesRequiredForCompilation()
{
	static const FName ModuleName(TEXT("UMGEditor"));
	FModuleManager::Get().LoadModule(ModuleName);
}
#endif // WITH_EDITOR
#undef LOCTEXT_NAMESPACE 
