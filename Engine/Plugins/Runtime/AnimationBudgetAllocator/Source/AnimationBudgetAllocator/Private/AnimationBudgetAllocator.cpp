// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AnimationBudgetAllocator.h"
#include "Engine/Engine.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimStats.h"
#include "AnimationRuntime.h"
#include "AnimationBudgetAllocatorModule.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "DrawDebugHelpers.h"

DECLARE_CYCLE_STAT(TEXT("InitialTick"), STAT_AnimationBudgetAllocator_Update, STATGROUP_AnimationBudgetAllocator);

DECLARE_DWORD_COUNTER_STAT(TEXT("Num Registered Components"), STAT_AnimationBudgetAllocator_NumRegisteredComponents, STATGROUP_AnimationBudgetAllocator);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Ticked Components"), STAT_AnimationBudgetAllocator_NumTickedComponents, STATGROUP_AnimationBudgetAllocator);

DECLARE_DWORD_COUNTER_STAT(TEXT("Demand"), STAT_AnimationBudgetAllocator_Demand, STATGROUP_AnimationBudgetAllocator);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Budget"), STAT_AnimationBudgetAllocator_Budget, STATGROUP_AnimationBudgetAllocator);
DECLARE_FLOAT_COUNTER_STAT(TEXT("Average Work Unit (ms)"), STAT_AnimationBudgetAllocator_AverageWorkUnitTime, STATGROUP_AnimationBudgetAllocator);
DECLARE_DWORD_COUNTER_STAT(TEXT("Always Tick"), STAT_AnimationBudgetAllocator_AlwaysTick, STATGROUP_AnimationBudgetAllocator);
DECLARE_DWORD_COUNTER_STAT(TEXT("Throttled"), STAT_AnimationBudgetAllocator_Throttled, STATGROUP_AnimationBudgetAllocator);
DECLARE_DWORD_COUNTER_STAT(TEXT("Interpolated"), STAT_AnimationBudgetAllocator_Interpolated, STATGROUP_AnimationBudgetAllocator);
DECLARE_FLOAT_COUNTER_STAT(TEXT("SmoothedBudgetPressure"), STAT_AnimationBudgetAllocator_SmoothedBudgetPressure, STATGROUP_AnimationBudgetAllocator);


CSV_DEFINE_CATEGORY(AnimationBudget, true);

bool FAnimationBudgetAllocator::bCachedEnabled = false;

static int32 GAnimationBudgetEnabled = 0;

static FAutoConsoleVariableRef CVarSkelBatch_Enabled(
	TEXT("a.Budget.Enabled"),
	GAnimationBudgetEnabled,
	TEXT("Values: 0/1\n")
	TEXT("Controls whether the skeletal mesh batching system is enabled. Should be set when there are no running skeletal meshes."),
	ECVF_Scalability);

static float GBudgetInMs = 1.0f;

static FAutoConsoleVariableRef CVarSkelBatch_Budget(
	TEXT("a.Budget.BudgetMs"),
	GBudgetInMs,
	TEXT("Values > 0.1, Default = 1.0\n")
	TEXT("The time in milliseconds that we allocate for skeletal mesh work to be performed.\n")
	TEXT("When overbudget various other CVars come into play, such as a.Budget.AlwaysTickFalloffAggression and a.Budget.InterpolationFalloffAggression."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GBudgetInMs = FMath::Max(GBudgetInMs, 0.1f);
	}),
	ECVF_Scalability);

static float GMinQuality = 0.0f;

static FAutoConsoleVariableRef CVarSkelBatch_MinQuality(
	TEXT("a.Budget.MinQuality"),
	GMinQuality,
	TEXT("Values [0.0, 1.0], Default = 0.0\n")
	TEXT("The minimum quality metric allowed. Quality is determined simply by NumComponentsTickingThisFrame / NumComponentsThatWeNeedToTick.\n")
	TEXT("If this is anything other than 0.0 then we can potentially go over our time budget."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GMinQuality = FMath::Clamp(GMinQuality, 0.0f, 1.0f);
	}),
	ECVF_Scalability);

static int32 GMaxTickRate = 10;

static FAutoConsoleVariableRef CVarSkelBatch_MaxTickRate(
	TEXT("a.Budget.MaxTickRate"),
	GMaxTickRate,
	TEXT("Values >= 1, Default = 10\n")
	TEXT("The maximum tick rate we allow. If this is set then we can potentially go over budget, but keep quality of individual meshes to a reasonable level.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GMaxTickRate = FMath::Max(GMaxTickRate, 1);
	}),
	ECVF_Scalability);

static float GWorkUnitSmoothingSpeed = 5.0f;

static FAutoConsoleVariableRef CVarSkelBatch_WorkUnitSmoothingSpeed(
	TEXT("a.Budget.WorkUnitSmoothingSpeed"),
	GWorkUnitSmoothingSpeed,
	TEXT("Values > 0.1, Default = 5.0\n")
	TEXT("The speed at which the average work unit converges on the measured amount.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GWorkUnitSmoothingSpeed = FMath::Max(GWorkUnitSmoothingSpeed, 0.1f);
	}));

static float GAlwaysTickFalloffAggression = 0.8f;

static FAutoConsoleVariableRef CVarSkelBatch_AlwaysTickFalloffAggression(
	TEXT("a.Budget.AlwaysTickFalloffAggression"),
	GAlwaysTickFalloffAggression,
	TEXT("Range [0.1, 0.9], Default = 0.8\n")
	TEXT("Controls the rate at which 'always ticked' components falloff under load.\n")
	TEXT("Higher values mean that we reduce the number of always ticking components by a larger amount when the allocated time budget is exceeded."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GAlwaysTickFalloffAggression = FMath::Clamp(GAlwaysTickFalloffAggression, 0.1f, 0.9f);
	}),
	ECVF_Scalability);

static float GInterpolationFalloffAggression = 0.4f;

static FAutoConsoleVariableRef CVarSkelBatch_InterpolationFalloffAggression(
	TEXT("a.Budget.InterpolationFalloffAggression"),
	GInterpolationFalloffAggression,
	TEXT("Range [0.1, 0.9], Default = 0.4\n")
	TEXT("Controls the rate at which interpolated components falloff under load.\n")
	TEXT("Higher values mean that we reduce the number of interpolated components by a larger amount when the allocated time budget is exceeded.\n")
	TEXT("Components are only interpolated when the time budget is exceeded."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GInterpolationFalloffAggression = FMath::Clamp(GInterpolationFalloffAggression, 0.1f, 0.9f);
	}),
	ECVF_Scalability);

static int32 GInterpolationMaxRate = 6;

