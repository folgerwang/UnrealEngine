/*
*/

#include "NiagaraDataInterfaceChaosDestruction.h"
#include "NiagaraTypes.h"
#include "Misc/FileHelper.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "PBDRigidsSolver.h"

#define LOCTEXT_NAMESPACE "ChaosNiagaraDestructionDataInterface"

// Name of all the functions available in the data interface
static const FName GetPositionName("GetPosition");
static const FName GetNormalName("GetNormal");
static const FName GetVelocityName("GetVelocity");
static const FName GetAngularVelocityName("GetAngularVelocity");
static const FName GetExtentMinName("GetExtentMin");
static const FName GetExtentMaxName("GetExtentMax");
static const FName GetParticleIdsToSpawnAtTimeName("GetParticleIdsToSpawnAtTime");
static const FName GetPointTypeName("GetPointType");
static const FName GetColorName("GetColor");

UNiagaraDataInterfaceChaosDestruction::UNiagaraDataInterfaceChaosDestruction(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DoSpawn = true;
	DataProcessFrequency = 1;
	MaxNumberOfDataEntries = 10;
	MinMassToSpawn = -1.f;
	MinImpulseToSpawn = -1.f;
	MinSpeedToSpawn = -1.f;
	DataSortingType = EDataSortTypeEnum::ChaosNiagara_DataSortType_NoSorting;
	SpawnMultiplierMin = 1;
	SpawnMultiplierMax = 1;
	RandomPositionMagnitude = 0.f;
	BreakingRegionRadiusMultiplier = 1.f;
	InheritedVelocityMultiplier = 1.f;
	RandomVelocityGenerationType = ERandomVelocityGenerationTypeEnum::ChaosNiagara_RandomVelocityGenerationType_RandomDistribution;
	RandomVelocityMagnitude = { 1.f, 2.f };
	SpreadAngleMax = 30.f;
	VelocityOffset = FVector(0.f, 0.f, 0.f);
	VelocityMagnitudeMax = -1;
	DebugType = EDebugTypeEnum::ChaosNiagara_DebugType_NoDebug;
	ParticleIndexToProcess = -1;

	LastSpawnedPointID = -1;
	PrevLastSpawnedPointID = -1;
	LastSpawnTime = -1.f;

	// Colors to visualize particles for debugging
	ColorArray.Add({ 1.0, 1.0, 1.0 }); // White
	ColorArray.Add({ 1.0, 0.0, 0.0 }); // Red
	ColorArray.Add({ 0.0, 1.0, 0.0 }); // Lime
	ColorArray.Add({ 0.0, 0.0, 1.0 }); // Blue
	ColorArray.Add({ 1.0, 1.0, 0.0 }); // Yellow
	ColorArray.Add({ 0.0, 1.0, 1.0 }); // Cyan
	ColorArray.Add({ 1.0, 0.0, 1.0 }); // Magenta
	ColorArray.Add({ 0.75, 0.75, 0.75 }); // Silver
	ColorArray.Add({ 0.5, 0.5, 0.5 }); // Gray
	ColorArray.Add({ 0.5, 0.0, 0.0 }); // Maroon
	ColorArray.Add({ 0.5, 0.5, 0.0 }); // Olive
	ColorArray.Add({ 0.0, 0.5, 0.0 }); // Green
	ColorArray.Add({ 0.5, 0.0, 0.5 }); // Purple
	ColorArray.Add({ 0.0, 0.5, 0.5 }); // Teal
	ColorArray.Add({ 0.0, 0.0, 0.5 }); // Navy
	ColorArray.Add({ 1.0, 165.0/255.0, 0.5 }); // Orange
	ColorArray.Add({ 1.0, 215.0 / 255.0, 0.5 }); // Gold
	ColorArray.Add({ 154.0 / 255.0, 205.0 / 255.0, 50.0 / 255.0}); // Yellow green
	ColorArray.Add({ 127.0 / 255.0, 255.0 / 255.0, 212.0 / 255.0 }); // Aqua marine
}

void UNiagaraDataInterfaceChaosDestruction::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), true, false, false);
		FNiagaraTypeRegistry::Register(FChaosDestructionEvent::StaticStruct(), true, true, false);
	}

	LastSpawnedPointID = -1;
	PrevLastSpawnedPointID = -1;
	LastSpawnTime = -1.f;
}

void UNiagaraDataInterfaceChaosDestruction::PostLoad()
{
	Super::PostLoad();

	LastSpawnedPointID = -1;
	PrevLastSpawnedPointID = -1;
	LastSpawnTime = -1.f;

	BuildPDBRigidSolverArray();
	int32 NumSolvers = ChaosSolverActorSet.Num();
	if (!NumSolvers)
	{
		LastDataTimeProcessedArray.SetNum(1);
		LastDataTimeProcessedArray[0] = -1.f;
	}
	else
	{
		LastDataTimeProcessedArray.SetNum(NumSolvers);
		for (int32 Idx = 0; Idx < NumSolvers; ++Idx)
		{
			LastDataTimeProcessedArray[Idx] = -1.f;
		}
	}
}

