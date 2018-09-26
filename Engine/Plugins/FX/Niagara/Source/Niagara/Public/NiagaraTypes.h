// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"
#include "Engine/UserDefinedStruct.h"
#include "Templates/SharedPointer.h"
#include "NiagaraTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNiagara, Log, Verbose);

// basic type struct definitions

USTRUCT(meta = (DisplayName = "float"))
struct FNiagaraFloat
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=Parameters)//Parameters? These are used for attrs too.
	float Value;
};

USTRUCT(meta = (DisplayName = "int32"))
struct FNiagaraInt32
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = Parameters)//Parameters? These are used for attrs too.
	int32 Value;
};

USTRUCT(meta=(DisplayName="bool"))
struct FNiagaraBool
{
	GENERATED_USTRUCT_BODY()

		// The Niagara VM expects this bitmask for its compare and select operators for false.
	enum BoolValues { 
		True = INDEX_NONE,
		False = 0
	}; 

	

	void SetValue(bool bValue) { Value = bValue ? True : False; }
	bool GetValue() const { return Value != False; }

	/** Sets this niagara bool's raw integer value directly using the special raw integer values expected by the VM and HLSL. */
	FORCEINLINE void SetRawValue(int32 RawValue) { Value = RawValue; }

	/** Gets this niagara bools raw integer value expected by the VM and HLSL. */
	FORCEINLINE int32 GetRawValue() const { return Value; }

	bool IsValid() const { return Value == True || Value == False; }
	
	FNiagaraBool():Value(False) {}
	FNiagaraBool(bool bInValue) : Value(bInValue ? True : False) {}
	FORCEINLINE operator bool() { return GetValue(); }

private:
	UPROPERTY(EditAnywhere, Category = Parameters)//Parameters? These are used for attrs too. Must be either FNiagaraBool::True or FNiagaraBool::False.
	int32 Value;
};

USTRUCT()
struct FNiagaraNumeric
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT()
struct FNiagaraParameterMap
{
	GENERATED_USTRUCT_BODY()
};

USTRUCT()
struct FNiagaraTestStructInner
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector InnerVector1;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector InnerVector2;
};

USTRUCT()
struct FNiagaraTestStruct
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector Vector1;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FVector Vector2;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FNiagaraTestStructInner InnerStruct1;

	UPROPERTY(EditAnywhere, Category = TestStruct)
	FNiagaraTestStructInner InnerStruct2;
};

USTRUCT(meta = (DisplayName = "Matrix"))
struct FNiagaraMatrix
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=NiagaraMatrix)
	FVector4 Row0;

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4 Row1;

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4 Row2;

	UPROPERTY(EditAnywhere, Category = NiagaraMatrix)
	FVector4 Row3;
};

/** Data controlling the spawning of particles */
USTRUCT(meta = (DisplayName = "Spawn Info", NiagaraClearEachFrame = "true"))
struct FNiagaraSpawnInfo
{
	GENERATED_USTRUCT_BODY();
	
	FNiagaraSpawnInfo()
		: Count(0)
		, InterpStartDt(0.0f)
		, IntervalDt(1.0f)
		, SpawnGroup(0)
	{}

	/** How many particles to spawn. */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	int32 Count;
	/** The sub frame delta time at which to spawn the first particle. */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	float InterpStartDt;
	/** The sub frame delta time between each particle. */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	float IntervalDt;
	/**
	 * An integer used to identify this spawn info.
	 * Typically this is unused.
	 * An example usage is when using multiple spawn modules to spawn from multiple discreet locations.
	 */
	UPROPERTY(EditAnywhere, Category = SpawnInfo)
	int32 SpawnGroup;
};

USTRUCT(Blueprintable, meta = (DisplayName = "Niagara ID"))
struct FNiagaraID
{
	GENERATED_USTRUCT_BODY()

	/** 
	Index in the indirection table for this particle. Allows fast access to this particles data.
	Is always unique among currently living particles but will be reused after the particle dies.
	*/
	UPROPERTY(EditAnywhere, Category = ID)
	int32 Index;