static FAutoConsoleVariableRef CVarSkelBatch_InterpolationMaxRate(
	TEXT("a.Budget.InterpolationMaxRate"),
	GInterpolationMaxRate,
	TEXT("Values > 1, Default = 6\n")
	TEXT("Controls the rate at which ticks happen when interpolating.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GInterpolationMaxRate = FMath::Max(GInterpolationMaxRate, 2);
	}),
	ECVF_Scalability);

static int32 GMaxInterpolatedComponents = 16;

static FAutoConsoleVariableRef CVarSkelBatch_MaxInterpolatedComponents(
	TEXT("a.Budget.MaxInterpolatedComponents"),
	GMaxInterpolatedComponents,
	TEXT("Range >= 0, Default = 16\n")
	TEXT("Max number of components to inteprolate before we start throttling.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GMaxInterpolatedComponents = FMath::Max(GMaxInterpolatedComponents, 0);
	}),
	ECVF_Scalability);

static float GInterpolationTickMultiplier = 0.75f;

static FAutoConsoleVariableRef CVarSkelBatch_InterpolationTickMultiplier(
	TEXT("a.Budget.InterpolationTickMultiplier"),
	GInterpolationTickMultiplier,
	TEXT("Range [0.1, 0.9], Default = 0.75\n")
	TEXT("Controls the expected value an amortized interpolated tick will take compared to a 'normal' tick.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GInterpolationTickMultiplier = FMath::Clamp(GInterpolationTickMultiplier, 0.1f, 0.9f);
	}),
	ECVF_Scalability);

static float GInitialEstimatedWorkUnitTimeMs = 0.08f;

static FAutoConsoleVariableRef CVarSkelBatch_InitialEstimatedWorkUnitTime(
	TEXT("a.Budget.InitialEstimatedWorkUnitTime"),
	GInitialEstimatedWorkUnitTimeMs,
	TEXT("Values > 0.0, Default = 0.08\n")
	TEXT("Controls the time in milliseconds we expect, on average, for a skeletal mesh component to execute.\n")
	TEXT("The value only applies for the first tick of a component, after which we use the real time the tick takes.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GInitialEstimatedWorkUnitTimeMs = FMath::Max(GInitialEstimatedWorkUnitTimeMs, KINDA_SMALL_NUMBER);
	}),
	ECVF_Scalability);

static int32 GMaxTickedOffsreenComponents = 4;

static FAutoConsoleVariableRef CVarSkelBatch_MaxTickedOffsreenComponents(
	TEXT("a.Budget.MaxTickedOffsreen"),
	GMaxTickedOffsreenComponents,
	TEXT("Values >= 1, Default = 4\n")
	TEXT("The maximum number of offscreen components we tick (most significant first)\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GMaxTickedOffsreenComponents = FMath::Max(GMaxTickedOffsreenComponents, 1);
	}),
	ECVF_Scalability);

static int32 GStateChangeThrottleInFrames = 30;

static FAutoConsoleVariableRef CVarSkelBatch_StateChangeThrottleInFrames(
	TEXT("a.Budget.StateChangeThrottleInFrames"),
	GStateChangeThrottleInFrames,
	TEXT("Range [1, 255], Default = 30\n")
	TEXT("Prevents throttle values from changing too often due to system and load noise.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GStateChangeThrottleInFrames = FMath::Clamp(GStateChangeThrottleInFrames, 1, 255);
	}),
	ECVF_Scalability);

static float GBudgetFactorBeforeReducedWork = 1.5f;

static FAutoConsoleVariableRef CVarSkelBatch_BudgetFactorBeforeReducedWork(
	TEXT("a.Budget.BudgetFactorBeforeReducedWork"),
	GBudgetFactorBeforeReducedWork,
	TEXT("Range > 1, Default = 1.5\n")
	TEXT("Reduced work will be delayed until budget pressure goes over this amount.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GBudgetFactorBeforeReducedWork = FMath::Max(GBudgetFactorBeforeReducedWork, 1.0f);
	}),
	ECVF_Scalability);

static float GBudgetFactorBeforeReducedWorkEpsilon = 0.25f;

static FAutoConsoleVariableRef CVarSkelBatch_BudgetFactorBeforeReducedWorkEpsilon(
	TEXT("a.Budget.BudgetFactorBeforeReducedWorkEpsilon"),
	GBudgetFactorBeforeReducedWorkEpsilon,
	TEXT("Range > 0.0, Default = 0.25\n")
	TEXT("Increased work will be delayed until budget pressure goes under a.Budget.BudgetFactorBeforeReducedWork minus this amount.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GBudgetFactorBeforeReducedWorkEpsilon = FMath::Max(GBudgetFactorBeforeReducedWorkEpsilon, 0.0f);
	}),
	ECVF_Scalability);

static float GBudgetPressureSmoothingSpeed = 3.0f;

static FAutoConsoleVariableRef CVarSkelBatch_BudgetPressureSmoothingSpeed(
	TEXT("a.Budget.BudgetPressureSmoothingSpeed"),
	GBudgetPressureSmoothingSpeed,
	TEXT("Range > 0.0, Default = 3.0\n")
	TEXT("How much to smooth the budget pressure value used to throttle reduced work.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GBudgetPressureSmoothingSpeed = FMath::Max(GBudgetPressureSmoothingSpeed, KINDA_SMALL_NUMBER);
	}),
	ECVF_Scalability);

static int32 GReducedWorkThrottleMinInFrames = 2;

static FAutoConsoleVariableRef CVarSkelBatch_ReducedWorkThrottleMinInFrames(
	TEXT("a.Budget.ReducedWorkThrottleMinInFrames"),
	GReducedWorkThrottleMinInFrames,
	TEXT("Range [1, 255], Default = 2\n")
	TEXT("Prevents reduced work from changing too often due to system and load noise. Min value used when over budget pressure (i.e. aggressive reduction).\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GReducedWorkThrottleMinInFrames = FMath::Clamp(GReducedWorkThrottleMinInFrames, 1, 255);
	}),
	ECVF_Scalability);

static int32 GReducedWorkThrottleMaxInFrames = 20;

static FAutoConsoleVariableRef CVarSkelBatch_ReducedWorkThrottleMaxInFrames(
	TEXT("a.Budget.ReducedWorkThrottleMaxInFrames"),
	GReducedWorkThrottleMaxInFrames,
	TEXT("Range [1, 255], Default = 20\n")
	TEXT("Prevents reduced work from changing too often due to system and load noise. Max value used when under budget pressure.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GReducedWorkThrottleMaxInFrames = FMath::Clamp(GReducedWorkThrottleMaxInFrames, 1, 255);
	}),
	ECVF_Scalability);

static float GBudgetFactorBeforeAggressiveReducedWork = 2.0f;