#if WITH_EDITOR

void UNiagaraDataInterfaceChaosDestruction::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && 
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceChaosDestruction, ChaosSolverActorSet))
	{
		Modify();
		if (ChaosSolverActorSet.Num())
		{
			LastSpawnedPointID = -1;
			PrevLastSpawnedPointID = -1;
			LastSpawnTime = -1.f;
		}

		BuildPDBRigidSolverArray();
	}
}

#endif

bool UNiagaraDataInterfaceChaosDestruction::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		if (UNiagaraDataInterfaceChaosDestruction* DestinationChaosDestruction = CastChecked<UNiagaraDataInterfaceChaosDestruction>(Destination))
		{
			DestinationChaosDestruction->ChaosSolverActorSet = ChaosSolverActorSet;
			DestinationChaosDestruction->DoSpawn = DoSpawn;
			DestinationChaosDestruction->DataProcessFrequency = DataProcessFrequency;
			DestinationChaosDestruction->MaxNumberOfDataEntries = MaxNumberOfDataEntries;
			DestinationChaosDestruction->MinMassToSpawn = MinMassToSpawn;
			DestinationChaosDestruction->MinImpulseToSpawn = MinMassToSpawn;
			DestinationChaosDestruction->MinSpeedToSpawn = MinSpeedToSpawn;
			DestinationChaosDestruction->DataSortingType = DataSortingType;
			DestinationChaosDestruction->SpawnMultiplierMin = SpawnMultiplierMin;
			DestinationChaosDestruction->SpawnMultiplierMax = SpawnMultiplierMax;
			DestinationChaosDestruction->RandomPositionMagnitude = RandomPositionMagnitude;
			DestinationChaosDestruction->BreakingRegionRadiusMultiplier = BreakingRegionRadiusMultiplier;
			DestinationChaosDestruction->InheritedVelocityMultiplier = InheritedVelocityMultiplier;
			DestinationChaosDestruction->VelocityOffset = VelocityOffset;
			DestinationChaosDestruction->RandomVelocityGenerationType = RandomVelocityGenerationType;
			DestinationChaosDestruction->RandomVelocityMagnitude = RandomVelocityMagnitude;
			DestinationChaosDestruction->SpreadAngleMax = SpreadAngleMax;
			DestinationChaosDestruction->VelocityMagnitudeMax = VelocityMagnitudeMax;
			DestinationChaosDestruction->DebugType = DebugType;
			DestinationChaosDestruction->ParticleIndexToProcess = ParticleIndexToProcess;

			DestinationChaosDestruction->LastSpawnedPointID = -1;
			DestinationChaosDestruction->PrevLastSpawnedPointID = -1;
			DestinationChaosDestruction->LastSpawnTime = -1.f;
			DestinationChaosDestruction->LastDataTimeProcessedArray = LastDataTimeProcessedArray;

			return true;
		}
	}
	return false;
}

bool UNiagaraDataInterfaceChaosDestruction::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
		return false;

	const UNiagaraDataInterfaceChaosDestruction* OtherChaosDestruction = CastChecked<const UNiagaraDataInterfaceChaosDestruction>(Other);
	if (OtherChaosDestruction == nullptr || OtherChaosDestruction->ChaosSolverActorSet.Num() == 0 || ChaosSolverActorSet.Num() == 0)
		return false;

	bool bResult = true;
	for (int32 Idx = 0; Idx < ChaosSolverActorSet.Num(); ++Idx) 
	{
		bResult = bResult && 
				  OtherChaosDestruction->ChaosSolverActorSet.Array()[Idx]->GetName().Equals(ChaosSolverActorSet.Array()[Idx]->GetName());
	}

	return bResult
		&& OtherChaosDestruction->DoSpawn == DoSpawn
		&& OtherChaosDestruction->DataProcessFrequency == DataProcessFrequency
		&& OtherChaosDestruction->MaxNumberOfDataEntries == MaxNumberOfDataEntries
		&& OtherChaosDestruction->MinMassToSpawn == MinMassToSpawn
		&& OtherChaosDestruction->MinImpulseToSpawn == MinImpulseToSpawn
		&& OtherChaosDestruction->MinSpeedToSpawn == MinSpeedToSpawn
		&& OtherChaosDestruction->DataSortingType == DataSortingType
		&& OtherChaosDestruction->SpawnMultiplierMin == SpawnMultiplierMin
		&& OtherChaosDestruction->SpawnMultiplierMax == SpawnMultiplierMax
		&& OtherChaosDestruction->RandomPositionMagnitude == RandomPositionMagnitude
		&& OtherChaosDestruction->BreakingRegionRadiusMultiplier == BreakingRegionRadiusMultiplier
		&& OtherChaosDestruction->VelocityOffset == VelocityOffset
		&& OtherChaosDestruction->InheritedVelocityMultiplier == InheritedVelocityMultiplier
		&& OtherChaosDestruction->RandomVelocityGenerationType == RandomVelocityGenerationType
		&& OtherChaosDestruction->RandomVelocityMagnitude == RandomVelocityMagnitude
		&& OtherChaosDestruction->SpreadAngleMax == SpreadAngleMax
		&& OtherChaosDestruction->VelocityMagnitudeMax == VelocityMagnitudeMax
		&& OtherChaosDestruction->DebugType == DebugType
		&& OtherChaosDestruction->ParticleIndexToProcess == ParticleIndexToProcess;
}

