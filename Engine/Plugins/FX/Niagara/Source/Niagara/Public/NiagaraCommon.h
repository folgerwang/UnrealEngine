// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraTypes.h"
#include "UObject/SoftObjectPath.h"
#include "RHI.h"
#include "NiagaraCommon.generated.h"

class UNiagaraSystem;
class UNiagaraScript;
class UNiagaraDataInterface;
class UNiagaraEmitter;
class FNiagaraSystemInstance;
class UNiagaraParameterCollection;
struct FNiagaraParameterStore;

//#define NIAGARA_NAN_CHECKING 1
#define NIAGARA_NAN_CHECKING 0

const uint32 NIAGARA_COMPUTE_THREADGROUP_SIZE = 64;
const uint32 NIAGARA_MAX_COMPUTE_THREADGROUPS = 65536;

const FString INTERPOLATED_PARAMETER_PREFIX = TEXT("PREV_");

enum ENiagaraBaseTypes
{
	NBT_Float,
	NBT_Int32,
	NBT_Bool,
	NBT_Max,
};

UENUM()
enum class ENiagaraSimTarget : uint8
{
	CPUSim,
	GPUComputeSim,
	DynamicLoadBalancedSim UMETA(Hidden)
};


/** Defines modes for updating the component's age. */
UENUM()
enum class ENiagaraAgeUpdateMode : uint8
{
	/** Update the age using the delta time supplied to the tick function. */
	TickDeltaTime,
	/** Update the age by seeking to the DesiredAge. To prevent major perf loss, we clamp to MaxClampTime*/
	DesiredAge
};


UENUM()
enum class ENiagaraDataSetType : uint8
{
	ParticleData,
	Shared,
	Event,
};


UENUM()
enum class ENiagaraInputNodeUsage : uint8
{
	Undefined = 0,
	Parameter,
	Attribute,
	SystemConstant,
	TranslatorConstant,
	RapidIterationParameter
};

/**
* Enumerates states a Niagara script can be in.
*/
UENUM()
enum class ENiagaraScriptCompileStatus : uint8
{
	/** Niagara script is in an unknown state. */
	NCS_Unknown,
	/** Niagara script has been modified but not recompiled. */
	NCS_Dirty,
	/** Niagara script tried but failed to be compiled. */
	NCS_Error,
	/** Niagara script has been compiled since it was last modified. */
	NCS_UpToDate,
	/** Niagara script is in the process of being created for the first time. */
	NCS_BeingCreated,
	/** Niagara script has been compiled since it was last modified. There are warnings. */
	NCS_UpToDateWithWarnings,
	/** Niagara script has been compiled for compute since it was last modified. There are warnings. */
	NCS_ComputeUpToDateWithWarnings,
	NCS_MAX,
};

USTRUCT()
struct FNiagaraDataSetID
{
	GENERATED_USTRUCT_BODY()

	FNiagaraDataSetID()
	: Name(NAME_None)
	, Type(ENiagaraDataSetType::Event)
	{}

	FNiagaraDataSetID(FName InName, ENiagaraDataSetType InType)
		: Name(InName)
		, Type(InType)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Set")
	FName Name;

	UPROPERTY()
	ENiagaraDataSetType Type;

	FORCEINLINE bool operator==(const FNiagaraDataSetID& Other)const
	{
		return Name == Other.Name && Type == Other.Type;
	}

	FORCEINLINE bool operator!=(const FNiagaraDataSetID& Other)const
	{
		return !(*this == Other);
	}
};


FORCEINLINE FArchive& operator<<(FArchive& Ar, FNiagaraDataSetID& VarInfo)
{
	Ar << VarInfo.Name << VarInfo.Type;
	return Ar;
}

FORCEINLINE uint32 GetTypeHash(const FNiagaraDataSetID& Var)
{
	return HashCombine(GetTypeHash(Var.Name), (uint32)Var.Type);
}

USTRUCT()
struct FNiagaraDataSetProperties
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category = "Data Set")
	FNiagaraDataSetID ID;

	UPROPERTY()
	TArray<FNiagaraVariable> Variables;
};

/** Information about an input or output of a Niagara operation node. */
class FNiagaraOpInOutInfo
{
public:
	FName Name;
	FNiagaraTypeDefinition DataType;
	FText FriendlyName;
	FText Description;
	FString Default;
	FString HlslSnippet;

	FNiagaraOpInOutInfo(FName InName, FNiagaraTypeDefinition InType, FText InFriendlyName, FText InDescription, FString InDefault, FString InHlslSnippet = TEXT(""))
		: Name(InName)
		, DataType(InType)
		, FriendlyName(InFriendlyName)
		, Description(InDescription)
		, Default(InDefault)
		, HlslSnippet(InHlslSnippet)
	{

	}
};


