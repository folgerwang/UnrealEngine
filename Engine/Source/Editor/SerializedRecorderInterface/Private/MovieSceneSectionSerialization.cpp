// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Serializers/MovieSceneSectionSerialization.h"
#include "Serializers/MovieSceneSectionSerialization.h"
#include "Serializers/MovieSceneActorSerialization.h"
#include "Serializers/MovieSceneManifestSerialization.h"
#include "Serializers/MovieScenePropertySerialization.h"
#include "Serializers/MovieSceneTransformSerialization.h"
#include "Serializers/MovieSceneSpawnSerialization.h"
#include "Serializers/MovieSceneAnimationSerialization.h"
#include "Serializers/MovieSceneSerializedType.h"
#include "Serializers/MovieSceneLiveLinkSerialization.h"


//#include "FrameDebuggerServer.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(MovieSceneSerialization);


const FGuid FTempCustomVersion::GUID(0xCB8AB0CD, 0xE78C4BDE, 0xA8621393, 0x14E9EF62);
FCustomVersionRegistration GRegisterLiveLinkCustomVersion(FTempCustomVersion::GUID, FTempCustomVersion::LatestVersion, TEXT("LiveLinkCustomVersion"));
const int64 MovieSceneSerializationNamespace::InvalidOffset = -1;
const float MovieSceneSerializationNamespace::SerializerSleepTime = 0.2f;
bool MovieSceneSerializationNamespace::bAutoSerialize = false;

/** ==  Template Statics Defined  --  **/
template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FPropertyFileHeader>* TMovieSceneSerializer<FPropertyFileHeader, FPropertyFileHeader>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FPropertyFileHeader>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FTransformFileHeader, FSerializedTransform>* TMovieSceneSerializer<FTransformFileHeader, FSerializedTransform>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FTransformFileHeader, FSerializedTransform>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FAnimationFileHeader, FSerializedAnimation>* TMovieSceneSerializer<FAnimationFileHeader, FSerializedAnimation>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FAnimationFileHeader, FSerializedAnimation>::Thread = nullptr;


template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FSpawnFileHeader, FSpawnProperty>* TMovieSceneSerializer<FSpawnFileHeader, FSpawnProperty>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FSpawnFileHeader, FSpawnProperty>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<int64>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<int64>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<int64>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<bool>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<bool>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<bool>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<uint8>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<uint8>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<uint8>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<float>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<float>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<float>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<FVector>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<FVector>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<FVector>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<FColor>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<FColor>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<FColor>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<int32>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<int32>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<int32>>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FPropertyFileHeader, FSerializedProperty<FString>>* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<FString>>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<FString>>::Thread = nullptr;

/** External Tempplate statics.  Need to define here due to linkage issues*/

/* From TakeRecorderActorSource */
template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FManifestFileHeader, FManifestProperty>* TMovieSceneSerializer<FManifestFileHeader, FManifestProperty>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FManifestFileHeader, FManifestProperty>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FActorFileHeader, FActorProperty>* TMovieSceneSerializer<FActorFileHeader, FActorProperty>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FActorFileHeader, FActorProperty>::Thread = nullptr;


/* From SerializedRecorder */
template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FSerializedTypeFileHeader, FSerializedTypeFileHeader>* TMovieSceneSerializer<FSerializedTypeFileHeader, FSerializedTypeFileHeader>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FSerializedTypeFileHeader, FSerializedTypeFileHeader>::Thread = nullptr;

template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FLiveLinkFileHeader, FLiveLinkFrame>* TMovieSceneSerializer<FLiveLinkFileHeader, FLiveLinkFrame>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FLiveLinkFileHeader, FLiveLinkFrame>::Thread = nullptr;


template<> SERIALIZEDRECORDERINTERFACE_API
FMovieSceneSerializerRunnable<FLiveLinkManifestHeader, FLiveLinkManifestHeader>* TMovieSceneSerializer<FLiveLinkManifestHeader, FLiveLinkManifestHeader>::Runnable = nullptr;
template<> SERIALIZEDRECORDERINTERFACE_API
FRunnableThread* TMovieSceneSerializer<FLiveLinkManifestHeader, FLiveLinkManifestHeader>::Thread = nullptr;