int32 UNiagaraDataInterfaceChaosDestruction::PerInstanceDataSize()const
{
	return sizeof(FNDIChaosDestruction_InstanceData);
}

bool UNiagaraDataInterfaceChaosDestruction::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIChaosDestruction_InstanceData* InstData = new (PerInstanceData) FNDIChaosDestruction_InstanceData();

	LastSpawnedPointID = -1;
	PrevLastSpawnedPointID = -1;
	LastSpawnTime = -1.0f;

	InitParticleDataArray(InstData->ParticleDataArray);

	return true;
}

void UNiagaraDataInterfaceChaosDestruction::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIChaosDestruction_InstanceData* InstData = (FNDIChaosDestruction_InstanceData*)PerInstanceData;
	InstData->~FNDIChaosDestruction_InstanceData();
}

bool UNiagaraDataInterfaceChaosDestruction::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	return false;
}

// Returns the signature of all the functions avaialable in the data interface
void UNiagaraDataInterfaceChaosDestruction::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	{
		// GetPosition
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPositionName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));			// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetPosition",
			"Helper function returning the position value for a given ..."));

		OutFunctions.Add(Sig);
	}

	{
		// GetNormal
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNormalName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Normal")));			// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetNormal",
			"Helper function returning the normal value for a given ..."));

		OutFunctions.Add(Sig);
	}

	{
		// GetVelocity
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetVelocityName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Velocity")));			// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetVelocity",
			"Helper function returning the velocity value for a given ..."));

		OutFunctions.Add(Sig);
	}

	{
		// GetAngularVelocity
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetAngularVelocityName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("AngularVelocity")));	// Vector3 Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetAngularVelocity",
			"Helper function returning the angular velocity value for a given ..."));

		OutFunctions.Add(Sig);
	}

	{
		// GetExtentMin
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetExtentMinName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ExtentMin")));		// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetExtentMin",
			"Helper function returning the min extent value for a given ..."));

		OutFunctions.Add(Sig);
	}

	{
		// GetExtentMax
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetExtentMaxName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ExtentMax")));		// Float Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetExtentMin",
			"Helper function returning the max extent value for a given ..."));

		OutFunctions.Add(Sig);
	}

	{
		// GetParticleIdsToSpawnAtTime
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetParticleIdsToSpawnAtTimeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));	// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time")));		    // Time in
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MinID")));			// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("MaxID")));			// Int Out
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));		    // Int Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetParticleIdsToSpawnAtTime",
			"Returns the count and IDs of the particles that should spawn for a given time value."));

		OutFunctions.Add(Sig);
	}

	{
		// GetPointType
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetPointTypeName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Type")));				// Int Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetPointType",
			"Helper function returning the type value for a given particle when spawned.\n"));

		OutFunctions.Add(Sig);
	}

	{
		// GetColor
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetColorName;
		Sig.bMemberFunction = true;
		Sig.bRequiresContext = false;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("DestructionData")));		// DestructionData in
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ParticleID")));			// ParticleID In		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));			// Color Out

		Sig.SetDescription(LOCTEXT("DataInterfaceChaos_GetColor",
			"Helper function returning the color for a given particle when spawned."));

		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetPosition);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetNormal);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetVelocity);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetAngularVelocity);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetExtentMin);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetExtentMax);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetParticleIdsToSpawnAtTime);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetPointType);
DEFINE_NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetColor);

void UNiagaraDataInterfaceChaosDestruction::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == GetPositionName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetPosition)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetNormalName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetNormal)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetVelocityName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetVelocity)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetAngularVelocityName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetAngularVelocity)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetExtentMinName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetExtentMin)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetExtentMaxName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetExtentMax)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetParticleIdsToSpawnAtTimeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 3)
	{
		TNDIParamBinder<0, float, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetParticleIdsToSpawnAtTime)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetPointTypeName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 1)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetPointType)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else if (BindingInfo.Name == GetColorName && BindingInfo.GetNumInputs() == 2 && BindingInfo.GetNumOutputs() == 4)
	{
		TNDIParamBinder<0, int32, NDI_FUNC_BINDER(UNiagaraDataInterfaceChaosDestruction, GetColor)>::Bind(this, BindingInfo, InstanceData, OutFunc);
	}
	else
	{
		UE_LOG(LogTemp, Error,
			TEXT("Could not find data interface function:\n\tName: %s\n\tInputs: %i\n\tOutputs: %i"),
			*BindingInfo.Name.ToString(), BindingInfo.GetNumInputs(), BindingInfo.GetNumOutputs());
		OutFunc = FVMExternalFunction();
	}
}