	/** 
	A unique tag for when this ID was acquired. 
	Allows us to differentiate between particles when one dies and another reuses it's Index.
	*/
	UPROPERTY(EditAnywhere, Category = ID)
	int32 AcquireTag;

	bool operator==(const FNiagaraID& Other)const { return Index == Other.Index && AcquireTag == Other.AcquireTag; }
	bool operator<(const FNiagaraID& Other)const { return Index < Other.Index && AcquireTag < Other.AcquireTag; }
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraID& ID)
{
	return HashCombine(GetTypeHash(ID.Index), GetTypeHash(ID.AcquireTag));
}

/** Information about how this type should be laid out in an FNiagaraDataSet */
struct FNiagaraTypeLayoutInfo
{
	FNiagaraTypeLayoutInfo()
	{}

	/** Byte offset of each float component in a structured layout. */
	TArray<uint32> FloatComponentByteOffsets;
	/** Offset into register table for each float component. */
	TArray<uint32> FloatComponentRegisterOffsets;

	/** Byte offset of each int32 component in a structured layout. */
	TArray<uint32> Int32ComponentByteOffsets;
	/** Offset into register table for each int32 component. */
	TArray<uint32> Int32ComponentRegisterOffsets;

	FORCEINLINE uint32 GetNumComponents()const { return FloatComponentByteOffsets.Num() + Int32ComponentByteOffsets.Num(); }

	static void GenerateLayoutInfo(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct)
	{
		Layout.FloatComponentByteOffsets.Empty();
		Layout.FloatComponentRegisterOffsets.Empty();
		Layout.Int32ComponentByteOffsets.Empty();
		Layout.Int32ComponentRegisterOffsets.Empty();
		GenerateLayoutInfoInternal(Layout, Struct);
	}

private:
	static void GenerateLayoutInfoInternal(FNiagaraTypeLayoutInfo& Layout, const UScriptStruct* Struct, int32 BaseOffest = 0)
	{
		for (TFieldIterator<UProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			int32 PropOffset = BaseOffest + Property->GetOffset_ForInternal();
			if (Property->IsA(UFloatProperty::StaticClass()))
			{
				Layout.FloatComponentRegisterOffsets.Add(Layout.GetNumComponents());
				Layout.FloatComponentByteOffsets.Add(PropOffset);
			}
			else if (Property->IsA(UIntProperty::StaticClass()) || Property->IsA(UBoolProperty::StaticClass()))
			{
				Layout.Int32ComponentRegisterOffsets.Add(Layout.GetNumComponents());
				Layout.Int32ComponentByteOffsets.Add(PropOffset);
			}
			//Should be able to support double easily enough
			else if (UStructProperty* StructProp = CastChecked<UStructProperty>(Property))
			{
				GenerateLayoutInfoInternal(Layout, StructProp->Struct, PropOffset);
			}
			else
			{
				check(false);
			}
		}
	}
};

/*
*  Can convert a UStruct with fields of base types only (float, int... - will likely add native vector types here as well)
*	to an FNiagaraTypeDefinition (internal representation)
*/
class NIAGARA_API FNiagaraTypeHelper
{
public:
	static FString ToString(const uint8* ValueData, const UScriptStruct* Struct);

};

/** Defines different modes for selecting the output numeric type of a function or operation based on the types of the inputs. */
UENUM()
enum class ENiagaraNumericOutputTypeSelectionMode : uint8
{
	/** Output type selection not supported. */
	None UMETA(Hidden),
	/** Select the largest of the numeric inputs. */
	Largest,
	/** Select the smallest of the numeric inputs. */
	Smallest,
	/** Selects the base scalar type for this numeric inputs. */
	Scalar,
};

/** 
The source from which a script execution state was set. Used to allow scalability etc to change the state but only if the state has not been defined by something with higher precedence. 
If this changes, all scripts must be recompiled by bumping the NiagaraCustomVersion
*/
UENUM()
enum class ENiagaraExecutionStateSource : uint32
{
	Scalability, //State set by Scalability logic. Lowest precedence.
	Internal, //Misc internal state. For example becoming inactive after we finish our set loops.
	Owner, //State requested by the owner. Takes precedence over everything but internal completion logic.
	InternalCompletion, // Internal completion logic. Has to take highest precedence for completion to be ensured.
};

