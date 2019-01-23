// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.h"
#include "TestObject.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

DECLARE_DYNAMIC_DELEGATE_OneParam(FRegularDelegate, int32, SomeArgument);
DECLARE_DYNAMIC_DELEGATE_OneParam(FDelegateWithDelegateParam, FRegularDelegate const &, RegularDelegate);

struct ITestObject
{
};

USTRUCT()
struct FContainsInstancedProperty
{
	GENERATED_BODY()

	UPROPERTY(Instanced)
	UObject* Prop;
};

UCLASS()
class alignas(8) UAlignedObject : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class UTestObject : public UObject, public ITestObject
{
	GENERATED_BODY()

public:
	UTestObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY()
	TArray<FContainsInstancedProperty> InstancedPropertyArray;

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> ObjectWrapperArray;

	UPROPERTY()
	TSet<FContainsInstancedProperty> InstancedPropertySet;

	UPROPERTY()
	TMap<FContainsInstancedProperty, TWeakObjectPtr<UObject>> InstancedPropertyToObjectWrapperMap;

	UPROPERTY()
	TMap<TWeakObjectPtr<UObject>, FContainsInstancedProperty> ObjectWrapperToInstancedPropertyMap;

	UFUNCTION(BlueprintCallable, Category="Random")
	void TestForNullPtrDefaults(UObject* Obj1 = NULL, UObject* Obj2 = nullptr, UObject* Obj3 = 0);

	UFUNCTION()
	void TestPassingArrayOfInterfaces(const TArray<TScriptInterface<ITestInterface> >& ArrayOfInterfaces);

	UPROPERTY()
	int32 Cpp11Init = 123;

	UPROPERTY()
	TArray<int> Cpp11BracedInit { 1, 2, 3 };

	UPROPERTY()
	TArray<FVector4> Cpp11NestedBracedInit { { 1, 2, 3, 4 }, { 5, 6, 7, 8 } };

	UPROPERTY()
	int RawInt;

	UPROPERTY()
	unsigned int RawUint;

	UFUNCTION()
	void FuncTakingRawInts(int Signed, unsigned int Unsigned);

	UPROPERTY()
	ECppEnum EnumProperty;

	UPROPERTY()
	TMap<int32, bool> TestMap;

	UPROPERTY()
	TSet<int32> TestSet;

	UPROPERTY()
	UObject* const ConstPointerProperty;

	UFUNCTION()
	void CodeGenTestForEnumClasses(ECppEnum Val);

	UFUNCTION(Category="Xyz", BlueprintCallable)
	TArray<UClass*> ReturnArrayOfUClassPtrs();

	UFUNCTION()
	inline int32 InlineFunc1()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCEINLINE int32 InlineFunc2()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCEINLINE_WHATEVER int32 InlineFunc3()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	FORCENOINLINE int32 NoInlineFunc()
	{
		return FString("Hello").Len();
	}

	UFUNCTION()
	int32 InlineFuncWithCppMacros()
#if CPP
	{
		return FString("Hello").Len();
	}
#endif

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "MyEditorOnlyFunction")
	void MyEditorOnlyFunction();
#endif

	UFUNCTION(BlueprintNativeEvent, Category="Game")
	UClass* BrokenReturnTypeForFunction();

	UEnum* SomeFunc() const;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