void UNiagaraDataInterfaceChaosDestruction::InitParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray)
{
	ParticleDataArray.Empty();
}

void UNiagaraDataInterfaceChaosDestruction::BuildPDBRigidSolverArray()
{
#if INCLUDE_CHAOS
	// Validate the ChaosSolverActor parameter from the UI
	// if no solver was specified use the always existing worldSolver
	// Duplicate items need to be skipped
	if (!ChaosSolverActorSet.Num())
	{
		if (TSharedPtr<FPhysScene_Chaos> WorldSolver = FPhysScene_Chaos::GetInstance())
		{
			if (Chaos::PBDRigidsSolver* Solver = WorldSolver->GetSolver())
			{
				PDBRigidSolverArray.Add(Solver);
			}
		}
	}
	else
	{
		for (AChaosSolverActor* ChaosSolverActorObject : ChaosSolverActorSet)
		{
			if (ChaosSolverActorObject)
			{
				if (Chaos::PBDRigidsSolver* Solver = ChaosSolverActorObject->GetSolver())
				{
					PDBRigidSolverArray.Add(Solver);
				}
			}
		}
	}
#endif
}

DEFINE_LOG_CATEGORY_STATIC(LogChaosNiagaraCollision, Verbose, All);

void UNiagaraDataInterfaceChaosDestruction::BuildCollisionParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray)
{
#if INCLUDE_CHAOS
	InitParticleDataArray(ParticleDataArray);

	int32 IdxSolver = 0;
	for (Chaos::PBDRigidsSolver* PDBRigidSolver : PDBRigidSolverArray)
	{
		if (PDBRigidSolver->GetSolverTime() == 0.f)
		{
			continue;
		}

		Chaos::PBDRigidsSolver::FCollisionData CollisionDataObject = PDBRigidSolver->GetCollisionData();
		Chaos::PBDRigidsSolver::FCollisionDataArray CollisionDataArray = CollisionDataObject.CollisionDataArray;

		if (CollisionDataArray.Num() == 0)
		{
			continue;
		}

		if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_SortByMassMaxToMin)
		{
			CollisionDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::CollisionDataSortByMassPredicateMaxToMin);
		}
		else if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_SortByMassMinToMax)
		{
			CollisionDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::CollisionDataSortByMassPredicateMinToMax);
		}
		else if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_RandomShuffle)
		{
			CollisionDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::CollisionDataRandomShuffleSortPredicate);
		}

		int32 NumCollisionsUsedToSpawn = 0;
		float LastDataTimeProcessedMax = -1.f;
		for (int32 IdxCollision = 0; IdxCollision < FMath::Min(CollisionDataObject.NumCollisions, CollisionDataArray.Num()); ++IdxCollision)
		{
			Chaos::TCollisionData<float, 3> CollisionData = CollisionDataArray[IdxCollision];

			if (CollisionData.Time <= LastDataTimeProcessedArray[IdxSolver])
			{
				continue;
			}
			LastDataTimeProcessedMax = FMath::Max(LastDataTimeProcessedMax, CollisionData.Time);

			if (NumCollisionsUsedToSpawn >= MaxNumberOfDataEntries)
			{
				break;
			}

			FVector CollisionLocation = CollisionData.Location;
			FVector CollisionVelocity1 = CollisionData.Velocity1;
			FVector CollisionVelocity2 = CollisionData.Velocity2;
			FVector CollisionNormal = CollisionData.Normal;
			ensure(CollisionData.Mass1 > 0.01);
			ensure(CollisionData.Mass2 > 0.01);
			float CollisionMass = FMath::Max(CollisionData.Mass1, CollisionData.Mass2);
			FVector AccumulatedImpulse = CollisionData.AccumulatedImpulse;
			int32 ParticleIndex = CollisionData.ParticleIndex;
			ensure(ParticleIndex >= 0);

			if (ParticleIndexToProcess != -1 && ParticleIndex != ParticleIndexToProcess)
			{
				continue;
			}

			if (MinSpeedToSpawn > 0.f && FMath::Max(CollisionVelocity1.Size(), CollisionVelocity2.Size()) < MinSpeedToSpawn)
			{
				continue;
			}

			if (MinImpulseToSpawn > 0.f && AccumulatedImpulse.Size() < MinImpulseToSpawn)
			{
				continue;
			}

			if (MinMassToSpawn > 0.0 && CollisionMass < MinMassToSpawn)
			{
				continue;
			}

			NumCollisionsUsedToSpawn++;

			int32 NumParticles = FMath::RandRange((int)SpawnMultiplierMin, (int)FMath::Max(SpawnMultiplierMin, SpawnMultiplierMax));
			for (int32 Idx = 0; Idx < NumParticles; ++Idx)
			{
				FVector RandomPosition(FMath::FRandRange(-RandomPositionMagnitude, RandomPositionMagnitude),
									   FMath::FRandRange(-RandomPositionMagnitude, RandomPositionMagnitude),
									   FMath::FRandRange(-RandomPositionMagnitude, RandomPositionMagnitude));

				FVector ParticleVelocity;
				if (RandomVelocityGenerationType == ERandomVelocityGenerationTypeEnum::ChaosNiagara_RandomVelocityGenerationType_RandomDistribution)
				{
					FVector RandomVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
					RandomVector.Normalize();

					ParticleVelocity = RandomVector * FMath::FRandRange(RandomVelocityMagnitude.X, FMath::Max(RandomVelocityMagnitude.X, RandomVelocityMagnitude.Y)) + VelocityOffset;
				}
				else if (RandomVelocityGenerationType == ERandomVelocityGenerationTypeEnum::ChaosNiagara_RandomVelocityGenerationType_CollisionNormalBased)
				{
					CollisionNormal.Normalize();

					// Create a random point in the plane perpendicular to N
					float W = FMath::Tan(FMath::DegreesToRadians(SpreadAngleMax));
					float X = CollisionLocation.X + FMath::FRandRange(-W, W);
					float Y = CollisionLocation.Y + FMath::FRandRange(-W, W);
					float Z = CollisionLocation.Z - (X - CollisionLocation.X) * CollisionNormal.X / CollisionNormal.Z - (Y - CollisionLocation.Y) * CollisionNormal.Y / CollisionNormal.Z;
					FVector PointInPlane(X, Y, Z);
					FVector NewVelocity = PointInPlane + CollisionNormal;
					NewVelocity.Normalize();
					ParticleVelocity = NewVelocity * FMath::FRandRange(RandomVelocityMagnitude.X, FMath::Max(RandomVelocityMagnitude.X, RandomVelocityMagnitude.Y)) + VelocityOffset;
				}
				// @todo (gustav) : Implement this
//				else if (RandomVelocityGenerationType == ERandomVelocityGenerationTypeEnum::ChaosNiagara_RandomVelocityGenerationType_NRandomSpread)
//				{
//				}

				// Clamp velocity
				FVector ComputedVelocity = (CollisionVelocity1 - CollisionVelocity2) * InheritedVelocityMultiplier + ParticleVelocity;
				if (VelocityMagnitudeMax > 0.f && ComputedVelocity.Size() > VelocityMagnitudeMax)
				{
					ComputedVelocity.Normalize();
					ComputedVelocity *= VelocityMagnitudeMax;
				}

				FVector ParticleColor = FVector::OneVector;
				if (DebugType == EDebugTypeEnum::ChaosNiagara_DebugType_ColorBySolver)
				{
					ParticleColor = ColorArray[IdxSolver % ColorArray.Num()];
				}
				else if (DebugType == EDebugTypeEnum::ChaosNiagara_DebugType_ColorByParticleIndex)
				{
					ParticleColor = ColorArray[ParticleIndex % ColorArray.Num()];
				}

				// Store particle data in the array
				FNDIChaosDestruction_InstanceData::FParticleData ParticleData{
					CollisionLocation + RandomPosition,
					CollisionNormal,
					ComputedVelocity,
					ParticleColor,
					IdxSolver
				};
				ParticleDataArray.Add(ParticleData);
			}
		}
		LastDataTimeProcessedArray[IdxSolver] = LastDataTimeProcessedMax;

		IdxSolver++;
	}
