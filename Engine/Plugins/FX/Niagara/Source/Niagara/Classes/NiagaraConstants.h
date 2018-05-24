// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraTypes.h"
#include "NiagaraCommon.h"

#define PARAM_MAP_NPC_STR TEXT("NPC.")
#define PARAM_MAP_ENGINE_STR TEXT("Engine.")
#define PARAM_MAP_ENGINE_OWNER_STR TEXT("Engine.Owner.")
#define PARAM_MAP_ENGINE_SYSTEM_STR TEXT("Engine.System.")
#define PARAM_MAP_ENGINE_EMITTER_STR TEXT("Engine.Emitter.")
#define PARAM_MAP_USER_STR TEXT("User.")
#define PARAM_MAP_SYSTEM_STR TEXT("System.")
#define PARAM_MAP_EMITTER_STR TEXT("Emitter.")
#define PARAM_MAP_MODULE_STR TEXT("Module.")
#define PARAM_MAP_ATTRIBUTE_STR TEXT("Particles.")
#define PARAM_MAP_INITIAL_STR TEXT("Initial.")
#define PARAM_MAP_INITIAL_BASE_STR TEXT("Initial")
#define PARAM_MAP_RAPID_ITERATION_STR TEXT("Constants.")
#define PARAM_MAP_RAPID_ITERATION_BASE_STR TEXT("Constants")


#define SYS_PARAM_ENGINE_DELTA_TIME					INiagaraModule::GetVar_Engine_DeltaTime()
#define SYS_PARAM_ENGINE_INV_DELTA_TIME				INiagaraModule::GetVar_Engine_InvDeltaTime()
#define SYS_PARAM_ENGINE_TIME						INiagaraModule::GetVar_Engine_Time()
#define SYS_PARAM_ENGINE_REAL_TIME					INiagaraModule::GetVar_Engine_RealTime()
#define SYS_PARAM_ENGINE_POSITION					INiagaraModule::GetVar_Engine_Owner_Position()
#define SYS_PARAM_ENGINE_VELOCITY					INiagaraModule::GetVar_Engine_Owner_Velocity()
#define SYS_PARAM_ENGINE_X_AXIS						INiagaraModule::GetVar_Engine_Owner_XAxis()
#define SYS_PARAM_ENGINE_Y_AXIS						INiagaraModule::GetVar_Engine_Owner_YAxis()
#define SYS_PARAM_ENGINE_Z_AXIS						INiagaraModule::GetVar_Engine_Owner_ZAxis()
#define SYS_PARAM_ENGINE_SCALE						INiagaraModule::GetVar_Engine_Owner_Scale()

#define SYS_PARAM_ENGINE_LOCAL_TO_WORLD				INiagaraModule::GetVar_Engine_Owner_SystemLocalToWorld()
#define SYS_PARAM_ENGINE_WORLD_TO_LOCAL				INiagaraModule::GetVar_Engine_Owner_SystemWorldToLocal()
#define SYS_PARAM_ENGINE_LOCAL_TO_WORLD_TRANSPOSED	INiagaraModule::GetVar_Engine_Owner_SystemLocalToWorldTransposed()
#define SYS_PARAM_ENGINE_WORLD_TO_LOCAL_TRANSPOSED	INiagaraModule::GetVar_Engine_Owner_SystemWorldToLocalTransposed()
#define SYS_PARAM_ENGINE_LOCAL_TO_WORLD_NO_SCALE	INiagaraModule::GetVar_Engine_Owner_SystemLocalToWorldNoScale()
#define SYS_PARAM_ENGINE_WORLD_TO_LOCAL_NO_SCALE	INiagaraModule::GetVar_Engine_Owner_SystemWorldToLocalNoScale()

#define SYS_PARAM_ENGINE_TIME_SINCE_RENDERED		INiagaraModule::GetVar_Engine_Owner_TimeSinceRendered()
#define SYS_PARAM_ENGINE_MIN_DIST_TO_CAMERA			INiagaraModule::GetVar_Engine_Owner_MinDistanceToCamera()

