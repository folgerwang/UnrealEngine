// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 2017 Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MagicLeapSecureStorage.generated.h"

/**
  Function library for the Magic Leap Secure Storage API.
  Currently supports bool, uint8, int32, float, FString, FVector, FRotator and FTransform via Blueprints.
  Provides a template function for any non specialized types to be used via C++.
  TODO: Support TArray and a generic USTRUCT.
*/
UCLASS(ClassGroup = MagicLeap)
class MAGICLEAPSECURESTORAGE_API UMagicLeapSecureStorage : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	  Stores the boolean under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureBool(const FString& Key, bool DataToStore);

	/**
	  Stores the byte (uint8) under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureByte(const FString& Key, uint8 DataToStore);

	/**
	  Stores the integer (int32) under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureInt(const FString& Key, int32 DataToStore);

	/**
	  Stores the float under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureFloat(const FString& Key, float DataToStore);

	/**
	  Stores the string under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureString(const FString& Key, const FString& DataToStore);

	/**
	  Stores the vector under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureVector(const FString& Key, const FVector& DataToStore);

	/**
	  Stores the rotator under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureRotator(const FString& Key, const FRotator& DataToStore);

	/**
	  Stores the transform under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool PutSecureTransform(const FString& Key, const FTransform& DataToStore);

	/**
	  Retrieves the boolean associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureBool(const FString& Key, bool& DataToRetrieve);

	/**
	  Retrieves the byte (uint8) associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureByte(const FString& Key, uint8& DataToRetrieve);

	/**
	  Retrieves the integer (int32) associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureInt(const FString& Key, int32& DataToRetrieve);

	/**
	  Retrieves the float associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureFloat(const FString& Key, float& DataToRetrieve);

	/**
	  Retrieves the string associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureString(const FString& Key, FString& DataToRetrieve);

	/**
	  Retrieves the vector associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureVector(const FString& Key, FVector& DataToRetrieve);

	/**
	  Retrieves the rotator associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureRotator(const FString& Key, FRotator& DataToRetrieve);

	/**
	  Retrieves the transform associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool GetSecureTransform(const FString& Key, FTransform& DataToRetrieve);

	/**
	  Deletes the data associated with the specified key.
	  @param   Key The string key of the data to delete.
	  @return  True if the data was deleted successfully or did not exist altogether, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "SecureStorage|MagicLeap")
	static bool DeleteSecureData(const FString& Key);

	/**
	  Template function to store the data under the specified key. An existing key would be overwritten.
	  @param   Key String key associated with the data.
	  @param   DataToStore The data to store.
	  @return  True if the data was stored successfully, false otherwise.
	*/
	template<class T>
	static bool PutSecureBlob(const FString& Key, const T* DataToStore);

	/**
	  Template function to retrieve the data associated with the specified key.
	  @param   Key The string key to look for.
	  @param   DataToRetrieve Reference to the variable that will be populated with the stored data.
	  @return  True if the key was found and output parameter was successfully populated with the data, false otherwise.
	*/
	template<class T>
	static bool GetSecureBlob(const FString& Key, T& DataToRetrieve);
};