static FAutoConsoleVariableRef CVarSkelBatch_BudgetFactorBeforeAggressiveReducedWork(
	TEXT("a.Budget.BudgetFactorBeforeAggressiveReducedWork"),
	GBudgetFactorBeforeAggressiveReducedWork,
	TEXT("Range > 1, Default = 2.0\n")
	TEXT("Reduced work will be applied more rapidly when budget pressure goes over this amount.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GBudgetFactorBeforeAggressiveReducedWork = FMath::Max(GBudgetFactorBeforeAggressiveReducedWork, 1.0f);
	}),
	ECVF_Scalability);

static int32 GReducedWorkThrottleMaxPerFrame = 4;

static FAutoConsoleVariableRef CVarSkelBatch_ReducedWorkThrottleMaxPerFrame(
	TEXT("a.Budget.ReducedWorkThrottleMaxPerFrame"),
	GReducedWorkThrottleMaxPerFrame,
	TEXT("Range [1, 255], Default = 4\n")
	TEXT("Controls the max number of components that are switched to/from reduced work per tick.\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GReducedWorkThrottleMaxPerFrame = FMath::Clamp(GReducedWorkThrottleMaxPerFrame, 1, 255);
	}),
	ECVF_Scalability);

static float GBudgetPressureBeforeEmergencyReducedWork = 2.5f;

static FAutoConsoleVariableRef CVarSkelBatch_BudgetPressureBeforeEmergencyReducedWork(
	TEXT("a.Budget.GBudgetPressureBeforeEmergencyReducedWork"),
	GBudgetPressureBeforeEmergencyReducedWork,
	TEXT("Range > 0.0, Default = 2.5\n")
	TEXT("Controls the budget pressure where emergency reduced work (applied to all components except those that are bAlwaysTick).\n"),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		GBudgetPressureBeforeEmergencyReducedWork = FMath::Max(GBudgetPressureBeforeEmergencyReducedWork, 0.0f);
	}),
	ECVF_Scalability);

FComponentData::FComponentData(USkeletalMeshComponentBudgeted* InComponent)
	: Component(InComponent)
	, RootPrerequisite(nullptr)
	, Significance(1.0f)
	, AccumulatedDeltaTime(0.0f)
	, GameThreadLastTickTimeMs(GInitialEstimatedWorkUnitTimeMs)
	, GameThreadLastCompletionTimeMs(0.0f)
	, FrameOffset(0)
	, DesiredTickRate(1)
	, TickRate(1)
	, SkippedTicks(0)
	, StateChangeThrottle(GStateChangeThrottleInFrames)
	, bTickEnabled(true)
	, bAlwaysTick(false)
	, bTickEvenIfNotRendered(false)
	, bInterpolate(false)
	, bReducedWork(false)
	, bAllowReducedWork(true)
	, bAutoCalculateSignificance(false)
	, bOnScreen(false)
	, bNeverThrottle(true)
{}

FAnimationBudgetAllocator::FAnimationBudgetAllocator(UWorld* InWorld)
	: World(InWorld)
	, AverageWorkUnitTimeMs(GInitialEstimatedWorkUnitTimeMs)
	, NumComponentsToNotSkip(0)
	, TotalEstimatedTickTimeMs(0.0f)
	, NumWorkUnitsForAverage(0.0f)
	, SmoothedBudgetPressure(0.0f)
	, ReducedComponentWorkCounter(0)
	, CurrentFrameOffset(0)
	, bEnabled(false)
{
	FAnimationBudgetAllocator::bCachedEnabled = GAnimationBudgetEnabled == 1 && bEnabled;
	PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FAnimationBudgetAllocator::HandlePostGarbageCollect);
	OnWorldPreActorTickHandle = FWorldDelegates::OnWorldPreActorTick.AddRaw(this, &FAnimationBudgetAllocator::OnWorldPreActorTick);
}

FAnimationBudgetAllocator::~FAnimationBudgetAllocator()
{
	FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);
	FWorldDelegates::OnWorldPreActorTick.Remove(OnWorldPreActorTickHandle);
}

IAnimationBudgetAllocator* IAnimationBudgetAllocator::Get(UWorld* InWorld)
{
	return IAnimationBudgetAllocatorModule::Get(InWorld);
}

void FAnimationBudgetAllocator::SetComponentTickEnabled(USkeletalMeshComponentBudgeted* Component, bool bShouldTick)
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		int32 Handle = Component->GetAnimationBudgetHandle();
		if(Handle != INDEX_NONE)
		{
			AllComponentData[Handle].bTickEnabled = bShouldTick;
		}

		TickEnableHelper(Component, bShouldTick);
	}
	else
#endif
	{
		TickEnableHelper(Component, bShouldTick);
	}
}

bool FAnimationBudgetAllocator::IsComponentTickEnabled(USkeletalMeshComponentBudgeted* Component) const
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		int32 Handle = Component->GetAnimationBudgetHandle();
		if(Handle != INDEX_NONE)
		{
			return AllComponentData[Handle].bTickEnabled;
		}

		return Component->PrimaryComponentTick.IsTickFunctionEnabled();
	}
	else
#endif
	{
		return Component->PrimaryComponentTick.IsTickFunctionEnabled();
	}
}

void FAnimationBudgetAllocator::SetComponentSignificance(USkeletalMeshComponentBudgeted* Component, float Significance, bool bAlwaysTick, bool bTickEvenIfNotRendered, bool bAllowReducedWork, bool bNeverThrottle)
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		int32 Handle = Component->GetAnimationBudgetHandle();
		if(Handle != INDEX_NONE)
		{
			FComponentData& ComponentData = AllComponentData[Handle];
			ComponentData.Significance = Significance;
			ComponentData.bAlwaysTick = bAlwaysTick;
			ComponentData.bTickEvenIfNotRendered = bTickEvenIfNotRendered;
			ComponentData.bAllowReducedWork = !bAlwaysTick && bAllowReducedWork;	// Dont allow reduced work if we are set to 'always tick'
			ComponentData.bNeverThrottle = bNeverThrottle;
		}
	}
#endif
}

void FAnimationBudgetAllocator::TickEnableHelper(USkeletalMeshComponent* InComponent, bool bInEnable)
{
	if(bInEnable)
	{
		InComponent->PrimaryComponentTick.SetTickFunctionEnable(true);
		if(InComponent->IsClothingSimulationSuspended())
		{
			InComponent->ResumeClothingSimulation();
			InComponent->ClothBlendWeight = 1.0f;
		}
	}
	else
	{
		InComponent->PrimaryComponentTick.SetTickFunctionEnable(false);
		if(!InComponent->IsClothingSimulationSuspended())
		{
			InComponent->SuspendClothingSimulation();
			InComponent->ClothBlendWeight = 0.0f;
		}
	}
}

