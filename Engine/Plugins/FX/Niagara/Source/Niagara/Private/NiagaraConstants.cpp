// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraConstants.h"
#include "NiagaraModule.h"

#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "FNiagaraConstants"

TArray<FNiagaraVariable> FNiagaraConstants::SystemParameters;
TArray<FNiagaraVariable> FNiagaraConstants::TranslatorParameters;
TMap<FName, FNiagaraVariable> FNiagaraConstants::UpdatedSystemParameters;
TMap<FNiagaraVariable, FText> FNiagaraConstants::SystemStrMap;
TArray<FNiagaraVariable> FNiagaraConstants::Attributes;
TMap<FNiagaraVariable, FString> FNiagaraConstants::AttrDefaultsStrMap;
TMap<FNiagaraVariable, FText> FNiagaraConstants::AttrDescStrMap;
TMap<FNiagaraVariable, FNiagaraVariableMetaData> FNiagaraConstants::AttrMetaData;
TMap<FNiagaraVariable, FNiagaraVariable> FNiagaraConstants::AttrDefaultsValueMap;
TMap<FNiagaraVariable, FNiagaraVariable> FNiagaraConstants::AttrDataSetKeyMap;
TArray<FNiagaraVariable> FNiagaraConstants::EngineManagedAttributes;