#endif
}

void UNiagaraDataInterfaceChaosDestruction::BuildBreakingParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray)
{
#if INCLUDE_CHAOS
	InitParticleDataArray(ParticleDataArray);

	int32 IdxSolver = 0;
	for (Chaos::PBDRigidsSolver* PDBRigidSolver : PDBRigidSolverArray)
	{
		if (PDBRigidSolver->GetSolverTime() == 0.f)
		{
			continue;
		}

		Chaos::PBDRigidsSolver::FBreakingData BreakingDataObject = PDBRigidSolver->GetBreakingData();
		Chaos::PBDRigidsSolver::FBreakingDataArray BreakingDataArray = BreakingDataObject.BreakingDataArray;

		if (BreakingDataArray.Num() == 0)
		{
			continue;
		}

		if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_SortByMassMaxToMin)
		{
			BreakingDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::BreakingDataSortByMassPredicateMaxToMin);
		}
		else if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_SortByMassMinToMax)
		{
			BreakingDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::BreakingDataSortByMassPredicateMinToMax);
		}
		else if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_RandomShuffle)
		{
			BreakingDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::BreakingDataRandomShuffleSortPredicate);
		}

		int32 NumBreakingsUsedToSpawn = 0;
		float LastDataTimeProcessedMax = -1.f;
		for (int32 IdxBreaking = 0; IdxBreaking < FMath::Min(BreakingDataObject.NumBreakings, BreakingDataArray.Num()); ++IdxBreaking)
		{
			Chaos::TBreakingData<float, 3> BreakingData = BreakingDataArray[IdxBreaking];

			if (BreakingData.Time <= LastDataTimeProcessedArray[IdxSolver])
			{
				continue;
			}
			LastDataTimeProcessedMax = FMath::Max(LastDataTimeProcessedMax, BreakingData.Time);

			if (NumBreakingsUsedToSpawn >= MaxNumberOfDataEntries)
			{
				break;
			}

			FVector BreakingLocation = BreakingData.BreakingRegionCentroid;
			float BreakingRadius = BreakingData.BreakingRegionRadius;
			FVector BreakingVelocity = BreakingData.Velocity;
			FVector BreakingNormal = BreakingData.BreakingRegionNormal;
			ensure(BreakingData.Mass > 0.01f);
			float BreakingMass = BreakingData.Mass;		
			int32 ParticleIndex = BreakingData.ParticleIndex;
			ensure(ParticleIndex >= 0);

			if (ParticleIndexToProcess != -1 && ParticleIndex != ParticleIndexToProcess)
			{
				continue;
			}

			if (MinSpeedToSpawn > 0.f && BreakingVelocity.Size() < MinSpeedToSpawn)
			{
				continue;
			}

			if (MinMassToSpawn > 0.0 && BreakingMass < MinMassToSpawn)
			{
				continue;
			}

			NumBreakingsUsedToSpawn++;

			int32 NumParticles = FMath::RandRange((int)SpawnMultiplierMin, (int)FMath::Max(SpawnMultiplierMin, SpawnMultiplierMax));
			float BreakingRadiusScaled = BreakingRadius * BreakingRegionRadiusMultiplier;
			for (int32 Idx = 0; Idx < NumParticles; ++Idx)
			{
				// Compute position
				FVector RandomPosition(FMath::FRandRange(-BreakingRadiusScaled, BreakingRadiusScaled),
									   FMath::FRandRange(-BreakingRadiusScaled, BreakingRadiusScaled),
									   FMath::FRandRange(-BreakingRadiusScaled, BreakingRadiusScaled));

				// Compute velocity
				FVector ParticleVelocity;
//				if (RandomVelocityGenerationType == ERandomVelocityGenerationTypeEnum::ChaosNiagara_RandomVelocityGenerationType_RandomDistribution)			
				{
					FVector RandomVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
					RandomVector.Normalize();

					ParticleVelocity = RandomVector * FMath::FRandRange(RandomVelocityMagnitude.X, FMath::Max(RandomVelocityMagnitude.X, RandomVelocityMagnitude.Y)) + VelocityOffset;
				}

				// Clamp computed velocity
				FVector ComputedVelocity = BreakingVelocity * InheritedVelocityMultiplier + ParticleVelocity;
				if (VelocityMagnitudeMax > 0.f && ComputedVelocity.Size() > VelocityMagnitudeMax)
				{
					ComputedVelocity.Normalize();
					ComputedVelocity *= VelocityMagnitudeMax;
				}

				FVector ParticleColor = FVector::OneVector;
				if (DebugType == EDebugTypeEnum::ChaosNiagara_DebugType_ColorBySolver)
				{
					ParticleColor = ColorArray[IdxSolver % ColorArray.Num()];
				}
				else if (DebugType == EDebugTypeEnum::ChaosNiagara_DebugType_ColorByParticleIndex)
				{
					ParticleColor = ColorArray[ParticleIndex % ColorArray.Num()];
				}

				// Store particle data in the array
				FNDIChaosDestruction_InstanceData::FParticleData ParticleData{
					BreakingLocation + RandomPosition,
					BreakingNormal,
					ComputedVelocity,
					ParticleColor,
					IdxSolver
				};
				ParticleDataArray.Add(ParticleData);
			}
		}
		LastDataTimeProcessedArray[IdxSolver] = LastDataTimeProcessedMax;

		IdxSolver++;
	}

