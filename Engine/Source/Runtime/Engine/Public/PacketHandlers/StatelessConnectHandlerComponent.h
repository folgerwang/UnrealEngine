// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PacketHandler.h"

/** Temporary version-check macro for randomized packet sequence, for backwards-compatibility. */
#define STATELESSCONNECT_HAS_RANDOM_SEQUENCE 1

class UNetDriver;

DECLARE_LOG_CATEGORY_EXTERN(LogHandshake, Log, All);


/**
 * Forward Declarations
 */

class UNetConnection;
class UNetDriver;


/**
 * Defines
 */

#define SECRET_BYTE_SIZE 64
#define SECRET_COUNT 2
#define COOKIE_BYTE_SIZE 20


/**
 * PacketHandler component for implementing a stateless (non-memory-consuming) connection handshake
 *
 * Partially based on the Datagram Transport Layer Security protocol.
 */
class StatelessConnectHandlerComponent : public HandlerComponent
{
public:
	/**
	 * Base constructor
	 */
	StatelessConnectHandlerComponent();

	virtual void CountBytes(FArchive& Ar) const override;

	virtual bool IsValid() const override { return true; }

	virtual void NotifyHandshakeBegin() override;

	/**
	 * Initializes a serverside UNetConnection-associated StatelessConnect,
	 * from the connectionless StatelessConnect that negotiated the handshake.
	 *
	 * @param InConnectionlessHandler	The connectionless StatelessConnect we're initializing from
	 */
	void InitFromConnectionless(StatelessConnectHandlerComponent* InConnectionlessHandler);

private:
	/**
	 * Constructs and sends the server response to the initial connect packet, from the server to the client.
	 *
	 * @param ClientAddress		The address of the client to send the challenge to.
	 */
	void SendConnectChallenge(const FString& ClientAddress);

	/**
	 * Constructs and sends the handshake challenge response packet, from the client to the server
	 *
	 * @param InSecretId		Which of the two server HandshakeSecret values this uses
	 * @param InTimestamp		The timestamp value to send
	 * @param InCookie			The cookie value to send
	 */
	void SendChallengeResponse(uint8 InSecretId, float InTimestamp, uint8 InCookie[COOKIE_BYTE_SIZE]);

	/**
	 * Constructs and sends the server ack to a successful challenge response, from the server to the client.
	 *
	 * @param ClientAddress		The address of the client to send the ack to.
	 * @param InCookie			The cookie value to send
	 */
	void SendChallengeAck(const FString& ClientAddress, uint8 InCookie[COOKIE_BYTE_SIZE]);

	/**
	 * Constructs and sends a request to resend the cookie, from the server to the client.
	 *
	 * @param ClientAddress		The address of the client to send the request to.
	 */
	void SendRestartHandshakeRequest(const FString& ClientAddress);


	/**
	 * Pads the handshake packet, to match the PacketBitAlignment of the PacketHandler, so that it will parse correctly.
	 *
	 * @param HandshakePacket	The handshake packet to be aligned.
	 */
	void CapHandshakePacket(FBitWriter& HandshakePacket);

public:
	/**
	 * Whether or not the specified connection address, has just passed the connection handshake challenge.
	 *
	 * @param Address					The address (including port, for UIpNetDriver) being checked
	 */
	UE_DEPRECATED(4.22, "HasPassedChallenge must return bOutRestartedHandshake, in order to correctly process the challenge. Use the new version")
	FORCEINLINE bool HasPassedChallenge(const FString& Address) const
	{
		bool bDud = false;

		return HasPassedChallenge(Address, bDud);
	}

	/**
	 * Whether or not the specified connection address, has just passed the connection handshake challenge.
	 *
	 * @param Address					The address (including port, for UIpNetDriver) being checked
	 * @param bOutRestartedHandshake	Whether or not the passed challenge, was a restarted handshake for an existing NetConnection
	 */
	FORCEINLINE bool HasPassedChallenge(const FString& Address, bool& bOutRestartedHandshake) const
	{
		bOutRestartedHandshake = bRestartedHandshake;

		return LastChallengeSuccessAddress == Address;
	}

	/**
	 * Used for retrieving the initial packet sequence values from the handshake data, after a successful challenge
	 *
	 * @param OutServerSequence		Outputs the initial sequence for the server
	 * @param OutClientSequence		Outputs the initial sequence for the client
	 */
	FORCEINLINE void GetChallengeSequence(int32& OutServerSequence, int32& OutClientSequence) const
	{
		OutServerSequence = LastServerSequence;
		OutClientSequence = LastClientSequence;
	}

