// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "ConcertClientLiveTransactionAuthors.h"
#include "IConcertSession.h"
#include "Scratchpad/ConcertScratchpad.h"

/** Flags used for the tests. */
static const int ConcertClientLiveTransactionAuthorsTestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;

namespace ConcertLiveTransactionAuthorsTestUtils
{
// Utility functions used to detect when a non-mocked function is called, so that we can mock it properly when required.
template<typename T> T NotMocked(T Ret) { check(false); return Ret; }
template<typename T> T NotMocked()      { check(false); return T(); }

/** Implements a not-working IConcertClientSession. It must be further overridden to implement just what is required by the tests */
class FConcertClientSessionMock : public IConcertClientSession
{
public:
	FConcertClientSessionMock(FConcertClientInfo InLocalClientInfo) : Name("FConcertClientSessionMock"), LocalClientInfo(MoveTemp(InLocalClientInfo)){ }

	// IConcertSession Begin.
	virtual const FString& GetName() const override                                                                                  { return NotMocked<const FString&>(Name); }
	virtual const FConcertSessionInfo& GetSessionInfo() const override                                                               { return NotMocked<const FConcertSessionInfo&>(SessionInfo); }
	virtual FString GetSessionWorkingDirectory() const override                                                                      { return NotMocked<FString>(); }
	virtual TArray<FGuid> GetSessionClientEndpointIds() const override                                                               { return NotMocked<TArray<FGuid>>(); }
	virtual TArray<FConcertSessionClientInfo> GetSessionClients() const override                                                     { return OtherClientsInfo; }
	virtual bool FindSessionClient(const FGuid&, FConcertSessionClientInfo&) const override                                          { return NotMocked<bool>(); }
	virtual void Startup() override                                                                                                  { return NotMocked<void>(); }
	virtual void Shutdown() override                                                                                                 { return NotMocked<void>(); };
	virtual FConcertScratchpadRef GetScratchpad() const override                                                                     { return NotMocked<FConcertScratchpadRef>(); }
	virtual FConcertScratchpadPtr GetClientScratchpad(const FGuid&) const override                                                   { return NotMocked<FConcertScratchpadRef>(); }
	virtual void InternalRegisterCustomEventHandler(const FName&, const TSharedRef<IConcertSessionCustomEventHandler>&) override     { return NotMocked<void>(); }
	virtual void InternalUnregisterCustomEventHandler(const FName&) override                                                         { return NotMocked<void>(); }
	virtual void InternalSendCustomEvent(const UScriptStruct*, const void*, const TArray<FGuid>&, EConcertMessageFlags) override     { return NotMocked<void>(); }
	virtual void InternalRegisterCustomRequestHandler(const FName&, const TSharedRef<IConcertSessionCustomRequestHandler>&) override { return NotMocked<void>(); }
	virtual void InternalUnregisterCustomRequestHandler(const FName&) override                                                       { return NotMocked<void>(); }
	virtual void InternalSendCustomRequest(const UScriptStruct*, const void*, const FGuid&, const TSharedRef<IConcertSessionCustomResponseHandler>&) override { NotMocked<void>(); }
	// IConcertSession End.

	// IConcertClientSession Begin
	virtual EConcertConnectionStatus GetConnectionStatus() const override            { return NotMocked<EConcertConnectionStatus>(EConcertConnectionStatus::Connected); }
	virtual FGuid GetSessionClientEndpointId() const override                        { return NotMocked<FGuid>(); }
	virtual FGuid GetSessionServerEndpointId() const override                        { return NotMocked<FGuid>(); }
	virtual const FConcertClientInfo& GetLocalClientInfo() const override            { return LocalClientInfo; }
	virtual void Connect() override                                                  { return NotMocked<void>(); }
	virtual void Disconnect() override                                               { return NotMocked<void>(); }
	virtual void Resume() override                                                   { return NotMocked<void>(); }
	virtual void Suspend() override                                                  { return NotMocked<void>(); }
	virtual bool IsSuspended() const override                                        { return NotMocked<bool>(false); }
	virtual FOnConcertClientSessionTick& OnTick() override                           { return NotMocked<FOnConcertClientSessionTick&>(Tick); }
	virtual FOnConcertClientSessionConnectionChanged& OnConnectionChanged() override { return NotMocked<FOnConcertClientSessionConnectionChanged&>(ConnectionChanged); }
	virtual FOnConcertClientSessionClientChanged& OnSessionClientChanged() override  { return NotMocked<FOnConcertClientSessionClientChanged&>(ClientChanged); }
	// IConcertClientSession End