#endif
}

void UNiagaraDataInterfaceChaosDestruction::BuildTrailingParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray)
{
#if INCLUDE_CHAOS
	InitParticleDataArray(ParticleDataArray);

	int32 IdxSolver = 0;
	for (Chaos::PBDRigidsSolver* PDBRigidSolver : PDBRigidSolverArray)
	{
		const float CurrentTime = PDBRigidSolver->GetSolverTime();
		if (CurrentTime == 0.f)
		{
			continue;
		}
		
		//float Dt = 1.f / DataProcessFrequency;
		//if (LastDataTimeProcessedArray[IdxSolver] != -1.f &&
		//	CurrentTime < LastDataTimeProcessedArray[IdxSolver] + Dt)
		//{
		//	continue;
		//}
		//LastDataTimeProcessedArray[IdxSolver] = CurrentTime;

		Chaos::PBDRigidsSolver::FTrailingData TrailingDataObject = PDBRigidSolver->GetTrailingData();
		Chaos::PBDRigidsSolver::FTrailingDataSet TrailingDataSet = TrailingDataObject.TrailingDataSet;

		if (TrailingDataSet.Num() == 0)
		{
			continue;
		}

//		if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_SortByMassMaxToMin)
//		{
//			TrailingDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::TrailingDataSortByMassPredicateMaxToMin);
//		}
//		else if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_SortByMassMinToMax)
//		{
//			TrailingDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::TrailingDataSortByMassPredicateMinToMax);
//		}
//		else if (DataSortingType == EDataSortTypeEnum::ChaosNiagara_DataSortType_RandomShuffle)
//		{
//			TrailingDataArray.Sort(UNiagaraDataInterfaceChaosDestruction::TrailingDataRandomShuffleSortPredicate);
//		}

		int32 NumTrailingsUsedToSpawn = 0;
//		for (int32 IdxTrailing = 0; IdxTrailing < TrailingDataSet.Num(); ++IdxTrailing)
//		{
//			Chaos::TTrailingData<float, 3> TrailingData = TrailingDataArray[IdxTrailing];
		for (auto& TrailingData: TrailingDataSet)
		{
			if (NumTrailingsUsedToSpawn >= MaxNumberOfDataEntries)
			{
				break;
			}

			FVector TrailingLocation = TrailingData.Location;
//			FVector TrailingNormal = TrailingData.Normal;
			FVector TrailingNormal = FVector(ForceInitToZero);
			float TrailingExtentMin = TrailingData.ExtentMin;
			float TrailingExtentMax = TrailingData.ExtentMax;
			FVector TrailingVelocity = TrailingData.Velocity;
			FVector TrailingAngularVelocity = TrailingData.AngularVelocity;
			ensure(TrailingData.Mass > 0.01);
			float TrailingMass = TrailingData.Mass;
			int32 ParticleIndex = TrailingData.ParticleIndex;
			ensure(ParticleIndex >= 0);

			//			FVector InvertedVelocity = TrailingVelocity * -1.f;
			//			InvertedVelocity.Normalize();
			//			TrailingNormal.Normalize();
			//			if (FVector::DotProduct(InvertedVelocity, TrailingNormal) > FMath::Cos(SpreadAngleMax))
			//			{
			//				continue;
			//			}

			if (ParticleIndexToProcess != -1 && ParticleIndex != ParticleIndexToProcess)
			{
				continue;
			}

			if (MinSpeedToSpawn > 0.f && TrailingVelocity.Size() < MinSpeedToSpawn)
			{
				continue;
			}

			if (MinMassToSpawn > 0.0 && TrailingMass < MinMassToSpawn)
			{
				continue;
			}

			NumTrailingsUsedToSpawn++;

			int32 NumParticles = FMath::RandRange((int)SpawnMultiplierMin, (int)FMath::Max(SpawnMultiplierMin, SpawnMultiplierMax));
			float TrailingRadiusScaled = TrailingExtentMin * BreakingRegionRadiusMultiplier;
			for (int32 Idx = 0; Idx < NumParticles; ++Idx)
			{
				// Compute position
				FVector RandomPosition(FMath::FRandRange(-TrailingRadiusScaled, TrailingRadiusScaled),
									   FMath::FRandRange(-TrailingRadiusScaled, TrailingRadiusScaled),
									   FMath::FRandRange(-TrailingRadiusScaled, TrailingRadiusScaled));

				// Compute velocity
				FVector ParticleVelocity;
				//				if (RandomVelocityGenerationType == ERandomVelocityGenerationTypeEnum::ChaosNiagara_RandomVelocityGenerationType_RandomDistribution)
				{
					FVector RandomVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
					RandomVector.Normalize();

					ParticleVelocity = RandomVector * FMath::FRandRange(RandomVelocityMagnitude.X, FMath::Max(RandomVelocityMagnitude.X, RandomVelocityMagnitude.Y)) + VelocityOffset;
				}

				// Clamp computed velocity
				FVector ComputedVelocity = TrailingVelocity * InheritedVelocityMultiplier + ParticleVelocity;
				if (VelocityMagnitudeMax > 0.f && ComputedVelocity.Size() > VelocityMagnitudeMax)
				{
					ComputedVelocity.Normalize();
					ComputedVelocity *= VelocityMagnitudeMax;
				}

				FVector ParticleColor = FVector::OneVector;
				if (DebugType == EDebugTypeEnum::ChaosNiagara_DebugType_ColorBySolver)
				{
					ParticleColor = ColorArray[IdxSolver % ColorArray.Num()];
				}
				else if (DebugType == EDebugTypeEnum::ChaosNiagara_DebugType_ColorByParticleIndex)
				{
					ParticleColor = ColorArray[ParticleIndex % ColorArray.Num()];
				}

				// Store particle data in the array
				FNDIChaosDestruction_InstanceData::FParticleData ParticleData{
					TrailingLocation + RandomPosition,
					TrailingNormal,
					ComputedVelocity,
					TrailingAngularVelocity,
					TrailingExtentMin,
					TrailingExtentMax,
					ParticleColor,
					IdxSolver
				};
				ParticleDataArray.Add(ParticleData);
			}
		}

		IdxSolver++;
	}
#endif
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetPosition(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
    VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;

			FVector V = InstData->ParticleDataArray[ParticleID].Position;

			*OutSampleX.GetDest() = V.X;
			*OutSampleY.GetDest() = V.Y;
			*OutSampleZ.GetDest() = V.Z;
		}

		ParticleIDParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetNormal(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;

			FVector V = InstData->ParticleDataArray[ParticleID].Normal;

			*OutSampleX.GetDest() = V.X;
			*OutSampleY.GetDest() = V.Y;
			*OutSampleZ.GetDest() = V.Z;
		}

		ParticleIDParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetVelocity(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;
			FVector V = InstData->ParticleDataArray[ParticleID].Velocity;

			*OutSampleX.GetDest() = V.X;
			*OutSampleY.GetDest() = V.Y;
			*OutSampleZ.GetDest() = V.Z;
		}

		ParticleIDParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetAngularVelocity(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;
			FVector W = InstData->ParticleDataArray[ParticleID].AngularVelocity;

			*OutSampleX.GetDest() = W.X;
			*OutSampleY.GetDest() = W.Y;
			*OutSampleZ.GetDest() = W.Z;
		}

		ParticleIDParam.Advance();
		OutSampleX.Advance();
		OutSampleY.Advance();
		OutSampleZ.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetExtentMin(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;
			float Value = InstData->ParticleDataArray[ParticleID].ExtentMin;

			*OutValue.GetDest() = Value;
		}

		ParticleIDParam.Advance();
		OutValue.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetExtentMax(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;
			float Value = InstData->ParticleDataArray[ParticleID].ExtentMax;

			*OutValue.GetDest() = Value;
		}

		ParticleIDParam.Advance();
		OutValue.Advance();
	}
}

