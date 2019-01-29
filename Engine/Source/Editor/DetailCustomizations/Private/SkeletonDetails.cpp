// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SkeletonDetails.h"
#include "DetailLayoutBuilder.h"
#include "Animation/Skeleton.h"

TSharedRef<IDetailCustomization> FSkeletonDetails::MakeInstance()
{
	return MakeShareable(new FSkeletonDetails());
}

void FSkeletonDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(USkeleton, BoneTree));
}