void FAnimationBudgetAllocator::QueueSortedComponentIndices(float InDeltaSeconds)
{
	const float WorldTime = World->TimeSeconds - 1.0f;

	NumComponentsToNotSkip = 0;
	NumComponentsToNotThrottle = 0;
	TotalEstimatedTickTimeMs = 0.0f;
	NumWorkUnitsForAverage = 0.0f;

	auto QueueComponentTick = [InDeltaSeconds, this](FComponentData& InComponentData, int32 InComponentIndex, bool bInOnScreen)
	{
		InComponentData.AccumulatedDeltaTime += InDeltaSeconds;
		InComponentData.bOnScreen = bInOnScreen;
		InComponentData.StateChangeThrottle = InComponentData.StateChangeThrottle < 0 ? InComponentData.StateChangeThrottle : InComponentData.StateChangeThrottle - 1;

		if(InComponentData.bAlwaysTick)
		{
			NumComponentsToNotSkip++;
		}
		else if(InComponentData.bNeverThrottle)
		{
			NumComponentsToNotThrottle++;
		}

		// Accumulate average tick time
		TotalEstimatedTickTimeMs += InComponentData.GameThreadLastTickTimeMs + InComponentData.GameThreadLastCompletionTimeMs;
		NumWorkUnitsForAverage += 1.0f;

		AllSortedComponentData.Add(InComponentIndex);
#if WITH_TICK_DEBUG
		AllSortedComponentDataDebug.Add(&InComponentData);
#endif

		// Auto-calculate significance here if we are set to
		if(InComponentData.bAutoCalculateSignificance)
		{
			check(USkeletalMeshComponentBudgeted::OnCalculateSignificance().IsBound());

			const float Significance = USkeletalMeshComponentBudgeted::OnCalculateSignificance().Execute(InComponentData.Component);
			SetComponentSignificance(InComponentData.Component, Significance);
		}
	};

	auto DisableComponentTick = [this](FComponentData& InComponentData)
	{
		InComponentData.SkippedTicks = 0;
		InComponentData.AccumulatedDeltaTime = 0.0f;

		// Re-distribute frame offsets for components that wont be ticked, to try to 'level' the distribution
		InComponentData.FrameOffset = CurrentFrameOffset++;

		TickEnableHelper(InComponentData.Component, false);
	};

	uint8 MaxComponentTickFunctionIndex = 0;
	int32 ComponentIndex = 0;
	for (FComponentData& ComponentData : AllComponentData)
	{
		if(ComponentData.bTickEnabled)
		{
			USkeletalMeshComponentBudgeted* Component = ComponentData.Component;
			if (Component && Component->IsRegistered())
			{
				auto ShouldComponentTick = [WorldTime](const USkeletalMeshComponentBudgeted* InComponent, const FComponentData& InComponentData)
				{
					return ((InComponent->LastRenderTime > WorldTime) ||
							InComponentData.bTickEvenIfNotRendered ||
							InComponent->ShouldTickPose() ||
							InComponent->ShouldUpdateTransform(false) ||	// We can force this to false, only used in WITH_EDITOR
							InComponent->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPose);
				};

				// Whether or not we will tick
				bool bShouldTick = ShouldComponentTick(Component, ComponentData);

				// Avoid ticking when root prerequisites dont tick (assumes master pose or copy pose relationship)
				if(bShouldTick && ComponentData.RootPrerequisite != nullptr)
				{
					const int32 PrerequisiteHandle = ComponentData.RootPrerequisite->GetAnimationBudgetHandle();
					if(PrerequisiteHandle != INDEX_NONE)
					{
						const FComponentData& RootPrerequisiteComponentData = AllComponentData[PrerequisiteHandle];
						bShouldTick &= ShouldComponentTick(ComponentData.RootPrerequisite, RootPrerequisiteComponentData);
					}
				}

				if(bShouldTick)
				{
					// Push into a separate limited list if we are 'tick even if not rendered'
					if(Component->LastRenderTime <= WorldTime && ComponentData.bTickEvenIfNotRendered)
					{
						NonRenderedComponentData.Add(ComponentIndex);
					}
					else
					{
						QueueComponentTick(ComponentData, ComponentIndex, true);
					}
				}
				else
				{
					DisableComponentTick(ComponentData);
				}
			}
		}

		if(ComponentData.bReducedWork)
		{
			if(!ComponentData.bAllowReducedWork)
			{
				DisallowedReducedWorkComponentData.Add(ComponentIndex);
			}
			else
			{
				ReducedWorkComponentData.Add(ComponentIndex);
			}
		}

		ComponentIndex++;
	}

	// Sort by significance, largest first
	auto SignificanceSortPredicate = [this](int32 InIndex0, int32 InIndex1)
	{
		const FComponentData& ComponentData0 = AllComponentData[InIndex0];
		const FComponentData& ComponentData1 = AllComponentData[InIndex1];
		return ComponentData0.Significance > ComponentData1.Significance;
	};

	AllSortedComponentData.Sort(SignificanceSortPredicate);
	ReducedWorkComponentData.Sort(SignificanceSortPredicate);
	NonRenderedComponentData.Sort(SignificanceSortPredicate);

	const int32 MaxOffscreenComponents = FMath::Min(NonRenderedComponentData.Num(), GMaxTickedOffsreenComponents);
	if(MaxOffscreenComponents > 0)
	{
		auto ReduceWorkForOffscreenComponent = [](FComponentData& InComponentData)
		{
			if(InComponentData.bAllowReducedWork && !InComponentData.bReducedWork && InComponentData.Component->OnReduceWork().IsBound())
			{
#if WITH_TICK_DEBUG
				UE_LOG(LogTemp, Warning, TEXT("Force-decreasing offscreen component work (mesh %s) (actor %llx)"), InComponentData.Component->SkeletalMesh ? *InComponentData.Component->SkeletalMesh->GetName() : TEXT("null"), (uint64)InComponentData.Component->GetOwner());
#endif
				InComponentData.Component->OnReduceWork().Execute(InComponentData.Component, true);
				InComponentData.bReducedWork = true;
			}
		};

		// Queue first N offscreen ticks
		int32 NonRenderedComponentIndex = 0;
		for(; NonRenderedComponentIndex < MaxOffscreenComponents; ++NonRenderedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[NonRenderedComponentData[NonRenderedComponentIndex]];
			QueueComponentTick(ComponentData, NonRenderedComponentData[NonRenderedComponentIndex], false);

			// Always move to reduced work offscreen
			ReduceWorkForOffscreenComponent(ComponentData);

			// Offscreen will need state changing ASAP when back onscreen
			ComponentData.StateChangeThrottle = -1;
		}

		// Disable ticks for the rest
		for(; NonRenderedComponentIndex < NonRenderedComponentData.Num(); ++NonRenderedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[NonRenderedComponentData[NonRenderedComponentIndex]];
			DisableComponentTick(AllComponentData[NonRenderedComponentData[NonRenderedComponentIndex]]);

			// Always move to reduced work offscreen
			ReduceWorkForOffscreenComponent(ComponentData);

			// Offscreen will need state changing ASAP when back onscreen
			ComponentData.StateChangeThrottle = -1;
		}

		// re-sort now we have inserted offscreen components
		AllSortedComponentData.Sort(SignificanceSortPredicate);
	}

#if WITH_TICK_DEBUG
	auto SignificanceDebugSortPredicate = [](const FComponentData& InComponentData0, const FComponentData& InComponentData1)
	{
		return InComponentData0.Significance > InComponentData1.Significance;
	};

	AllSortedComponentDataDebug.Sort(SignificanceDebugSortPredicate);
#endif
}

