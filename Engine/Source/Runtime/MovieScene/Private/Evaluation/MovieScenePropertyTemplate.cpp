// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieScenePropertyTemplate.h"

static TMovieSceneAnimTypeIDContainer<FString> PropertyTypeIDs;

// Default property ID to our own type - this implies an empty property
PropertyTemplate::FSectionData::FSectionData()
	: PropertyID(TMovieSceneAnimTypeID<FSectionData>())
{
}

void PropertyTemplate::FSectionData::Initialize(FName InPropertyName, FString InPropertyPath, FName InFunctionName, FName InNotifyFunctionName)
{
	PropertyBindings = MakeShareable(new FTrackInstancePropertyBindings(InPropertyName, MoveTemp(InPropertyPath), InFunctionName, InNotifyFunctionName));
	PropertyID = PropertyTypeIDs.GetAnimTypeID(InPropertyPath);
}

FMovieScenePropertySectionTemplate::FMovieScenePropertySectionTemplate(FName PropertyName, const FString& InPropertyPath)
	: PropertyData(PropertyName, InPropertyPath)
{}

void FMovieScenePropertySectionTemplate::Setup(FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) const
{
	PropertyData.SetupTrack(PersistentData);
}

FMovieSceneAnimTypeID FMovieScenePropertySectionTemplate::GetPropertyTypeID() const
{
	return PropertyTypeIDs.GetAnimTypeID(PropertyData.PropertyPath);
}

const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetFloatInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetInt32InterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetTransformInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetEulerTransformInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetVector4InterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetVectorInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}
const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetVector2DInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}

const FMovieSceneInterrogationKey FMovieScenePropertySectionTemplate::GetColorInterrogationKey()
{
	const static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique();
	return TypeID;
}