#define SYS_PARAM_ENGINE_EXECUTION_STATE			INiagaraModule::GetVar_Engine_Owner_ExecutionState()

#define SYS_PARAM_ENGINE_EXEC_COUNT					INiagaraModule::GetVar_Engine_ExecutionCount()
#define SYS_PARAM_ENGINE_EMITTER_NUM_PARTICLES		INiagaraModule::GetVar_Engine_Emitter_NumParticles()
#define SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS_ALIVE	INiagaraModule::GetVar_Engine_System_NumEmittersAlive()
#define SYS_PARAM_ENGINE_SYSTEM_NUM_EMITTERS		INiagaraModule::GetVar_Engine_System_NumEmitters()
#define SYS_PARAM_ENGINE_NUM_SYSTEM_INSTANCES		INiagaraModule::GetVar_Engine_NumSystemInstances()

#define SYS_PARAM_ENGINE_GLOBAL_SPAWN_COUNT_SCALE	INiagaraModule::GetVar_Engine_GlobalSpawnCountScale()
#define SYS_PARAM_ENGINE_GLOBAL_SYSTEM_COUNT_SCALE	INiagaraModule::GetVar_Engine_GlobalSystemScale()

#define SYS_PARAM_ENGINE_SYSTEM_AGE					INiagaraModule::GetVar_Engine_System_Age()

#define SYS_PARAM_EMITTER_AGE						INiagaraModule::GetVar_Emitter_Age()
#define SYS_PARAM_EMITTER_LOCALSPACE				INiagaraModule::GetVar_Emitter_LocalSpace()
#define SYS_PARAM_EMITTER_SPAWNRATE					INiagaraModule::GetVar_Emitter_SpawnRate()
#define SYS_PARAM_EMITTER_SPAWN_INTERVAL			INiagaraModule::GetVar_Emitter_SpawnInterval()
#define SYS_PARAM_EMITTER_INTERP_SPAWN_START_DT		INiagaraModule::GetVar_Emitter_InterpSpawnStartDt()

#define SYS_PARAM_PARTICLES_ID						INiagaraModule::GetVar_Particles_ID()
#define SYS_PARAM_PARTICLES_POSITION				INiagaraModule::GetVar_Particles_Position()
#define SYS_PARAM_PARTICLES_VELOCITY				INiagaraModule::GetVar_Particles_Velocity()
#define SYS_PARAM_PARTICLES_COLOR					INiagaraModule::GetVar_Particles_Color()
#define SYS_PARAM_PARTICLES_SPRITE_ROTATION			INiagaraModule::GetVar_Particles_SpriteRotation()
#define SYS_PARAM_PARTICLES_NORMALIZED_AGE			INiagaraModule::GetVar_Particles_NormalizedAge()
#define SYS_PARAM_PARTICLES_SPRITE_SIZE				INiagaraModule::GetVar_Particles_SpriteSize()
#define SYS_PARAM_PARTICLES_SPRITE_FACING			INiagaraModule::GetVar_Particles_SpriteFacing()
#define SYS_PARAM_PARTICLES_SPRITE_ALIGNMENT		INiagaraModule::GetVar_Particles_SpriteAlignment()
#define SYS_PARAM_PARTICLES_SUB_IMAGE_INDEX			INiagaraModule::GetVar_Particles_SubImageIndex()
#define SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM	INiagaraModule::GetVar_Particles_DynamicMaterialParameter()
#define SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_1	INiagaraModule::GetVar_Particles_DynamicMaterialParameter1()
#define SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_2	INiagaraModule::GetVar_Particles_DynamicMaterialParameter2()
#define SYS_PARAM_PARTICLES_DYNAMIC_MATERIAL_PARAM_3	INiagaraModule::GetVar_Particles_DynamicMaterialParameter3()
#define SYS_PARAM_PARTICLES_SCALE					INiagaraModule::GetVar_Particles_Scale()
#define SYS_PARAM_PARTICLES_LIFETIME				INiagaraModule::GetVar_Particles_Lifetime()
#define SYS_PARAM_PARTICLES_MESH_ORIENTATION		INiagaraModule::GetVar_Particles_MeshOrientation()
#define SYS_PARAM_PARTICLES_UV_SCALE				INiagaraModule::GetVar_Particles_UVScale()
#define SYS_PARAM_PARTICLES_CAMERA_OFFSET			INiagaraModule::GetVar_Particles_CameraOffset()
#define SYS_PARAM_PARTICLES_MATERIAL_RANDOM			INiagaraModule::GetVar_Particles_MaterialRandom()
#define SYS_PARAM_PARTICLES_LIGHT_RADIUS			INiagaraModule::GetVar_Particles_LightRadius()

