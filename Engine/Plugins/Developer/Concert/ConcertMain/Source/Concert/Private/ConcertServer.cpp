// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertServer.h"

#include "ConcertLogger.h"
#include "ConcertSettings.h"
#include "ConcertServerSession.h"
#include "ConcertLogGlobal.h"

#include "Backends/JsonStructDeserializerBackend.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "StructDeserializer.h"
#include "StructSerializer.h"

#define LOCTEXT_NAMESPACE "ConcertServer"

namespace ConcertServerUtils
{
	/** Get the working directory. This is were the active sessions store their files */
	const FString& GetWorkingDir()
	{
		static const FString WorkingDir = FPaths::ProjectIntermediateDir() / TEXT("Concert");
		return WorkingDir;
	}

	/** Return the working directory for a specific session */
	FString GetSessionWorkingDir(const FString& InSessionName)
	{
		return GetWorkingDir() / InSessionName;
	}

	/** Get the directory where the sessions are saved */
	const FString& GetSavedDir()
	{
		static const FString SavedDir = FPaths::ProjectSavedDir() / TEXT("Concert");
		return SavedDir;
	}
	
	/** Get the saved session directory for a specific save */
	FString GetSavedSessionDir(const FString& InSaveName)
	{
		return GetSavedDir() / InSaveName;
	}

	/** Delete a directory */
	bool DeleteDirectory(const FString& InDirectoryToDelete)
	{
		// HACK: avoid the issues related to fact that an operating system might take some time to delete a huge folder
		FString TempDirToDelete = FPaths::ProjectIntermediateDir() / TEXT("__Concert");
		if (IFileManager::Get().Move(*TempDirToDelete, *InDirectoryToDelete, true, true, true, true))
		{
			return IFileManager::Get().DeleteDirectory(*TempDirToDelete, false, true);
		}
		return false;
	}

	const FString SessionInfoFileExtension = TEXT("uinfo");

	/** Get the path to the session info file for a working session */
	FString GetSessionInfoFilePath(const FString& SessionName)
	{
		return GetSessionWorkingDir(SessionName) / FString::Printf(TEXT("%s.%s"), *SessionName, *SessionInfoFileExtension);
	}

	/** Get the name of all the saved sessions available */
	TArray<FString> GetSavedSessionNames()
	{
		TArray<FString> SaveNames;

		IFileManager::Get().FindFiles(SaveNames, *(GetSavedDir() / TEXT("*")), false, true);

		return SaveNames;
	}

	/** Delete a saved session */
	bool DeleteSaveSession(const FString& InSaveName)
	{
		return DeleteDirectory(GetSavedSessionDir(InSaveName));
	}

	/** Delete the folder and files of a working session */
	bool DeleteWorkingSession(const FString& InSessionName)
	{
		return DeleteDirectory(GetSessionWorkingDir(InSessionName));
	}

	/** Delete all the saved session */
	void DeleteAllSavedSessions()
	{
		DeleteDirectory(GetSavedDir());
	}

	/** Delete the folder and files of all the working sessions */
	void DeleteAllWorkingSessions()
	{
		DeleteDirectory(GetWorkingDir());
	}

	/** Take a saved session and make copy of it in the working directory */
	bool RestoreSavedSession(const FString& SaveName, const FString& SessionName)
	{
		DeleteWorkingSession(SessionName);

		const FString WorkingSessionPath = GetSessionWorkingDir(SessionName);
		const FString SavePath = GetSavedSessionDir(SaveName);

		bool bSuccess = false;

		if (!IFileManager::Get().DirectoryExists(*GetWorkingDir()))
		{
			IFileManager::Get().MakeDirectory(*GetWorkingDir());
		}

		bSuccess = IPlatformFile::GetPlatformPhysical().CopyDirectoryTree(*WorkingSessionPath, *SavePath, true);

		if (bSuccess)
		{
			TArray<FString> SessionInfoFiles;
			IFileManager::Get().FindFiles(SessionInfoFiles, *WorkingSessionPath, *FString::Printf(TEXT("*.%s"), *SessionInfoFileExtension));

			for (const FString& SessionInfoFile : SessionInfoFiles)
			{
				// Rename the session info file to the new session name
				IFileManager::Get().Move(*GetSessionInfoFilePath(SessionName), *(WorkingSessionPath / SessionInfoFile), true, true, true, true);
			}
		}

		return bSuccess;
	}