/** Struct containing usage information about a script. Things such as whether it reads attribute data, reads or writes events data etc.*/
USTRUCT()
struct FNiagaraScriptDataUsageInfo
{
	GENERATED_BODY()

		FNiagaraScriptDataUsageInfo()
		: bReadsAttributeData(false)
	{}

	/** If true, this script reads attribute data. */
	UPROPERTY()
	bool bReadsAttributeData;
};


USTRUCT()
struct NIAGARA_API FNiagaraFunctionSignature
{
	GENERATED_BODY()

	/** Name of the function. */
	UPROPERTY()
	FName Name;
	/** Input parameters to this function. */
	UPROPERTY()
	TArray<FNiagaraVariable> Inputs;
	/** Input parameters of this function. */
	UPROPERTY()
	TArray<FNiagaraVariable> Outputs;
	/** Id of the owner is this is a member function. */
	UPROPERTY()
	FName OwnerName;
	UPROPERTY()
	bool bRequiresContext;
	/** True if this is the signature for a "member" function of a data interface. If this is true, the first input is the owner. */
	UPROPERTY()
	bool bMemberFunction;

	/** Localized description of this node. Note that this is *not* used during the operator == below since it may vary from culture to culture.*/
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FText Description;
#endif

	FNiagaraFunctionSignature() 
		: bRequiresContext(false)
		, bMemberFunction(false)
	{
	}

	FNiagaraFunctionSignature(FName InName, TArray<FNiagaraVariable>& InInputs, TArray<FNiagaraVariable>& InOutputs, FName InSource, bool bInRequiresContext, bool bInMemberFunction)
		: Name(InName)
		, Inputs(InInputs)
		, Outputs(InOutputs)
		, bRequiresContext(bInRequiresContext)
		, bMemberFunction(bInMemberFunction)
	{

	}

	bool operator==(const FNiagaraFunctionSignature& Other) const
	{
		bool bNamesEqual = Name.ToString().Equals(Other.Name.ToString());
		bool bInputsEqual = Inputs == Other.Inputs;
		bool bOutputsEqual = Outputs == Other.Outputs;
		bool bContextsEqual = bRequiresContext == Other.bRequiresContext;
		bool bMemberFunctionsEqual = bMemberFunction == Other.bMemberFunction;
		bool bOwnerNamesEqual = OwnerName == Other.OwnerName;
		return bNamesEqual && bInputsEqual && bOutputsEqual && bContextsEqual && bMemberFunctionsEqual && bOwnerNamesEqual;
	}

	FString GetName()const { return Name.ToString(); }

	void SetDescription(const FText& Desc)
	{
	#if WITH_EDITORONLY_DATA
		Description = Desc;
	#endif
	}
	FText GetDescription() const
	{
	#if WITH_EDITORONLY_DATA
		return Description;
	#else
		return FText::FromName(Name);
	#endif
	}
	bool IsValid()const { return Name != NAME_None && (Inputs.Num() > 0 || Outputs.Num() > 0); }
};



USTRUCT()
struct NIAGARA_API FNiagaraScriptDataInterfaceInfo
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraScriptDataInterfaceInfo()
		: DataInterface(nullptr)
		, Name(NAME_None)
		, UserPtrIdx(INDEX_NONE)
	{

	}

	UPROPERTY()
	class UNiagaraDataInterface* DataInterface;
	
	UPROPERTY()
	FName Name;

	/** Index of the user pointer for this data interface. */
	UPROPERTY()
	int32 UserPtrIdx;

	UPROPERTY()
	FNiagaraTypeDefinition Type;

	UPROPERTY()
	FName RegisteredParameterMapRead;

	UPROPERTY()
	FName RegisteredParameterMapWrite;

	//TODO: Allow data interfaces to own datasets
	void CopyTo(FNiagaraScriptDataInterfaceInfo* Destination, UObject* Outer) const;
};

USTRUCT()
struct NIAGARA_API FNiagaraScriptDataInterfaceCompileInfo
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraScriptDataInterfaceCompileInfo()
		: Name(NAME_None)
		, UserPtrIdx(INDEX_NONE)
		, bIsPlaceholder(false)
	{

	}

	UPROPERTY()
	FName Name;

	/** Index of the user pointer for this data interface. */
	UPROPERTY()
	int32 UserPtrIdx;

	UPROPERTY()
	FNiagaraTypeDefinition Type;

	UPROPERTY()
	TArray<FNiagaraFunctionSignature> RegisteredFunctions;

	UPROPERTY()
	FName RegisteredParameterMapRead;

	UPROPERTY()
	FName RegisteredParameterMapWrite;

	UPROPERTY()
	bool bIsPlaceholder;

	/** Would this data interface work on the target execution type? Only call this on the game thread.*/
	bool CanExecuteOnTarget(ENiagaraSimTarget SimTarget) const;

	/** Note that this is the CDO for this type of data interface, as we often cannot guarantee that the same instance of the data interface we compiled with is the one the user ultimately executes.  Only call this on the game thread.*/
	UNiagaraDataInterface* GetDefaultDataInterface() const;
};

