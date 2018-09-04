// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
// FSetSessionDescriptionObserver
// WebRTC requires an implementation of `webrtc::SetSessionDescriptionObserver` interface as a callback
// for setting session description, either on receiving remote `offer` (`PeerConnection::SetRemoteDescription`)
// of on sending `answer` (`PeerConnection::SetLocalDescription`)
class FSetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver
{
public:
	using FSuccessCallback = std::function<void()>;
	using FFailureCallback = std::function<void(const std::string&)>;

	static FSetSessionDescriptionObserver*
	Create(FSuccessCallback successCallback, FFailureCallback failureCallback)
	{
		return new rtc::RefCountedObject<FSetSessionDescriptionObserver>(std::move(successCallback), std::move(failureCallback));
	}

	FSetSessionDescriptionObserver(FSuccessCallback successCallback, FFailureCallback failureCallback)
		: SuccessCallback(std::move(successCallback))
		, FailureCallback(std::move(failureCallback))
	{}

	// we don't need to do anything on success
	void OnSuccess() override
	{
		SuccessCallback();
	}

	// errors usually mean incompatibility between our session configuration (often H.264, its profile and level) and
	// client (browser), malformed SDP or if client doesn't support PlanB/UnifiedPlan (whatever was used by proxy)
	void OnFailure(const std::string& Error) override
	{
		FailureCallback(Error);
	}

private:
	FSuccessCallback SuccessCallback;
	FFailureCallback FailureCallback;
};
