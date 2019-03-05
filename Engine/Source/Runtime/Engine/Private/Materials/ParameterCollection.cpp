// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ParameterCollection.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "RenderingThread.h"
#include "UniformBuffer.h"
#include "Engine/World.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionCollectionParameter.h"

int32 GDeferUpdateRenderStates = 1;
FAutoConsoleVariableRef CVarDeferUpdateRenderStates(
	TEXT("r.DeferUpdateRenderStates"),
	GDeferUpdateRenderStates,
	TEXT("Whether to defer updating the render states of material parameter collections when a paramter is changed until a rendering command needs them up to date.  Deferring updates is more efficient because multiple SetVectorParameterValue and SetScalarParameterValue calls in a frame will only result in one update."),
	ECVF_RenderThreadSafe
);

TMap<FGuid, FMaterialParameterCollectionInstanceResource*> GDefaultMaterialParameterCollectionInstances;

UMaterialParameterCollection::UMaterialParameterCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultResource = nullptr;
}

void UMaterialParameterCollection::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		DefaultResource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollection::PostLoad()
{
	Super::PostLoad();
	
	if (!StateId.IsValid())
	{
		StateId = FGuid::NewGuid();
	}

	CreateBufferStruct();

	// Create an instance for this collection in every world
	for (TObjectIterator<UWorld> It; It; ++It)
	{
		UWorld* CurrentWorld = *It;
		CurrentWorld->AddParameterCollectionInstance(this, true);
	}

	UpdateDefaultResource(true);
}

void UMaterialParameterCollection::BeginDestroy()
{
	if (DefaultResource)
	{
		FGuid Id = StateId;
		ENQUEUE_RENDER_COMMAND(RemoveDefaultResourceCommand)(
			[Id](FRHICommandListImmediate& RHICmdList)
			{	
				GDefaultMaterialParameterCollectionInstances.Remove(Id);			
			}
		);

		DefaultResource->GameThread_Destroy();
		DefaultResource = nullptr;
	}

	Super::BeginDestroy();
}

#if WITH_EDITOR

template<typename ParameterType>
FName CreateUniqueName(TArray<ParameterType>& Parameters, int32 RenameParameterIndex)
{
	FString RenameString;
	Parameters[RenameParameterIndex].ParameterName.ToString(RenameString);

	int32 NumberStartIndex = RenameString.FindLastCharByPredicate([](TCHAR Letter){ return !FChar::IsDigit(Letter); }) + 1;
	
	int32 RenameNumber = 0;
	if (NumberStartIndex < RenameString.Len() - 1)
	{
		FString RenameStringNumberPart = RenameString.RightChop(NumberStartIndex);
		ensure(RenameStringNumberPart.IsNumeric());

		TTypeFromString<int32>::FromString(RenameNumber, *RenameStringNumberPart);
	}

	FString BaseString = RenameString.Left(NumberStartIndex);

	FName Renamed = FName(*FString::Printf(TEXT("%s%u"), *BaseString, ++RenameNumber));

	bool bMatchFound = false;
	
	do
	{
		bMatchFound = false;

		for (int32 i = 0; i < Parameters.Num(); ++i)
		{
			if (Parameters[i].ParameterName == Renamed && RenameParameterIndex != i)
			{
				Renamed = FName(*FString::Printf(TEXT("%s%u"), *BaseString, ++RenameNumber));
				bMatchFound = true;
				break;
			}
		}
	} while (bMatchFound);
	
	return Renamed;
}

template<typename ParameterType>
void SanitizeParameters(TArray<ParameterType>& Parameters)
{
	for (int32 i = 0; i < Parameters.Num() - 1; ++i)
	{
		for (int32 j = i + 1; j < Parameters.Num(); ++j)
		{
			if (Parameters[i].Id == Parameters[j].Id)
			{
				FPlatformMisc::CreateGuid(Parameters[j].Id);
			}

			if (Parameters[i].ParameterName == Parameters[j].ParameterName)
			{
				Parameters[j].ParameterName = CreateUniqueName(Parameters, j);
			}
		}
	}
}

