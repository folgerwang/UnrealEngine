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
	virtual void AddCertificatesToSslContext(SSL_CTX* SslContextPtr) override;

	void BuildRootCertificateArray();
	void EmptyRootCertificateArray();

protected:
	TArray<X509*> RootCertificateArray;
};

#endif
