// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

struct x509_store_ctx_st;
typedef struct x509_store_ctx_st X509_STORE_CTX;

class ISslCertificateManager
{
public:
	virtual ~ISslCertificateManager() {}

	/**
	 * Add trusted root certificates to the SSL context
	 *
	 * @param SslContextPtr Ssl context
	 */
	virtual void AddCertificatesToSslContext(SSL_CTX* SslContextPtr) const = 0;

	/**
	 * @return true if certificates are available
	 */
	virtual bool HasCertificatesAvailable() const = 0;

	/**
	 * Clear all pinned keys
	 */
	virtual void ClearAllPinnedPublicKeys() = 0;

	/**
	* Set digests for pinned certificate public key for a domain
	*
	* @param Domain Domain the pinned keys are valid for. If Domain starts with a '.' it will match any subdomain
	* @param PinnedKeyDigests Semicolon separated base64 encoded SHA256 digests of pinned public keys
	*/
	virtual void SetPinnedPublicKeys(const FString& Domain, const FString& PinnedKeyDigests) = 0;

	/**
	 * Performs additional ssl validation (certificate pinning)
	 *
	 * @param Context Pointer to the x509 context containing a certificate chain
	 * @param Domain Domain we are connected to
	 *
	 * @return false if validation fails
	 */
	virtual bool VerifySslCertificates(X509_STORE_CTX* Context, const FString& Domain) const = 0;
};