	/**
	 * Save a working session by moving it's data to a saved session
	 */
	bool PersistWorkingSession(const FString& InSessionName, const FString& InSaveName)
	{
		const FString WorkingSessionPath = GetSessionWorkingDir(InSessionName);

		DeleteSaveSession(InSaveName);

		FString SavedSessionPath = GetSavedSessionDir(InSaveName);

		bool bSaveFailed = !IFileManager::Get().Move(*SavedSessionPath, *WorkingSessionPath);	
	
		DeleteWorkingSession(InSessionName);

		if (bSaveFailed)
		{
			DeleteSaveSession(InSaveName);
			return false;
		}

		return true;
	}

	/** Write the session info of a working session on a file */
	bool WriteSessionInfoToWorking(const FConcertSessionInfo& InSessionInfo)
	{
		// Write the session info on the disk so that session can be restore if the server crash
		const FString FilePath = GetSessionInfoFilePath(InSessionInfo.SessionName);

		// Delete the old file
		IFileManager::Get().Delete(*FilePath, false, true, true);

		if (TUniquePtr<FArchive> FileWriter = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*FilePath)))
		{
			FJsonStructSerializerBackend Backend(*FileWriter, EStructSerializerBackendFlags::Default);

			FStructSerializer::Serialize<FConcertSessionInfo>(InSessionInfo, Backend);

			FileWriter->Close();
			return !FileWriter->IsError();
		}
	
		return false;
	}

	/** Read the session info file of a working session */
	bool ReadSessionInfoFromWorking(const FString& InSessionInfoFilePath, FConcertSessionInfo& OutSessionInfo)
	{
		if (TUniquePtr<FArchive> FileReader = TUniquePtr<FArchive>(IFileManager::Get().CreateFileReader(*InSessionInfoFilePath)))
		{
			FJsonStructDeserializerBackend Backend(*FileReader);

			FStructDeserializer::Deserialize<FConcertSessionInfo>(OutSessionInfo, Backend);

			FileReader->Close();
			return !FileReader->IsError();
		}

		return false;
	}

	/** Read the session info file from all the working sessions */
	TArray<FConcertSessionInfo> GetAllSessionInfoFromWorking()
	{
		TArray<FString> SessionInfoFilePaths;
		IFileManager::Get().FindFilesRecursive(SessionInfoFilePaths, *GetWorkingDir(), *FString::Printf(TEXT("*.%s"), *SessionInfoFileExtension), true, false, false);

		TArray<FConcertSessionInfo> SessionInfos;
		for (const FString& SessionInfoFilePath : SessionInfoFilePaths)
		{
			FConcertSessionInfo SessionInfo;
			if (ReadSessionInfoFromWorking(SessionInfoFilePath, SessionInfo))
			{
				SessionInfos.Emplace(MoveTemp(SessionInfo));
			}
		}
		return SessionInfos;
	}
}

FConcertServer::FConcertServer()
{
}

FConcertServer::~FConcertServer()
{
	// if ServerAdminEndpoint is valid, then Shutdown wasn't called
	check(!ServerAdminEndpoint.IsValid());
}

void FConcertServer::SetEndpointProvider(const TSharedPtr<IConcertEndpointProvider>& Provider)
{
	EndpointProvider = Provider;
}

void FConcertServer::Configure(const UConcertServerConfig* InSettings)
{
	ServerInfo.Initialize();
	check(InSettings != nullptr);
	Settings = TStrongObjectPtr<UConcertServerConfig>(const_cast<UConcertServerConfig*>(InSettings));

	if (InSettings->ServerSettings.bIgnoreSessionSettingsRestriction)
	{
		ServerInfo.ServerFlags |= EConcertSeverFlags::IgnoreSessionRequirement;
	}
}

bool FConcertServer::IsConfigured() const
{
	// if the instance id hasn't been set yet, then Configure wasn't called.
	return ServerInfo.InstanceInfo.InstanceId.IsValid();
}

bool FConcertServer::IsStarted() const
{
	return ServerAdminEndpoint.IsValid();
}

