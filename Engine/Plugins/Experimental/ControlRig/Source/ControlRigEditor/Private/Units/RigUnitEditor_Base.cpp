// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "Units/RigUnitEditor_Base.h"
#include "ControlRig.h"
#include "HelperUtil.h"
#include "PropertyPathHelpers.h"

/////////////////////////////////////////////////////
// URigUnitEditor_Base

URigUnitEditor_Base::URigUnitEditor_Base(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URigUnitEditor_Base::UpdateSourceProperties(const FString& PropertyName) const
{
	if (HasValidReference())
	{
		// @Todo: clean up all this text ops
		FName RigUnitName = SourceRigUnit->RigUnitName;
		FName SearchName = FName(*(RigUnitName.ToString() + TEXT(".") + PropertyName));
		const FString* SourcePropertyName = ControlRig->AllowSourceAccessProperties.Find(SearchName);
		if (SourcePropertyName)
		{
			FCachedPropertyPath SourcePropertyPath(*SourcePropertyName);
			FCachedPropertyPath TargetPropertyPath(SearchName.ToString());
			FName SourceRootName = SourcePropertyPath.GetSegment(0).GetName();

			FRigUnit_Control* ControlUnit = ControlRig->GetControlRigUnitFromName(SourceRootName);
			if (ControlUnit)
			{
				//FTransform Transform;
				//PropertyPathHelpers::GetPropertyValue(ControlRig, TargetPropertyPath, Transform);
				// Read the value from property
				FCachedPropertyPath CachedPath(SearchName.ToString());
				CachedPath.Resolve(ControlRig);
				FTransform TransformValue;
				if (PropertyPathHelpers::GetPropertyValue<FTransform>(ControlRig, CachedPath, TransformValue))
				{
					ControlUnit->SetResultantTransform(TransformValue);
				}
				else
				{
					ensure(false);
				}
			}
			else
			{
				// namewise, it is confusing, but we're updating source from dest (in this case, we're updating back source)
				FCachedPropertyPath Source(SearchName.ToString());
				FCachedPropertyPath Dest(*SourcePropertyName);
				if (!PropertyPathHelpers::CopyPropertyValue(ControlRig, Dest, Source))
				{
					ensure(false);
				}
			}
		}

		return true;
	}

	return false;
}