UENUM()
enum class ENiagaraExecutionState : uint32
{
	/**  Run all scripts. Allow spawning.*/
	Active,
	/** Run all scripts but suppress any new spawning.*/
	Inactive,
	/** Clear all existing particles and move to inactive.*/
	InactiveClear,
	/** Complete. When the system or all emitters are complete the effect is considered finished. */
	Complete,
	/** Emitter only. Emitter is disabled. Will not tick or render again until a full re initialization of the system. */
	Disabled,

	// insert new states before
	Num
};

USTRUCT()
struct NIAGARA_API FNiagaraVariableMetaData
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraVariableMetaData()
		:EditorSortPriority(0)
		, CallSortPriority(0)
	{
	}
public:
	UPROPERTY(EditAnywhere, Category = "Variable")
	TMap<FName, FString> PropertyMetaData;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditAnywhere, Category = "Variable")
	FText CategoryName;

	UPROPERTY(EditAnywhere, Category = "Variable", meta = (ToolTip = "Affects the sort order in the editor stacks. Use a smaller number to push it to the top. Defaults to zero."))
	int32 EditorSortPriority;

	UPROPERTY()
	int32 CallSortPriority;

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> ReferencerNodes;
};

USTRUCT()
struct NIAGARA_API FNiagaraTypeDefinition
{
	GENERATED_USTRUCT_BODY()
public:

	// Construct blank raw type definition 
	FNiagaraTypeDefinition(UClass *ClassDef)
		: Struct(ClassDef), Enum(nullptr), Size(INDEX_NONE), Alignment(INDEX_NONE)
	{
		checkSlow(Struct != nullptr);
	}

	FNiagaraTypeDefinition(UEnum *EnumDef)
		: Struct(IntStruct), Enum(EnumDef), Size(INDEX_NONE), Alignment(INDEX_NONE)
	{
		checkSlow(Struct != nullptr);
	}

	FNiagaraTypeDefinition(UScriptStruct *StructDef)
		: Struct(StructDef), Enum(nullptr), Size(INDEX_NONE), Alignment(INDEX_NONE)
	{
		checkSlow(Struct != nullptr);
	}

	FNiagaraTypeDefinition(const FNiagaraTypeDefinition &Other)
		: Struct(Other.Struct), Enum(Other.Enum), Size(INDEX_NONE), Alignment(INDEX_NONE)
	{
	}

	// Construct a blank raw type definition
	FNiagaraTypeDefinition()
		: Struct(nullptr), Enum(nullptr), Size(INDEX_NONE), Alignment(INDEX_NONE)
	{}

	bool operator !=(const FNiagaraTypeDefinition &Other) const
	{
		return !(*this == Other);
	}

	bool operator == (const FNiagaraTypeDefinition &Other) const
	{
		return Struct == Other.Struct && Enum == Other.Enum;
	}

	FText GetNameText()const
	{
		if (IsValid() == false)
		{
			return NSLOCTEXT("NiagaraTypeDefinition", "InvalidNameText", "Invalid (null type)");
		}

		if (Enum != nullptr)
		{
			return  FText::FromString(Enum->GetName());
		}

#if WITH_EDITOR
		return GetStruct()->GetDisplayNameText();
#else
		return FText::FromString( GetStruct()->GetName() );
#endif
	}

	FString GetName()const
	{
		if (IsValid() == false)
		{
			return TEXT("Invalid");
		}

		if (Enum != nullptr)
		{
			return  Enum->GetName();
		}
		return GetStruct()->GetName();
	}

	UStruct* GetStruct()const
	{
		return Struct;
	}

	UScriptStruct* GetScriptStruct()const
	{
		return Cast<UScriptStruct>(Struct);
	}

	/** Gets the class ptr for this type if it is a class. */
	UClass* GetClass()const
	{
		return Cast<UClass>(Struct);
	}

	UEnum* GetEnum() const
	{
		return Enum;
	}