USTRUCT()
struct FNiagaraStatScope
{
	GENERATED_USTRUCT_BODY();

	FNiagaraStatScope() {}
	FNiagaraStatScope(FName InFullName, FName InFriendlyName):FullName(InFullName), FriendlyName(InFriendlyName){}

	UPROPERTY()
	FName FullName;

	UPROPERTY()
	FName FriendlyName;

	bool operator==(const FNiagaraStatScope& Other) const { return FullName == Other.FullName; }
};

USTRUCT()
struct FVMExternalFunctionBindingInfo
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName OwnerName;

	UPROPERTY()
	TArray<bool> InputParamLocations;

	UPROPERTY()
	int32 NumOutputs;

	FORCEINLINE int32 GetNumInputs()const { return InputParamLocations.Num(); }
	FORCEINLINE int32 GetNumOutputs()const { return NumOutputs; }
};

struct NIAGARA_API FNiagaraSystemUpdateContext
{
	FNiagaraSystemUpdateContext(const UNiagaraSystem* System, bool bReInit) { Add(System, bReInit); }
#if WITH_EDITORONLY_DATA
	FNiagaraSystemUpdateContext(const UNiagaraEmitter* Emitter, bool bReInit) { Add(Emitter, bReInit); }
	FNiagaraSystemUpdateContext(const UNiagaraScript* Script, bool bReInit) { Add(Script, bReInit); }
	//FNiagaraSystemUpdateContext(UNiagaraDataInterface* Interface, bool bReinit) : Add(Interface, bReinit) {}
	FNiagaraSystemUpdateContext(const UNiagaraParameterCollection* Collection, bool bReInit) { Add(Collection, bReInit); }
#endif
	FNiagaraSystemUpdateContext() { }

	~FNiagaraSystemUpdateContext();

	void Add(const UNiagaraSystem* System, bool bReInit);
#if WITH_EDITORONLY_DATA
	void Add(const UNiagaraEmitter* Emitter, bool bReInit);
	void Add(const UNiagaraScript* Script, bool bReInit);
	//void Add(UNiagaraDataInterface* Interface, bool bReinit);
	void Add(const UNiagaraParameterCollection* Collection, bool bReInit);
#endif

	/** Adds all currently active systems.*/
	void AddAll(bool bReInit);

private:
	void AddInternal(class UNiagaraComponent* Comp, bool bReInit);
	FNiagaraSystemUpdateContext(FNiagaraSystemUpdateContext& Other) { }

	TArray<UNiagaraComponent*> ComponentsToReset;
	TArray<UNiagaraComponent*> ComponentsToReInit;

	TArray<UNiagaraSystem*> SystemSimsToDestroy;

	//TODO: When we allow component less systems we'll also want to find and reset those.
};





/** Defines different usages for a niagara script. */
UENUM()
enum class ENiagaraScriptUsage : uint8
{
	/** The script defines a function for use in modules. */
	Function,
	/** The script defines a module for use in particle, emitter, or system scripts. */
	Module,
	/** The script defines a dynamic input for use in particle, emitter, or system scripts. */
	DynamicInput,
	/** The script is called when spawning particles. */
	ParticleSpawnScript UMETA(Hidden),
	/** Particle spawn script that handles intra-frame spawning and also pulls in the update script. */
	ParticleSpawnScriptInterpolated UMETA(Hidden),
	/** The script is called to update particles every frame. */
	ParticleUpdateScript UMETA(Hidden),
	/** The script is called to update particles in response to an event. */
	ParticleEventScript UMETA(Hidden),
	/** The script is called to update particles on the GPU. */
	ParticleGPUComputeScript UMETA(Hidden),
	/** The script is called once when the emitter spawns. */
	EmitterSpawnScript UMETA(Hidden),
	/** The script is called every frame to tick the emitter. */
	EmitterUpdateScript UMETA(Hidden),
	/** The script is called once when the system spawns. */
	SystemSpawnScript UMETA(Hidden),
	/** The script is called every frame to tick the system. */
	SystemUpdateScript UMETA(Hidden),
};

UENUM()
enum class ENiagaraScriptGroup : uint8
{
	Particle = 0,
	Emitter,
	System,
	Max
};


/** Defines all you need to know about a variable.*/
USTRUCT()
struct FNiagaraVariableInfo
{
	GENERATED_USTRUCT_BODY();