void FConcertServer::Startup()
{
	check(IsConfigured());
	if (!ServerAdminEndpoint.IsValid() && EndpointProvider.IsValid())
	{
		// Create the server administration endpoint
		ServerAdminEndpoint = EndpointProvider->CreateLocalEndpoint(TEXT("Admin"), Settings->EndpointSettings, &FConcertLogger::CreateLogger);
		ServerInfo.AdminEndpointId = ServerAdminEndpoint->GetEndpointContext().EndpointId;

		// Make it discoverable
		ServerAdminEndpoint->SubscribeEventHandler<FConcertAdmin_DiscoverServersEvent>(this, &FConcertServer::HandleDiscoverServersEvent);
		
		// Add Session connection handling
		ServerAdminEndpoint->RegisterRequestHandler<FConcertAdmin_CreateSessionRequest, FConcertAdmin_SessionInfoResponse>(this, &FConcertServer::HandleCreateSessionRequest);
		ServerAdminEndpoint->RegisterRequestHandler<FConcertAdmin_FindSessionRequest, FConcertAdmin_SessionInfoResponse>(this, &FConcertServer::HandleFindSessionRequest);
		ServerAdminEndpoint->RegisterRequestHandler<FConcertAdmin_DeleteSessionRequest, FConcertResponseData>(this, &FConcertServer::HandleDeleteSessionRequest);
		
		ServerAdminEndpoint->RegisterRequestHandler<FConcertAdmin_GetSessionsRequest, FConcertAdmin_GetSessionsResponse>(this, &FConcertServer::HandleGetSessionsRequest);
		ServerAdminEndpoint->RegisterRequestHandler<FConcertAdmin_GetSessionClientsRequest, FConcertAdmin_GetSessionClientsResponse>(this, &FConcertServer::HandleGetSessionClientsRequest);
		ServerAdminEndpoint->RegisterRequestHandler<FConcertAdmin_GetSavedSessionNamesRequest, FConcertAdmin_GetSavedSessionNamesResponse>(this, &FConcertServer::HandleGetSavedSessionNamesRequest);


		RestoreSessions();
	}
}

void FConcertServer::Shutdown()
{
	// Server Query
	if (ServerAdminEndpoint.IsValid())
	{
		// Discovery
		ServerAdminEndpoint->UnsubscribeEventHandler<FConcertAdmin_DiscoverServersEvent>();

		// Session connection
		ServerAdminEndpoint->UnregisterRequestHandler<FConcertAdmin_CreateSessionRequest>();
		ServerAdminEndpoint->UnregisterRequestHandler<FConcertAdmin_FindSessionRequest>();
		ServerAdminEndpoint->UnregisterRequestHandler<FConcertAdmin_DeleteSessionRequest>();

		ServerAdminEndpoint->UnregisterRequestHandler<FConcertAdmin_GetSessionsRequest>();
		ServerAdminEndpoint->UnregisterRequestHandler<FConcertAdmin_GetSessionClientsRequest>();
		ServerAdminEndpoint->UnregisterRequestHandler<FConcertAdmin_GetSavedSessionNamesRequest>();

		ServerAdminEndpoint.Reset();
	}

	// Destroy the actives sessions
	TArray<FName> SessionNames;
	Sessions.GetKeys(SessionNames);
	for (const FName& SessionName : SessionNames)
	{
		DestroySession(SessionName);
	}

	Sessions.Empty();
}

FOnConcertServerSessionStartupOrShutdown& FConcertServer::OnSessionStartup()
{
	return OnSessionStartupDelegate;
}

FOnConcertServerSessionStartupOrShutdown& FConcertServer::OnSessionShutdown()
{
	return OnSessionShutdownDelegate;
}

FConcertSessionInfo FConcertServer::CreateSessionInfo() const
{
	FConcertSessionInfo SessionInfo;
	SessionInfo.ServerInstanceId = ServerInfo.InstanceInfo.InstanceId;
	SessionInfo.OwnerInstanceId = ServerInfo.InstanceInfo.InstanceId;
	SessionInfo.OwnerUserName = FApp::GetSessionOwner();
	SessionInfo.OwnerDeviceName = FPlatformProcess::ComputerName();
	return SessionInfo;
}

TSharedPtr<IConcertServerSession> FConcertServer::CreateSession(const FConcertSessionInfo& SessionInfo)
{
	const FName SessionName = *SessionInfo.SessionName;
	if (SessionName.IsNone() || Sessions.Contains(SessionName))
	{
		return nullptr;
	}

	// load the saved session data if specified
	if (!SessionInfo.Settings.SessionToRestore.IsEmpty())
	{
		if (ConcertServerUtils::RestoreSavedSession(SessionInfo.Settings.SessionToRestore, SessionInfo.SessionName))
		{
			UE_LOG(LogConcert, Display, TEXT("Saved Session '%s' was restored for session '%s'"), *SessionInfo.Settings.SessionToRestore, *SessionInfo.SessionName);
		}
		else
		{
			ConcertServerUtils::DeleteWorkingSession(SessionInfo.SessionName);
			UE_LOG(LogConcert, Warning, TEXT("Saved Session '%s' wasn't found for session '%s'. Creating a new empty session."), *SessionInfo.Settings.SessionToRestore, *SessionInfo.SessionName);
		}
	}

	return InternalCreateSession(SessionInfo);
}