	bool IsDataInterface()const;

	bool IsEnum() const { return Enum != nullptr; }
	
	int32 GetSize()const
	{
		if (Size == INDEX_NONE)
		{
			checkfSlow(IsValid(), TEXT("Type definition is not valid."));
			if (GetClass())
			{
				Size = 0;//TODO: sizeof(void*);//If we're a class then we allocate space for the user to instantiate it. This and stopping it being GCd is up to the user.
			}
			else
			{
				Size = CastChecked<UScriptStruct>(Struct)->GetStructureSize();	// TODO: cache this here?
			}
		}
		return Size;
	}

	int32 GetAlignment()const
	{
		if (Alignment == INDEX_NONE)
		{
			checkfSlow(IsValid(), TEXT("Type definition is not valid."));
			if (GetClass())
			{
				Alignment = 0;//TODO: sizeof(void*);//If we're a class then we allocate space for the user to instantiate it. This and stopping it being GCd is up to the user.
			}
			else
			{
				Alignment = CastChecked<UScriptStruct>(Struct)->GetMinAlignment();
			}
		}
		return Alignment;
	}

	bool IsFloatPrimitive() const
	{
		return Struct == FNiagaraTypeDefinition::GetFloatStruct() || Struct == FNiagaraTypeDefinition::GetVec2Struct() || Struct == FNiagaraTypeDefinition::GetVec3Struct() || Struct == FNiagaraTypeDefinition::GetVec4Struct() ||
			Struct == FNiagaraTypeDefinition::GetMatrix4Struct() || Struct == FNiagaraTypeDefinition::GetColorStruct() || Struct == FNiagaraTypeDefinition::GetQuatStruct();
 	}

	bool IsValid() const 
	{ 
		return Struct != nullptr;
	}

	/**
	UStruct specifying the type for this variable.
	For most types this will be a UScriptStruct pointing to a something like the struct for an FVector etc.
	In occasional situations this may be a UClass when we're dealing with DataInterface etc.
	*/
	UPROPERTY()
	UStruct *Struct;

	UPROPERTY()
	UEnum* Enum;

private:
	mutable int16 Size;
	mutable int16 Alignment;

public:

	static void Init();
	static void RecreateUserDefinedTypeRegistry();
	static const FNiagaraTypeDefinition& GetFloatDef() { return FloatDef; }
	static const FNiagaraTypeDefinition& GetBoolDef() { return BoolDef; }
	static const FNiagaraTypeDefinition& GetIntDef() { return IntDef; }
	static const FNiagaraTypeDefinition& GetVec2Def() { return Vec2Def; }
	static const FNiagaraTypeDefinition& GetVec3Def() { return Vec3Def; }
	static const FNiagaraTypeDefinition& GetVec4Def() { return Vec4Def; }
	static const FNiagaraTypeDefinition& GetColorDef() { return ColorDef; }
	static const FNiagaraTypeDefinition& GetQuatDef() { return QuatDef; }
	static const FNiagaraTypeDefinition& GetMatrix4Def() { return Matrix4Def; }
	static const FNiagaraTypeDefinition& GetGenericNumericDef() { return NumericDef; }
	static const FNiagaraTypeDefinition& GetParameterMapDef() { return ParameterMapDef; }
	static const FNiagaraTypeDefinition& GetIDDef() { return IDDef; }

	static UScriptStruct* GetFloatStruct() { return FloatStruct; }
	static UScriptStruct* GetBoolStruct() { return BoolStruct; }
	static UScriptStruct* GetIntStruct() { return IntStruct; }
	static UScriptStruct* GetVec2Struct() { return Vec2Struct; }
	static UScriptStruct* GetVec3Struct() { return Vec3Struct; }
	static UScriptStruct* GetVec4Struct() { return Vec4Struct; }
	static UScriptStruct* GetColorStruct() { return ColorStruct; }
	static UScriptStruct* GetQuatStruct() { return QuatStruct; }
	static UScriptStruct* GetMatrix4Struct() { return Matrix4Struct; }
	static UScriptStruct* GetGenericNumericStruct() { return NumericStruct; }
	static UScriptStruct* GetParameterMapStruct() { return ParameterMapStruct; }
	static UScriptStruct* GetIDStruct() { return IDStruct; }

