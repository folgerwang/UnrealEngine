// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UnrealType.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetTree.h"
#include "Evaluation/MovieSceneSegment.h"
#include "Evaluation/MovieSceneTrackImplementation.h"
#include "Evaluation/MovieSceneEvalTemplate.h"
#include "Compilation/MovieSceneCompiler.h"

void FBackendHelperUMG::WidgetFunctionsInHeader(FEmitterLocalContext& Context)
{
	if (Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s(TArray<FName>& SlotNames) const override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
		Context.Header.AddLine(FString::Printf(TEXT("virtual void %s(const class ITargetPlatform* TargetPlatform) override;"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, PreSave)));
		Context.Header.AddLine(TEXT("virtual void InitializeNativeClassData() override;"));
	}
}

void FBackendHelperUMG::AdditionalHeaderIncludeForWidget(FEmitterLocalContext& Context)
{
	if (!Context.NativizationOptions.bExcludeMonolithicHeaders
		&& Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(TEXT("#include \"Runtime/UMG/Public/UMG.h\""));
	}
}

void FBackendHelperUMG::CreateClassSubobjects(FEmitterLocalContext& Context, bool bCreate, bool bInitialize)
{
	if (UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		// Currently nativization does not support widget templates. This method will need to be revised if that changes.
		check(!WidgetClass->HasTemplate());
		
		// Child widgets may actually use the widget tree from a parent class.
		// - @see UUserWidget::Initialize()
		WidgetClass = WidgetClass->FindWidgetTreeOwningClass();

		// Initialize the WidgetTree only if it's owned by the current widget class.
		if (WidgetClass == Context.GetCurrentlyGeneratedClass())
		{
			if (WidgetClass->WidgetTree)
			{
				ensure(WidgetClass->WidgetTree->GetOuter() == Context.GetCurrentlyGeneratedClass());
				FEmitDefaultValueHelper::HandleClassSubobject(Context, WidgetClass->WidgetTree, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, bCreate, bInitialize);
			}

			for (UWidgetAnimation* Anim : WidgetClass->Animations)
			{
				ensure(Anim->GetOuter() == Context.GetCurrentlyGeneratedClass());

				// We need the same regeneration like for cooking. See UMovieSceneSequence::Serialize
				FMovieSceneSequencePrecompiledTemplateStore Store;
				FMovieSceneCompiler::Compile(*Anim, Store);

				FEmitDefaultValueHelper::HandleClassSubobject(Context, Anim, FEmitterLocalContext::EClassSubobjectList::MiscConvertedSubobjects, bCreate, bInitialize);
			}
		}
	}
}