int32 PreviousNumScalarParameters = 0;
int32 PreviousNumVectorParameters = 0;

void UMaterialParameterCollection::PreEditChange(UProperty* PropertyThatWillChange)
{
	Super::PreEditChange(PropertyThatWillChange);

	PreviousNumScalarParameters = ScalarParameters.Num();
	PreviousNumVectorParameters = VectorParameters.Num();
}

void UMaterialParameterCollection::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SanitizeParameters(ScalarParameters);
	SanitizeParameters(VectorParameters);

	// If the array counts have changed, an element has been added or removed, and we need to update the uniform buffer layout,
	// Which also requires recompiling any referencing materials
	if (ScalarParameters.Num() != PreviousNumScalarParameters || VectorParameters.Num() != PreviousNumVectorParameters)
	{
		// Limit the count of parameters to fit within uniform buffer limits
		const uint32 MaxScalarParameters = 1024;

		if (ScalarParameters.Num() > MaxScalarParameters)
		{
			ScalarParameters.RemoveAt(MaxScalarParameters, ScalarParameters.Num() - MaxScalarParameters);
		}

		const uint32 MaxVectorParameters = 1024;

		if (VectorParameters.Num() > MaxVectorParameters)
		{
			VectorParameters.RemoveAt(MaxVectorParameters, VectorParameters.Num() - MaxVectorParameters);
		}

		// Generate a new Id so that unloaded materials that reference this collection will update correctly on load
		// Now that we changed the guid, we must recompile all materials which reference this collection
		StateId = FGuid::NewGuid();

		// Update the uniform buffer layout
		CreateBufferStruct();

		// Create a material update context so we can safely update materials using this parameter collection.
		{
			FMaterialUpdateContext UpdateContext;

			// Go through all materials in memory and recompile them if they use this material parameter collection
			for (TObjectIterator<UMaterial> It; It; ++It)
			{
				UMaterial* CurrentMaterial = *It;

				bool bRecompile = false;

				// Preview materials often use expressions for rendering that are not in their Expressions array, 
				// And therefore their MaterialParameterCollectionInfos are not up to date.
				if (CurrentMaterial->bIsPreviewMaterial || CurrentMaterial->bIsFunctionPreviewMaterial)
				{
					bRecompile = true;
				}
				else
				{
					for (int32 FunctionIndex = 0; FunctionIndex < CurrentMaterial->MaterialParameterCollectionInfos.Num() && !bRecompile; FunctionIndex++)
					{
						if (CurrentMaterial->MaterialParameterCollectionInfos[FunctionIndex].ParameterCollection == this)
						{
							bRecompile = true;
							break;
						}
					}
				}

				if (bRecompile)
				{
					UpdateContext.AddMaterial(CurrentMaterial);

					// Propagate the change to this material
					CurrentMaterial->PreEditChange(nullptr);
					CurrentMaterial->PostEditChange();
					CurrentMaterial->MarkPackageDirty();
				}
			}

			// Recreate all uniform buffers based off of this collection
			for (TObjectIterator<UWorld> It; It; ++It)
			{
				UWorld* CurrentWorld = *It;
				CurrentWorld->UpdateParameterCollectionInstances(true, true);
			}

			UpdateDefaultResource(true);
		}
	}
	else
	{
		// We didn't need to recreate the uniform buffer, just update its contents
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* CurrentWorld = *It;
			CurrentWorld->UpdateParameterCollectionInstances(true, false);
		}

		UpdateDefaultResource(false);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

FName UMaterialParameterCollection::GetParameterName(const FGuid& Id) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			return Parameter.ParameterName;
		}
	}

	return NAME_None;
}

FGuid UMaterialParameterCollection::GetParameterId(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return Parameter.Id;
		}
	}

	return FGuid();
}

void UMaterialParameterCollection::GetParameterIndex(const FGuid& Id, int32& OutIndex, int32& OutComponentIndex) const
{
	// The parameter and component index allocated in this function must match the memory layout in UMaterialParameterCollectionInstance::GetParameterData

	OutIndex = -1;
	OutComponentIndex = -1;

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			// Scalar parameters are packed into float4's
			OutIndex = ParameterIndex / 4;
			OutComponentIndex = ParameterIndex % 4;
			break;
		}
	}

	const int32 VectorParameterBase = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4);

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.Id == Id)
		{
			OutIndex = ParameterIndex + VectorParameterBase;
			break;
		}
	}
}

