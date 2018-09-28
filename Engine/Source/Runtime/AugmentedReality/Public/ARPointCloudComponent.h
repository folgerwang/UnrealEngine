// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"
#include "ARPointCloudComponent.generated.h"

class FPrimitiveSceneProxy;

/**
 * Component for rendering a point cloud
 */
UCLASS(hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent), ClassGroup = AR)
class AUGMENTEDREALITY_API UARPointCloudComponent :
	public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/**	If true, each tick the component will try to update its point cloud data from the AR system */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Point Cloud")
	bool bAutoBindToARSystem;
	
	/**	If true, each tick the component will render its point cloud */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AR|Point Cloud")
	bool bIsVisible;
	
	/** Changes the visibility setting */
	UFUNCTION(BlueprintCallable, Category = "AR|Point Cloud")
	void SetIsVisible(bool bNewVisibility);

	/** Point cloud data that will be used for rendering, assumes each point is in world space */
	UPROPERTY(BlueprintReadOnly, Category = "AR|Point Cloud")
	TArray<FVector> PointCloud;
	
	/**
	 * Updates the point cloud data with the new set of points
	 *
	 * @param Points the new set of points to use
	 */
	UFUNCTION(BlueprintCallable, Category = "AR|Point Cloud")
	void SetPointCloud(const TArray<FVector>& Points);
	
	/**
	 * Empties the point cloud
	 */
	UFUNCTION(BlueprintCallable, Category = "AR|Point Cloud")
	void ClearPointCloud();
	
	/** The color to render the points with */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AR|Point Cloud")
	FLinearColor PointColor;

	/** Allows you to change the color of the points being rendered */
	UFUNCTION(BlueprintCallable, Category = "AR|Point Cloud")
	void SetPointColor(const FLinearColor& Color);

	/** The size of the point when rendering */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AR|Point Cloud")
	float PointSize;
	
	/** Allows you to change the size of the points being rendered */
	UFUNCTION(BlueprintCallable, Category = "AR|Point Cloud")
	void SetPointSize(float Size);

	/** Determines which points are within the box and returns those to the caller */
	UFUNCTION(BlueprintPure, Category = "AR|Point Cloud")
	TArray<FVector> GetPointsInBox(const FBox& WorldSpaceBox) const;

	/** Determines which points are outside the box and returns those to the caller */
	UFUNCTION(BlueprintPure, Category = "AR|Point Cloud")
	TArray<FVector> GetPointsOutsideBox(const FBox& WorldSpaceBox) const;
	
private:
	virtual FMatrix GetRenderMatrix() const override;
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual void InitializeComponent() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** World space bounds of the point cloud */
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
	
	/** Used to know whether to update the point cloud or not */
	float LastUpdateTimestamp;

	friend class FARPointCloudSceneProxy;
};