int32 FAnimationBudgetAllocator::CalculateWorkDistributionAndQueue(float InDeltaSeconds, float& OutAverageTickRate)
{
	int32 NumTicked = 0;

	auto QueueForTick = [&NumTicked, this](FComponentData& InComponentData, int32 InStateChangeThrottleInFrames)
	{
		const int32 PrerequisiteHandle = InComponentData.RootPrerequisite != nullptr ? InComponentData.RootPrerequisite->GetAnimationBudgetHandle() : INDEX_NONE;
		const FComponentData& ComponentDataToCheck = PrerequisiteHandle != INDEX_NONE ? AllComponentData[PrerequisiteHandle] : InComponentData;

		// Using (frame offset + frame counter) % tick rate allows us to only tick at the specified interval,
		// but at a roughly even distribution over all registered components
		const bool bTickThisFrame = (((GFrameCounter + ComponentDataToCheck.FrameOffset) % ComponentDataToCheck.TickRate) == 0);
		if((ComponentDataToCheck.bInterpolate && ComponentDataToCheck.bOnScreen) || bTickThisFrame)
		{
			InComponentData.bInterpolate = ComponentDataToCheck.bInterpolate;
			InComponentData.SkippedTicks = bTickThisFrame ? 0 : (InComponentData.SkippedTicks + 1);

			// Reset completion time as it may not always be run
			InComponentData.GameThreadLastCompletionTimeMs = 0.0f;

			InComponentData.Component->EnableExternalInterpolation(InComponentData.TickRate > 1 && InComponentData.bInterpolate);
			InComponentData.Component->EnableExternalUpdate(bTickThisFrame);
			InComponentData.Component->EnableExternalEvaluationRateLimiting(InComponentData.TickRate > 1);
			InComponentData.Component->SetExternalDeltaTime(InComponentData.AccumulatedDeltaTime);

			InComponentData.AccumulatedDeltaTime = bTickThisFrame ? 0.0f : InComponentData.AccumulatedDeltaTime;

			if(InComponentData.bInterpolate)
			{
				float Alpha = FMath::Clamp((1.0f / (InComponentData.TickRate - InComponentData.SkippedTicks + 1)), 0.0f, 1.0f);
				InComponentData.Component->SetExternalInterpolationAlpha(Alpha);
			}

			TickEnableHelper(InComponentData.Component, true);

			// Only switch to desired tick rate when we actually tick (throttled)
			if(bTickThisFrame && InComponentData.StateChangeThrottle < 0)
			{
				InComponentData.TickRate = InComponentData.DesiredTickRate;
				InComponentData.StateChangeThrottle = InStateChangeThrottleInFrames;
			}

			NumTicked++;
		}
		else
		{
			TickEnableHelper(InComponentData.Component, false);
		}
	};

	const int32 TotalIdealWorkUnits = AllSortedComponentData.Num();

	SET_DWORD_STAT(STAT_AnimationBudgetAllocator_Demand, TotalIdealWorkUnits);

	if(TotalIdealWorkUnits > 0)
	{
		// Calc smoothed average of last frames' work units
		const float AverageTickTimeMs = TotalEstimatedTickTimeMs / NumWorkUnitsForAverage;
		AverageWorkUnitTimeMs = FMath::FInterpTo(AverageWorkUnitTimeMs, AverageTickTimeMs, InDeltaSeconds, GWorkUnitSmoothingSpeed);

		SET_FLOAT_STAT(STAT_AnimationBudgetAllocator_AverageWorkUnitTime, AverageWorkUnitTimeMs);
		CSV_CUSTOM_STAT(AnimationBudget, AverageWorkUnitTimeMs, AverageTickTimeMs, ECsvCustomStatOp::Set);

		// Want to map the remaining (non-fixed) work units so that we only execute N work units per frame.
		// If we can go over budget to keep quality then we use that value
		const float WorkUnitBudget = FMath::Max(GBudgetInMs / AverageWorkUnitTimeMs, (float)TotalIdealWorkUnits * GMinQuality);

		SET_FLOAT_STAT(STAT_AnimationBudgetAllocator_Budget, WorkUnitBudget);

		// Ramp-off work units that we tick every frame once required ticks start exceeding budget
		const float WorkUnitsExcess = FMath::Max(0.0f, TotalIdealWorkUnits - WorkUnitBudget);
		const float WorkUnitsToRunInFull = FMath::Clamp(WorkUnitBudget - (WorkUnitsExcess * GAlwaysTickFalloffAggression), (float)NumComponentsToNotSkip, (float)TotalIdealWorkUnits);
		SET_DWORD_STAT(STAT_AnimationBudgetAllocator_AlwaysTick, WorkUnitsToRunInFull);
		BUDGET_CSV_STAT(AnimationBudget, NumAlwaysTicked, WorkUnitsToRunInFull, ECsvCustomStatOp::Set);
		const int32 FullIndexEnd = (int32)WorkUnitsToRunInFull;

		// Account for the actual time that we think the fixed ticks will take
		// This works better when budget to work unit ratio is low
		float FullTickTime = 0.0f;
		int32 SortedComponentIndex;
		for (SortedComponentIndex = 0; SortedComponentIndex < FullIndexEnd; ++SortedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[AllSortedComponentData[SortedComponentIndex]];
			FullTickTime += ComponentData.GameThreadLastCompletionTimeMs + ComponentData.GameThreadLastTickTimeMs;
		}

		float FullTickWorkUnits = FMath::Min(FullTickTime / AverageWorkUnitTimeMs, WorkUnitsToRunInFull);

		float RemainingBudget = FMath::Max(0.0f, WorkUnitBudget - FullTickWorkUnits);
		float RemainingWorkUnitsToRun = FMath::Max(0.0f, TotalIdealWorkUnits - FullTickWorkUnits);

		// Ramp off interpolated units in a similar way
		const float WorkUnitsToInterpolate = FMath::Min(FMath::Max(RemainingBudget - (WorkUnitsExcess * GInterpolationFalloffAggression), (float)FMath::Min(GMaxInterpolatedComponents, NumComponentsToNotThrottle)), RemainingWorkUnitsToRun);
		SET_DWORD_STAT(STAT_AnimationBudgetAllocator_Interpolated, WorkUnitsToInterpolate);

		const int32 InterpolationIndexEnd = FMath::Min((int32)WorkUnitsToInterpolate + (int32)WorkUnitsToRunInFull, TotalIdealWorkUnits);

		const float MaxInterpolationRate = (float)GInterpolationMaxRate;

		// Calc remaining (throttled) work units
		RemainingBudget = FMath::Max(0.0f, RemainingBudget - (WorkUnitsToInterpolate * GInterpolationTickMultiplier));
		RemainingWorkUnitsToRun = FMath::Max(0.0f, RemainingWorkUnitsToRun - (WorkUnitsToInterpolate * GInterpolationTickMultiplier));

		SET_DWORD_STAT(STAT_AnimationBudgetAllocator_Throttled, RemainingWorkUnitsToRun);

		// Midpoint of throttle gradient is RemainingWorkUnitsToRun / RemainingBudget.
		// If we distributed this as a constant we would get each component ticked
		// at the same rate. However we want to tick more significant meshes more often,
		// so we keep the area under the curve constant and intercept the line with this centroid.
		// Care must be taken with rounding to keep workload in-budget.
		const float ThrottleRateDenominator = RemainingBudget > 1.0f ? RemainingBudget : 1.0f;
		const float MaxThrottleRate = FMath::Min(FMath::CeilToFloat(FMath::Max(1.0f, RemainingWorkUnitsToRun / ThrottleRateDenominator) * 2.0f), (float)GMaxTickRate);
		const float ThrottleDenominator = RemainingWorkUnitsToRun > 0.0f ? RemainingWorkUnitsToRun : 1.0f;

		// Bucket 1: always ticked
		for (SortedComponentIndex = 0; SortedComponentIndex < FullIndexEnd; ++SortedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[AllSortedComponentData[SortedComponentIndex]];

			// not skipping frames here as we can either match demand or these components need a full update
			ComponentData.TickRate = 1;
			ComponentData.DesiredTickRate = 1;
			ComponentData.bInterpolate = false;
		}

		// Bucket 2: interpolated
		int32 NumInterpolated = 0;
		for (SortedComponentIndex = FullIndexEnd; SortedComponentIndex < InterpolationIndexEnd; ++SortedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[AllSortedComponentData[SortedComponentIndex]];

			const float Alpha = (((float)SortedComponentIndex - FullIndexEnd) / WorkUnitsToInterpolate);
			ComponentData.DesiredTickRate = FMath::Min((int32)FMath::FloorToFloat(FMath::Lerp(2.0f, MaxInterpolationRate, Alpha) + 0.5f), 255);
			ComponentData.bInterpolate = true;
			NumInterpolated++;
		}

		// Bucket 3: Rate limited
		int32 NumThrottled = 0;
		for (SortedComponentIndex = InterpolationIndexEnd; SortedComponentIndex < TotalIdealWorkUnits; ++SortedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[AllSortedComponentData[SortedComponentIndex]];

			const float Alpha = (((float)SortedComponentIndex - InterpolationIndexEnd) / ThrottleDenominator);
			ComponentData.DesiredTickRate = FMath::Min((int32)FMath::FloorToFloat(FMath::Lerp(2.0f, MaxThrottleRate, Alpha) + 0.5f), 255);
			ComponentData.bInterpolate = false;
			NumThrottled++;
		}

		BUDGET_CSV_STAT(AnimationBudget, NumInterpolated, NumInterpolated, ECsvCustomStatOp::Set);
		BUDGET_CSV_STAT(AnimationBudget, NumThrottled, RemainingWorkUnitsToRun, ECsvCustomStatOp::Set);

		const float BudgetPressure = (float)TotalIdealWorkUnits / WorkUnitBudget;
		SmoothedBudgetPressure = FMath::FInterpTo(SmoothedBudgetPressure, BudgetPressure, InDeltaSeconds, GBudgetPressureSmoothingSpeed);

		float BudgetPressureInterpAlpha = FMath::Clamp((SmoothedBudgetPressure - GBudgetFactorBeforeAggressiveReducedWork) * 0.5f, 0.0f, 1.0f);
		int32 StateChangeThrottleInFrames = (int32)FMath::Lerp(4.0f, (float)GStateChangeThrottleInFrames, BudgetPressureInterpAlpha);

		SET_FLOAT_STAT(STAT_AnimationBudgetAllocator_SmoothedBudgetPressure, SmoothedBudgetPressure);

		// Queue for tick
		for (SortedComponentIndex = 0; SortedComponentIndex < TotalIdealWorkUnits; ++SortedComponentIndex)
		{
			FComponentData& ComponentData = AllComponentData[AllSortedComponentData[SortedComponentIndex]];

			// Ensure that root prerequisite doesnt end up with a lower (or different) tick rate than dependencies
			if(ComponentData.RootPrerequisite != nullptr)
			{
				const int32 PrerequisiteHandle = ComponentData.RootPrerequisite->GetAnimationBudgetHandle();
				if(PrerequisiteHandle != INDEX_NONE)
				{
					FComponentData& RootPrerequisiteComponentData = AllComponentData[PrerequisiteHandle];
					RootPrerequisiteComponentData.TickRate = ComponentData.TickRate = FMath::Min(ComponentData.TickRate, RootPrerequisiteComponentData.TickRate);
					RootPrerequisiteComponentData.DesiredTickRate = ComponentData.DesiredTickRate = FMath::Min(ComponentData.DesiredTickRate, RootPrerequisiteComponentData.DesiredTickRate);
					RootPrerequisiteComponentData.StateChangeThrottle = ComponentData.StateChangeThrottle = FMath::Min(ComponentData.StateChangeThrottle, RootPrerequisiteComponentData.StateChangeThrottle);
				}
			}

			QueueForTick(ComponentData, StateChangeThrottleInFrames);
		}

		// If any components are not longer allowed to perform reduced work, force them back out
		for(int32 DisallowedReducedWorkComponentIndex : DisallowedReducedWorkComponentData)
		{
			FComponentData& ComponentData = AllComponentData[DisallowedReducedWorkComponentIndex];
			if(ComponentData.bReducedWork && ComponentData.Component->OnReduceWork().IsBound())
			{
#if WITH_TICK_DEBUG
				UE_LOG(LogTemp, Warning, TEXT("Force-increasing component work (mesh %s) (actor %llx)"), ComponentData.Component->SkeletalMesh ? *ComponentData.Component->SkeletalMesh->GetName() : TEXT("null"), (uint64)ComponentData.Component->GetOwner());
#endif
				ComponentData.Component->OnReduceWork().Execute(ComponentData.Component, false);
				ComponentData.bReducedWork = false;
			}
		}

		if(--ReducedComponentWorkCounter <= 0)
		{
			const bool bEmergencyReducedWork = SmoothedBudgetPressure >= GBudgetPressureBeforeEmergencyReducedWork;

			// Scale num components to switch based on budget pressure
			const int32 NumComponentsToSwitch = (int32)FMath::Lerp(1.0f, (float)GReducedWorkThrottleMaxPerFrame, BudgetPressureInterpAlpha);
			int32 ComponentsSwitched = 0;

			// If we have any components running reduced work when we have an excess, then move them out of the 'reduced' pool per tick
			if (ReducedWorkComponentData.Num() > 0 && SmoothedBudgetPressure < GBudgetFactorBeforeReducedWork - GBudgetFactorBeforeReducedWorkEpsilon)
			{
				for(int32 ReducedWorkComponentIndex : ReducedWorkComponentData)
				{
					FComponentData& ComponentData = AllComponentData[ReducedWorkComponentIndex];
					if(ComponentData.bReducedWork && ComponentData.Component->OnReduceWork().IsBound())
					{
#if WITH_TICK_DEBUG
						UE_LOG(LogTemp, Warning, TEXT("Increasing component work (mesh %s) (actor %llx)"), ComponentData.Component->SkeletalMesh ? *ComponentData.Component->SkeletalMesh->GetName() : TEXT("null"), (uint64)ComponentData.Component->GetOwner());
#endif
						ComponentData.Component->OnReduceWork().Execute(ComponentData.Component, false);
						ComponentData.bReducedWork = false;

						ComponentsSwitched++;
						if(ComponentsSwitched >= NumComponentsToSwitch)
						{
							break;	
						}
					}
				}
			}
			else if(SmoothedBudgetPressure > GBudgetFactorBeforeReducedWork)
			{
				// Any work units that we interpolate or throttle should also be eligible for work reduction (which can involve disabling other ticks), so set them all now if needed
				for (SortedComponentIndex = TotalIdealWorkUnits - 1; SortedComponentIndex >= FullIndexEnd; --SortedComponentIndex)
				{
					FComponentData& ComponentData = AllComponentData[AllSortedComponentData[SortedComponentIndex]];

					const bool bAllowReducedWork = (ComponentData.bAllowReducedWork || bEmergencyReducedWork) && !ComponentData.bAlwaysTick;

					if(bAllowReducedWork && !ComponentData.bReducedWork && ComponentData.Component->OnReduceWork().IsBound())
					{
#if WITH_TICK_DEBUG
						UE_LOG(LogTemp, Warning, TEXT("Reducing component work (mesh %s) (actor %llx)"), ComponentData.Component->SkeletalMesh ? *ComponentData.Component->SkeletalMesh->GetName() : TEXT("null"), (uint64)ComponentData.Component->GetOwner());
#endif
						ComponentData.Component->OnReduceWork().Execute(ComponentData.Component, true);
						ComponentData.bReducedWork = true;

						ComponentsSwitched++;
						if(ComponentsSwitched >= NumComponentsToSwitch)
						{
							break;	
						}
					}
				}
			}

			// Scale the rate at which we consider reducing component work based on budget pressure
			ReducedComponentWorkCounter = (int32)FMath::Lerp((float)GReducedWorkThrottleMaxInFrames, (float)GReducedWorkThrottleMinInFrames, BudgetPressureInterpAlpha);
		}
	}

#if CSV_PROFILER
	if(AllSortedComponentData.Num() > 0)
	{
		for (int32 ComponentDataIndex : AllSortedComponentData)
		{
			FComponentData& ComponentData = AllComponentData[ComponentDataIndex];
			OutAverageTickRate += (float)ComponentData.TickRate;
		}

		OutAverageTickRate /= (float)AllSortedComponentData.Num();
	}
#endif

	return NumTicked;
}

