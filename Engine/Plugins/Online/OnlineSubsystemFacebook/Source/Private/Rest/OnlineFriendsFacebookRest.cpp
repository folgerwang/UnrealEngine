// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#if USES_RESTFUL_FACEBOOK

#include "OnlineFriendsFacebookRest.h"
#include "OnlineSubsystemFacebookPrivate.h"

FOnlineFriendsFacebook::FOnlineFriendsFacebook(FOnlineSubsystemFacebook* InSubsystem) 
	: FOnlineFriendsFacebookCommon(InSubsystem)
{
}

FOnlineFriendsFacebook::FOnlineFriendsFacebook()
	: FOnlineFriendsFacebookCommon(nullptr)
{
}

FOnlineFriendsFacebook::~FOnlineFriendsFacebook()
{
}

#endif // USES_RESTFUL_FACEBOOK