	static UEnum* GetExecutionStateEnum() { return ExecutionStateEnum; }

	static const FNiagaraTypeDefinition& GetCollisionEventDef() { return CollisionEventDef; }

	static bool IsScalarDefinition(const FNiagaraTypeDefinition& Type);

	FString ToString(const uint8* ValueData)const
	{
		checkf(IsValid(), TEXT("Type definition is not valid."));
		if (ValueData == nullptr)
		{
			return TEXT("(null)");
		}
		return FNiagaraTypeHelper::ToString(ValueData, CastChecked<UScriptStruct>(Struct));
	}

	static bool TypesAreAssignable(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB);
	static bool IsLossyConversion(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB);
	static FNiagaraTypeDefinition GetNumericOutputType(const TArray<FNiagaraTypeDefinition> TypeDefinintions, ENiagaraNumericOutputTypeSelectionMode SelectionMode);

	static const TArray<FNiagaraTypeDefinition>& GetNumericTypes() { return OrderedNumericTypes; }
	static bool IsValidNumericInput(const FNiagaraTypeDefinition& TypeDef);
private:

	static FNiagaraTypeDefinition FloatDef;
	static FNiagaraTypeDefinition BoolDef;
	static FNiagaraTypeDefinition IntDef;
	static FNiagaraTypeDefinition Vec2Def;
	static FNiagaraTypeDefinition Vec3Def;
	static FNiagaraTypeDefinition Vec4Def;
	static FNiagaraTypeDefinition ColorDef;
	static FNiagaraTypeDefinition QuatDef;
	static FNiagaraTypeDefinition Matrix4Def;
	static FNiagaraTypeDefinition NumericDef;
	static FNiagaraTypeDefinition ParameterMapDef;
	static FNiagaraTypeDefinition IDDef;

	static UScriptStruct* FloatStruct;
	static UScriptStruct* BoolStruct;
	static UScriptStruct* IntStruct;
	static UScriptStruct* Vec2Struct;
	static UScriptStruct* Vec3Struct;
	static UScriptStruct* Vec4Struct;
	static UScriptStruct* QuatStruct;
	static UScriptStruct* ColorStruct;
	static UScriptStruct* Matrix4Struct;
	static UScriptStruct* NumericStruct;

	static UEnum* ExecutionStateEnum;
	static UEnum* ExecutionStateSourceEnum;

	static UScriptStruct* ParameterMapStruct;
	static UScriptStruct* IDStruct;

	static TSet<UScriptStruct*> NumericStructs;
	static TArray<FNiagaraTypeDefinition> OrderedNumericTypes;

	static TSet<UScriptStruct*> ScalarStructs;

	static TSet<UStruct*> FloatStructs;
	static TSet<UStruct*> IntStructs;
	static TSet<UStruct*> BoolStructs;

	static FNiagaraTypeDefinition CollisionEventDef;

};

/* Contains all types currently available for use in Niagara
* Used by UI to provide selection; new uniforms and variables
* may be instanced using the types provided here
*/
class NIAGARA_API FNiagaraTypeRegistry
{
public:
	static const TArray<FNiagaraTypeDefinition> &GetRegisteredTypes()
	{
		return RegisteredTypes;
	}

	static const TArray<FNiagaraTypeDefinition> &GetRegisteredParameterTypes()
	{
		return RegisteredParamTypes;
	}