void FConcertServer::RestoreSessions()
{
	if (Settings->bCleanWorkingDir)
	{
		ConcertServerUtils::DeleteAllWorkingSessions();
	}
	else
	{
		for (FConcertSessionInfo& SessionInfo : ConcertServerUtils::GetAllSessionInfoFromWorking())
		{
			// Update the session info with new server info
			SessionInfo.ServerInstanceId = ServerInfo.InstanceInfo.InstanceId;
			const FName SessionName = *SessionInfo.SessionName;
			if (!SessionName.IsNone() && !Sessions.Contains(SessionName) && InternalCreateSession(SessionInfo))
			{
				UE_LOG(LogConcert, Display, TEXT("Session '%s' was restored."), *SessionInfo.SessionName);
			}
		}
	}
}

bool FConcertServer::DestroySession(const FName& SessionName)
{
	TSharedPtr<IConcertServerSession> Session = Sessions.FindRef(SessionName);
	if (Session.IsValid())
	{
		OnSessionShutdownDelegate.Broadcast(Session.ToSharedRef());

		const FString& SaveSessionAs = Session->GetSessionInfo().Settings.SaveSessionAs;
		const FString SessionNameAsString = SessionName.ToString(); 
		if (SaveSessionAs.IsEmpty())
		{
			// Delete the session data if we don't save it's data
			ConcertServerUtils::DeleteWorkingSession(SessionName.ToString());
		}
		else
		{
			if (ConcertServerUtils::PersistWorkingSession(SessionNameAsString, SaveSessionAs))
			{
				UE_LOG(LogConcert, Display, TEXT("Session '%s' was saved to '%s'"), *SessionNameAsString, *SaveSessionAs);
			}
			else
			{
				UE_LOG(LogConcert, Error, TEXT("Session '%s' couldn't be saved to '%s'. Save and working files might be corrupt!. All files releted to this session were deleted."), *SessionNameAsString, *SaveSessionAs);
			}
		}

		Session->Shutdown();
		Sessions.Remove(SessionName);

		return true;
	}
	return false;
}

TArray<FConcertSessionClientInfo> FConcertServer::GetSessionClients(const FName& SessionName) const
{
	TSharedPtr<IConcertServerSession> ServerSession = GetSession(SessionName);
	if (ServerSession.IsValid())
	{
		return ServerSession->GetSessionClients();
	}
	return TArray<FConcertSessionClientInfo>();
}

TArray<FConcertSessionInfo> FConcertServer::GetSessionsInfo() const
{
	TArray<FConcertSessionInfo> SessionsInfo;
	SessionsInfo.Reserve(Sessions.Num());
	for (auto& SessionPair : Sessions)
	{
		SessionsInfo.Add(SessionPair.Value->GetSessionInfo());
	}
	return SessionsInfo;
}

TArray<TSharedPtr<IConcertServerSession>> FConcertServer::GetSessions() const
{
	TArray<TSharedPtr<IConcertServerSession>> SessionsArray;
	SessionsArray.Reserve(Sessions.Num());
	for (auto& SessionPair : Sessions)
	{
		SessionsArray.Add(SessionPair.Value);
	}
	return SessionsArray;
}

TSharedPtr<IConcertServerSession> FConcertServer::GetSession(const FName& SessionName) const
{
	return Sessions.FindRef(SessionName);
}

void FConcertServer::HandleDiscoverServersEvent(const FConcertMessageContext& Context)
{
	if (ServerAdminEndpoint.IsValid())
	{
		FConcertAdmin_ServerDiscoveredEvent DiscoveryInfo;
		DiscoveryInfo.ServerName = ServerInfo.ServerName;
		DiscoveryInfo.InstanceInfo = ServerInfo.InstanceInfo;
		DiscoveryInfo.ServerFlags = ServerInfo.ServerFlags;
		ServerAdminEndpoint->SendEvent(DiscoveryInfo, Context.SenderConcertEndpointId);
	}
}

