// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Field/FieldSystemComponent.h"

#include "Async/ParallelFor.h"
#include "Field/FieldSystemCoreAlgo.h"
#include "Field/FieldSystemSceneProxy.h"
#include "Misc/CoreMiscDefines.h"
#include "Physics/Experimental/PhysScene_Chaos.h"
#include "Field/FieldSystemCoreAlgo.h"
#include "Field/FieldSystemNodes.h"
#include "Modules/ModuleManager.h"
#include "ChaosSolversModule.h"

DEFINE_LOG_CATEGORY_STATIC(FSC_Log, NoLogging, All);

UFieldSystemComponent::UFieldSystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if INCLUDE_CHAOS
	, PhysicsProxy(nullptr)
	, ChaosModule(nullptr)
#endif
	, bHasPhysicsState(false)
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::UFieldSystemComponent()"),this);
}


FBoxSphereBounds UFieldSystemComponent::CalcBounds(const FTransform& LocalToWorldIn) const
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::CalcBounds()[%p]"), this, FieldSystem);
	return FBox(ForceInit);
}

void UFieldSystemComponent::CreateRenderState_Concurrent()
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::CreateRenderState_Concurrent()"), this);

	Super::CreateRenderState_Concurrent();

	if (SceneProxy && FieldSystem )
	{
		FFieldSystemSampleData * SampleData = ::new FFieldSystemSampleData;
		InitSampleData(SampleData);

		// Enqueue command to send to render thread
		FFieldSystemSceneProxy* FieldSystemSceneProxy = (FFieldSystemSceneProxy*)SceneProxy;
		ENQUEUE_RENDER_COMMAND(FSendFieldSystemData)(
			[FieldSystemSceneProxy, SampleData](FRHICommandListImmediate& RHICmdList)
			{
				//FieldSystemSceneProxy->SetSampleData_RenderThread(SampleData);
			});
	}
}


FPrimitiveSceneProxy* UFieldSystemComponent::CreateSceneProxy()
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::CreateSceneProxy()"), this);

	if (FieldSystem)
	{
		return new FFieldSystemSceneProxy(this);
	}
	return nullptr;
}

void UFieldSystemComponent::InitSampleData(FFieldSystemSampleData * SampleData)
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::InitSampleData()"), this);

	check(SampleData);

	//
	// build and sample grid
	//
}

void UFieldSystemComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UFieldSystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::TickComponent()"), this);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (FieldSystem && FieldSystem->IsVisible() )
	{
		MarkRenderDynamicDataDirty();
	}
}

void UFieldSystemComponent::SendRenderDynamicData_Concurrent()
{
	UE_LOG(FSC_Log, Log, TEXT("FieldSystemComponent[%p]::SendRenderDynamicData_Concurrent()"), this);
	Super::SendRenderDynamicData_Concurrent();
	if (SceneProxy)
	{
		if (FieldSystem)
		{
			FFieldSystemSampleData * SampleData = ::new FFieldSystemSampleData;
			InitSampleData(SampleData);

			// Enqueue command to send to render thread
			FFieldSystemSceneProxy* FieldSystemSceneProxy = (FFieldSystemSceneProxy*)SceneProxy;
			ENQUEUE_RENDER_COMMAND(FSendFieldSystemData)(
				[FieldSystemSceneProxy, SampleData](FRHICommandListImmediate& RHICmdList)
				{
					//FieldSystemSceneProxy->SetSampleData_RenderThread(SampleData);
				});
		}
	}
}

void UFieldSystemComponent::OnCreatePhysicsState()
{
	UActorComponent::OnCreatePhysicsState();
	
	FieldSystem = NewObject<UFieldSystem>();
	
	if (!FieldSystem)
	{
		return;
	}

	// @hack(Serialization) to hard code the field system.
	FieldSystemAlgo::InitDefaultFieldData(FieldSystem->GetFieldData());

#if INCLUDE_CHAOS
	// Check we can get a suitable dispatcher
	ChaosModule = FModuleManager::Get().GetModulePtr<FChaosSolversModule>("ChaosSolvers");
	check(ChaosModule);

	PhysicsProxy = new FFieldSystemSimulationProxy(FieldSystem->GetFieldData());
	TSharedPtr<FPhysScene_Chaos> Scene = FPhysScene_Chaos::GetInstance();
	Scene->AddFieldProxy(PhysicsProxy);
#endif

	bHasPhysicsState = true;
	
}