	static const TArray<FNiagaraTypeDefinition> &GetRegisteredPayloadTypes()
	{
		return RegisteredPayloadTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetUserDefinedTypes()
	{
		return RegisteredUserDefinedTypes;
	}

	static const TArray<FNiagaraTypeDefinition>& GetNumericTypes()
	{ 
		return RegisteredNumericTypes;
	}

	static void ClearUserDefinedRegistry()
	{
		for (const FNiagaraTypeDefinition& Def : RegisteredUserDefinedTypes)
		{
			RegisteredTypes.Remove(Def);
			RegisteredPayloadTypes.Remove(Def);
			RegisteredParamTypes.Remove(Def);
		}

		RegisteredNumericTypes.Empty();
		RegisteredUserDefinedTypes.Empty();
	}

	static void Register(const FNiagaraTypeDefinition &NewType, bool bCanBeParameter, bool bCanBePayload, bool bIsUserDefined)
	{
		//TODO: Make this a map of type to a more verbose set of metadata? Such as the hlsl defs, offset table for conversions etc.
		RegisteredTypes.AddUnique(NewType);

		if (bCanBeParameter)
		{
			RegisteredParamTypes.AddUnique(NewType);
		}

		if (bCanBePayload)
		{
			RegisteredPayloadTypes.AddUnique(NewType);
		}

		if (bIsUserDefined)
		{
			RegisteredUserDefinedTypes.AddUnique(NewType);
		}

		if (FNiagaraTypeDefinition::IsValidNumericInput(NewType))
		{
			RegisteredNumericTypes.AddUnique(NewType);
		}
	}

	FNiagaraTypeDefinition GetTypeDefFromStruct(UStruct* Struct)
	{
		for (FNiagaraTypeDefinition& TypeDef : RegisteredTypes)
		{
			if (Struct == TypeDef.GetStruct())
			{
				return TypeDef;
			}
		}

		return FNiagaraTypeDefinition();
	}

private:
	static TArray<FNiagaraTypeDefinition> RegisteredTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredParamTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredPayloadTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredUserDefinedTypes;
	static TArray<FNiagaraTypeDefinition> RegisteredNumericTypes;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraTypeDefinition& Type)
{
	return HashCombine(GetTypeHash(Type.GetStruct()), GetTypeHash(Type.GetEnum()));
}

//////////////////////////////////////////////////////////////////////////

USTRUCT()
struct FNiagaraVariable
{
	GENERATED_USTRUCT_BODY()

	FNiagaraVariable()
		: Name(NAME_None)
		, TypeDef(FNiagaraTypeDefinition::GetVec4Def())
	{
	}

	FNiagaraVariable(const FNiagaraVariable &Other)
		: Name(Other.Name)
		, TypeDef(Other.TypeDef)
	{
		if (Other.IsDataAllocated())
		{
			SetData(Other.GetData());
		}
	}

	FNiagaraVariable(FNiagaraTypeDefinition InType, FName InName)
		: Name(InName)
		, TypeDef(InType)
	{
	}
	
	/** Check if Name and Type definition are the same. The actual stored value is not checked here.*/
	bool operator==(const FNiagaraVariable& Other)const
	{
		return Name == Other.Name && TypeDef == Other.TypeDef;
	}

	/** Check if Name and Type definition are not the same. The actual stored value is not checked here.*/
	bool operator!=(const FNiagaraVariable& Other)const
	{
		return !(*this == Other);
	}

	/** Variables are the same name but if types are auto-assignable, allow them to match. */
	bool IsEquivalent(const FNiagaraVariable& Other, bool bAllowAssignableTypes = true)const
	{
		return Name == Other.Name && (TypeDef == Other.TypeDef || (bAllowAssignableTypes && FNiagaraTypeDefinition::TypesAreAssignable(TypeDef, Other.TypeDef)));
	}
	
	void SetName(FName InName) { Name = InName; }
	FName GetName()const { return Name; }

	void SetType(const FNiagaraTypeDefinition& InTypeDef) { TypeDef = InTypeDef; }
	const FNiagaraTypeDefinition& GetType()const { return TypeDef; }

	FORCEINLINE bool IsDataInterface()const { return GetType().IsDataInterface(); }

	void AllocateData()
	{
		if (VarData.Num() != TypeDef.GetSize())
		{
			VarData.SetNumZeroed(TypeDef.GetSize());
		}
	}

	bool IsDataAllocated()const { return VarData.Num() > 0 && VarData.Num() == TypeDef.GetSize(); }

