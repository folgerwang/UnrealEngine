/*
*/
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "NiagaraDataInterface.h"
#include "Chaos/ChaosSolver.h"
#include "Chaos/ChaosSolverActor.h"
#include "NiagaraDataInterfaceChaosDestruction.generated.h"

USTRUCT()
struct FChaosDestructionEvent
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	FVector Position;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	FVector Normal;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event") 
	FVector Velocity;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	FVector AngularVelocity;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	float ExtentMin;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	float ExtentMax;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	int32 ParticleID;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	float Time;

	UPROPERTY(EditAnywhere, Category = "Chaos Destruction Event")
	int32 Type;

	FChaosDestructionEvent()
		: Position(FVector::ZeroVector)
		, Normal(FVector::ZeroVector)
		, Velocity(FVector::ZeroVector)
		, AngularVelocity(FVector::ZeroVector)
		, ExtentMin(0.f)
		, ExtentMax(0.f)
		, ParticleID(-1)
		, Time(0.f)
		, Type(-1)
	{
	}

	inline bool operator==(const FChaosDestructionEvent& Other) const
	{
		if ((Other.Position != Position) || (Other.Normal != Normal)
			|| (Other.Velocity != Velocity) 
			|| (Other.AngularVelocity != AngularVelocity)
			|| (Other.ExtentMin != ExtentMin)
			|| (Other.ExtentMax != ExtentMax)
			|| (Other.ParticleID != ParticleID) || (Other.Time != Time)
			|| (Other.Type != Type))
			return false;

		return true;
	}

	inline bool operator!=(const FChaosDestructionEvent& Other) const
	{
		return !(*this == Other);
	}
};

struct FNDIChaosDestruction_InstanceData
{
	struct FParticleData
	{
		FVector Position;
		FVector Normal;
		FVector Velocity;
		FVector AngularVelocity;
		float ExtentMin;
		float ExtentMax;
		FVector Color;
		int32 SolverID;
	};
	TArray<FParticleData> ParticleDataArray;
};

