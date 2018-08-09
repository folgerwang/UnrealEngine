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

#include "Components/ActorComponent.h"
#include "PrivilegeUtils.h"
#include "MagicLeapPrivileges.generated.h"

/**
 *  Class which provides functions to check and request the priviliges the app has at runtime.
 */
UCLASS(ClassGroup = MagicLeap, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class MAGICLEAP_API UMagicLeapPrivileges : public UActorComponent
{
	GENERATED_BODY()

public:
	UMagicLeapPrivileges();
	virtual ~UMagicLeapPrivileges();

	/** Polls for and handles the results of the async privilege requests. */
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/**
	  Delegate for the result of requesting a privilege asynchronously.
	  @param RequestedPrivilege The privilege that was requested.
	  @param WasGranted True if the privilege was granted, false otherwise.
	*/
	DECLARE_DYNAMIC_DELEGATE_TwoParams(FPrivilegeRequestDelegate, EMagicLeapPrivilege, RequestedPrivilege, bool, WasGranted);

	/**
	  Check whether the application has the specified privilege.
	  This does not solicit consent from the end-user and is non-blocking.
	  @param Privilege The privilege to check.
	  @return True if the privilege is granted, false otherwise.
	*/
	UFUNCTION(BlueprintCallable, Category = "Privileges|MagicLeap")
	bool CheckPrivilege(EMagicLeapPrivilege Privilege);

	/**
	  Request the specified privilege.
	  This may possibly solicit consent from the end-user; if so it will block.
	  @param Privilege The privilege to request.
	  @return True if the privilege is granted, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Privileges|MagicLeap")
	bool RequestPrivilege(EMagicLeapPrivilege Privilege);

	/**
	  Request the specified privilege asynchronously.
	  This may possibly solicit consent from the end-user. Result will be delivered
	  to the specified delegate.
	  @param Privilege The privilege to request.
	  @param ResultDelegate Callback which reports the result of the request.
	  @return True if the privilege request was successfully dispatched, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "Privileges|MagicLeap")
	bool RequestPrivilegeAsync(EMagicLeapPrivilege Privilege, const FPrivilegeRequestDelegate& ResultDelegate);

private:
	struct FPendingAsyncRequest
	{
		EMagicLeapPrivilege Privilege;
		struct MLPrivilegesAsyncRequest* Request;
		FPrivilegeRequestDelegate Delegate;
	};
	TArray<FPendingAsyncRequest> PendingAsyncRequests;
};