// Returns the last index of the points that should be spawned at time t
template<typename TimeParamType>
void UNiagaraDataInterfaceChaosDestruction::GetParticleIdsToSpawnAtTime(FVectorVMContext& Context)
{
	TimeParamType TimeParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutMinValue(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutMaxValue(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutCountValue(Context);

	if (DoSpawn)
	{
		float CurrentTime = TimeParam.Get();
		float EllapsedTime = CurrentTime - LastSpawnTime;
		if (LastSpawnTime > 0.f && 
			EllapsedTime < 1.f / (float)DataProcessFrequency)
			return;
		
		if (DataSourceType == EDataSourceTypeEnum::ChaosNiagara_DataSourceType_Collision)
		{
			BuildCollisionParticleDataArray(InstData->ParticleDataArray);
		}
		else if (DataSourceType == EDataSourceTypeEnum::ChaosNiagara_DataSourceType_Breaking)
		{
			BuildBreakingParticleDataArray(InstData->ParticleDataArray);
		}
		else if (DataSourceType == EDataSourceTypeEnum::ChaosNiagara_DataSourceType_Trailing)
		{
			BuildTrailingParticleDataArray(InstData->ParticleDataArray);
		}

		int32 Min = LastSpawnedPointID + 1;
		int32 Count = InstData->ParticleDataArray.Num();
		int32 Max = Min + Count - 1;
		LastSpawnTime = CurrentTime;
		PrevLastSpawnedPointID = LastSpawnedPointID;
		LastSpawnedPointID = Max;

		*OutMinValue.GetDest() = Min;
		*OutMaxValue.GetDest() = Max;
		*OutCountValue.GetDest() = Count;

		TimeParam.Advance();
		OutMinValue.Advance();
		OutMaxValue.Advance();
		OutCountValue.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetPointType(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<int32> OutValue(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;

			int32 Value = 0;

			*OutValue.GetDest() = Value;
		}

		ParticleIDParam.Advance();
		OutValue.Advance();
	}
}

template<typename ParticleIDParamType>
void UNiagaraDataInterfaceChaosDestruction::GetColor(FVectorVMContext& Context)
{
	ParticleIDParamType ParticleIDParam(Context);
	VectorVM::FUserPtrHandler<FNDIChaosDestruction_InstanceData> InstData(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleR(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleG(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleB(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleA(Context);

	for (int32 i = 0; i < Context.NumInstances; ++i)
	{
		if (InstData->ParticleDataArray.Num())
		{
			int32 ParticleID = ParticleIDParam.Get();
			// Remap ParticleID
			ParticleID -= PrevLastSpawnedPointID + 1;

			FVector V = InstData->ParticleDataArray[ParticleID].Color;

			FLinearColor C = FLinearColor::White;
			C.R = V.X;
			C.G = V.Y;
			C.B = V.Z;
			C.A = 1.0;

			*OutSampleR.GetDest() = C.R;
			*OutSampleG.GetDest() = C.G;
			*OutSampleB.GetDest() = C.B;
			*OutSampleA.GetDest() = C.A;
		}

		ParticleIDParam.Advance();
		OutSampleR.Advance();
		OutSampleG.Advance();
		OutSampleB.Advance();
		OutSampleA.Advance();
	}
}

#undef LOCTEXT_NAMESPACE