#define SYS_PARAM_PARTICLES_RIBBONID				INiagaraModule::GetVar_Particles_RibbonID()
#define SYS_PARAM_PARTICLES_RIBBONWIDTH				INiagaraModule::GetVar_Particles_RibbonWidth()
#define SYS_PARAM_PARTICLES_RIBBONTWIST				INiagaraModule::GetVar_Particles_RibbonTwist()
#define SYS_PARAM_PARTICLES_RIBBONFACING			INiagaraModule::GetVar_Particles_RibbonFacing()
#define SYS_PARAM_PARTICLES_RIBBONLINKORDER			INiagaraModule::GetVar_Particles_RibbonLinkOrder()

#define SYS_PARAM_INSTANCE_ALIVE					INiagaraModule::GetVar_DataInstance_Alive()

#define TRANSLATOR_PARAM_BEGIN_DEFAULTS				INiagaraModule::GetVar_BeginDefaults()

struct NIAGARA_API FNiagaraConstants
{
	static void Init();
	static const TArray<FNiagaraVariable>& GetEngineConstants();
	static const TArray<FNiagaraVariable>& GetTranslatorConstants();
	static FNiagaraVariable UpdateEngineConstant(const FNiagaraVariable& InVar);
	static const FNiagaraVariable *FindEngineConstant(const FNiagaraVariable& InVar);
	static FText GetEngineConstantDescription(const FNiagaraVariable& InVar);

	static const TArray<FNiagaraVariable>& GetCommonParticleAttributes();
	static FText GetAttributeDescription(const FNiagaraVariable& InVar);
	static FString GetAttributeDefaultValue(const FNiagaraVariable& InVar);
	static FNiagaraVariable GetAttributeWithDefaultValue(const FNiagaraVariable& InAttribute);
	static FNiagaraVariable GetAttributeAsDataSetKey(const FNiagaraVariable& InAttribute);
	static FNiagaraVariableAttributeBinding GetAttributeDefaultBinding(const FNiagaraVariable& InAttribute);

	static bool IsNiagaraConstant(const FNiagaraVariable& InVar);
	static const FNiagaraVariableMetaData* GetConstantMetaData(const FNiagaraVariable& InVar);

	static const FNiagaraVariable* GetKnownConstant(const FName& InName, bool bAllowPartialNameMatch);

	static bool IsEngineManagedAttribute(const FNiagaraVariable& Var);

private:
	static TArray<FNiagaraVariable> SystemParameters;
	static TArray<FNiagaraVariable> TranslatorParameters;
	static TMap<FName, FNiagaraVariable> UpdatedSystemParameters;
	static TMap<FNiagaraVariable, FText> SystemStrMap;
	static TArray<FNiagaraVariable> Attributes;
	static TMap<FNiagaraVariable, FString> AttrDefaultsStrMap;
	static TMap<FNiagaraVariable, FNiagaraVariable> AttrDefaultsValueMap;
	static TMap<FNiagaraVariable, FNiagaraVariable> AttrDataSetKeyMap;
	static TMap<FNiagaraVariable, FText> AttrDescStrMap;
	static TMap<FNiagaraVariable, FNiagaraVariableMetaData> AttrMetaData;
	
	static TArray<FNiagaraVariable> EngineManagedAttributes;

};