void FBackendHelperUMG::EmitWidgetInitializationFunctions(FEmitterLocalContext& Context)
{
	if (UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.ResetPropertiesForInaccessibleStructs();

		const FString CppClassName = FEmitHelper::GetCppName(WidgetClass);

		auto GenerateLocalProperty = [](FEmitterLocalContext& InContext, UProperty* InProperty, const uint8* DataPtr) -> FString
		{
			check(InProperty && DataPtr);
			const FString NativeName = InContext.GenerateUniqueLocalName();
			
			const uint32 CppTemplateTypeFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst | EPropertyExportCPPFlags::CPPF_NoRef;
			const FString Target = InContext.ExportCppDeclaration(InProperty, EExportedDeclaration::Local, CppTemplateTypeFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);

			InContext.AddLine(FString::Printf(TEXT("%s %s;"), *Target, *NativeName));
			FEmitDefaultValueHelper::InnerGenerate(InContext, InProperty, NativeName, DataPtr, nullptr, true);
			return NativeName;
		};

		{	// GetSlotNames
			Context.AddLine(FString::Printf(TEXT("void %s::%s(TArray<FName>& SlotNames) const"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();

			const FString LocalNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("NamedSlots")), reinterpret_cast<const uint8*>(&WidgetClass->NamedSlots));
			Context.AddLine(FString::Printf(TEXT("SlotNames.Append(%s);"), *LocalNativeName));

			Context.DecreaseIndent();
			Context.AddLine(TEXT("}"));
		}

		{	// InitializeNativeClassData
			Context.AddLine(FString::Printf(TEXT("void %s::InitializeNativeClassData()"), *CppClassName));
			Context.AddLine(TEXT("{"));
			Context.IncreaseIndent();

			// Child widgets may actually use the widget tree from a parent class.
			// - @see UUserWidget::Initialize()
			UWidgetBlueprintGeneratedClass* WidgetTreeOwningClass = WidgetClass->FindWidgetTreeOwningClass();

			// If we have a valid WidgetTree instance, emit code to initialize the widget using the owning class.
			if (WidgetTreeOwningClass->WidgetTree != nullptr)
			{
				FString WidgetClassStr;
				FString WidgetTreeStr;

				if (WidgetClass == WidgetTreeOwningClass)
				{
					// Simple case - WidgetTree instance is owned by the current class.
					WidgetClassStr = TEXT("GetClass()");

					// This object was already created as a class-owned subobject and mapped to the 'WidgetTree' value.
					// - @see CreateClassSubobjects()
					WidgetTreeStr = Context.FindGloballyMappedObject(WidgetClass->WidgetTree, UWidgetTree::StaticClass());
				}
				else
				{
					// Emit code to assign the owning class to a local variable.
					WidgetClassStr = Context.GenerateUniqueLocalName();
					Context.AddLine(FString::Printf(TEXT("UClass* %s = %s;"),
						*WidgetClassStr,
						*Context.FindGloballyMappedObject(WidgetTreeOwningClass, UClass::StaticClass(), true)));

					// Emit code to locate and assign the owning class's WidgetTree instance to a local variable. This will have been created as part of the owning class's ctor, but note
					// that we have to look it up by name/outer because the converted class is a UDynamicClass and not a UWidgetBlueprintGeneratedClass, so there is no 'WidgetTree' member.
					WidgetTreeStr = Context.GenerateUniqueLocalName();
					Context.AddLine(FString::Printf(TEXT("UWidgetTree* %s = CastChecked<UWidgetTree>(StaticFindObjectFast(UWidgetTree::StaticClass(), %s, TEXT(\"WidgetTree\")));"),
						*WidgetTreeStr,
						*WidgetClassStr));
				}

				ensure(!WidgetTreeStr.IsEmpty());
				ensure(!WidgetClassStr.IsEmpty());

				const FString AnimationsArrayNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("Animations")), reinterpret_cast<const uint8*>(&WidgetTreeOwningClass->Animations));
				const FString BindingsArrayNativeName = GenerateLocalProperty(Context, FindFieldChecked<UArrayProperty>(UWidgetBlueprintGeneratedClass::StaticClass(), TEXT("Bindings")), reinterpret_cast<const uint8*>(&WidgetTreeOwningClass->Bindings));

				Context.AddLine(FString::Printf(TEXT("UWidgetBlueprintGeneratedClass::%s(this, %s, %s, %s, %s, %s, %s);")
					, GET_FUNCTION_NAME_STRING_CHECKED(UWidgetBlueprintGeneratedClass, InitializeWidgetStatic)
					, *WidgetClassStr
					, WidgetTreeOwningClass->HasTemplate() ? TEXT("true") : TEXT("false")
					, WidgetTreeOwningClass->bAllowDynamicCreation ? TEXT("true") : TEXT("false")
					, *WidgetTreeStr
					, *AnimationsArrayNativeName
					, *BindingsArrayNativeName));
			}

			Context.DecreaseIndent();
			Context.AddLine(TEXT("}"));
		}

		// PreSave
		Context.AddLine(FString::Printf(TEXT("void %s::%s(const class ITargetPlatform* TargetPlatform)"), *CppClassName, GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, PreSave)));
		Context.AddLine(TEXT("{"));
		Context.IncreaseIndent();
		Context.AddLine(FString::Printf(TEXT("Super::%s(TargetPlatform);"), GET_FUNCTION_NAME_STRING_CHECKED(UObject, PreSave)));
		Context.AddLine(TEXT("TArray<FName> LocalNamedSlots;"));
		Context.AddLine(FString::Printf(TEXT("%s(LocalNamedSlots);"), GET_FUNCTION_NAME_STRING_CHECKED(UUserWidget, GetSlotNames)));
		Context.AddLine(TEXT("RemoveObsoleteBindings(LocalNamedSlots);")); //RemoveObsoleteBindings is protected - no check
		Context.DecreaseIndent();
		Context.AddLine(TEXT("}"));
	}
}