void UFieldSystemComponent::OnDestroyPhysicsState()
{
	UActorComponent::OnDestroyPhysicsState();
#if INCLUDE_CHAOS
	if (!PhysicsProxy)
	{
		check(!bHasPhysicsState);
		return;
	}

	TSharedPtr<FPhysScene_Chaos> Scene = FPhysScene_Chaos::GetInstance();
	Scene->RemoveFieldProxy(PhysicsProxy);
	
	ChaosModule = nullptr;
	PhysicsProxy = nullptr;
#endif

	bHasPhysicsState = false;
	
}

bool UFieldSystemComponent::ShouldCreatePhysicsState() const
{
	return true;
}

bool UFieldSystemComponent::HasValidPhysicsState() const
{
	return bHasPhysicsState;
}


//
//
//

void UFieldSystemComponent::ClearFieldSystem()
{
	if (FieldSystem)
	{
		FieldSystem->Reset();
		FieldSystemAlgo::InitDefaultFieldData(FieldSystem->GetFieldData());
	}
}

int
UFieldSystemComponent::AddRadialIntMask(FName Name, FVector Position, float Radius, int32 InteriorValue, int32 ExteriorValue, TEnumAsByte<ESetMaskConditionType> Set)
{
	if (FieldSystem)
	{
		FRadialIntMask & RadialMask = FieldSystem->NewNode<FRadialIntMask>(Name);
		RadialMask.Position = Position;
		RadialMask.Radius = Radius;
		RadialMask.InteriorValue = InteriorValue;
		RadialMask.ExteriorValue = ExteriorValue;
		RadialMask.SetMaskCondition = Set;
		// ?? RecreatePhysicsState();
		return RadialMask.GetTerminalID();
	}
	//ensure(false);
	return FFieldNodeBase::Invalid;
}


int
UFieldSystemComponent::AddRadialFalloff(FName Name, float Magnitude, FVector Position, float Radius)
{
	if (FieldSystem)
	{
		FRadialFalloff & RadialFalloff = FieldSystem->NewNode<FRadialFalloff>(Name);
		RadialFalloff.Position = Position;
		RadialFalloff.Radius = Radius;
		RadialFalloff.Magnitude = Magnitude;
		// ??RecreatePhysicsState();
		return RadialFalloff.GetTerminalID();
	}
	//ensure(false);
	return FFieldNodeBase::Invalid;
}


int
UFieldSystemComponent::AddUniformVector(FName Name, float Magnitude, FVector Direction)
{
	if (FieldSystem)
	{
		FUniformVector & UniformVector = FieldSystem->NewNode<FUniformVector>(Name);
		UniformVector.Direction = Direction;
		UniformVector.Magnitude = Magnitude;
		// ?? RecreatePhysicsState();
		return UniformVector.GetTerminalID();
	}
	//ensure(false);
	return FFieldNodeBase::Invalid;
}


int
UFieldSystemComponent::AddRadialVector(FName Name, float Magnitude, FVector Position)
{
	if (FieldSystem)
	{
		FRadialVector & RadialVector = FieldSystem->NewNode<FRadialVector>(Name);
		RadialVector.Position = Position;
		RadialVector.Magnitude = Magnitude;
		// ?? RecreatePhysicsState();
		return RadialVector.GetTerminalID();
	}
	//ensure(false);
	return FFieldNodeBase::Invalid;
}

int
UFieldSystemComponent::AddSumVector(FName Name, float Magnitude, int32 ScalarField, int32 RightVectorField, int32 LeftVectorField, EFieldOperationType Operation)
{
	if (FieldSystem)
	{
		FSumVector & SumVector = FieldSystem->NewNode<FSumVector>(Name);
		SumVector.Magnitude = Magnitude;
		SumVector.Scalar = ScalarField;
		SumVector.VectorRight = RightVectorField;
		SumVector.VectorLeft = LeftVectorField;
		SumVector.Operation = Operation;
		// ?? RecreatePhysicsState();
		return SumVector.GetTerminalID();
	}
	//ensure(false);
	return FFieldNodeBase::Invalid;
}

int
UFieldSystemComponent::AddSumScalar(FName Name, float Magnitude, int32 RightScalarField, int32 LeftScalarField, EFieldOperationType Operation)
{
	if (FieldSystem)
	{
		FSumScalar & SumVector = FieldSystem->NewNode<FSumScalar>(Name);
		SumVector.Magnitude = Magnitude;
		SumVector.ScalarRight = RightScalarField;
		SumVector.ScalarLeft = LeftScalarField;
		SumVector.Operation = Operation;
		// ?? RecreatePhysicsState();
		return SumVector.GetTerminalID();
	}
	//ensure(false);
	return FFieldNodeBase::Invalid;
}

