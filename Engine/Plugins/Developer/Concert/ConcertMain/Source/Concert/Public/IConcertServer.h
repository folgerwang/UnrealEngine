// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IConcertSession.h"

class UConcertServerConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnConcertServerSessionStartupOrShutdown, TSharedRef<IConcertServerSession>);

/** Interface for Concert server */
class IConcertServer
{
public:
	virtual ~IConcertServer() {}

	/**
	 * Configure the Concert settings and its information
	 */
	virtual void Configure(const UConcertServerConfig* ServerConfig) = 0;
	
	/** 
	 * Return true if the server has been configured.
	 */
	virtual bool IsConfigured() const = 0;

	/**
	 *	Returns if the server has already been started up.
	 */
	virtual bool IsStarted() const = 0;

	/**
	 * Startup the server, this can be called multiple time
	 * Configure needs to be called before startup
	 * @return true if the server was properly started or already was
	 */
	virtual void Startup() = 0;

	/**
	 * Shutdown the server, this can be called multiple time with no ill effect.
 	 * However it depends on the UObject system so need to be called before its exit.
	 */
	virtual void Shutdown() = 0;

	/**
	 * Get the delegate that is called right before the client session startup
	 */
	virtual FOnConcertServerSessionStartupOrShutdown& OnSessionStartup() = 0;

	/**
	 * Get the delegate that is called right before the client session shutdown
	 */
	virtual FOnConcertServerSessionStartupOrShutdown& OnSessionShutdown() = 0;

	/**
	 * Create a session description for this server
	 */
	virtual FConcertSessionInfo CreateSessionInfo() const = 0;

	/**
	 * Get the sessions information list
	 */
	virtual	TArray<FConcertSessionInfo> GetSessionsInfo() const = 0;

	/**
	 * Get all server sessions
	 * @return array of server sessions
	 */
	virtual TArray<TSharedPtr<IConcertServerSession>> GetSessions() const = 0;

	/**
	 * Get a server session
	 * @param SessionName	The name of the session we want
	 * @return the server session or an invalid pointer if no session was found
	 */
	virtual TSharedPtr<IConcertServerSession> GetSession(const FName& SessionName) const = 0;

	/** 
	 * Create a new Concert server session based on the passed session info
	 * @return the created server session
	 */
	virtual TSharedPtr<IConcertServerSession> CreateSession(const FConcertSessionInfo& SessionInfo) = 0;

	/**
	 * Destroy a Concert server session
	 * @param SessionName the name of the session to destroy
	 * @return true if the session was found and destroyed
	 */
	virtual bool DestroySession(const FName& SessionName) = 0;

	/**
	 * Get the list of clients for a session
	 * @param SessionName	The session name
	 * @return A list of clients  connected to the session
	 */
	virtual TArray<FConcertSessionClientInfo> GetSessionClients(const FName& SessionName) const = 0;
};