bool FBackendHelperUMG::SpecialStructureConstructorUMG(const UStruct* Struct, const uint8* ValuePtr, /*out*/ FString* OutResult)
{
	check(ValuePtr || !OutResult);

	auto FrameNumberRangeBoundConstructorLambda = [](const TRangeBound<FFrameNumber>& InRangeBound, const FFrameNumber& InRangeBoundValue) -> FString
	{
		if (InRangeBound.IsExclusive())
		{
			return FString::Printf(TEXT("TRangeBound<FFrameNumber>::Exclusive(%d)"), InRangeBoundValue.Value);
		}
		else if (InRangeBound.IsInclusive())
		{
			return FString::Printf(TEXT("TRangeBound<FFrameNumber>::Inclusive(%d)"), InRangeBoundValue.Value);
		}
		else
		{
			return FString::Printf(TEXT("TRangeBound<FFrameNumber>::Open()"));
		}
	};

	if (FSectionEvaluationData::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FSectionEvaluationData* SectionEvaluationData = reinterpret_cast<const FSectionEvaluationData*>(ValuePtr);
			if (SectionEvaluationData->ForcedTime == TNumericLimits<int32>::Lowest())
			{
				*OutResult = FString::Printf(TEXT("FSectionEvaluationData(%d, ESectionEvaluationFlags(0x%02x))")
					, SectionEvaluationData->ImplIndex
					, (uint8)SectionEvaluationData->Flags);
			}
			else
			{
				*OutResult = FString::Printf(TEXT("FSectionEvaluationData(%d, %d)")
					, SectionEvaluationData->ImplIndex
					, SectionEvaluationData->ForcedTime.Value);
			}
		}
		return true;
	}

	if (FMovieSceneSegment::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FMovieSceneSegment* MovieSceneSegment = reinterpret_cast<const FMovieSceneSegment*>(ValuePtr);
			FString SegmentsInitializerList;
			for (const FSectionEvaluationData& SectionEvaluationData : MovieSceneSegment->Impls)
			{
				if (!SegmentsInitializerList.IsEmpty())
				{
					SegmentsInitializerList += TEXT(", ");
				}
				FString SectionEvaluationDataStr;
				FBackendHelperUMG::SpecialStructureConstructorUMG(FSectionEvaluationData::StaticStruct()
					, reinterpret_cast<const uint8*>(&SectionEvaluationData)
					, &SectionEvaluationDataStr);
				SegmentsInitializerList += SectionEvaluationDataStr;
			}
			const FString LowerBoundStr = FrameNumberRangeBoundConstructorLambda(MovieSceneSegment->Range.GetLowerBound(),
				MovieSceneSegment->Range.GetLowerBound().IsClosed() ? MovieSceneSegment->Range.GetLowerBoundValue() : FFrameNumber());
			const FString UpperBoundStr = FrameNumberRangeBoundConstructorLambda(MovieSceneSegment->Range.GetUpperBound(),
				MovieSceneSegment->Range.GetUpperBound().IsClosed() ? MovieSceneSegment->Range.GetUpperBoundValue() : FFrameNumber());
			*OutResult = FString::Printf(TEXT("FMovieSceneSegment(TRange<FFrameNumber>(%s, %s), {%s})"), *LowerBoundStr, *UpperBoundStr, *SegmentsInitializerList);
		}
		return true;
	}

	if (FMovieSceneFrameRange::StaticStruct() == Struct)
	{
		if (OutResult)
		{
			const FMovieSceneFrameRange* MovieSceneFrameRange = reinterpret_cast<const FMovieSceneFrameRange*>(ValuePtr);
			const FString LowerBoundStr = FrameNumberRangeBoundConstructorLambda(MovieSceneFrameRange->Value.GetLowerBound(),
				MovieSceneFrameRange->Value.GetLowerBound().IsClosed() ? MovieSceneFrameRange->Value.GetLowerBoundValue() : FFrameNumber());
			const FString UpperBoundStr = FrameNumberRangeBoundConstructorLambda(MovieSceneFrameRange->Value.GetUpperBound(),
				MovieSceneFrameRange->Value.GetUpperBound().IsClosed() ? MovieSceneFrameRange->Value.GetUpperBoundValue() : FFrameNumber());
			*OutResult = FString::Printf(TEXT("FMovieSceneFrameRange(TRange<FFrameNumber>(%s, %s))"), *LowerBoundStr, *UpperBoundStr);
		}
		return true;
	}

	return false;
}

bool FBackendHelperUMG::IsTInlineStruct(UScriptStruct* OuterStruct)
{
	return (OuterStruct == FMovieSceneTrackImplementationPtr::StaticStruct())
		|| (OuterStruct == FMovieSceneEvalTemplatePtr::StaticStruct());
}

UScriptStruct* FBackendHelperUMG::InlineValueStruct(UScriptStruct* OuterStruct, const uint8* ValuePtr)
{ 
	if (OuterStruct == FMovieSceneTrackImplementationPtr::StaticStruct())
	{
		const FMovieSceneTrackImplementation* MovieSceneTrackImplementation = reinterpret_cast<const FMovieSceneTrackImplementationPtr*>(ValuePtr)->GetPtr();
		if (MovieSceneTrackImplementation)
		{
			return &(MovieSceneTrackImplementation->GetScriptStruct());
		}
	}

	if (OuterStruct == FMovieSceneEvalTemplatePtr::StaticStruct())
	{
		const FMovieSceneEvalTemplate* MovieSceneEvalTemplate = reinterpret_cast<const FMovieSceneEvalTemplatePtr*>(ValuePtr)->GetPtr();
		if (MovieSceneEvalTemplate)
		{
			return &(MovieSceneEvalTemplate->GetScriptStruct());
		}
	}
	return nullptr;
}

const uint8* FBackendHelperUMG::InlineValueData(UScriptStruct* OuterStruct, const uint8* ValuePtr)
{ 
	if (ValuePtr)
	{
		if (OuterStruct == FMovieSceneTrackImplementationPtr::StaticStruct())
		{
			return reinterpret_cast<const uint8*>(reinterpret_cast<const FMovieSceneTrackImplementationPtr*>(ValuePtr)->GetPtr());
		}
		if (OuterStruct == FMovieSceneEvalTemplatePtr::StaticStruct())
		{
			return reinterpret_cast<const uint8*>(reinterpret_cast<const FMovieSceneEvalTemplatePtr*>(ValuePtr)->GetPtr());
		}
	}
	return nullptr;
}