	FNiagaraVariableInfo() : DataInterface(nullptr) {}

	UPROPERTY()
	FNiagaraVariable Variable;

	UPROPERTY()
	FText Definition;

	UPROPERTY()
	UNiagaraDataInterface* DataInterface;
};

USTRUCT()
struct FNiagaraVariableAttributeBinding
{
	GENERATED_USTRUCT_BODY();

	FNiagaraVariableAttributeBinding() {}
	FNiagaraVariableAttributeBinding(const FNiagaraVariable& InVar, const FNiagaraVariable& InAttrVar) : BoundVariable(InVar), DataSetVariable(InAttrVar), DefaultValueIfNonExistent(InAttrVar)
	{
		check(InVar.GetType() == InAttrVar.GetType());
	}
	FNiagaraVariableAttributeBinding(const FNiagaraVariable& InVar, const FNiagaraVariable& InAttrVar, const FNiagaraVariable& InNonExistentValue) : BoundVariable(InVar), DataSetVariable(InAttrVar), DefaultValueIfNonExistent(InNonExistentValue)
	{
		check(InVar.GetType() == InAttrVar.GetType() && InNonExistentValue.GetType() == InAttrVar.GetType());
	}


	UPROPERTY()
	FNiagaraVariable BoundVariable;

	UPROPERTY()
	FNiagaraVariable DataSetVariable;

	UPROPERTY()
	FNiagaraVariable DefaultValueIfNonExistent;
};

USTRUCT()
struct FNiagaraVariableDataInterfaceBinding
{
	GENERATED_USTRUCT_BODY();

	FNiagaraVariableDataInterfaceBinding() {}
	FNiagaraVariableDataInterfaceBinding(const FNiagaraVariable& InVar) : BoundVariable(InVar)
	{
		check(InVar.IsDataInterface() == true);
	}

	UPROPERTY()
	FNiagaraVariable BoundVariable;
};


namespace FNiagaraUtilities
{
	/** Builds a unique name from a candidate name and a set of existing names.  The candidate name will be made unique
	if necessary by adding a 3 digit index to the end. */
	FName NIAGARA_API GetUniqueName(FName CandidateName, const TSet<FName>& ExistingNames);

	FNiagaraVariable NIAGARA_API ConvertVariableToRapidIterationConstantName(FNiagaraVariable InVar, const TCHAR* InEmitterName, ENiagaraScriptUsage InUsage);

	void CollectScriptDataInterfaceParameters(const UObject& Owner, const TArray<UNiagaraScript*>& Scripts, FNiagaraParameterStore& OutDataInterfaceParameters);

	inline bool SupportsNiagaraRendering(ERHIFeatureLevel::Type FeatureLevel)
	{
		return FeatureLevel == ERHIFeatureLevel::SM4 || FeatureLevel == ERHIFeatureLevel::SM5 || FeatureLevel == ERHIFeatureLevel::ES3_1;
	}

	inline bool SupportsNiagaraRendering(EShaderPlatform ShaderPlatform)
	{
		// Note:
		// IsFeatureLevelSupported does a FeatureLevel < MaxFeatureLevel(ShaderPlatform) so checking ES3.1 support will return true for SM4. I added it explicitly to be clear what we are doing.
		return IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::SM5) || IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::ES3_1) || IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::SM4);
	}

	inline bool SupportsGPUParticles(ERHIFeatureLevel::Type FeatureLevel)
	{
		EShaderPlatform ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];
		return RHISupportsComputeShaders(ShaderPlatform);
	}

	inline bool SupportsGPUParticles(EShaderPlatform ShaderPlatform)
	{
		return RHISupportsComputeShaders(ShaderPlatform);
	}

#if WITH_EDITORONLY_DATA
	/**
	 * Prepares rapid iteration parameter stores for simulation by removing old parameters no longer used by functions, by initializing new parameters
	 * added to functions, and by copying parameters across parameter stores for interscript dependencies.
	 * @param Scripts The scripts who's rapid iteration parameter stores will be processed.
	 * @param ScriptDependencyMap A map of script dependencies where the key is the source script and the value is the script which depends on the source.  All scripts in this
	 * map must be contained in the Scripts array, both keys and values.
	 * @param ScriptToEmitterNameMap An array of scripts to the name of the emitter than owns them.  If this is a system script the name can be empty.  All scripts in the
	 * scripts array must have an entry in this map.
	 */
	void NIAGARA_API PrepareRapidIterationParameters(const TArray<UNiagaraScript*>& Scripts, const TMap<UNiagaraScript*, UNiagaraScript*>& ScriptDependencyMap, const TMap<UNiagaraScript*, FString>& ScriptToEmitterNameMap);
#endif
};