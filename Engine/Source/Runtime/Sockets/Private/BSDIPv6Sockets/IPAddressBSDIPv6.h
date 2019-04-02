// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "SocketSubsystemBSDIPv6.h"
#include "IPAddress.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

/**
* Represents an internet ip address, using the relatively standard sockaddr_in6 structure. All data is in network byte order
*/
class FInternetAddrBSDIPv6 : public FInternetAddr
{
private:
	/** The internet ip address structure */
	sockaddr_in6 Addr;

	/** Horrible hack to catch hard coded multicasting on IPv4 **/
	static const uint32 IPv4MulitcastAddr = ((230 << 24) | (0 << 16) | (0 << 8) | (1 << 0));

public:
	/**
	 * Constructor. Sets address to default state
	 */
	FInternetAddrBSDIPv6();

	/**
	 * Sets the ip address from a raw network byte order array.
	 *
	 * @param RawAddr the new address to use (must be converted to network byte order)
	 */
	virtual void SetRawIp(const TArray<uint8>& RawAddr) override;

	/**
	 * Gets the ip address in a raw array stored in network byte order.
	 *
	 * @return The raw address stored in an array
	 */
	virtual TArray<uint8> GetRawIp() const override;

	/**
	 * Sets the ip address from a host byte order uint32, convert the IPv4 address supplied to an IPv6 address
	 *
	 * @param InAddr the new address to use (must convert to network byte order)
	 */
	virtual void SetIp(uint32 InAddr) override;

	/**
	 * Sets the ip address from a string IPv6 or IPv4 address.
	 * Ports may be included using the form Address:Port, or excluded and set manually.
	 *
	 * IPv6 - [1111:2222:3333:4444:5555:6666:7777:8888]:PORT || [1111:2222:3333::]:PORT || [::ffff:IPv4]:PORT
	 * IPv4 - aaa.bbb.ccc.ddd:PORT || 127.0.0.1:1234:PORT
	 *
	 * @param InAddr the string containing the new ip address to use
	 * @param bIsValid will be set to true if InAddr was a valid IPv6 or IPv4 address, false if not.
	 */
	virtual void SetIp(const TCHAR* InAddr, bool& bIsValid) override;

	/**
	 * Sets the ip address using a network byte order ipv4 address
	 *
	 * @param IPv4Addr the new ip address to use
	 */
	void SetIp(const in_addr& IPv4Addr);

	/**
	 * Sets the ip address using a network byte order ipv6 address
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetIp(const in6_addr& IpAddr);
	
#if PLATFORM_IOS
	void SetIp(const sockaddr_in6& IpAddr)
	{
		Addr = IpAddr;
	}
#endif

	/**
	 * Sets the ip address using a generic sockaddr_storage
	 *
	 * @param IpAddr the new ip address to use
	 */
	void SetIp(const sockaddr_storage& IpAddr);

	/**
	 * Copies the network byte order ip address to a host byte order dword, doesn't exist with IPv6
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	virtual void GetIp(uint32& OutAddr) const override;

	/**
	 * Copies the network byte order ip address
	 *
	 * @param OutAddr the out param receiving the ip address
	 */
	void GetIp(in6_addr& OutAddr) const;

	/**
	 * Sets the port number from a host byte order int
	 *
	 * @param InPort the new port to use (must convert to network byte order)
	 */
	virtual void SetPort(int32 InPort) override;

	/** Returns the port number from this address in host byte order */
	virtual int32 GetPort() const override;

	/** Sets the address to be any address */
	virtual void SetAnyAddress() override;

	/** Sets the address to broadcast */
	virtual void SetBroadcastAddress() override;

	/** Sets the address to the loopback address */
	virtual void SetLoopbackAddress() override;

	/**
	 * Converts this internet ip address to string form. String will be enclosed in square braces.
	 *
	 * @param bAppendPort whether to append the port information or not
	 */
	virtual FString ToString(bool bAppendPort) const override;

	/**
	 * Compares two internet ip addresses for equality
	 *
	 * @param Other the address to compare against
	 */
	virtual bool operator==(const FInternetAddr& Other) const override;

	virtual uint32 GetTypeHash() const override;

	/**
	 * Is this a well formed internet address, the only criteria being non-zero
	 *
	 * @return true if a valid IP, false otherwise
	 */
	virtual bool IsValid() const override;

	operator sockaddr*(void)
	{
		return (sockaddr*)&Addr;
	}

	operator const sockaddr*(void) const
	{
		return (const sockaddr*)&Addr;
	}

	virtual TSharedRef<FInternetAddr> Clone() const override;

	/**
	 * Sets the scope interface id of the currently held address if this address
	 * is an IPv6 address internally. If it is not, no data will be assigned.
	 * The NewScopeId must be in host byte order.
	 *
	 * @param NewScopeId the new scope interface id to set this address to
	 */
	virtual void SetScopeId(uint32 NewScopeId);

	/**
	 * Returns the IPv6 scope interface id of the currently held address
	 * if the address is an IPv6 address.
	 *
	 * @return the scope interface id
	 */
	virtual uint32 GetScopeId() const;
};


class UE_DEPRECATED(4.21, "This class is no longer needed as the base class can handle the proper construction now.") FResolveInfoCachedBSDIPv6 : public FResolveInfoCached
{
	FResolveInfoCachedBSDIPv6();

public:
	FResolveInfoCachedBSDIPv6(const FInternetAddr& InAddr);
};

#endif
