// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DataBunch.cpp: Unreal bunch (sub-packet) functions.
=============================================================================*/

#include "Net/DataBunch.h"
#include "Engine/NetConnection.h"
#include "Engine/ControlChannel.h"

const int32 MAX_BUNCH_SIZE = 1024 * 1024; 


/*-----------------------------------------------------------------------------
	FInBunch implementation.
-----------------------------------------------------------------------------*/

FInBunch::FInBunch( UNetConnection* InConnection, uint8* Src, int64 CountBits )
:	FNetBitReader	(InConnection->PackageMap, Src, CountBits)
,	PacketId	( 0 )
,	Next ( NULL )
,	Connection ( InConnection )
,	ChIndex ( 0 )
,	ChType ( 0 )
,	ChSequence ( 0 )
,	bOpen ( 0 )
,	bClose ( 0 )
,	bDormant ( 0 )
,	bReliable ( 0 )
,	bPartial ( 0 )
,	bPartialInitial ( 0 )
,	bPartialFinal ( 0 )
,	bHasPackageMapExports ( 0 )
,	bHasMustBeMappedGUIDs ( 0 )
,	bIgnoreRPCs ( 0 )
{
	check(Connection);
	// Match the byte swapping settings of the connection
	SetByteSwapping(Connection->bNeedsByteSwapping);

	// Copy network version info
	this->SetEngineNetVer(InConnection->EngineNetworkProtocolVersion);
	this->SetGameNetVer(InConnection->GameNetworkProtocolVersion);

	// Crash protection: the max string size serializable on this archive 
	ArMaxSerializeSize = MAX_STRING_SERIALIZE_SIZE;
}

/** Copy constructor but with optional parameter to not copy buffer */
FInBunch::FInBunch( FInBunch &InBunch, bool CopyBuffer )
{
	// Copy fields
//	FMemory::Memcpy(&PacketId,&InBunch.PacketId,sizeof(FInBunch) - sizeof(FNetBitReader));

	// This is portable
	PacketId =	InBunch.PacketId;
	Next =	InBunch.Next;
	Connection = InBunch.Connection;
	ChIndex = InBunch.ChIndex;
	ChType = InBunch.ChType;
	ChSequence = InBunch.ChSequence;
	bOpen =	InBunch.bOpen;
	bDormant = InBunch.bDormant;
	bIsReplicationPaused = InBunch.bIsReplicationPaused;
	bReliable =	InBunch.bReliable;
	bPartial = InBunch.bPartial;
	bPartialInitial = InBunch.bPartialInitial;
	bPartialFinal =	InBunch.bPartialFinal;
	bHasPackageMapExports = InBunch.bHasPackageMapExports;
	bHasMustBeMappedGUIDs =	InBunch.bHasMustBeMappedGUIDs;
	bIgnoreRPCs = InBunch.bIgnoreRPCs;

	// Copy network version info
	this->SetEngineNetVer(InBunch.EngineNetVer());
	this->SetGameNetVer(InBunch.GameNetVer());

	PackageMap = InBunch.PackageMap;
	
	ArMaxSerializeSize = MAX_STRING_SERIALIZE_SIZE;

	if (CopyBuffer)
		FBitReader::operator=(InBunch);

	Pos = 0;
}

/*-----------------------------------------------------------------------------
	FOutBunch implementation.
-----------------------------------------------------------------------------*/

//
// Construct an outgoing bunch for a channel.
// It is ok to either send or discard an FOutbunch after construction.
//
FOutBunch::FOutBunch()
: FNetBitWriter( 0 )
{}
FOutBunch::FOutBunch( UChannel* InChannel, bool bInClose )
:	FNetBitWriter	( InChannel->Connection->PackageMap, InChannel->Connection->GetMaxSingleBunchSizeBits())
,	Next		( NULL )
,	Channel		( InChannel )
,	Time		( 0 )
,	ReceivedAck	(false)
,	ChIndex     ( InChannel->ChIndex )
,	ChType      ( InChannel->ChType )
,	ChSequence	( 0 )
,	PacketId	( 0 )
,	bOpen		( 0 )
,	bClose		( bInClose )
,	bDormant	( 0 )
,	bIsReplicationPaused	( 0 )
,	bReliable	( 0 )
,	bPartial	( 0 )
,	bPartialInitial			( 0 )
,	bPartialFinal			( 0 )
,	bHasPackageMapExports	( 0 )
,	bHasMustBeMappedGUIDs	( 0 )
{
	checkSlow(!Channel->Closing);
	checkSlow(Channel->Connection->Channels[Channel->ChIndex]==Channel);

	// Match the byte swapping settings of the connection
	SetByteSwapping(Channel->Connection->bNeedsByteSwapping);

	// Reserve channel and set bunch info.
	if( Channel->NumOutRec >= RELIABLE_BUFFER-1+bClose )
	{
		SetOverflowed(-1);
		return;
	}
}
FOutBunch::FOutBunch( UPackageMap *InPackageMap, int64 MaxBits )
:	FNetBitWriter	( InPackageMap, MaxBits )
,	Next		( NULL )
,	Channel		( NULL )
,   Time		( 0 )
,	ReceivedAck	(false)
,	ChIndex     ( 0 )
,	ChType      ( 0 )
,	ChSequence	( 0 )
,	PacketId	( 0 )
,	bOpen		( 0 )
,	bClose		( 0 )
,	bDormant	( 0 )
,   bIsReplicationPaused	( 0 )
,	bReliable	( 0 )
,	bPartial	( 0 )
,	bPartialInitial ( 0 )
,	bPartialFinal	( 0 )
,	bHasPackageMapExports	( 0 )
,	bHasMustBeMappedGUIDs	( 0 )
{
}


FControlChannelOutBunch::FControlChannelOutBunch(UChannel* InChannel, bool bClose)
	: FOutBunch(InChannel, bClose)
{
	checkSlow(Cast<UControlChannel>(InChannel) != NULL);
	// control channel bunches contain critical handshaking/synchronization and should always be reliable
	bReliable = true;
}