UENUM(BlueprintType)
enum class EDataSortTypeEnum : uint8
{
	ChaosNiagara_DataSortType_NoSorting UMETA(DisplayName = "No Sorting"),
	ChaosNiagara_DataSortType_RandomShuffle UMETA(DisplayName = "Random Shuffle"),
	ChaosNiagara_DataSortType_SortByMassMaxToMin UMETA(DisplayName = "Sort by Mass - Max to Min"),
	ChaosNiagara_DataSortType_SortByMassMinToMax UMETA(DisplayName = "Sort by Mass - Min to Max"),
	//~~~
	//256th entry
	ChaosNiagara_Max                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ERandomVelocityGenerationTypeEnum : uint8
{
	ChaosNiagara_RandomVelocityGenerationType_RandomDistribution UMETA(DisplayName = "Random Distribution"),
	ChaosNiagara_RandomVelocityGenerationType_CollisionNormalBased UMETA(DisplayName = "Collision Normal Based (Collision Data Only)"),
//	ChaosNiagara_RandomVelocityGenerationType_NRandomSpread UMETA(DisplayName = "N Random Spread (Collision Data Only)"),
	//~~~
	//256th entry
	ChaosNiagara_Max                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDebugTypeEnum : uint8
{
	ChaosNiagara_DebugType_NoDebug UMETA(DisplayName = "No Debug"),
	ChaosNiagara_DebugType_ColorBySolver UMETA(DisplayName = "Color by Solver"),
	ChaosNiagara_DebugType_ColorByParticleIndex UMETA(DisplayName = "Color by ParticleIndex"),
	//~~~
	//256th entry
	ChaosNiagara_Max                UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EDataSourceTypeEnum : uint8
{
	ChaosNiagara_DataSourceType_Collision UMETA(DisplayName = "Collision Data"),
	ChaosNiagara_DataSourceType_Breaking UMETA(DisplayName = "Breaking Data"),
	ChaosNiagara_DataSourceType_Trailing UMETA(DisplayName = "Trailing Data"),
	//~~~
	//256th entry
	ChaosNiagara_Max                UMETA(Hidden)
};

/** Data Interface allowing sampling of Chaos Destruction data. */
UCLASS(EditInlineNew, Category = "Chaos Niagara", meta = (DisplayName = "Chaos Destruction Data"))
class CHAOSNIAGARA_API UNiagaraDataInterfaceChaosDestruction : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	/* Chaos Solver */
	UPROPERTY(EditAnywhere, Category = "Solver", meta = (DisplayName = "Chaos Solver"))
	TSet<AChaosSolverActor*> ChaosSolverActorSet;

	/**
	* Sorting method to sort the collision data
	*/
	UPROPERTY(EditAnywhere, Category = "Solver Data", meta = (DisplayName = "Data Source"))
	EDataSourceTypeEnum DataSourceType;

	/* Number of times the RBD collision data gets processed every second */
	UPROPERTY(EditAnywhere, Category = "Solver Data", meta = (DisplayName = "Data Process Frequency", UIMin = 0))
	int32 DataProcessFrequency;

	/* Maximum number of collisions used for spawning particles every time RBD collision data gets processed */
	UPROPERTY(EditAnywhere, Category = "Solver Data", meta = (DisplayName = "Max Number of Data Entries to Process", UIMin = 0))
	int32 MaxNumberOfDataEntries;

	/* Turn on/off spawning */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings", meta = (DisplayName = "Spawn Particles"))
	bool DoSpawn;

	/* Minimum accumulated impulse in a RBD collision to spawn particles from */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings - Thresholds to Spawn", meta = (DisplayName = "Min Impulse To Spawn Particles (Collision Data Only)", UIMin = 0.0))
	float MinImpulseToSpawn;

	/* Minimum speed in a RBD collision to spawn particles from */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings - Thresholds to Spawn", meta = (DisplayName = "Min Speed To Spawn Particles", UIMin = 0.0))
	float MinSpeedToSpawn;

	/* Minimum mass in a RBD collision to spawn particles from */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings - Thresholds to Spawn", meta = (DisplayName = "Min Mass To Spawn Particles", UIMin = 0.0))
	float MinMassToSpawn;

	/**
	* Sorting method to sort the collision data
	*/
	UPROPERTY(EditAnywhere, Category = "Spawn Settings - Sorting Solver Data", meta = (DisplayName = "Sorting Method"))
	EDataSortTypeEnum DataSortingType;

	/* For every collision random number of particles will be spawned in the range of [SpawnMultiplierMin, SpawnMultiplierMax]  */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings", meta = (DisplayName = "Spawn Multiplier Min", UIMin = 0))
	int32 SpawnMultiplierMin;

	/* For every collision random number of particles will be spawned in the range of [SpawnMultiplierMin, SpawnMultiplierMax]  */
	UPROPERTY(EditAnywhere, Category = "Spawn Settings", meta = (DisplayName = "Spawn Multiplier Max", UIMin = 0))
	int32 SpawnMultiplierMax;

	/* Random displacement value for the particle spawn position */
	UPROPERTY(EditAnywhere, Category = "Spawn Position Settings", meta = (DisplayName = "Spread Random Magnitude (Collision Data Only)", UIMin = 0.0))
	float RandomPositionMagnitude;

	/**/
	UPROPERTY(EditAnywhere, Category = "Spawn Position Settings", meta = (DisplayName = "Breaking Region Radius Multiplier (Breaking and Trailing Data Only)", UIMin = 0.0))
	float BreakingRegionRadiusMultiplier;

	/* How much of the collision velocity gets inherited */
	UPROPERTY(EditAnywhere, Category = "Velocity Settings - Inherited Velocity", meta = (DisplayName = "Inherited Velocity Multiplier"))
	float InheritedVelocityMultiplier;

	/**
	* The method used to create the random velocities for the newly spawned particles
	*/
	UPROPERTY(EditAnywhere, Category = "Velocity Settings - Random Generation", meta = (DisplayName = "Algorithm"))
	ERandomVelocityGenerationTypeEnum RandomVelocityGenerationType;

	/* Every particles will be spawned with random velocity with magnitude in the range of [RandomVelocityMagnitudeMin, RandomVelocityMagnitudeMax] */
	UPROPERTY(EditAnywhere, Category = "Velocity Settings", meta = (DisplayName = "Velocity Random Magnitude Min/Max", UIMin = 0.0))
	FVector2D RandomVelocityMagnitude;

	/**/
	UPROPERTY(EditAnywhere, Category = "Velocity Settings - Collision Data/Collision Normal Based or Breaking Data", meta = (DisplayName = "Spread Angle Max [Degrees]", UIMin = 0.0))
	float SpreadAngleMax;

	/* Offset value added to spawned particles velocity */
	UPROPERTY(EditAnywhere, Category = "Velocity Settings", meta = (DisplayName = "Velocity Offset"))
	FVector VelocityOffset;

	/* Clamp particles velocity */
	UPROPERTY(EditAnywhere, Category = "Velocity Settings", meta = (DisplayName = "Final Velocity Magnitude Maximum"))
	float VelocityMagnitudeMax;

	/* Debug visualization method */
	UPROPERTY(EditAnywhere, Category = "Debug Settings", meta = (DisplayName = "Debug Visualization"))
	EDebugTypeEnum DebugType;

	/* ParticleIndex to process collisionData for */
	UPROPERTY(EditAnywhere, Category = "Debug Settings", meta = (DisplayName = "ParticleIndex to Process"))
	int32 ParticleIndexToProcess;

	//----------------------------------------------------------------------------
	// UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//----------------------------------------------------------------------------
	// UNiagaraDataInterface Interface
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual int32 PerInstanceDataSize() const override;

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::CPUSim; }

	//----------------------------------------------------------------------------
	// EXPOSED FUNCTIONS
	template<typename ParticleIDParamType>
	void GetPosition(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetNormal(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetVelocity(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetAngularVelocity(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetExtentMin(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetExtentMax(FVectorVMContext& Context);

	template<typename TimeParamType>
	void GetParticleIdsToSpawnAtTime(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetPointType(FVectorVMContext& Context);

	template<typename ParticleIDParamType>
	void GetColor(FVectorVMContext& Context);

	// Sort predicates to sort CollisionData
	inline static bool CollisionDataSortByMassPredicateMaxToMin(const Chaos::TCollisionData<float, 3>& Lhs, const Chaos::TCollisionData<float, 3>& Rhs)
	{
		return FMath::Max(Lhs.Mass1, Lhs.Mass2) > FMath::Max(Rhs.Mass1, Rhs.Mass2);
	}

	inline static bool CollisionDataSortByMassPredicateMinToMax(const Chaos::TCollisionData<float, 3>& Lhs, const Chaos::TCollisionData<float, 3>& Rhs)
	{
		return FMath::Max(Lhs.Mass1, Lhs.Mass2) < FMath::Max(Rhs.Mass1, Rhs.Mass2);
	}

	inline static bool CollisionDataRandomShuffleSortPredicate(const Chaos::TCollisionData<float, 3>& Lhs, const Chaos::TCollisionData<float, 3>& Rhs)
	{
		return FMath::FRand() < 0.5f;
	}

	inline static bool BreakingDataSortByMassPredicateMaxToMin(const Chaos::TBreakingData<float, 3>& Lhs, const Chaos::TBreakingData<float, 3>& Rhs)
	{
		return Lhs.Mass > Rhs.Mass;
	}

	inline static bool BreakingDataSortByMassPredicateMinToMax(const Chaos::TBreakingData<float, 3>& Lhs, const Chaos::TBreakingData<float, 3>& Rhs)
	{
		return Lhs.Mass < Rhs.Mass;
	}

	inline static bool BreakingDataRandomShuffleSortPredicate(const Chaos::TBreakingData<float, 3>& Lhs, const Chaos::TBreakingData<float, 3>& Rhs)
	{
		return FMath::FRand() < 0.5f;
	}

	inline static bool TrailingDataSortByMassPredicateMaxToMin(const Chaos::TTrailingData<float, 3>& Lhs, const Chaos::TTrailingData<float, 3>& Rhs)
	{
		return Lhs.Mass > Rhs.Mass;
	}

	inline static bool TrailingDataSortByMassPredicateMinToMax(const Chaos::TTrailingData<float, 3>& Lhs, const Chaos::TTrailingData<float, 3>& Rhs)
	{
		return Lhs.Mass < Rhs.Mass;
	}

	inline static bool TrailingDataRandomShuffleSortPredicate(const Chaos::TTrailingData<float, 3>& Lhs, const Chaos::TTrailingData<float, 3>& Rhs)
	{
		return FMath::FRand() < 0.5f;
	}

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	void InitParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray);
	void BuildCollisionParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray);
	void BuildBreakingParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray);
	void BuildTrailingParticleDataArray(TArray<FNDIChaosDestruction_InstanceData::FParticleData>& ParticleDataArray);
	void BuildPDBRigidSolverArray();

	UPROPERTY()
	int32 LastSpawnedPointID;

	UPROPERTY()
	int32 PrevLastSpawnedPointID;

	UPROPERTY()
	float LastSpawnTime;

	UPROPERTY()
	TArray<float> LastDataTimeProcessedArray;

	// Colors for debugging particles
	TArray<FVector> ColorArray;

#if INCLUDE_CHAOS
	TArray<Chaos::PBDRigidsSolver*> PDBRigidSolverArray;
#endif
};