TFuture<FConcertAdmin_SessionInfoResponse> FConcertServer::HandleCreateSessionRequest(const FConcertMessageContext& Context)
{
	const FConcertAdmin_CreateSessionRequest* Message = Context.GetMessage<FConcertAdmin_CreateSessionRequest>();

	// Create a new server session 
	TSharedPtr<IConcertServerSession> NewServerSession = CreateServerSession(*Message);

	// We have a valid session if it succeeded
	FConcertAdmin_SessionInfoResponse ResponseData;
	if (NewServerSession.IsValid())
	{
		ResponseData.SessionInfo = NewServerSession->GetSessionInfo();
		ResponseData.ResponseCode = EConcertResponseCode::Success;
	}
	else
	{
		ResponseData.ResponseCode = EConcertResponseCode::Failed;
		if (Message->SessionName.IsEmpty())
		{
			ResponseData.Reason = LOCTEXT("Error_EmptySessionName", "Empty session name");
		}
		else
		{
			ResponseData.Reason = LOCTEXT("Error_SessionAlreadyExists", "Session already exists");
		}
		UE_LOG(LogConcert, Display, TEXT("Session creation failed. (User: %s, Reason: %s)"), *Message->OwnerClientInfo.UserName, *ResponseData.Reason.ToString());
	}

	return FConcertAdmin_SessionInfoResponse::AsFuture(MoveTemp(ResponseData));
}

TFuture<FConcertAdmin_SessionInfoResponse> FConcertServer::HandleFindSessionRequest(const FConcertMessageContext& Context)
{
	const FConcertAdmin_FindSessionRequest* Message = Context.GetMessage<FConcertAdmin_FindSessionRequest>();

	FConcertAdmin_SessionInfoResponse ResponseData;

	// Find the session requested
	TSharedPtr<IConcertServerSession> ServerSession = GetSession(*Message->SessionName);
	if (CheckSessionRequirements(ServerSession, Message->SessionSettings, &ResponseData.Reason))
	{
		ResponseData.ResponseCode = EConcertResponseCode::Success;
		ResponseData.SessionInfo = ServerSession->GetSessionInfo();
		UE_LOG(LogConcert, Display, TEXT("Allowing user %s to join session %s (Owner: %s)"), *Message->OwnerClientInfo.UserName, *Message->SessionName, *ServerSession->GetSessionInfo().OwnerUserName);
	}
	else
	{
		ResponseData.ResponseCode = EConcertResponseCode::Failed;
		UE_LOG(LogConcert, Display, TEXT("Refusing user %s to join session %s (Owner: %s, Reason: %s)"), *Message->OwnerClientInfo.UserName, *Message->SessionName, *ServerSession->GetSessionInfo().OwnerUserName, *ResponseData.Reason.ToString());
	}

	return FConcertAdmin_SessionInfoResponse::AsFuture(MoveTemp(ResponseData));
}

TFuture<FConcertResponseData> FConcertServer::HandleDeleteSessionRequest(const FConcertMessageContext & Context)
{
	const FConcertAdmin_DeleteSessionRequest* Message = Context.GetMessage<FConcertAdmin_DeleteSessionRequest>();

	FConcertResponseData ResponseData;

	// Find the session requested and check if it should be deleted
	TSharedPtr<IConcertServerSession> ServerSession = GetSession(*Message->SessionName);
	if (ServerSession.IsValid())
	{
		if (IsRequestFromSessionOwner(ServerSession, *Message))
		{
			DestroySession(*Message->SessionName);
			ResponseData.ResponseCode = EConcertResponseCode::Success;
			UE_LOG(LogConcert, Display, TEXT("User %s deleted session %s"), *Message->UserName, *Message->SessionName);
		}
		else
		{
			ResponseData.ResponseCode = EConcertResponseCode::Failed;
			ResponseData.Reason = LOCTEXT("Error_InvalidPerms_NotOwner", "Not the session owner.");
			UE_LOG(LogConcert, Display, TEXT("User %s failed to delete session %s (Owner: %s, Reason: %s)"), *Message->UserName, *Message->SessionName, *ServerSession->GetSessionInfo().OwnerUserName, *ResponseData.Reason.ToString());
		}
	}
	else
	{
		ResponseData.ResponseCode = EConcertResponseCode::Failed;
		ResponseData.Reason = LOCTEXT("Error_SessionDoesNotExist", "Session does not exist.");
		UE_LOG(LogConcert, Display, TEXT("User %s failed to delete session %s (Reason: %s)"), *Message->UserName, *Message->SessionName, *ResponseData.Reason.ToString());
	}

	return FConcertResponseData::AsFuture(MoveTemp(ResponseData));
}

