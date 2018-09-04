// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceSkeletalMeshDetails.h"
#include "NiagaraDetailSourcedArrayBuilder.h"
#include "NiagaraDataInterfaceDetails.h"
#include "NiagaraDataInterfaceSkeletalMesh.h" 
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "NiagaraComponent.h"
#include "Engine/SkeletalMeshSocket.h"

 #define LOCTEXT_NAMESPACE "FNiagaraDataInterfaceSkeletalMeshDetails"

void FNiagaraDataInterfaceSkeletalMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	 LayoutBuilder = &DetailBuilder;
	 static const FName MeshCategoryName = TEXT("Mesh");
	 static const FName SkelCategoryName = TEXT("Skeleton");

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

	 MeshCategory = &DetailBuilder.EditCategory(MeshCategoryName, LOCTEXT("Mesh", "Mesh"));
	 {
		 TArray<TSharedRef<IPropertyHandle>> MeshProperties;
		 MeshCategory->GetDefaultProperties(MeshProperties, true, true);

		 TSharedPtr<IPropertyHandle> RegionsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSkeletalMesh, SamplingRegions));
		
		 for (TSharedPtr<IPropertyHandle> Property : MeshProperties)
		 {
			 UProperty* PropertyPtr = Property->GetProperty();
			 TArray<TSharedPtr<FName>> PossibleNames;
			 if (PropertyPtr == RegionsProperty->GetProperty())
			 {
				 GenerateRegionsArray(PossibleNames);
				 RegionsBuilder = TSharedPtr<FNiagaraDetailSourcedArrayBuilder>(new FNiagaraDetailSourcedArrayBuilder(Property.ToSharedRef(), PossibleNames));
				 MeshCategory->AddCustomBuilder(RegionsBuilder.ToSharedRef());
			 }
			 else
			 {
				 MeshCategory->AddProperty(Property);
			 }
		 }
	 }

	 SkelCategory = &DetailBuilder.EditCategory(SkelCategoryName, LOCTEXT("SkeletonCat", "Skeleton"));
	 {
		 TArray<TSharedRef<IPropertyHandle>> SkelProperties;
		 SkelCategory->GetDefaultProperties(SkelProperties, true, true);

		 TSharedPtr<IPropertyHandle> BonesProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSkeletalMesh, SpecificBones));
		 TSharedPtr<IPropertyHandle> SocketsProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceSkeletalMesh, SpecificSockets));

		 for (TSharedPtr<IPropertyHandle> Property : SkelProperties)
		 {
			 UProperty* PropertyPtr = Property->GetProperty();
			 TArray<TSharedPtr<FName>> PossibleNames;

			if (PropertyPtr == BonesProperty->GetProperty())
			 {
				GenerateBonesArray(PossibleNames);
				BonesBuilder = TSharedPtr<FNiagaraDetailSourcedArrayBuilder>(new FNiagaraDetailSourcedArrayBuilder(Property.ToSharedRef(), PossibleNames));
				SkelCategory->AddCustomBuilder(BonesBuilder.ToSharedRef());
			 }
			 else if (PropertyPtr == SocketsProperty->GetProperty())
			 {
				 GenerateSocketsArray(PossibleNames);
				 SocketsBuilder = TSharedPtr<FNiagaraDetailSourcedArrayBuilder>(new FNiagaraDetailSourcedArrayBuilder(Property.ToSharedRef(), PossibleNames));
				 SkelCategory->AddCustomBuilder(SocketsBuilder.ToSharedRef());
			 }
			 else
			 {
				 SkelCategory->AddProperty(Property);
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
	TArray<TSharedPtr<FName>> PossibleNames;
	GenerateRegionsArray(PossibleNames);
	RegionsBuilder->SetSourceArray(PossibleNames);

	GenerateBonesArray(PossibleNames);
	BonesBuilder->SetSourceArray(PossibleNames);

	GenerateSocketsArray(PossibleNames);
	SocketsBuilder->SetSourceArray(PossibleNames);
 }

void FNiagaraDataInterfaceSkeletalMeshDetails::GenerateRegionsArray(TArray<TSharedPtr<FName>>& SourceArray)
 {
	SourceArray.Reset();
	 if (MeshInterface.IsValid())
	 {
		 TWeakObjectPtr<USceneComponent> SceneComponent;
		 USkeletalMeshComponent* FoundSkelComp = nullptr;
		 USkeletalMesh* Mesh = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(MeshInterface.Get(), Cast<UNiagaraComponent>(MeshInterface->GetOuter()), SceneComponent, FoundSkelComp);

		 if (Mesh != nullptr)
		 {
			 for (FSkeletalMeshSamplingRegion Region : Mesh->GetSamplingInfo().Regions)
			 {
				 SourceArray.Add(MakeShared<FName>(Region.Name));
			 }
		 }
	 }
 }

 void FNiagaraDataInterfaceSkeletalMeshDetails::GenerateBonesArray(TArray<TSharedPtr<FName>>& SourceArray)
 {
	 SourceArray.Reset();
	 if (MeshInterface.IsValid())
	 {
		 TWeakObjectPtr<USceneComponent> SceneComponent;
		 USkeletalMeshComponent* FoundSkelComp = nullptr;
		 USkeletalMesh* Mesh = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(MeshInterface.Get(), Cast<UNiagaraComponent>(MeshInterface->GetOuter()), SceneComponent, FoundSkelComp);

		 if (Mesh != nullptr)
		 {
			 for (const FMeshBoneInfo& Bone : Mesh->RefSkeleton.GetRefBoneInfo())
			 {
				 SourceArray.Add(MakeShared<FName>(Bone.Name));
			 }
		 }
	 }
 }

void FNiagaraDataInterfaceSkeletalMeshDetails::GenerateSocketsArray(TArray<TSharedPtr<FName>>& SourceArray)
 {
	SourceArray.Reset();
	 if (MeshInterface.IsValid())
	 {
		 TWeakObjectPtr<USceneComponent> SceneComponent;
		 USkeletalMeshComponent* FoundSkelComp = nullptr;
		 USkeletalMesh* Mesh = UNiagaraDataInterfaceSkeletalMesh::GetSkeletalMeshHelper(MeshInterface.Get(), Cast<UNiagaraComponent>(MeshInterface->GetOuter()), SceneComponent, FoundSkelComp);

		 if (Mesh != nullptr)
		 {
			 for (int32 SocketIdx = 0; SocketIdx < Mesh->NumSockets(); ++SocketIdx)
			 {
				 const USkeletalMeshSocket* SocketInfo = Mesh->GetSocketByIndex(SocketIdx);
				 SourceArray.Add(MakeShared<FName>(SocketInfo->SocketName));
			 }
		 }
	 }
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