void FAnimationBudgetAllocator::OnWorldPreActorTick(UWorld* InWorld, ELevelTick InLevelTick, float InDeltaSeconds)
{
	if(World == InWorld && InLevelTick == LEVELTICK_All)
	{
		Update(InDeltaSeconds);
	}
}

void FAnimationBudgetAllocator::Update(float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimationBudgetAllocator_Update);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(AnimationBudgetAllocator);

	FAnimationBudgetAllocator::bCachedEnabled = GAnimationBudgetEnabled == 1 && bEnabled;

#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		check(IsInGameThread());

		AllSortedComponentData.Reset();
		ReducedWorkComponentData.Reset();
		DisallowedReducedWorkComponentData.Reset();
		NonRenderedComponentData.Reset();

#if WITH_TICK_DEBUG
		AllSortedComponentDataDebug.Reset();
#endif

		QueueSortedComponentIndices(DeltaSeconds);

		float AverageTickRate = 0.0f;
		const int32 NumTicked = CalculateWorkDistributionAndQueue(DeltaSeconds, AverageTickRate);

		// Update stats			
		SET_DWORD_STAT(STAT_AnimationBudgetAllocator_NumTickedComponents, NumTicked);
		SET_DWORD_STAT(STAT_AnimationBudgetAllocator_NumRegisteredComponents, AllComponentData.Num());
		BUDGET_CSV_STAT(AnimationBudget, NumTicked, NumTicked, ECsvCustomStatOp::Set);
		BUDGET_CSV_STAT(AnimationBudget, AnimQuality, AllSortedComponentData.Num() > 0 ? (float)NumTicked / (float)AllSortedComponentData.Num() : 0.0f, ECsvCustomStatOp::Set);
		BUDGET_CSV_STAT(AnimationBudget, AverageTickRate, AverageTickRate, ECsvCustomStatOp::Set);