TFuture<FConcertAdmin_GetSessionsResponse> FConcertServer::HandleGetSessionsRequest(const FConcertMessageContext& Context)
{
	const FConcertAdmin_GetSessionsRequest* Message = Context.GetMessage<FConcertAdmin_GetSessionsRequest>();

	FConcertAdmin_GetSessionsResponse ResponseData;
	ResponseData.Sessions = GetSessionsInfo();
	
	return FConcertAdmin_GetSessionsResponse::AsFuture(MoveTemp(ResponseData));
}

TFuture<FConcertAdmin_GetSessionClientsResponse> FConcertServer::HandleGetSessionClientsRequest(const FConcertMessageContext& Context)
{
	const FConcertAdmin_GetSessionClientsRequest* Message = Context.GetMessage<FConcertAdmin_GetSessionClientsRequest>();

	FConcertAdmin_GetSessionClientsResponse ResponseData;
	ResponseData.SessionClients = GetSessionClients(*Message->SessionName);
	
	return FConcertAdmin_GetSessionClientsResponse::AsFuture(MoveTemp(ResponseData));
}

TFuture<FConcertAdmin_GetSavedSessionNamesResponse> FConcertServer::HandleGetSavedSessionNamesRequest(const FConcertMessageContext& Context)
{
	FConcertAdmin_GetSavedSessionNamesResponse ResponseData;

	ResponseData.ResponseCode = EConcertResponseCode::Success;
	ResponseData.SavedSessionNames = ConcertServerUtils::GetSavedSessionNames();

	return FConcertAdmin_GetSavedSessionNamesResponse::AsFuture(MoveTemp(ResponseData));
}

TSharedPtr<IConcertServerSession> FConcertServer::CreateServerSession(const FConcertAdmin_CreateSessionRequest& CreateSessionRequest)
{
	FConcertSessionInfo SessionInfo = CreateSessionInfo();
	SessionInfo.OwnerInstanceId = CreateSessionRequest.OwnerClientInfo.InstanceInfo.InstanceId;
	SessionInfo.OwnerUserName = CreateSessionRequest.OwnerClientInfo.UserName;
	SessionInfo.OwnerDeviceName = CreateSessionRequest.OwnerClientInfo.DeviceName;
	SessionInfo.SessionName = CreateSessionRequest.SessionName;
	SessionInfo.Settings = CreateSessionRequest.SessionSettings;

	return CreateSession(SessionInfo);
}

bool FConcertServer::CheckSessionRequirements(const TSharedPtr<IConcertServerSession>& ServerSession, const FConcertSessionSettings& SessionSettings, FText* OutFailureReason)
{
	if (!ServerSession.IsValid())
	{
		if (OutFailureReason)
		{
			*OutFailureReason = LOCTEXT("Error_UnknownSession", "Unknown session");
		}
		return false;
	}

	if (Settings->ServerSettings.bIgnoreSessionSettingsRestriction || ServerSession->GetSessionInfo().Settings.ValidateRequirements(SessionSettings, OutFailureReason))
	{
		return true;
	}

	return false;
}

bool FConcertServer::IsRequestFromSessionOwner(const TSharedPtr<IConcertServerSession>& SessionToDelete, const FConcertAdmin_DeleteSessionRequest& DeleteSessionRequest)
{
	if (SessionToDelete.IsValid())
	{
		const FConcertSessionInfo& SessionInfo = SessionToDelete->GetSessionInfo();
		return SessionInfo.OwnerUserName == DeleteSessionRequest.UserName && SessionInfo.OwnerDeviceName == DeleteSessionRequest.DeviceName;
	}
	return false;
}

TSharedPtr<IConcertServerSession> FConcertServer::InternalCreateSession(const FConcertSessionInfo& SessionInfo)
{
	TSharedPtr<FConcertServerSession> Session = MakeShared<FConcertServerSession>(SessionInfo
		, Settings->ServerSettings
		, EndpointProvider->CreateLocalEndpoint(SessionInfo.SessionName, Settings->EndpointSettings, &FConcertLogger::CreateLogger)
		, ConcertServerUtils::GetWorkingDir());
	
	// Write the session info
	ConcertServerUtils::WriteSessionInfoToWorking(Session->GetSessionInfo());

	OnSessionStartupDelegate.Broadcast(Session.ToSharedRef());
	Session->Startup();

	const FName SessionName = *SessionInfo.SessionName;
	Sessions.Add(SessionName, Session);

	return Session;
}

#undef LOCTEXT_NAMESPACE
