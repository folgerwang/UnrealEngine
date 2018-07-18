// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

struct ssl_ctx_st;
typedef struct ssl_ctx_st SSL_CTX;

class ISslCertificateManager
{
public:
	virtual ~ISslCertificateManager() {}
	virtual void AddCertificatesToSslContext(SSL_CTX* SslContextPtr) = 0;
};