#if WITH_TICK_DEBUG
		for (int32 ComponentDataIndex : AllSortedComponentData)
		{
			FComponentData& ComponentData = AllComponentData[ComponentDataIndex];
			DrawDebugString(World, ComponentData.Component->GetOwner()->GetActorLocation(), FString::Printf(TEXT("0x%llx\n%d (%s)\n%s, %s"), &ComponentData, ComponentData.TickRate, ComponentData.bInterpolate ? TEXT("Interp") : TEXT("No Interp"), ComponentData.bReducedWork ? TEXT("Reduced") : TEXT("NotReduced"), ComponentData.bAllowReducedWork ? TEXT("AllowReduced") : TEXT("DisallowReduced")), nullptr, FColor::White, 0.016f, false);
		}
#endif
	}
#endif
}

void FAnimationBudgetAllocator::RemoveHelper(int32 Index)
{
	if(AllComponentData.IsValidIndex(Index))
	{
		if(AllComponentData[Index].Component != nullptr)
		{
			AllComponentData[Index].Component->SetAnimationBudgetHandle(INDEX_NONE);
		}

		AllComponentData.RemoveAtSwap(Index, 1, false);

		// Update handle of swapped component
		const int32 NumRemaining = AllComponentData.Num();
		if(NumRemaining > 0 && Index != NumRemaining)
		{
			if(AllComponentData[Index].Component != nullptr)
			{
				AllComponentData[Index].Component->SetAnimationBudgetHandle(Index);
			}
		}
	}
}