FName FieldTypeToName(EFieldPhysicsDefaultFields InType)
{
	switch(InType)
	{
	case EFieldPhysicsDefaultFields::Field_RadialIntMask:
		return "RadialIntMask";
	case EFieldPhysicsDefaultFields::Field_RadialFalloff:
		return "RadialFalloff";
	case EFieldPhysicsDefaultFields::Field_RadialVector:
		return "RadialVector";
	case EFieldPhysicsDefaultFields::Field_UniformVector:
		return "UniformVector";
	case EFieldPhysicsDefaultFields::Field_RadialVectorFalloff:
		return "RadialVectorFalloff";
	default:
		break;
	}

	return NAME_None;
}

void UFieldSystemComponent::DispatchCommand(const FFieldSystemCommand& InCommand)
{
#if INCLUDE_CHAOS
	checkSlow(ChaosModule && PhysicsProxy && PhysicsProxy->GetCallbacks()); // Should already be checked from OnCreatePhysicsState
	Chaos::IDispatcher* PhysicsDispatcher = ChaosModule->GetDispatcher();
	checkSlow(PhysicsDispatcher); // Should always have one of these

	PhysicsDispatcher->EnqueueCommand([PhysicsProxy = this->PhysicsProxy, NewCommand = InCommand]()
	{
		FFieldSystemSolverCallbacks* Callbacks = (FFieldSystemSolverCallbacks*)PhysicsProxy->GetCallbacks();
		Callbacks->BufferCommand(NewCommand);
	});
#endif
}

void UFieldSystemComponent::DispatchCommand(const FName& InName, EFieldPhysicsType InType, const FVector& InPosition, const FVector& InDirection, float InRadius, float InMagnitude)
{
	DispatchCommand({InName, InType, InPosition, InDirection, InRadius, InMagnitude});
}

void UFieldSystemComponent::ApplyField(TEnumAsByte<EFieldPhysicsDefaultFields> FieldName, TEnumAsByte<EFieldPhysicsType> Type, bool Enabled, FVector Position, FVector Direction, float Radius, float Magnitude)
{
	if (FieldSystem)
	{
		DispatchCommand(FieldTypeToName(FieldName), Type, Position, Direction, Radius, Magnitude);
	}
}


void UFieldSystemComponent::ApplyLinearForce(bool Enabled, FVector Direction, float Magnitude)
{
	if (Enabled)
	{
		if (FieldSystem)
		{
			DispatchCommand("UniformVector", EFieldPhysicsType::Field_LinearForce, FVector::ZeroVector, Direction, 0.0f, Magnitude);
		}
	}
}

void UFieldSystemComponent::ApplyRadialForce(bool Enabled, FVector Position, float Magnitude)
{
	if(Enabled)
	{
		if(FieldSystem)
		{
			DispatchCommand("RadialVector", EFieldPhysicsType::Field_LinearForce, Position, FVector::ZeroVector, 0.0f, Magnitude);
		}
	}
}

void UFieldSystemComponent::ApplyStayDynamicField(bool Enabled, FVector Position, float Radius, int MaxLevelPerCommand)
{
	if (Enabled)
	{
		if (FieldSystem)
		{
			FFieldSystemCommand StayDynamicCommand("RadialIntMask", EFieldPhysicsType::Field_StayDynamic, Position, FVector::ZeroVector, Radius, 0.0f);
			StayDynamicCommand.MaxClusterLevel = MaxLevelPerCommand;

			DispatchCommand(StayDynamicCommand);
		}
	}
}

void UFieldSystemComponent::ApplyRadialVectorFalloffForce(bool Enabled, FVector Position, float Radius, float Magnitude)
{
	if (Enabled)
	{
		if (FieldSystem)
		{
			DispatchCommand("RadialVectorFalloff", EFieldPhysicsType::Field_LinearForce, Position, FVector::ZeroVector, Radius, Magnitude);
		}
	}
}

void UFieldSystemComponent::ApplyUniformVectorFalloffForce(bool Enabled, FVector Position, FVector Direction, float Radius, float Magnitude)
{
	if (Enabled)
	{
		if (FieldSystem)
		{
			DispatchCommand("UniformVectorFalloff", EFieldPhysicsType::Field_LinearForce, Position, Direction, Radius, Magnitude);
		}
	}
}
