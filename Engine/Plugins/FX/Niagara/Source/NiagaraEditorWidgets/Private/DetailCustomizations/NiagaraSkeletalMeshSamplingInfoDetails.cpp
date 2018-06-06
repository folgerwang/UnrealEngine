// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSkeletalMeshSamplingInfoDetails.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Engine/SkeletalMesh.h"
#include "NiagaraDetailSourcedArrayBuilder.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "PropertyCustomizationHelpers.h"
#include "IDetailChildrenBuilder.h"


 #define LOCTEXT_NAMESPACE "FNiagaraSkeletalMeshSamplingInfoDetails"

 void FNiagaraSkeletalMeshSamplingInfoDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
 {
	 static const FName SamplingCategoryName = TEXT("Sampling");
	 static const FString RegionsPropertyNameString = "Regions";

	 TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	 DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	 check(SelectedObjects.Num() == 1);
	 USkeletalMesh*  MeshObject = Cast<USkeletalMesh>(SelectedObjects[0].Get());
	 check(MeshObject);
	 
	 if (MeshObject->Skeleton != nullptr)
	 {
		 //Populate PossibleBonesNames
		 for (FMeshBoneInfo Bone : MeshObject->Skeleton->GetReferenceSkeleton().GetRefBoneInfo())
		 {
			 TSharedPtr<FName> BoneName(new FName(Bone.Name));
			 PossibleBonesNames.Add(BoneName);
		 }
	 }

	 //Populate PossibleMaterialsNames
	 for (FSkeletalMaterial Material : MeshObject->Materials)
	 {
		 TSharedPtr<FName> MaterialName(new FName(Material.MaterialSlotName));
		 PossibleMaterialsNames.Add(MaterialName);
	 }

	 IDetailCategoryBuilder& SamplingCategory = DetailBuilder.EditCategory(SamplingCategoryName, LOCTEXT("Sampling", "Sampling"));
	 {
		 TArray<TSharedRef<IPropertyHandle>> SamplingProperties;
		 SamplingCategory.GetDefaultProperties(SamplingProperties, true, false);

		 for (TSharedPtr<IPropertyHandle> Property : SamplingProperties)
		 {
			 if (Property->GetProperty()->GetName() == RegionsPropertyNameString)
			 {
				 TSharedRef<FDetailArrayBuilder> RegionsBuilder = MakeShareable(new FDetailArrayBuilder(Property.ToSharedRef(), true, true));
				 RegionsBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FNiagaraSkeletalMeshSamplingInfoDetails::OnGenerateRegionEntry));
				 SamplingCategory.AddCustomBuilder(RegionsBuilder);
			 }
			 else
			 {
				 SamplingCategory.AddProperty(Property);
			 }
		 }
	 }
 }

 void FNiagaraSkeletalMeshSamplingInfoDetails::OnGenerateRegionEntry(TSharedRef<IPropertyHandle> PropertyHandle, int32 ArrayIndex, IDetailChildrenBuilder& ChildrenBuilder)
  {
 	 IDetailPropertyRow& RegionRow = ChildrenBuilder.AddProperty(PropertyHandle);
 
 	 FNumberFormattingOptions NoCommas;
 	 NoCommas.UseGrouping = false;
 	 const FText SlotDesc = FText::Format(LOCTEXT("RegionSlotIndex", "Region #{0}"), FText::AsNumber(ArrayIndex, &NoCommas));
 
 	 RegionRow.DisplayName(SlotDesc);
 
 	 RegionRow.ShowPropertyButtons(true);
	 // Add custom builder for properties
	 TSharedPtr<IPropertyHandle> BoneFiltersProperty = PropertyHandle->GetChildHandle("BoneFilters");
	 TSharedRef<FNiagaraDetailSourcedArrayBuilder> BoneBuilder = MakeShareable(new FNiagaraDetailSourcedArrayBuilder(BoneFiltersProperty.ToSharedRef(), PossibleBonesNames, "BoneName"));
	 ChildrenBuilder.AddCustomBuilder(BoneBuilder);

	 TSharedPtr<IPropertyHandle> MaterialFiltersProperty = PropertyHandle->GetChildHandle("MaterialFilters");
	 TSharedRef<FNiagaraDetailSourcedArrayBuilder> MaterialBuilder = MakeShareable(new FNiagaraDetailSourcedArrayBuilder(MaterialFiltersProperty.ToSharedRef(), PossibleMaterialsNames, "MaterialName"));
	 ChildrenBuilder.AddCustomBuilder(MaterialBuilder);

  }

 TSharedRef<IDetailCustomization> FNiagaraSkeletalMeshSamplingInfoDetails::MakeInstance()
 {
	 return MakeShared<FNiagaraSkeletalMeshSamplingInfoDetails>();
 }
#undef LOCTEXT_NAMESPACE