	void AddClient(FConcertSessionClientInfo Client) { OtherClientsInfo.Add(MoveTemp(Client)); }

protected:
	// Those below are unused mocked data member, but required to get the code compiling.
	FString Name;
	FConcertSessionInfo SessionInfo;
	FOnConcertClientSessionTick Tick;
	FOnConcertClientSessionConnectionChanged ConnectionChanged;
	FOnConcertClientSessionClientChanged ClientChanged;
	FConcertClientInfo LocalClientInfo;
	TArray<FConcertSessionClientInfo> OtherClientsInfo;
};

/** Ensures the live transaction authors works correctly when there is no other clients connected. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcertLiveTransactionAuthorsSingleClient, "Concert.LiveTransactionAuthors.SingleClient", ConcertClientLiveTransactionAuthorsTestFlags)

bool FConcertLiveTransactionAuthorsSingleClient::RunTest(const FString& Parameters)
{
	FConcertClientInfo ThisClient;
	ThisClient.Initialize();

	TSharedRef<IConcertClientSession> Session = MakeShared<FConcertClientSessionMock>(ThisClient);
	FConcertClientLiveTransactionAuthors LiveTransactionAuthors(Session);

	// Test without any transaction.
	FName PackageName(TEXT("MyLevel"));
	uint64 TransactionIndex = 1;
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	// Add a live transaction from this client. Ensure it doesn't affect the authored by others.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, ThisClient, TransactionIndex++);
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	// Add a live transaction on another package.
	LiveTransactionAuthors.AddLiveTransaction(FName(TEXT("OtherPackage")), ThisClient, TransactionIndex++);
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	// Trim all transactions. Ensure it doesn't affect the package authored by others.
	LiveTransactionAuthors.TrimLiveTransactions(PackageName, TransactionIndex);

	int OtherClientCount = 0;
	TArray<FConcertClientInfo> OtherClients;
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount, &OtherClients, 10));
	TestTrueExpr(OtherClientCount == 0);
	TestTrueExpr(OtherClients.Num() == 0);

	return true;
}

/** Ensures the live transaction authors works correctly when there are many clients connected. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcertLiveTransactionAuthorsManyClients, "Concert.LiveTransactionAuthors.ManyClients", ConcertClientLiveTransactionAuthorsTestFlags)

bool FConcertLiveTransactionAuthorsManyClients::RunTest(const FString& Parameters)
{
	// Represents the local client.
	FConcertClientInfo ThisClient;
	ThisClient.Initialize();

	// Represents the other clients.
	FConcertClientInfo OtherClient1;
	OtherClient1.Initialize();
	OtherClient1.InstanceInfo.InstanceId.A += 1; // Make the InstanceId unique, Initialize() uses the AppId which is the same across the app.

	FConcertClientInfo OtherClient2;
	OtherClient2.Initialize();
	OtherClient2.InstanceInfo.InstanceId.B += 1; // Make the InstanceId unique, Initialize() uses the AppId which is the same across the app.

	// Ensure each client has a unique instance Id. The value is not important, but they must be different for the tests to work.
	TestTrueExpr(ThisClient.InstanceInfo.InstanceId != OtherClient1.InstanceInfo.InstanceId);
	TestTrueExpr(ThisClient.InstanceInfo.InstanceId != OtherClient2.InstanceInfo.InstanceId);
	TestTrueExpr(OtherClient1.InstanceInfo.InstanceId != OtherClient2.InstanceInfo.InstanceId);

	// Create the session.
	TSharedRef<FConcertClientSessionMock> Session = MakeShared<FConcertClientSessionMock>(ThisClient);

	// Add other clients to the session. Note that we don't care about the end point GUID, they are not used by FConcertClientLiveTransactionAuthors implementation.
	Session->AddClient(FConcertSessionClientInfo{FGuid(), OtherClient1});
	Session->AddClient(FConcertSessionClientInfo{FGuid(), OtherClient2});

	// Create the live transaction author tracker.
	FConcertClientLiveTransactionAuthors LiveTransactionAuthors(Session);

	// An hypothetical package.
	FName PackageName(TEXT("MyLevel"));
	FName OtherPackageName(TEXT("OtherLevel"));
	
	// Test without any transaction.
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	int OtherClientCount = 0;
	TArray<FConcertClientInfo> OtherClients;
	uint64 TransactionIndex = 1;

	// Add a live transaction from client 1. Ensure it is tracked.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient1, TransactionIndex++);
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount, &OtherClients, 10));
	TestTrueExpr(OtherClientCount == 1);
	TestTrueExpr(OtherClients[0].InstanceInfo.InstanceId == OtherClient1.InstanceInfo.InstanceId);
	OtherClients.Empty();

	// Add a live transaction from client 2. Ensure it is tracked.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient2, TransactionIndex++);
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount, &OtherClients, 10));
	TestTrueExpr(OtherClientCount == 2);
	TestTrueExpr(OtherClients[0].InstanceInfo.InstanceId == OtherClient1.InstanceInfo.InstanceId || OtherClients[0].InstanceInfo.InstanceId == OtherClient1.InstanceInfo.InstanceId);
	TestTrueExpr(OtherClients[1].InstanceInfo.InstanceId == OtherClient2.InstanceInfo.InstanceId || OtherClients[1].InstanceInfo.InstanceId == OtherClient2.InstanceInfo.InstanceId);
	TestTrueExpr(OtherClients[0].InstanceInfo.InstanceId != OtherClients[1].InstanceInfo.InstanceId);
	OtherClients.Empty();

	// Ensure the API only returns 1 client out of 2 if only 1 is requested.
	LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount, &OtherClients, 1);
	TestTrueExpr(OtherClients.Num() == 1);
	OtherClients.Empty();

	// Trim all transactions.
	LiveTransactionAuthors.TrimLiveTransactions(PackageName, TransactionIndex);
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	// Add a live transaction on another package just to make noise.
	LiveTransactionAuthors.AddLiveTransaction(OtherPackageName, ThisClient, TransactionIndex++);
	LiveTransactionAuthors.AddLiveTransaction(OtherPackageName, OtherClient2, TransactionIndex++);

	// Add more transactions from client 1.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient1, TransactionIndex++);
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient1, TransactionIndex++);
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient1, TransactionIndex++);
	uint64 TrimClient1TransactionIndex = TransactionIndex;
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount));
	TestTrueExpr(OtherClientCount == 1);

	// Add more transactions from client 2.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient2, TransactionIndex++);
	LiveTransactionAuthors.AddLiveTransaction(PackageName, OtherClient2, TransactionIndex++);
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount));
	TestTrueExpr(OtherClientCount == 2);

	// Trim the transaction from client 1 only.
	LiveTransactionAuthors.TrimLiveTransactions(PackageName, TrimClient1TransactionIndex);
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount, &OtherClients, 1));
	TestTrueExpr(OtherClientCount == 1);
	TestTrueExpr(OtherClients[0].InstanceInfo.InstanceId == OtherClient2.InstanceInfo.InstanceId);

	// Trim all remaining transactions.
	LiveTransactionAuthors.TrimLiveTransactions(PackageName, TransactionIndex);
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName, &OtherClientCount));
	TestTrueExpr(OtherClientCount == 0);

	// Ensure trim only trimmed for "PackageName", not "OtherPackageName". Client2 has a transaction on OtherPackageName.
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(OtherPackageName, &OtherClientCount));
	TestTrueExpr(OtherClientCount == 1);

	return true;
}

/** Ensures the live transaction authors works correctly when they are some transaction owned by a disconnected client. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcertLiveTransactionAuthorsDisconnectedClient, "Concert.LiveTransactionAuthors.DisconnectedClient", ConcertClientLiveTransactionAuthorsTestFlags)

bool FConcertLiveTransactionAuthorsDisconnectedClient::RunTest(const FString& Parameters)
{
	// Represents the current local client. Let say it represents a person named 'Joe Smith' currently connected.
	FConcertClientInfo CurrentInstanceOfJoeSmith;
	CurrentInstanceOfJoeSmith.Initialize();

	// Represents a previous editor instance used by 'Joe Smith'. In that previous instance, Joe had another InstanceId, but he
	// closed (or crashed) the editor without saving. So the previous instance of Joe has live transaction pending. He now has
	// launched a new editor and rejoined the session from the same computer. Below, we simulate its previous instance id.
	FConcertClientInfo PreviousInstanceOfJoeSmith;
	PreviousInstanceOfJoeSmith.Initialize();
	PreviousInstanceOfJoeSmith.InstanceInfo.InstanceId.A += 1; // Make the InstanceId unique, Initialize() uses the AppId which is the same across the app.

	// Represents a disconnected user name Jane Doe who left the session without saving her modifications.
	FConcertClientInfo DisconnectedInstanceOfJaneDoe;
	DisconnectedInstanceOfJaneDoe.Initialize();
	DisconnectedInstanceOfJaneDoe.InstanceInfo.InstanceId.B += 1; // Make the InstanceId unique, Initialize() uses the AppId which is the same across the app.
	DisconnectedInstanceOfJaneDoe.DeviceName = TEXT("ThisIsJaneDoeComputer");
	DisconnectedInstanceOfJaneDoe.UserName = TEXT("jane.doe");
	DisconnectedInstanceOfJaneDoe.DisplayName = TEXT("Jane Doe");

	// Create the session and transaction author tracker. Don't add the disconnected client to the session.
	TSharedRef<FConcertClientSessionMock> Session = MakeShared<FConcertClientSessionMock>(CurrentInstanceOfJoeSmith);
	FConcertClientLiveTransactionAuthors LiveTransactionAuthors(Session);

	// An hypothetical package.
	FName PackageName(TEXT("MyLevel"));
	uint64 TransactionIndex = 1;

	// Add live transactions from the disconnected client, just like when a client connects, it gets all live transactions from the transaction ledger,
	// resolve their author using the activity ledger, then populate the live transaction author tracker. During that process, some live transactions may be
	// resolved to author that are now disconnected. The code below simulate that.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, PreviousInstanceOfJoeSmith, TransactionIndex++);
	LiveTransactionAuthors.AddLiveTransaction(PackageName, PreviousInstanceOfJoeSmith, TransactionIndex++);

	// We expect the LiveTransactionAuthors to map the previous instance of Joe Smith to the actual instance of Joe Smith because the instance are not
	// run simultaneously, but rather run one after the other (When the same person runs 2 editors in parallel, the person is recognized as 2 different clients).
	// In that case, the live transaction performed by 'PreviousInstanceOfJoeSmith' should be assigned to 'CurrentInstanceOfJoeSmith' instance.
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	// Jane Doe is not connected anymore and she doesn't match Joe Smith identity signature (user name, display name, device name, etc). She should be recognize as a different user.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, DisconnectedInstanceOfJaneDoe, TransactionIndex++);
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	return true;
}

/** Ensures the live transaction authors works correctly when the same person is editing a package from two editors, on the same machine, concurrently. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConcertLiveTransactionAuthorsClientUsingTwoEditors, "Concert.LiveTransactionAuthors.SamePersonUsingTwoEditorsConcurrently", ConcertClientLiveTransactionAuthorsTestFlags)

bool FConcertLiveTransactionAuthorsClientUsingTwoEditors::RunTest(const FString& Parameters)
{
	// Represents the current local client. Let say it represents a person named 'Joe Smith' currently connected.
	FConcertClientInfo ThisJoeSmithInstance;
	ThisJoeSmithInstance.Initialize();

	// Represents also the person 'Joe Smith' but from another editor instance, on the same machine, running concurrently with 'ThisJoeSmithInstance'.
	// Both editors used by Joe are connected to the same session.
	FConcertClientInfo AnotherInstanceOfJoeSmith;
	AnotherInstanceOfJoeSmith.Initialize();
	AnotherInstanceOfJoeSmith.InstanceInfo.InstanceId.A += 1; // Make the InstanceId unique, Initialize() uses the AppId which is the same across the app.

	// Create the session and transaction author tracker.
	TSharedRef<FConcertClientSessionMock> Session = MakeShared<FConcertClientSessionMock>(ThisJoeSmithInstance);
	FConcertClientLiveTransactionAuthors LiveTransactionAuthors(Session);

	// Add the other Joe instance to the session. Note that we don't care about the end point GUID, they are not used by FConcertClientLiveTransactionAuthors implementation.
	Session->AddClient(FConcertSessionClientInfo{FGuid(), AnotherInstanceOfJoeSmith});

	// An hypothetical package.
	FName PackageName(TEXT("MyLevel"));
	uint64 TransactionIndex = 1;

	// Add transaction from the local instance of JoeSmith. He should be recognized as himself.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, ThisJoeSmithInstance, TransactionIndex++);
	TestTrueExpr(!LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	// Add transaction from the other instance of JoeSmith. He should be recognized as a different client.
	LiveTransactionAuthors.AddLiveTransaction(PackageName, AnotherInstanceOfJoeSmith, TransactionIndex++);
	TestTrueExpr(LiveTransactionAuthors.IsPackageAuthoredByOtherClients(PackageName));

	return true;
}

}

