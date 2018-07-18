// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceSkeletalMeshDetails.h"
#include "NiagaraDetailSourcedArrayBuilder.h"
#include "NiagaraDataInterfaceDetails.h"
#include "NiagaraDataInterfaceSkeletalMesh.h" 
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "NiagaraComponent.h"

 #define LOCTEXT_NAMESPACE "FNiagaraDataInterfaceSkeletalMeshDetails"

void FNiagaraDataInterfaceSkeletalMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	 LayoutBuilder = &DetailBuilder;
	 static const FName MeshCategoryName = TEXT("Mesh");

	 TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	 DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	 if(SelectedObjects.Num() != 1 || SelectedObjects[0]->IsA<UNiagaraDataInterfaceSkeletalMesh>() == false)
	 {
	 	return;
	 }

	 MeshInterface = CastChecked<UNiagaraDataInterfaceSkeletalMesh>(SelectedObjects[0].Get());
	 MeshInterface->OnChanged().RemoveAll(this);
	 MeshInterface->OnChanged().AddSP(this, &FNiagaraDataInterfaceSkeletalMeshDetails::OnInterfaceChanged);

	 TWeakObjectPtr<USceneComponent> SceneComponent;
	 USkeletalMeshComponent* FoundSkelComp = nullptr;
	 MeshObject = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(MeshInterface.Get(), Cast<UNiagaraComponent>(MeshInterface->GetOuter()), SceneComponent, FoundSkelComp);
	 if (MeshObject.IsValid())
	 {
		 MeshObject->GetOnMeshChanged().RemoveAll(this);
		 MeshObject->GetOnMeshChanged().AddSP(this, &FNiagaraDataInterfaceSkeletalMeshDetails::OnDataChanged);
	 }

	 IDetailCategoryBuilder& MeshCategory = DetailBuilder.EditCategory(MeshCategoryName, LOCTEXT("Mesh", "Mesh"));
	 {
		 TArray<TSharedRef<IPropertyHandle>> MeshProperties;
		 MeshCategory.GetDefaultProperties(MeshProperties, true, false);

		 TSharedPtr<IPropertyHandle> RegionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSkeletalMesh, SamplingRegions));
		
		 for (TSharedPtr<IPropertyHandle> Property : MeshProperties)
		 {
			 if (Property->GetProperty() != RegionsProperty->GetProperty())
			 {
				 MeshCategory.AddProperty(Property);
			 }
			 else
			 {
				 TArray<TSharedPtr<FName>> PossibleRegions = GenerateSourceArray();
				 RegionsBuilder = TSharedPtr<FNiagaraDetailSourcedArrayBuilder>(new FNiagaraDetailSourcedArrayBuilder(Property.ToSharedRef(), PossibleRegions));
				 MeshCategory.AddCustomBuilder(RegionsBuilder.ToSharedRef());
			 }
		 }
	 }
 }

 TSharedRef<IDetailCustomization> FNiagaraDataInterfaceSkeletalMeshDetails::MakeInstance()
 {
	 return MakeShared<FNiagaraDataInterfaceSkeletalMeshDetails>();
 }

 void FNiagaraDataInterfaceSkeletalMeshDetails::OnInterfaceChanged()
 {
	 // Rebuild the data changed listener
	 TWeakObjectPtr<USceneComponent> SceneComponent;
	 USkeletalMeshComponent* FoundSkelComp = nullptr;
	 if (MeshObject.IsValid())
	 {
		 MeshObject->GetOnMeshChanged().RemoveAll(this);
	 }
	 MeshObject = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(MeshInterface.Get(), Cast<UNiagaraComponent>(MeshInterface->GetOuter()), SceneComponent, FoundSkelComp);
	 if (MeshObject.IsValid())
	 {
		 MeshObject->GetOnMeshChanged().AddSP(this, &FNiagaraDataInterfaceSkeletalMeshDetails::OnDataChanged);
	 }
	 OnDataChanged();
 }

 void FNiagaraDataInterfaceSkeletalMeshDetails::OnDataChanged()
 {
	 TArray<TSharedPtr<FName>> PossibleRegions = GenerateSourceArray();
	 RegionsBuilder->SetSourceArray(PossibleRegions);
 }

 TArray<TSharedPtr<FName>> FNiagaraDataInterfaceSkeletalMeshDetails::GenerateSourceArray()
 {
	TArray<TSharedPtr<FName>> SourceArray;
	if (MeshInterface.IsValid())
	{
			TWeakObjectPtr<USceneComponent> SceneComponent;
			USkeletalMeshComponent* FoundSkelComp = nullptr;
			USkeletalMesh* Mesh = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(MeshInterface.Get(), Cast<UNiagaraComponent>(MeshInterface->GetOuter()), SceneComponent, FoundSkelComp);

			if (Mesh != nullptr)
			{
				for (FSkeletalMeshSamplingRegion Region : Mesh->GetSamplingInfo().Regions)
				{
					TSharedPtr<FName> RegionName(new FName(Region.Name));
					SourceArray.Add(RegionName);
				}
			}
	}
	return SourceArray;
 }

 FNiagaraDataInterfaceSkeletalMeshDetails::~FNiagaraDataInterfaceSkeletalMeshDetails()
 {
	 if (MeshInterface.IsValid())
	 {
		 MeshInterface->OnChanged().RemoveAll(this);
	 }
	 if (MeshObject.IsValid())
	 {
		MeshObject->GetOnMeshChanged().RemoveAll(this);
	 }
 }

#undef LOCTEXT_NAMESPACE