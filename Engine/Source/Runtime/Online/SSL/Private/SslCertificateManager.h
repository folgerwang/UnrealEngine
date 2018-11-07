// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_SSL

#include "Interfaces/ISslCertificateManager.h"

struct x509_st;
typedef struct x509_st X509;

class FSslCertificateManager : public ISslCertificateManager
{
public:
	virtual void AddCertificatesToSslContext(SSL_CTX* SslContextPtr) const override;
	virtual bool HasCertificatesAvailable() const override;

	virtual void ClearAllPinnedPublicKeys() override;
	virtual void SetPinnedPublicKeys(const FString& Domain, const FString& PinnedKeyDigests) override;
	virtual bool VerifySslCertificates(X509_STORE_CTX* Context, const FString& Domain) const override;

	virtual void BuildRootCertificateArray();
	virtual void EmptyRootCertificateArray();

protected:
	void AddPEMFileToRootCertificateArray(const FString& Path);
	void AddCertificateToRootCertificateArray(X509* Certificate);

	static constexpr int PUBLIC_KEY_DIGEST_SIZE = 32;
	TArray<X509*> RootCertificateArray;
	TArray<TPair<FString, TArray<TArray<uint8, TFixedAllocator<PUBLIC_KEY_DIGEST_SIZE>>>>> PinnedPublicKeys;
};

#endif