static USkeletalMeshComponentBudgeted* FindRootPrerequisiteRecursive(USkeletalMeshComponentBudgeted* InComponent, TArray<USkeletalMeshComponentBudgeted*>& InVisitedComponents)
{
	InVisitedComponents.Add(InComponent);

	USkeletalMeshComponentBudgeted* Root = InComponent;

	for(FTickPrerequisite& TickPrerequisite : InComponent->PrimaryComponentTick.GetPrerequisites())
	{
		if(USkeletalMeshComponentBudgeted* PrerequisiteObject = Cast<USkeletalMeshComponentBudgeted>(TickPrerequisite.PrerequisiteObject.Get()))
		{
			if(!InVisitedComponents.Contains(PrerequisiteObject))
			{
				Root = FindRootPrerequisiteRecursive(PrerequisiteObject, InVisitedComponents);
			}
		}
	}

	return Root;
}

static USkeletalMeshComponentBudgeted* FindRootPrerequisite(USkeletalMeshComponentBudgeted* InComponent)
{
	check(IsInGameThread());

	static TArray<USkeletalMeshComponentBudgeted*> VisitedComponents;
	VisitedComponents.Reset();

	return FindRootPrerequisiteRecursive(InComponent, VisitedComponents);
}

void FAnimationBudgetAllocator::RegisterComponent(USkeletalMeshComponentBudgeted* InComponent)
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		if (InComponent->GetAnimationBudgetHandle() == INDEX_NONE)
		{
			InComponent->bEnableUpdateRateOptimizations = false;
			InComponent->EnableExternalTickRateControl(true);
			InComponent->SetAnimationBudgetHandle(AllComponentData.Num());

			// Setup frame offset
			FComponentData& ComponentData = AllComponentData.Emplace_GetRef(InComponent);
			USkeletalMeshComponentBudgeted* RootPrerequisite = FindRootPrerequisite(InComponent);
			ComponentData.RootPrerequisite = (RootPrerequisite != nullptr && RootPrerequisite != InComponent) ? RootPrerequisite : nullptr;
			ComponentData.FrameOffset = CurrentFrameOffset++;
			ComponentData.bAutoCalculateSignificance = InComponent->GetAutoCalculateSignificance();

			InComponent->SetAnimationBudgetAllocator(this);
		}
		else
		{
			UpdateComponentTickPrerequsites(InComponent);
		}
	}
#endif
}

void FAnimationBudgetAllocator::UnregisterComponent(USkeletalMeshComponentBudgeted* InComponent)
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		int32 ManagerHandle = InComponent->GetAnimationBudgetHandle();
		if(ManagerHandle != INDEX_NONE)
		{
			RemoveHelper(ManagerHandle);

			InComponent->bEnableUpdateRateOptimizations = true;
			InComponent->EnableExternalTickRateControl(false);
			InComponent->SetAnimationBudgetAllocator(nullptr);
		}
	}
#endif
}

void FAnimationBudgetAllocator::UpdateComponentTickPrerequsites(USkeletalMeshComponentBudgeted* InComponent)
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		int32 ManagerHandle = InComponent->GetAnimationBudgetHandle();
		if(ManagerHandle != INDEX_NONE)
		{
			FComponentData& ComponentData = AllComponentData[ManagerHandle];
			USkeletalMeshComponentBudgeted* RootPrerequisite = FindRootPrerequisite(InComponent);
			ComponentData.RootPrerequisite = (RootPrerequisite != nullptr && RootPrerequisite != InComponent) ? RootPrerequisite : nullptr;
		}
	}
#endif
}

void FAnimationBudgetAllocator::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(World);

	for (FComponentData& ComponentData : AllComponentData)
	{
		Collector.AddReferencedObject(ComponentData.Component);
		Collector.AddReferencedObject(ComponentData.RootPrerequisite);
	}
}

void FAnimationBudgetAllocator::HandlePostGarbageCollect()
{
	// Remove dead components, readjusting indices
	bool bRemoved = false;
	do
	{
		bRemoved = false;
		for(int32 DataIndex = 0; DataIndex < AllComponentData.Num(); ++DataIndex)
		{
			if(AllComponentData[DataIndex].Component == nullptr)
			{
				// We can remove while iterating here as we swap internally
				RemoveHelper(DataIndex);
				bRemoved = true;
			}
		}
	}
	while(bRemoved);
}

void FAnimationBudgetAllocator::SetGameThreadLastTickTimeMs(int32 InManagerHandle, float InGameThreadLastTickTimeMs)
{
	if(InManagerHandle != INDEX_NONE)
	{
		FComponentData& ComponentData = AllComponentData[InManagerHandle];
		ComponentData.GameThreadLastTickTimeMs = InGameThreadLastTickTimeMs;
	}
}

void FAnimationBudgetAllocator::SetGameThreadLastCompletionTimeMs(int32 InManagerHandle, float InGameThreadLastCompletionTimeMs)
{
	if(InManagerHandle != INDEX_NONE)
	{
		FComponentData& ComponentData = AllComponentData[InManagerHandle];
		ComponentData.GameThreadLastCompletionTimeMs = InGameThreadLastCompletionTimeMs;
	}
}
void FAnimationBudgetAllocator::SetIsRunningReducedWork(USkeletalMeshComponentBudgeted* InComponent, bool bInReducedWork)
{
#if USE_SKEL_BATCHING
	if (FAnimationBudgetAllocator::bCachedEnabled)
	{
		int32 ManagerHandle = InComponent->GetAnimationBudgetHandle();
		if(ManagerHandle != INDEX_NONE)
		{
			FComponentData& ComponentData = AllComponentData[ManagerHandle];
			ComponentData.bReducedWork = bInReducedWork;
		}
	}
#endif
}

void FAnimationBudgetAllocator::SetEnabled(bool bInEnabled)
{
	bEnabled = bInEnabled;

	if(!bEnabled)
	{
		// Remove all components we are currently tracking
		for(int32 DataIndex = 0; DataIndex < AllComponentData.Num(); ++DataIndex)
		{
			FComponentData& ComponentData = AllComponentData[DataIndex];
			if(ComponentData.Component != nullptr)
			{
				ComponentData.Component->SetAnimationBudgetHandle(INDEX_NONE);
				ComponentData.Component->bEnableUpdateRateOptimizations = true;
				ComponentData.Component->EnableExternalTickRateControl(false);
				ComponentData.Component->SetAnimationBudgetAllocator(nullptr);
			}
		}

		AllComponentData.Reset();
	}

	FAnimationBudgetAllocator::bCachedEnabled = GAnimationBudgetEnabled == 1 && bEnabled;
}

bool FAnimationBudgetAllocator::GetEnabled() const
{
	return bEnabled;
}