void FNiagaraConstants::Init()
{
	if (SystemParameters.Num() == 0)
	{
		SystemParameters.Add(SYS_PARAM_ENGINE_DELTA_TIME);
		SystemParameters.Add(SYS_PARAM_ENGINE_INV_DELTA_TIME);
		SystemParameters.Add(SYS_PARAM_ENGINE_TIME);
		SystemParameters.Add(SYS_PARAM_ENGINE_REAL_TIME);

		SystemParameters.Add(SYS_PARAM_ENGINE_POSITION);
		SystemParameters.Add(SYS_PARAM_ENGINE_SCALE);
		SystemParameters.Add(SYS_PARAM_ENGINE_VELOCITY);
		SystemParameters.Add(SYS_PARAM_ENGINE_X_AXIS);
		SystemParameters.Add(SYS_PARAM_ENGINE_Y_AXIS);
		SystemParameters.Add(SYS_PARAM_ENGINE_Z_AXIS);

		SystemParameters.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
		SystemParameters.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
		SystemParameters.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
		SystemParameters.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);
		SystemParameters.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_NO_SCALE);
		SystemParameters.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_NO_SCALE);

		SystemParameters.Add(SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA);
		SystemParameters.Add(SYS_PARAM_ENGINE_TIME_SINCE_RENDERED);

		SystemParameters.Add(SYS_PARAM_ENGINE_EXECUTION_STATE);

		SystemParameters.Add(SYS_PARAM_ENGINE_EXEC_COUNT);
		SystemParameters.Add(SYS_PARAM_ENGINE_EMITTER_NUM_PARTICLES);
		SystemParameters.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE);
		SystemParameters.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS);
		SystemParameters.Add(SYS_PARAM_ENGINE_NUM_SYSTEM_INSTANCES);
		SystemParameters.Add(SYS_PARAM_ENGINE_GLOBAL_SPAWN_COUNT_SCALE);
		SystemParameters.Add(SYS_PARAM_ENGINE_GLOBAL_SYSTEM_COUNT_SCALE);
		SystemParameters.Add(SYS_PARAM_ENGINE_SYSTEM_AGE);
		SystemParameters.Add(SYS_PARAM_EMITTER_AGE);
		SystemParameters.Add(SYS_PARAM_EMITTER_LOCALSPACE);
		SystemParameters.Add(SYS_PARAM_EMITTER_SPAWN_GROUP);
	}

	if (TranslatorParameters.Num() == 0)
	{
		TranslatorParameters.Add(TRANSLATOR_PARAM_BEGIN_DEFAULTS);
	}

	if (UpdatedSystemParameters.Num() == 0)
	{
		UpdatedSystemParameters.Add(FName(TEXT("System Delta Time")), SYS_PARAM_ENGINE_DELTA_TIME);
		UpdatedSystemParameters.Add(FName(TEXT("System Inv Delta Time")), SYS_PARAM_ENGINE_INV_DELTA_TIME);
		UpdatedSystemParameters.Add(FName(TEXT("System Position")), SYS_PARAM_ENGINE_POSITION);
		UpdatedSystemParameters.Add(FName(TEXT("System Velocity")), SYS_PARAM_ENGINE_VELOCITY);
		UpdatedSystemParameters.Add(FName(TEXT("System X Axis")), SYS_PARAM_ENGINE_X_AXIS);
		UpdatedSystemParameters.Add(FName(TEXT("System Y Axis")), SYS_PARAM_ENGINE_Y_AXIS);
		UpdatedSystemParameters.Add(FName(TEXT("System Z Axis")), SYS_PARAM_ENGINE_Z_AXIS);

		UpdatedSystemParameters.Add(FName(TEXT("System Local To World")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
		UpdatedSystemParameters.Add(FName(TEXT("System World To Local")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
		UpdatedSystemParameters.Add(FName(TEXT("System Local To World Transposed")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
		UpdatedSystemParameters.Add(FName(TEXT("System World To Local Transposed")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);
		UpdatedSystemParameters.Add(FName(TEXT("System Local To World No Scale")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD_NO_SCALE);
		UpdatedSystemParameters.Add(FName(TEXT("System World To Local No Scale")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL_NO_SCALE);

		UpdatedSystemParameters.Add(FName(TEXT("Emitter Execution Count")), SYS_PARAM_ENGINE_EXEC_COUNT);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Age")), SYS_PARAM_EMITTER_AGE);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Local Space")), SYS_PARAM_EMITTER_LOCALSPACE);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Spawn Rate")), SYS_PARAM_EMITTER_SPAWNRATE);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Spawn Interval")), SYS_PARAM_EMITTER_SPAWN_INTERVAL);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Interp Spawn Start Dt")), SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Spawn Group")), SYS_PARAM_EMITTER_SPAWN_GROUP);

		UpdatedSystemParameters.Add(FName(TEXT("Delta Time")), SYS_PARAM_ENGINE_DELTA_TIME);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Age")), SYS_PARAM_EMITTER_AGE);
		UpdatedSystemParameters.Add(FName(TEXT("Emitter Local Space")), SYS_PARAM_EMITTER_LOCALSPACE);
		UpdatedSystemParameters.Add(FName(TEXT("Effect Position")), SYS_PARAM_ENGINE_POSITION);
		UpdatedSystemParameters.Add(FName(TEXT("Effect Velocity")), SYS_PARAM_ENGINE_VELOCITY);
		UpdatedSystemParameters.Add(FName(TEXT("Effect X Axis")), SYS_PARAM_ENGINE_X_AXIS);
		UpdatedSystemParameters.Add(FName(TEXT("Effect Y Axis")), SYS_PARAM_ENGINE_Y_AXIS);
		UpdatedSystemParameters.Add(FName(TEXT("Effect Z Axis")), SYS_PARAM_ENGINE_Z_AXIS);

		UpdatedSystemParameters.Add(FName(TEXT("Effect Local To World")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD);
		UpdatedSystemParameters.Add(FName(TEXT("Effect World To Local")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL);
		UpdatedSystemParameters.Add(FName(TEXT("Effect Local To World Transposed")), SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED);
		UpdatedSystemParameters.Add(FName(TEXT("Effect World To Local Transposed")), SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED);
		UpdatedSystemParameters.Add(FName(TEXT("Execution Count")), SYS_PARAM_ENGINE_EXEC_COUNT);
		UpdatedSystemParameters.Add(FName(TEXT("Spawn Rate")), SYS_PARAM_EMITTER_SPAWNRATE);
		UpdatedSystemParameters.Add(FName(TEXT("Spawn Interval")), SYS_PARAM_EMITTER_SPAWN_INTERVAL);
		UpdatedSystemParameters.Add(FName(TEXT("Interp Spawn Start Dt")), SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT);
		UpdatedSystemParameters.Add(FName(TEXT("Spawn Group")), SYS_PARAM_EMITTER_SPAWN_GROUP);
		UpdatedSystemParameters.Add(FName(TEXT("Inv Delta Time")), SYS_PARAM_ENGINE_INV_DELTA_TIME);
	}

	if (SystemStrMap.Num() == 0)
	{
		SystemStrMap.Add(SYS_PARAM_ENGINE_DELTA_TIME, LOCTEXT("EngineDeltaTimeDesc", "Time in seconds since the last tick."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_INV_DELTA_TIME, LOCTEXT("EngineInvDeltaTimeDesc", "One over Engine.DeltaTime"));
		SystemStrMap.Add(SYS_PARAM_ENGINE_TIME, LOCTEXT("EngineTimeDesc", "Time in seconds since level began play, but IS paused when the game is paused, and IS dilated/clamped."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_REAL_TIME, LOCTEXT("EngineRealTimeDesc", "Time in seconds since level began play, but IS NOT paused when the game is paused, and IS NOT dilated/clamped."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_SYSTEM_AGE, LOCTEXT("EngineSystemTimeDesc", "Time in seconds since the system was first created. Managed by the NiagaraSystemInstance in code."));

		SystemStrMap.Add(SYS_PARAM_ENGINE_POSITION, LOCTEXT("EnginePositionDesc", "The owning component's position in world space."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_SCALE, LOCTEXT("EngineScaleDesc", "The owning component's scale in world space."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_VELOCITY, LOCTEXT("EngineVelocityDesc", "The owning component's velocity in world space."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_X_AXIS, LOCTEXT("XAxisDesc", "The X-axis of the owning component."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_Y_AXIS, LOCTEXT("YAxisDesc", "The Y-axis of the owning component."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_Z_AXIS, LOCTEXT("ZAxisDesc", "The Z-axis of the owning component."));

		SystemStrMap.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD, LOCTEXT("LocalToWorldDesc", "Owning component's local space to world space transform matrix."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL, LOCTEXT("WorldToLocalDesc", "Owning component's world space to local space transform matrix."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED, LOCTEXT("LocalToWorldTransposeDesc", "Owning component's local space to world space transform matrix transposed."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED, LOCTEXT("WorldToLocalTransposeDesc", "Owning component's world space to local space transform matrix transposed."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_LOCAL_TO_WORLD_NO_SCALE, LOCTEXT("LocalToWorldNoScaleDesc", "Owning component's local space to world space transform matrix with scaling removed."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_WORLD_TO_LOCAL_NO_SCALE, LOCTEXT("WorldToLocalNoScaleDesc", "Owning component's world space to local space transform matrix with scaling removed."));

		SystemStrMap.Add(SYS_PARAM_ENGINE_TIME_SINCE_RENDERED, LOCTEXT("TimeSinceRendered", "The time in seconds that have passed since this system was last rendered."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA, LOCTEXT("MinDistanceToCamera", "The distance from the owner component to the nearest local player viewpoint."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_EXECUTION_STATE, LOCTEXT("ExecutionState", "The execution state of the systems owner. Takes precedence over the systems internal execution state."));

		SystemStrMap.Add(SYS_PARAM_ENGINE_EXEC_COUNT, LOCTEXT("ExecCountDesc", "The index of this particle in the read buffer."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_EMITTER_NUM_PARTICLES, LOCTEXT("EmitterNumParticles", "The number of particles for this emitter at the beginning of simulation. Should only be used in Emitter scripts."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE, LOCTEXT("SystemNumEmittersAlive", "The number of emitters still alive attached to this system. Should only be used in System scripts."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS, LOCTEXT("SystemNumEmitters", "The number of emitters attached to this system. Should only be used in System scripts."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_NUM_SYSTEM_INSTANCES, LOCTEXT("SystemNumInstances", "The number of instances of the this system currently ticking. Should only be used in System scripts."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_GLOBAL_SPAWN_COUNT_SCALE, LOCTEXT("GlobalSpawnCountScale", "Global Spawn Count Scale. Should only be used in System scripts."));
		SystemStrMap.Add(SYS_PARAM_ENGINE_GLOBAL_SYSTEM_COUNT_SCALE, LOCTEXT("GlobalSystemCountScale", "Global System Count Scale. Should only be used in System scripts."));
	}

	if (Attributes.Num() == 0)
	{
		Attributes.Add(SYS_PARAM_PARTICLES_ID);
		Attributes.Add(SYS_PARAM_PARTICLES_POSITION);
		Attributes.Add(SYS_PARAM_PARTICLES_VELOCITY);
		Attributes.Add(SYS_PARAM_PARTICLES_COLOR);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION);
		Attributes.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_FACING);
		Attributes.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT);
		Attributes.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX);
		Attributes.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM);
		Attributes.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1);
		Attributes.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2);
		Attributes.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3);
		Attributes.Add(SYS_PARAM_PARTICLES_SCALE);
		Attributes.Add(SYS_PARAM_PARTICLES_LIFETIME);
		Attributes.Add(SYS_PARAM_PARTICLES_MESH_ORIENTATION);
		Attributes.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET);
		Attributes.Add(SYS_PARAM_PARTICLES_UV_SCALE);
		Attributes.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM);
		Attributes.Add(SYS_PARAM_PARTICLES_LIGHT_RADIUS);
		Attributes.Add(SYS_PARAM_PARTICLES_RIBBONID);
		Attributes.Add(SYS_PARAM_PARTICLES_RIBBONWIDTH);
		Attributes.Add(SYS_PARAM_PARTICLES_RIBBONTWIST);
		Attributes.Add(SYS_PARAM_PARTICLES_RIBBONFACING);
		Attributes.Add(SYS_PARAM_PARTICLES_RIBBONLINKORDER);


		Attributes.Add(SYS_PARAM_INSTANCE_ALIVE);
	}

	if (AttrDataSetKeyMap.Num() == 0)
	{
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_POSITION, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_POSITION));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_VELOCITY, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_VELOCITY));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_COLOR, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_COLOR));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_SPRITE_ROTATION));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_NORMALIZED_AGE));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_SPRITE_SIZE));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_SPRITE_FACING, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_SPRITE_FACING));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_SCALE, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_SCALE));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_LIFETIME, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_LIFETIME));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_MESH_ORIENTATION, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_MESH_ORIENTATION));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_CAMERA_OFFSET));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_UV_SCALE, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_UV_SCALE));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_MATERIAL_RANDOM));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_LIGHT_RADIUS, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_LIGHT_RADIUS));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_RIBBONID, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_RIBBONID));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_RIBBONWIDTH, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_RIBBONWIDTH));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_RIBBONTWIST, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_RIBBONTWIST));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_RIBBONFACING, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_RIBBONFACING));
		AttrDataSetKeyMap.Add(SYS_PARAM_PARTICLES_RIBBONLINKORDER, GetAttributeAsDataSetKey(SYS_PARAM_PARTICLES_RIBBONLINKORDER));
	}

	if (AttrDefaultsStrMap.Num() == 0)
	{
		FNiagaraVariable Var;
		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_POSITION, SYS_PARAM_ENGINE_POSITION.GetName().ToString());
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_POSITION, SYS_PARAM_ENGINE_POSITION);
		
		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_VELOCITY, TEXT("0.0,0.0,0.0"));
		Var = SYS_PARAM_PARTICLES_VELOCITY;
		Var.SetValue<FVector>(FVector(0.0f, 0.0f, 0.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_VELOCITY, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_COLOR, FLinearColor(1.0f, 1.0f, 1.0f, 1.0f).ToString());
		Var = SYS_PARAM_PARTICLES_COLOR;
		Var.SetValue<FLinearColor>(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_COLOR, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION, TEXT("0.0"));
		Var = SYS_PARAM_PARTICLES_SPRITE_ROTATION;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE, TEXT("0.0"));
		Var = SYS_PARAM_PARTICLES_NORMALIZED_AGE;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE, TEXT("X=50.0 Y=50.0"));
		Var = SYS_PARAM_PARTICLES_SPRITE_SIZE;
		Var.SetValue<FVector2D>(FVector2D(50.0f, 50.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_FACING, TEXT("1.0,0.0,0.0"));
		Var = SYS_PARAM_PARTICLES_SPRITE_FACING;
		Var.SetValue<FVector>(FVector(1.0f, 0.0f, 0.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_SPRITE_FACING, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT, TEXT("1.0,0.0,0.0"));
		Var = SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT;
		Var.SetValue<FVector>(FVector(1.0f, 0.0f, 0.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT, Var);
		
		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX, TEXT("0.0"));
		Var = SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM, TEXT("1.0,1.0,1.0,1.0"));
		Var = SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM;
		Var.SetValue<FVector4>(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1, TEXT("1.0,1.0,1.0,1.0"));
		Var = SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1;
		Var.SetValue<FVector4>(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2, TEXT("1.0,1.0,1.0,1.0"));
		Var = SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2;
		Var.SetValue<FVector4>(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3, TEXT("1.0,1.0,1.0,1.0"));
		Var = SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3;
		Var.SetValue<FVector4>(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_SCALE, TEXT("1.0,1.0,1.0"));
		Var = SYS_PARAM_PARTICLES_SCALE;
		Var.SetValue<FVector>(FVector(1.0f, 1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_SCALE, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_LIFETIME, TEXT("5.0"));
		Var = SYS_PARAM_PARTICLES_LIFETIME;
		Var.SetValue<float>(5.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_LIFETIME, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_MESH_ORIENTATION, TEXT("0.0,0.0,0.0,1.0"));
		Var = SYS_PARAM_PARTICLES_MESH_ORIENTATION;
		Var.SetValue<FQuat>(FQuat::Identity);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_MESH_ORIENTATION, Var);
		
		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET, TEXT("0.0"));
		Var = SYS_PARAM_PARTICLES_CAMERA_OFFSET;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_UV_SCALE, TEXT("X=1.0 Y=1.0"));
		Var = SYS_PARAM_PARTICLES_UV_SCALE;
		Var.SetValue<FVector2D>(FVector2D(1.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_UV_SCALE, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM, TEXT("0.0"));
		Var = SYS_PARAM_PARTICLES_MATERIAL_RANDOM;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_LIGHT_RADIUS, TEXT("100.0"));
		Var = SYS_PARAM_PARTICLES_LIGHT_RADIUS;
		Var.SetValue<float>(100.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_LIGHT_RADIUS, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_RIBBONID, TEXT("0"));
		Var = SYS_PARAM_PARTICLES_RIBBONID;
		Var.SetValue<FNiagaraID>(FNiagaraID());
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_RIBBONID, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_RIBBONWIDTH, TEXT("1.0"));
		Var = SYS_PARAM_PARTICLES_RIBBONWIDTH;
		Var.SetValue<float>(1.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_RIBBONWIDTH, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_RIBBONTWIST, TEXT("0.0"));
		Var = SYS_PARAM_PARTICLES_RIBBONTWIST;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_RIBBONTWIST, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_RIBBONFACING, TEXT("0.0, 0.0, 1.0"));
		Var = SYS_PARAM_PARTICLES_RIBBONFACING;
		Var.SetValue<FVector>(FVector(0.0f, 0.0f, 1.0f));
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_RIBBONFACING, Var);

		AttrDefaultsStrMap.Add(SYS_PARAM_PARTICLES_RIBBONLINKORDER, TEXT("0"));
		Var = SYS_PARAM_PARTICLES_RIBBONLINKORDER;
		Var.SetValue<float>(0.0f);
		AttrDefaultsValueMap.Add(SYS_PARAM_PARTICLES_RIBBONLINKORDER, Var);
	}

	if (AttrDescStrMap.Num() == 0)
	{
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_POSITION, LOCTEXT("PositionDesc", "The position of the particle."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_VELOCITY, LOCTEXT("VelocityDesc", "The velocity in cm/s of the particle."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_COLOR, LOCTEXT("ColorDesc", "The color of the particle."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ROTATION, LOCTEXT("SpriteRotDesc", "The screen aligned roll of the particle in degrees."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_NORMALIZED_AGE, LOCTEXT("NormalizedAgeDesc", "The age in seconds divided by lifetime in seconds. Useful for animation as the value is between 0 and 1."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_SIZE, LOCTEXT("SpriteSizeDesc", "The size of the sprite quad."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_FACING, LOCTEXT("FacingDesc", "Makes the surface of the sprite face towards a custom vector. Must be used with the SpriteRenderer's CustomFacingVector FacingMode and CustomFacingVectorMask options."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT, LOCTEXT("AlignmentDesc", "Imagine the texture having an arrow pointing up, this attribute makes the arrow point towards the alignment axis. Must be used with the SpriteRenderer's CustomAlignment Alignment option."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX, LOCTEXT("SubImageIndexDesc", "A value from 0 to the number of entries in the table of SubUV images."));
		FText DynParamText = LOCTEXT("DynamicMaterialParameterDesc", "The 4-float vector used to send custom data to renderer.");
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM, DynParamText);
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1, DynParamText);
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2, DynParamText);
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3, DynParamText);
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_SCALE, LOCTEXT("ScaleParamDesc", "The XYZ scale of the non-sprite based particle."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_LIFETIME, LOCTEXT("LifetimeParamDesc", "The lifetime of a particle in seconds."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_MESH_ORIENTATION, LOCTEXT("MeshOrientParamDesc", "The axis-angle rotation to be applied to the mesh particle."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_CAMERA_OFFSET, LOCTEXT("CamOffsetParamDesc", "Used to offset position in the direction of the camera. The value is multiplied by the direction vector from the camera to the particle."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_UV_SCALE, LOCTEXT("UVScalerParamDesc", "Used to multiply the generated UVs for Sprite renderers."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_MATERIAL_RANDOM, LOCTEXT("MaterialRandomParamDesc", "Used to drive the Particle Random node in the Material Editor. Without this set, any Particle Randoms will get 0.0."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_LIGHT_RADIUS, LOCTEXT("LightRadiusParamDesc", "Used to drive the radius of the light when using a Light renderer."));
		AttrDescStrMap.Add(SYS_PARAM_INSTANCE_ALIVE, LOCTEXT("AliveParamDesc", "Used to determine whether or not this particle instance is still valid or if it can be deleted."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_RIBBONID, LOCTEXT("RibbonIDDesc", "Sets the ribbon id for a particle. Particles with the same ribbon id will be connected into a ribbon."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_RIBBONWIDTH, LOCTEXT("RibbonWidthDesc", "Sets the ribbon width for a particle, in UE4 units."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_RIBBONTWIST, LOCTEXT("RibbonTwistDesc", "Sets the ribbon twist for a particle, in degrees."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_RIBBONFACING, LOCTEXT("RibbonFacingDesc", "Sets the facing vector of the ribbon at the particle position."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_RIBBONLINKORDER, LOCTEXT("RibbonLinkOrderDesc", "Explicit order for linking particles within a ribbon. Particles of the same ribbon id will be connected into a ribbon in incrementing order of this attribute value."));
		AttrDescStrMap.Add(SYS_PARAM_PARTICLES_ID, LOCTEXT("IDDesc", "Engine managed particle attribute that is a persistent ID for each particle."));
	}

	if (AttrMetaData.Num() == 0)
	{
		// Add the engine default attributes..
		{
			for (FNiagaraVariable Var : Attributes)
			{
				FNiagaraVariableMetaData MetaData;
				MetaData.Description = GetAttributeDescription(Var);
				AttrMetaData.Add(Var, MetaData);
			}
		}

		// Add the engine constants..
		{
			for (FNiagaraVariable Var : SystemParameters)
			{
				FNiagaraVariableMetaData MetaData;
				MetaData.Description = GetEngineConstantDescription(Var);
				AttrMetaData.Add(Var, MetaData);
			}
		}

	}

	if (EngineManagedAttributes.Num() == 0)
	{
		EngineManagedAttributes.Add(SYS_PARAM_PARTICLES_ID);
	}

}

const TArray<FNiagaraVariable>& FNiagaraConstants::GetEngineConstants()
{
	check(SystemParameters.Num() != 0);
	return SystemParameters;
}

const TArray<FNiagaraVariable>& FNiagaraConstants::GetTranslatorConstants()
{
	check(TranslatorParameters.Num() != 0);
	return TranslatorParameters;
}

bool FNiagaraConstants::IsEngineManagedAttribute(const FNiagaraVariable& Var)
{
	return EngineManagedAttributes.Contains(Var);
}

FNiagaraVariable FNiagaraConstants::UpdateEngineConstant(const FNiagaraVariable& InVar)
{
	const FNiagaraVariable* FoundSystemVar = FindEngineConstant(InVar);
	if (nullptr != FoundSystemVar)
	{
		return *FoundSystemVar;
	}
	else
	{
		check(UpdatedSystemParameters.Num() != 0);
		const FNiagaraVariable* FoundSystemVarUpdate = UpdatedSystemParameters.Find(InVar.GetName());
		if (FoundSystemVarUpdate != nullptr)
		{
			return *FoundSystemVarUpdate;
		}
	}
	return InVar;

}


const FNiagaraVariable* FNiagaraConstants::FindEngineConstant(const FNiagaraVariable& InVar)
{
	const TArray<FNiagaraVariable>& LocalSystemParameters = GetEngineConstants();
	const FNiagaraVariable* FoundSystemVar = LocalSystemParameters.FindByPredicate([&](const FNiagaraVariable& Var)
	{
		return Var.GetName() == InVar.GetName();
	});
	return FoundSystemVar;
}

FText FNiagaraConstants::GetEngineConstantDescription(const FNiagaraVariable& InAttribute)
{
	check(SystemStrMap.Num() != 0);
	FText* FoundStr = SystemStrMap.Find(InAttribute);
	if (FoundStr != nullptr && !FoundStr->IsEmpty())
	{
		return *FoundStr;
	}
	return FText();
}

const TArray<FNiagaraVariable>& FNiagaraConstants::GetCommonParticleAttributes()
{
	check(Attributes.Num() != 0);
	return Attributes;
}

FString FNiagaraConstants::GetAttributeDefaultValue(const FNiagaraVariable& InAttribute)
{
	check(AttrDefaultsStrMap.Num() != 0);
	FString* FoundStr = AttrDefaultsStrMap.Find(InAttribute);
	if (FoundStr != nullptr && !FoundStr->IsEmpty())
	{
		return *FoundStr;
	}
	return FString();
}

FText FNiagaraConstants::GetAttributeDescription(const FNiagaraVariable& InAttribute)
{
	check(AttrDescStrMap.Num() != 0);
	FText* FoundStr = AttrDescStrMap.Find(InAttribute);
	if (FoundStr != nullptr && !FoundStr->IsEmpty())
	{
		return *FoundStr;
	}
	return FText();
}

bool FNiagaraConstants::IsNiagaraConstant(const FNiagaraVariable& InVar)
{
	if (GetConstantMetaData(InVar) != nullptr)
	{
		return true;
	}
	return false;
}

const FNiagaraVariableMetaData* FNiagaraConstants::GetConstantMetaData(const FNiagaraVariable& InVar)
{
	check(AttrMetaData.Num() != 0);
	return AttrMetaData.Find(InVar);
}

FNiagaraVariable FNiagaraConstants::GetAttributeWithDefaultValue(const FNiagaraVariable& InAttribute)
{
	check(AttrDefaultsValueMap.Num() != 0);
	FNiagaraVariable* FoundValue = AttrDefaultsValueMap.Find(InAttribute);
	if (FoundValue != nullptr)
	{
		return *FoundValue;
	}
	return FNiagaraVariable();
}

FNiagaraVariable FNiagaraConstants::GetAttributeAsDataSetKey(const FNiagaraVariable& InVar)
{
	FNiagaraVariable OutVar = InVar;
	FString DataSetName = InVar.GetName().ToString();
	DataSetName.RemoveFromStart(TEXT("Particles."));
	OutVar.SetName(*DataSetName);
	return OutVar;
}

FNiagaraVariableAttributeBinding FNiagaraConstants::GetAttributeDefaultBinding(const FNiagaraVariable& InVar)
{
	if (AttrDefaultsValueMap.Num() == 0)
	{
		Init();
	}

	FNiagaraVariableAttributeBinding Binding;
	Binding.BoundVariable = InVar;
	Binding.DataSetVariable = InVar;
	const FNiagaraVariable* FoundVar = AttrDataSetKeyMap.Find(InVar);
	if (FoundVar)
	{
		Binding.DataSetVariable = *FoundVar;
	}

	Binding.DefaultValueIfNonExistent = GetAttributeWithDefaultValue(InVar);
	return Binding;
}

const FNiagaraVariable* FNiagaraConstants::GetKnownConstant(const FName& InName, bool bAllowPartialNameMatch)
{
	const TArray<FNiagaraVariable>& EngineConstants = GetEngineConstants();

	if (!bAllowPartialNameMatch)
	{
		const FNiagaraVariable* FoundSystemVar = EngineConstants.FindByPredicate([&](const FNiagaraVariable& Var)
		{
			return Var.GetName() == InName;
		});

		if (FoundSystemVar)
		{
			return FoundSystemVar;
		}
	}
	else
	{
		int32 FoundIdx = FNiagaraVariable::SearchArrayForPartialNameMatch(EngineConstants, InName);
		if (FoundIdx != INDEX_NONE)
		{
			return &EngineConstants[FoundIdx];
		}
	}

	const TArray<FNiagaraVariable>& LocalAttributes = GetCommonParticleAttributes();
	if (!bAllowPartialNameMatch)
	{
		const FNiagaraVariable* FoundAttribVar = LocalAttributes.FindByPredicate([&](const FNiagaraVariable& Var)
		{
			return Var.GetName() == InName;
		});

		if (FoundAttribVar)
		{
			return FoundAttribVar;
		}
	}
	else
	{
		int32 FoundIdx = FNiagaraVariable::SearchArrayForPartialNameMatch(Attributes, InName);
		if (FoundIdx != INDEX_NONE)
		{
			return &Attributes[FoundIdx];
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