void UMaterialParameterCollection::GetParameterNames(TArray<FName>& OutParameterNames, bool bVectorParameters) const
{
	if (bVectorParameters)
	{
		for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
	else
	{
		for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];
			OutParameterNames.Add(Parameter.ParameterName);
		}
	}
}

const FCollectionScalarParameter* UMaterialParameterCollection::GetScalarParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return nullptr;
}

const FCollectionVectorParameter* UMaterialParameterCollection::GetVectorParameterByName(FName ParameterName) const
{
	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];

		if (Parameter.ParameterName == ParameterName)
		{
			return &Parameter;
		}
	}

	return nullptr;
}

void UMaterialParameterCollection::CreateBufferStruct()
{	
	TArray<FShaderParametersMetadata::FMember> Members;
	uint32 NextMemberOffset = 0;

	const uint32 NumVectors = FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num();
	new(Members) FShaderParametersMetadata::FMember(TEXT("Vectors"),TEXT(""),NextMemberOffset,UBMT_FLOAT32,EShaderPrecisionModifier::Half,1,4,NumVectors, nullptr);
	const uint32 VectorArraySize = NumVectors * sizeof(FVector4);
	NextMemberOffset += VectorArraySize;
	static FName LayoutName(TEXT("MaterialCollection"));
	const uint32 StructSize = Align(NextMemberOffset, SHADER_PARAMETER_STRUCT_ALIGNMENT);

	// If Collections ever get non-numeric resources (eg Textures), OutEnvironment.ResourceTableMap has a map by name
	// and the N ParameterCollection Uniform Buffers ALL are named "MaterialCollection" with different hashes!
	// (and the hlsl cbuffers are named MaterialCollection0, etc, so the names don't match the layout)
	UniformBufferStruct = MakeUnique<FShaderParametersMetadata>(
		FShaderParametersMetadata::EUseCase::DataDrivenShaderParameterStruct,
		LayoutName,
		TEXT("MaterialCollection"),
		TEXT("MaterialCollection"),
		StructSize,
		Members
		);
}

void UMaterialParameterCollection::GetDefaultParameterData(TArray<FVector4>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	ParameterData.Empty(FMath::DivideAndRoundUp(ScalarParameters.Num(), 4) + VectorParameters.Num());

	for (int32 ParameterIndex = 0; ParameterIndex < ScalarParameters.Num(); ParameterIndex++)
	{
		const FCollectionScalarParameter& Parameter = ScalarParameters[ParameterIndex];

		// Add a new vector for each packed vector
		if (ParameterIndex % 4 == 0)
		{
			ParameterData.Add(FVector4(0, 0, 0, 0));
		}

		FVector4& CurrentVector = ParameterData.Last();
		// Pack into the appropriate component of this packed vector
		CurrentVector[ParameterIndex % 4] = Parameter.DefaultValue;
	}

	for (int32 ParameterIndex = 0; ParameterIndex < VectorParameters.Num(); ParameterIndex++)
	{
		const FCollectionVectorParameter& Parameter = VectorParameters[ParameterIndex];
		ParameterData.Add(Parameter.DefaultValue);
	}
}

void UMaterialParameterCollection::UpdateDefaultResource(bool bRecreateUniformBuffer)
{
	// Propagate the new values to the rendering thread
	TArray<FVector4> ParameterData;
	GetDefaultParameterData(ParameterData);
	DefaultResource->GameThread_UpdateContents(StateId, ParameterData, GetFName(), bRecreateUniformBuffer);

	FGuid Id = StateId;
	FMaterialParameterCollectionInstanceResource* Resource = DefaultResource;
	ENQUEUE_RENDER_COMMAND(UpdateDefaultResourceCommand)(
		[Id, Resource](FRHICommandListImmediate& RHICmdList)
		{	
			GDefaultMaterialParameterCollectionInstances.Add(Id, Resource);
		}
	);
}