	void CopyTo(uint8* Dest) const
	{
		check(TypeDef.GetSize() == VarData.Num());
		check(IsDataAllocated());
		FMemory::Memcpy(Dest, VarData.GetData(), VarData.Num());
	}
		
	template<typename T>
	void SetValue(const T& Data)
	{
		check(sizeof(T) == TypeDef.GetSize());
		AllocateData();
		FMemory::Memcpy(VarData.GetData(), &Data, VarData.Num());
	}

	template<typename T>
	T GetValue() const
	{
		check(sizeof(T) == TypeDef.GetSize());
		check(IsDataAllocated());
		T Value;
		FMemory::Memcpy(&Value, GetData(), TypeDef.GetSize());
		return Value;
	}

	void SetData(const uint8* Data)
	{
		check(Data);
		AllocateData();
		FMemory::Memcpy(VarData.GetData(), Data, VarData.Num());
	}

	const uint8* GetData() const
	{
		return VarData.GetData();
	}

	uint8* GetData()
	{
		return VarData.GetData();
	}

	int32 GetSizeInBytes() const
	{
		return TypeDef.GetSize();
	}

	int32 GetAlignment()const
	{
		return TypeDef.GetAlignment();
	}

	int32 GetAllocatedSizeInBytes() const
	{
		return VarData.Num();
	}

	FString ToString()const
	{
		FString Ret = Name.ToString() + TEXT("(");
		Ret += TypeDef.ToString(VarData.GetData());
		Ret += TEXT(")");
		return Ret;
	}

	bool IsValid() const
	{
		return Name != NAME_None && TypeDef.IsValid();
	}

	FORCEINLINE bool IsInNameSpace(FString Namespace) 
	{
		return Name.ToString().StartsWith(Namespace + TEXT("."));
	}

	static FNiagaraVariable ResolveAliases(const FNiagaraVariable& InVar, const TMap<FString, FString>& InAliases, const TCHAR* InJoinSeparator = TEXT("."))
	{
		FNiagaraVariable OutVar = InVar;

		FString OutVarStrName = InVar.GetName().ToString();
		TArray<FString> SplitName;
		OutVarStrName.ParseIntoArray(SplitName, TEXT("."));

		for (int32 i = 0; i < SplitName.Num() - 1; i++)
		{
			TMap<FString, FString>::TConstIterator It = InAliases.CreateConstIterator();
			while (It)
			{
				if (SplitName[i].Equals(It.Key()))
				{
					SplitName[i] = It.Value();
				}
				++It;
			}
		}

		OutVarStrName = FString::Join(SplitName, InJoinSeparator);

		OutVar.SetName(*OutVarStrName);
		return OutVar;
	}

	static int32 SearchArrayForPartialNameMatch(const TArray<FNiagaraVariable>& Variables, const FName& VariableName)
	{
		FString VarNameStr = VariableName.ToString();
		FString BestMatchSoFar;
		int32 BestMatchIdx = INDEX_NONE;

		for (int32 i = 0; i < Variables.Num(); i++)
		{
			const FNiagaraVariable& TestVar = Variables[i];
			FString TestVarNameStr = TestVar.GetName().ToString();
			if (TestVarNameStr == VarNameStr)
			{
				return i;
			}
			else if (VarNameStr.StartsWith(TestVarNameStr + TEXT(".")) && (BestMatchSoFar.Len() == 0 || TestVarNameStr.Len() > BestMatchSoFar.Len()))
			{
				BestMatchIdx = i;
				BestMatchSoFar = TestVarNameStr;
			}
		}

		return BestMatchIdx;
	}

private:
	UPROPERTY(EditAnywhere, Category = "Variable")
	FName Name;
	UPROPERTY(EditAnywhere, Category = "Variable")
	FNiagaraTypeDefinition TypeDef;
	//This gets serialized but do we need to worry about endianness doing things like this? If not, where does that get handled?
	//TODO: Remove storage here entirely and move everything to an FNiagaraParameterStore.
	UPROPERTY()
	TArray<uint8> VarData;
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraVariable& Var)
{
	return HashCombine(GetTypeHash(Var.GetType()), GetTypeHash(Var.GetName()));
}