	/**
	 * When a restarted handshake is completed, this is used to match it up with the existing NetConnection
	 *
	 * @param NetConnComponent	The NetConnection StatelessConnectHandlerComponent, which is being checked for a match
	 * @return					Whether or not the specified component, belongs to the client which restarted the handshake
	 */
	FORCEINLINE bool DoesRestartedHandshakeMatch(StatelessConnectHandlerComponent& NetConnComponent) const
	{
		return FMemory::Memcmp(AuthorisedCookie, NetConnComponent.AuthorisedCookie, COOKIE_BYTE_SIZE) == 0;
	}

	/**
	 * Used to reset cached handshake success/fail state, when done processing it
	 */
	FORCEINLINE void ResetChallengeData()
	{
		LastChallengeSuccessAddress.Empty();
		bRestartedHandshake = false;
		LastServerSequence = 0;
		LastClientSequence = 0;
		FMemory::Memzero(AuthorisedCookie, COOKIE_BYTE_SIZE);
	}


	/**
	 * Sets the net driver this handler is associated with
	 *
	 * @param InDriver	The net driver to set
	 */
	void SetDriver(UNetDriver* InDriver);


protected:
	virtual void Initialize() override;

	virtual void Incoming(FBitReader& Packet) override;

	virtual void Outgoing(FBitWriter& Packet, FOutPacketTraits& Traits) override;

	virtual void IncomingConnectionless(const FString& Address, FBitReader& Packet) override;

	virtual void OutgoingConnectionless(const FString& Address, FBitWriter& Packet, FOutPacketTraits& Traits) override
	{
	}

	virtual bool CanReadUnaligned() const override
	{
		return true;
	}

	virtual int32 GetReservedPacketBits() const override;

	virtual void Tick(float DeltaTime) override;

private:
	/**
	 * Parses an incoming handshake packet (does not parse the handshake bit though)
	 *
	 * @param Packet				The packet the handshake is being parsed from
	 * @param bOutRestartHandshake	Whether or not this packet is a restart handshake packet
	 * @param OutSecretId			Which of the two serverside HandshakeSecret values this is based on
	 * @param OutTimestamp			The server timestamp, from the moment the challenge was sent (or 0.f if from the client)
	 * @param OutCookie				A unique identifier, generated by the server, which the client must reply with (or 0, for initial packet)
	 * @param OutOrigCookie			If this is a restart handshake challenge response, this is the original handshake's cookie
	 * @return						Whether or not the handshake packet was parsed successfully
	 */
	bool ParseHandshakePacket(FBitReader& Packet, bool& bOutRestartHandshake, uint8& OutSecretId, float& OutTimestamp,
								uint8 (&OutCookie)[COOKIE_BYTE_SIZE], uint8 (&OutOrigCookie)[COOKIE_BYTE_SIZE]);

	/**
	 * Takes the client address plus server timestamp, and outputs a deterministic cookie value
	 *
	 * @param ClientAddress		The address of the client, including port
	 * @param SecretId			Which of the two HandshakeSecret values the cookie will be based on
	 * @param TimeStamp			The serverside timestamp
	 * @param OutCookie			Outputs the generated cookie value.
	 */
	void GenerateCookie(FString ClientAddress, uint8 SecretId, float Timestamp, uint8 (&OutCookie)[COOKIE_BYTE_SIZE]);

	/**
	 * Generates a new HandshakeSecret value
	 */
	void UpdateSecret();


private:
	/** The net driver associated with this handler - for performing connectionless sends */
	UNetDriver* Driver;


	/** Serverside variables */

	/** The serverside-only 'secret' value, used to help with generating cookies. */
	TArray<uint8> HandshakeSecret[SECRET_COUNT];

	/** Which of the two secret values above is active (values are changed frequently, to limit replay attacks) */
	uint8 ActiveSecret;

	/** The time of the last secret value update */
	float LastSecretUpdateTimestamp;

	/** The last address to successfully complete the handshake challenge */
	FString LastChallengeSuccessAddress;

	/** The initial server sequence value, from the last successful handshake */
	int32 LastServerSequence;

	/** The initial client sequence value, from the last successful handshake */
	int32 LastClientSequence;


	/** Clientside variables */

	/** The last time a handshake packet was sent - used for detecting failed sends. */
	double LastClientSendTimestamp;


	/** The local (client) time at which the challenge was last updated */
	double LastChallengeTimestamp;

	/** The SecretId value of the last challenge response sent */
	uint8 LastSecretId;

	/** The Timestamp value of the last challenge response sent */
	float LastTimestamp;

	/** The Cookie value of the last challenge response sent. Will differ from AuthorisedCookie, if a handshake retry is triggered. */
	uint8 LastCookie[COOKIE_BYTE_SIZE];


	/** Both Serverside and Clientside variables */

	/** Client: Whether or not we are in the middle of a restarted handshake. Server: Whether or not the last handshake was a restarted handshake. */
	bool bRestartedHandshake;

	/** The cookie which completed the connection handshake. */
	uint8 AuthorisedCookie[COOKIE_BYTE_SIZE];
};