UMaterialParameterCollectionInstance::UMaterialParameterCollectionInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Resource = nullptr;
	bNeedsRenderStateUpdate = false;
}

void UMaterialParameterCollectionInstance::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Resource = new FMaterialParameterCollectionInstanceResource();
	}
}

void UMaterialParameterCollectionInstance::SetCollection(UMaterialParameterCollection* InCollection, UWorld* InWorld)
{
	Collection = InCollection;
	World = InWorld;
}

bool UMaterialParameterCollectionInstance::SetScalarParameterValue(FName ParameterName, float ParameterValue)
{
	check(World.IsValid() && Collection);

	if (Collection->GetScalarParameterByName(ParameterName))
	{
		float* ExistingValue = ScalarParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			ScalarParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			UpdateRenderState(false);
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::SetVectorParameterValue(FName ParameterName, const FLinearColor& ParameterValue)
{
	check(World.IsValid() && Collection);

	if (Collection->GetVectorParameterByName(ParameterName))
	{
		FLinearColor* ExistingValue = VectorParameterValues.Find(ParameterName);
		bool bUpdateUniformBuffer = false;

		if (ExistingValue && *ExistingValue != ParameterValue)
		{
			// Update the existing instance override if the new value is different
			bUpdateUniformBuffer = true;
			*ExistingValue = ParameterValue;
		}
		else if (!ExistingValue)
		{
			// Add a new instance override
			bUpdateUniformBuffer = true;
			VectorParameterValues.Add(ParameterName, ParameterValue);
		}

		if (bUpdateUniformBuffer)
		{
			UpdateRenderState(false);
		}

		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetScalarParameterValue(FName ParameterName, float& OutParameterValue) const
{
	const FCollectionScalarParameter* Parameter = Collection->GetScalarParameterByName(ParameterName);

	if (Parameter)
	{
		const float* InstanceValue = ScalarParameterValues.Find(ParameterName);
		OutParameterValue = InstanceValue != nullptr ? *InstanceValue : Parameter->DefaultValue;
		return true;
	}

	return false;
}

bool UMaterialParameterCollectionInstance::GetVectorParameterValue(FName ParameterName, FLinearColor& OutParameterValue) const
{
	const FCollectionVectorParameter* Parameter = Collection->GetVectorParameterByName(ParameterName);

	if (Parameter)
	{
		const FLinearColor* InstanceValue = VectorParameterValues.Find(ParameterName);
		OutParameterValue = InstanceValue != nullptr ? *InstanceValue : Parameter->DefaultValue;
		return true;
	}

	return false;
}

void UMaterialParameterCollectionInstance::UpdateRenderState(bool bRecreateUniformBuffer)
{
	// Don't need material parameters on the server
	if (!World.IsValid() || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	bNeedsRenderStateUpdate = true;
	World->SetMaterialParameterCollectionInstanceNeedsUpdate();

	if (!GDeferUpdateRenderStates || bRecreateUniformBuffer)
	{
		DeferredUpdateRenderState(bRecreateUniformBuffer);
	}
}

void UMaterialParameterCollectionInstance::DeferredUpdateRenderState(bool bRecreateUniformBuffer)
{
	checkf(bNeedsRenderStateUpdate || !bRecreateUniformBuffer, TEXT("DeferredUpdateRenderState was told to recreate the uniform buffer, but there's nothing to update"));

	if (bNeedsRenderStateUpdate && World.IsValid())
	{
		// Propagate the new values to the rendering thread
		TArray<FVector4> ParameterData;
		GetParameterData(ParameterData);
		Resource->GameThread_UpdateContents(Collection ? Collection->StateId : FGuid(), ParameterData, GetFName(), bRecreateUniformBuffer);
	}

	bNeedsRenderStateUpdate = false;
}

void UMaterialParameterCollectionInstance::GetParameterData(TArray<FVector4>& ParameterData) const
{
	// The memory layout created here must match the index assignment in UMaterialParameterCollection::GetParameterIndex

	if (Collection)
	{
		ParameterData.Empty(FMath::DivideAndRoundUp(Collection->ScalarParameters.Num(), 4) + Collection->VectorParameters.Num());

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->ScalarParameters.Num(); ParameterIndex++)
		{
			const FCollectionScalarParameter& Parameter = Collection->ScalarParameters[ParameterIndex];

			// Add a new vector for each packed vector
			if (ParameterIndex % 4 == 0)
			{
				ParameterData.Add(FVector4(0, 0, 0, 0));
			}

			FVector4& CurrentVector = ParameterData.Last();
			const float* InstanceData = ScalarParameterValues.Find(Parameter.ParameterName);
			// Pack into the appropriate component of this packed vector
			CurrentVector[ParameterIndex % 4] = InstanceData ? *InstanceData : Parameter.DefaultValue;
		}

		for (int32 ParameterIndex = 0; ParameterIndex < Collection->VectorParameters.Num(); ParameterIndex++)
		{
			const FCollectionVectorParameter& Parameter = Collection->VectorParameters[ParameterIndex];
			const FLinearColor* InstanceData = VectorParameterValues.Find(Parameter.ParameterName);
			ParameterData.Add(InstanceData ? *InstanceData : Parameter.DefaultValue);
		}
	}
}

void UMaterialParameterCollectionInstance::FinishDestroy()
{
	if (Resource)
	{
		Resource->GameThread_Destroy();
		Resource = nullptr;
	}

	Super::FinishDestroy();
}

void FMaterialParameterCollectionInstanceResource::GameThread_UpdateContents(const FGuid& InGuid, const TArray<FVector4>& Data, const FName& InOwnerName, bool bRecreateUniformBuffer)
{
	FMaterialParameterCollectionInstanceResource* Resource = this;
	ENQUEUE_RENDER_COMMAND(UpdateCollectionCommand)(
		[InGuid, Data, InOwnerName, Resource, bRecreateUniformBuffer](FRHICommandListImmediate& RHICmdList)
		{
			Resource->UpdateContents(InGuid, Data, InOwnerName, bRecreateUniformBuffer);
		}
	);
}

void FMaterialParameterCollectionInstanceResource::GameThread_Destroy()
{
	FMaterialParameterCollectionInstanceResource* Resource = this;
	ENQUEUE_RENDER_COMMAND(DestroyCollectionCommand)(
		[Resource](FRHICommandListImmediate& RHICmdList)
		{
			delete Resource;
		}
	);
}

static FName MaterialParameterCollectionInstanceResourceName(TEXT("MaterialParameterCollectionInstanceResource"));
FMaterialParameterCollectionInstanceResource::FMaterialParameterCollectionInstanceResource() :
	UniformBufferLayout(MaterialParameterCollectionInstanceResourceName)
{
}

FMaterialParameterCollectionInstanceResource::~FMaterialParameterCollectionInstanceResource()
{
	check(IsInRenderingThread());
	UniformBuffer.SafeRelease();
}

void FMaterialParameterCollectionInstanceResource::UpdateContents(const FGuid& InId, const TArray<FVector4>& Data, const FName& InOwnerName, bool bRecreateUniformBuffer)
{
	Id = InId;
	OwnerName = InOwnerName;

	if (InId != FGuid() && Data.Num() > 0)
	{
		const uint32 NewSize = Data.GetTypeSize() * Data.Num();
		check(UniformBufferLayout.Resources.Num() == 0);

		if (!bRecreateUniformBuffer && IsValidRef(UniformBuffer))
		{
			check(NewSize == UniformBufferLayout.ConstantBufferSize);
			check(UniformBuffer->GetLayout() == UniformBufferLayout);
			RHIUpdateUniformBuffer(UniformBuffer, Data.GetData());
		}
		else
		{
			UniformBufferLayout.ConstantBufferSize = NewSize;
			UniformBufferLayout.ComputeHash();
			UniformBuffer = RHICreateUniformBuffer(Data.GetData(), UniformBufferLayout, UniformBuffer_MultiFrame);
		}
	}
}
