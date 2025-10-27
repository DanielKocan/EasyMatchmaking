#include "Session/EOSSessionManager.h"
#include "EOSLobbyManager.h"
#include "EOSManager.h"
#include "EasyMatchmakingLog.h"
#include "IEOSSDKManager.h"

#include "GameFramework/Character.h"  
#include "GameFramework/GameModeBase.h"

void UEOSSessionManager::Init(void* InPlatformHandle, void* InSessionHandle, void* InLocalUserId, UEOSManager* InEOSManager)
{
    PlatformHandle = static_cast<EOS_HPlatform>(InPlatformHandle);
    SessionHandle = static_cast<EOS_HSessions>(InSessionHandle);
    LocalUserId = static_cast<EOS_ProductUserId>(InLocalUserId);

    EOSManager = InEOSManager;

    EM_LOG_INFO(TEXT("Initialized EOSSessionManager"));
}

void UEOSSessionManager::BeginDestroy()
{
    for (auto& Pair : CachedSessionDetails)
    {
        if (Pair.Value)
        {
            EOS_SessionDetails_Release(Pair.Value);
        }
    }
    CachedSessionDetails.Empty();

    // Clean up search handle
    if (CurrentSessionSearchHandle)
    {
        EOS_SessionSearch_Release(CurrentSessionSearchHandle);
        CurrentSessionSearchHandle = nullptr;
    }

    // Auto-cleanup when subsystem is destroyed
    if (!CurrentSessionId.IsEmpty())
    {
        DestroySession();
    }

    Super::BeginDestroy();
}

void UEOSSessionManager::DestroySession()
{
    if (!SessionHandle || CurrentSessionId.IsEmpty())
    {
        EM_LOG_WARNING(TEXT("No active session to destroy"));
        return;
    }

    EOS_Sessions_DestroySessionOptions DestroyOptions = {};
    DestroyOptions.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST;

    FTCHARToUTF8 SessionIdConverter(*CurrentSessionId);
    DestroyOptions.SessionName = SessionIdConverter.Get();

    EM_LOG_INFO(TEXT("Destroying session: %s"), *CurrentSessionId);
    EOS_Sessions_DestroySession(SessionHandle, &DestroyOptions, this, OnDestroySessionComplete);
}

void UEOSSessionManager::JoinSessionById(const FString& SessionId)
{
    if (!SessionHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot join session - invalid handles"));
        return;
    }

    if (SessionId.IsEmpty())
    {
        EM_LOG_ERROR(TEXT("Session ID is empty!"));
        return;
    }

    EM_LOG_INFO(TEXT("Searching for session: %s"), *SessionId);

    // Check if we already have this session cached from SearchSessions(), if you dont do it then callback possibli will not be executed :(
    EOS_HSessionDetails* CachedDetails = CachedSessionDetails.Find(SessionId);
    if (CachedDetails && *CachedDetails)
    {
        // Use cached details directly - no need for another search!
        EM_LOG_INFO(TEXT("Using cached session details for join"));

        EOS_Sessions_JoinSessionOptions JoinOptions = {};
        JoinOptions.ApiVersion = EOS_SESSIONS_JOINSESSION_API_LATEST;
        JoinOptions.SessionHandle = *CachedDetails;
        JoinOptions.LocalUserId = LocalUserId;
        JoinOptions.bPresenceEnabled = EOS_FALSE;
        JoinOptions.SessionName = "MyGameSession";

        PendingJoinSessionId = SessionId;

        EOS_Sessions_JoinSession(SessionHandle, &JoinOptions, this, OnJoinSessionComplete);
        return;
    }

    // Store the Session ID we want to join
    PendingJoinSessionId = SessionId;

    // Clean up previous search
    if (CurrentSessionSearchHandle)
    {
        EOS_SessionSearch_Release(CurrentSessionSearchHandle);
        CurrentSessionSearchHandle = nullptr;
    }

    // Create search for this specific session
    EOS_Sessions_CreateSessionSearchOptions SearchOptions = {};
    SearchOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST;
    SearchOptions.MaxSearchResults = 1;

    EOS_EResult Result = EOS_Sessions_CreateSessionSearch(SessionHandle, &SearchOptions, &CurrentSessionSearchHandle);

    if (Result == EOS_EResult::EOS_Success)
    {
        EM_LOG_INFO(TEXT("Created session search"));

        // Set the specific Session ID
        FTCHARToUTF8 SessionIdConverter(*SessionId);
        EOS_SessionSearch_SetSessionIdOptions SetIdOptions = {};
        SetIdOptions.ApiVersion = EOS_SESSIONSEARCH_SETSESSIONID_API_LATEST;
        SetIdOptions.SessionId = SessionIdConverter.Get();

        EOS_SessionSearch_SetSessionId(CurrentSessionSearchHandle, &SetIdOptions);

        // Execute search - this will call OnFindSessionComplete
        EOS_SessionSearch_FindOptions FindOptions = {};
        FindOptions.ApiVersion = EOS_SESSIONSEARCH_FIND_API_LATEST;
        FindOptions.LocalUserId = LocalUserId;

        EOS_SessionSearch_Find(CurrentSessionSearchHandle, &FindOptions, this, OnFindSessionComplete);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create session search: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Result)));
        PendingJoinSessionId.Empty();
    }
}

void UEOSSessionManager::InitServer()
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            TEXT("EasyMatchmakingServerGameMode Initialized!"));
    }

    // Check if running on dedicated server
    if (IsRunningDedicatedServer())
    {
        EM_LOG_INFO(TEXT("ServerHasAuthority"));
        CreateSession("MyGameSession", 4);
    }
    else
    {
        EM_LOG_INFO(TEXT("Not a dedicated server - not creating a new session"));
    }
}

void UEOSSessionManager::CreateSession(const FString& SessionName, int32 MaxPlayers)
{
    // Check if EOS SDK Manager is ready
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (!SDKManager || !SDKManager->IsInitialized())
    {
        EM_LOG_ERROR(TEXT("EOS SDK Manager not available or not initialized"));
        return;
	}

    if (!SessionHandle)
    {
        EM_LOG_ERROR(TEXT("Cannot create session - invalid handles"));
        return;
    }

    // For dedicated servers, LocalUserId can be null
    if (!IsRunningDedicatedServer() && !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot create session - user not authenticated (client build)"));
        return;
    }

    // Create session modification
    EOS_Sessions_CreateSessionModificationOptions ModOptions = {};
    ModOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
    ModOptions.SessionName = TCHAR_TO_UTF8(*SessionName);
    ModOptions.BucketId = "GameSession";
    ModOptions.MaxPlayers = MaxPlayers;
    ModOptions.LocalUserId = LocalUserId;
    ModOptions.bPresenceEnabled = EOS_FALSE;

    EOS_HSessionModification SessionModHandle = nullptr;
    EOS_EResult Result = EOS_Sessions_CreateSessionModification(SessionHandle, &ModOptions, &SessionModHandle);

    if (Result == EOS_EResult::EOS_Success)
    {
        // Update session to create it
        EOS_Sessions_UpdateSessionOptions UpdateOptions = {};
        UpdateOptions.ApiVersion = EOS_SESSIONS_UPDATESESSION_API_LATEST;
        UpdateOptions.SessionModificationHandle = SessionModHandle;

        EOS_Sessions_UpdateSession(SessionHandle, &UpdateOptions, this, OnCreateSessionComplete);
        EOS_SessionModification_Release(SessionModHandle);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create session modification: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Result)));
    }
}

void UEOSSessionManager::SearchSessions(const FString& BucketId)
{
    if (!SessionHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot search sessions - invalid handles"));
        return;
    }

    // Create session search
    EOS_Sessions_CreateSessionSearchOptions SearchOptions = {};
    SearchOptions.ApiVersion = EOS_SESSIONS_CREATESESSIONSEARCH_API_LATEST;
    SearchOptions.MaxSearchResults = 50;

    EOS_EResult Result = EOS_Sessions_CreateSessionSearch(SessionHandle, &SearchOptions, &CurrentSessionSearchHandle);

    if (Result == EOS_EResult::EOS_Success && CurrentSessionSearchHandle)
    {
        // Set bucket filter to find specific game sessions
        EOS_Sessions_AttributeData BucketAttribute = {};
        BucketAttribute.ApiVersion = EOS_SESSIONS_ATTRIBUTEDATA_API_LATEST;

        FTCHARToUTF8 BucketIdConverter(*BucketId);
        BucketAttribute.Key = EOS_SESSIONS_SEARCH_BUCKET_ID;
        BucketAttribute.Value.AsUtf8 = BucketIdConverter.Get();
        BucketAttribute.ValueType = EOS_ESessionAttributeType::EOS_SAT_String;

        EOS_SessionSearch_SetParameterOptions SetParamOptions = {};
        SetParamOptions.ApiVersion = EOS_SESSIONSEARCH_SETPARAMETER_API_LATEST;
        SetParamOptions.Parameter = &BucketAttribute;

        EOS_SessionSearch_SetParameter(CurrentSessionSearchHandle, &SetParamOptions);

        // Execute search
        EOS_SessionSearch_FindOptions FindOptions = {};
        FindOptions.ApiVersion = EOS_SESSIONSEARCH_FIND_API_LATEST;
        FindOptions.LocalUserId = LocalUserId;

        EM_LOG_INFO(TEXT("Searching for sessions..."));
        EOS_SessionSearch_Find(CurrentSessionSearchHandle, &FindOptions, this, OnSessionSearchComplete);
    }
}

void UEOSSessionManager::OnSessionSearchComplete(const EOS_SessionSearch_FindCallbackInfo* Data)
{
    UEOSSessionManager* SessionManager = static_cast<UEOSSessionManager*>(Data->ClientData);

    if (!IsValid(SessionManager))
    {
        return;
    }

    // Clear old cached session details
    for (auto& Pair : SessionManager->CachedSessionDetails)
    {
        if (Pair.Value)
        {
            EOS_SessionDetails_Release(Pair.Value);
        }
    }
    SessionManager->CachedSessionDetails.Empty();

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        // Get search result count
        EOS_SessionSearch_GetSearchResultCountOptions CountOptions = {};
        CountOptions.ApiVersion = EOS_SESSIONSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;

        uint32_t SessionCount = EOS_SessionSearch_GetSearchResultCount(SessionManager->CurrentSessionSearchHandle, &CountOptions);
        EM_LOG_INFO(TEXT("Found %d sessions"), SessionCount);

        TArray<FString> FoundSessionIds;

        for (uint32_t i = 0; i < SessionCount; i++)
        {
            EOS_SessionSearch_CopySearchResultByIndexOptions CopyOptions = {};
            CopyOptions.ApiVersion = EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
            CopyOptions.SessionIndex = i;

            EOS_HSessionDetails SessionDetails = nullptr;
            if (EOS_SessionSearch_CopySearchResultByIndex(SessionManager->CurrentSessionSearchHandle, &CopyOptions, &SessionDetails) == EOS_EResult::EOS_Success)
            {
                // Get session info
                EOS_SessionDetails_CopyInfoOptions InfoOptions = {};
                InfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;

                EOS_SessionDetails_Info* SessionInfo = nullptr;
                if (EOS_SessionDetails_CopyInfo(SessionDetails, &InfoOptions, &SessionInfo) == EOS_EResult::EOS_Success)
                {
                    FString SessionId = UTF8_TO_TCHAR(SessionInfo->SessionId);
                    FoundSessionIds.Add(SessionId);

                    SessionManager->CachedSessionDetails.Add(SessionId, SessionDetails);
                    if (SessionInfo->HostAddress && FCStringAnsi::Strlen(SessionInfo->HostAddress) > 0)
                    {
                        FString HostAddress = UTF8_TO_TCHAR(SessionInfo->HostAddress);
                        EM_LOG_INFO(TEXT("Host Address: %s"), *HostAddress);
                    }

                    EM_LOG_INFO(TEXT("Found session: %s"), *SessionId);

                    EOS_SessionDetails_Info_Release(SessionInfo);
                }
            }
        }

        // Broadcast found sessions to Blueprint
        SessionManager->OnSessionsFound.Broadcast(FoundSessionIds);
    }

    // Clean up
    if (SessionManager->CurrentSessionSearchHandle)
    {
        EOS_SessionSearch_Release(SessionManager->CurrentSessionSearchHandle);
        SessionManager->CurrentSessionSearchHandle = nullptr;
    }
}

void UEOSSessionManager::OnFindSessionComplete(const EOS_SessionSearch_FindCallbackInfo* Data)
{
    EM_LOG_INFO(TEXT("OnFindSessionComplete CALLED!")); 
    UEOSSessionManager* SessionManager = static_cast<UEOSSessionManager*>(Data->ClientData);

    if (!IsValid(SessionManager))
    {
        EM_LOG_ERROR(TEXT("INVALID SESSION MANAGER ONFINSESSIONCOMPLETE"));
        return;
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
		EM_LOG_INFO(TEXT("Session search successful, we got data from session search"));
        // Get the first search result
        EOS_SessionSearch_CopySearchResultByIndexOptions CopyOptions = {};
        CopyOptions.ApiVersion = EOS_SESSIONSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
        CopyOptions.SessionIndex = 0;

        EOS_HSessionDetails SessionDetails = nullptr;
        EOS_EResult CopyResult = EOS_SessionSearch_CopySearchResultByIndex(SessionManager->CurrentSessionSearchHandle, &CopyOptions, &SessionDetails);

        if (CopyResult == EOS_EResult::EOS_Success && SessionDetails)
        {
            FString SessionId = SessionManager->PendingJoinSessionId;

            // CACHE IT!
            if (!SessionId.IsEmpty())
            {
                SessionManager->CachedSessionDetails.Add(SessionId, SessionDetails);
                EM_LOG_INFO(TEXT("Cached SessionDetails for member join"));
            }

            // Extract server address for later use
            FString ServerAddress = SessionManager->GetServerAddressFromSessionDetails(SessionDetails);

            if (ServerAddress.IsEmpty())
            {
                EM_LOG_ERROR(TEXT("No server address in session"));
                EOS_SessionDetails_Release(SessionDetails);
                return;
            }
            EM_LOG_INFO(TEXT("Found server at: %s"), *ServerAddress);

            // Join the session
            EOS_Sessions_JoinSessionOptions JoinOptions = {};
            JoinOptions.ApiVersion = EOS_SESSIONS_JOINSESSION_API_LATEST;
            JoinOptions.SessionHandle = SessionDetails;
            JoinOptions.LocalUserId = SessionManager->LocalUserId;
            JoinOptions.bPresenceEnabled = EOS_FALSE;
            JoinOptions.SessionName = "MyGameSession";

            EM_LOG_INFO(TEXT("Joining session..."));

            EOS_Sessions_JoinSession(
                SessionManager->SessionHandle,
                &JoinOptions,
                SessionManager,
                OnJoinSessionComplete
            );
        }
        else
        {
            EM_LOG_ERROR(TEXT("Failed to get session details from search"));
        }
    }
    else
    {
        EM_LOG_ERROR(TEXT("Session search failed: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSSessionManager::OnJoinSessionComplete(const EOS_Sessions_JoinSessionCallbackInfo* Data)
{
    UEOSSessionManager* SessionManager = static_cast<UEOSSessionManager*>(Data->ClientData);

    if (!IsValid(SessionManager))
    {
        return;
    }

    EM_LOG_INFO(TEXT("=== OnJoinSessionComplete executed ==="));

    SessionManager->EOSManager->GetLobbyManager()->UnregisterLobbyNotifications();

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        FString SessionId = SessionManager->PendingJoinSessionId;
        if (SessionId.IsEmpty())
        {
            EM_LOG_ERROR(TEXT("No session ID stored!"));
            return;
        }

        EM_LOG_INFO(TEXT("Joined session successfully, Session ID: %s"), *SessionId);
        SessionManager->CurrentSessionId = SessionId;

        // If in a lobby, share the Session ID (NOT IP!)
        if (SessionManager->EOSManager)
        {
            if (UEOSLobbyManager* LobbyManager = SessionManager->EOSManager->GetLobbyManager())
            {
                if (LobbyManager->IsInLobby())
                {
                    if (LobbyManager->IsLobbyOwner())
                    {
                        LobbyManager->SetLobbySessionAddress(SessionId);
                        EM_LOG_INFO(TEXT("[OWNER] Shared Session ID with lobby: %s"), *SessionId);
                    }
                    else
                    {
                        EM_LOG_INFO(TEXT("[MEMBER] Not lobby owner - skipping session address update"));
                    }
                }
            }
        }

        EOS_HSessionDetails* SessionDetailsPtr = SessionManager->CachedSessionDetails.Find(SessionId);
        EM_LOG_INFO(TEXT("Checking CACHED DETAILS"));
        if (SessionDetailsPtr && *SessionDetailsPtr)
        {
            FString ServerAddress = SessionManager->GetServerAddressFromSessionDetails(*SessionDetailsPtr);

            if (!ServerAddress.IsEmpty())
            {
                if (UWorld* World = GEngine->GetWorldFromContextObject(SessionManager, EGetWorldErrorMode::LogAndReturnNull))
                {
                    APlayerController* PC = World->GetFirstPlayerController();
                    if (PC)
                    {
                        // Clear UI input mode
                        FInputModeGameOnly InputMode;
                        PC->SetInputMode(InputMode);
                        PC->SetShowMouseCursor(false);

                        // Travel to server
                        PC->ClientTravel(ServerAddress, TRAVEL_Absolute);
                        EM_LOG_INFO(TEXT("Traveling to server: %s"), *ServerAddress);
                    }
                }
            }
            else
            {
                EM_LOG_ERROR(TEXT("No server address found in session"));
            }
        }
        else
        {
            EM_LOG_ERROR(TEXT("No CachedSessionDetails"));
        }

        // Clear pending ID
        SessionManager->PendingJoinSessionId.Empty();
    }
    else if (Data->ResultCode == EOS_EResult::EOS_Sessions_SessionAlreadyExists) // For production with real players, this error shouldn't happen because each player has a unique account. 
    {
        // Special handling for "already in session" error
        EM_LOG_WARNING(TEXT("Already in session - traveling anyway(PIE testing)"));

        FString SessionId = SessionManager->PendingJoinSessionId;
        SessionManager->CurrentSessionId = SessionId;

        // Still get server address and travel
        EOS_HSessionDetails* SessionDetailsPtr = SessionManager->CachedSessionDetails.Find(SessionId);

        if (SessionDetailsPtr && *SessionDetailsPtr)
        {
            FString ServerAddress = SessionManager->GetServerAddressFromSessionDetails(*SessionDetailsPtr);

            if (!ServerAddress.IsEmpty())
            {
                if (UWorld* World = GEngine->GetWorldFromContextObject(SessionManager, EGetWorldErrorMode::LogAndReturnNull))
                {
                    APlayerController* PC = World->GetFirstPlayerController();
                    if (PC)
                    {
                        FInputModeGameOnly InputMode;
                        PC->SetInputMode(InputMode);
                        PC->SetShowMouseCursor(false);

                        PC->ClientTravel(ServerAddress, TRAVEL_Absolute);
                        EM_LOG_INFO(TEXT("Traveling anyway: %s"), *ServerAddress);
                    }
                }
            }
        }

        SessionManager->PendingJoinSessionId.Empty();
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to join session: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }

    //Clean up all cached session details after join attempt
    for (auto& Pair : SessionManager->CachedSessionDetails)
    {
        if (Pair.Value)
        {
            EOS_SessionDetails_Release(Pair.Value);
        }
    }
    SessionManager->CachedSessionDetails.Empty();
}

FString UEOSSessionManager::GetServerAddressFromSessionDetails(EOS_HSessionDetails SessionDetails)
{
    if (!SessionDetails)
    {
        EM_LOG_ERROR(TEXT("SessionDetails is null"));
        return FString();
    }

    FString HostIP;
    int32 Port = 7777; // Default port

    // Get session info
    EOS_SessionDetails_CopyInfoOptions InfoOptions = {};
    InfoOptions.ApiVersion = EOS_SESSIONDETAILS_COPYINFO_API_LATEST;

    EOS_SessionDetails_Info* SessionInfo = nullptr;
    if (EOS_SessionDetails_CopyInfo(SessionDetails, &InfoOptions, &SessionInfo) == EOS_EResult::EOS_Success)
    {
        if (SessionInfo->HostAddress && FCStringAnsi::Strlen(SessionInfo->HostAddress) > 0)
        {
            HostIP = UTF8_TO_TCHAR(SessionInfo->HostAddress);
        }

        EOS_SessionDetails_Info_Release(SessionInfo);
    }

    if (HostIP.IsEmpty())
    {
        EM_LOG_WARNING(TEXT("No HostAddress found, using localhost"));
        return TEXT("127.0.0.1:7777");
    }

    // Detect local vs remote
    if (FParse::Param(FCommandLine::Get(), TEXT("ForceLocalServer")) || HostIP == TEXT("127.0.0.1"))
    {
        EM_LOG_INFO(TEXT("Forced Local server"));
        return TEXT("127.0.0.1:7777");
    }

    // Check if already has port
    if (HostIP.Contains(TEXT(":")))
    {
        EM_LOG_INFO(TEXT("Server address: %s"), *HostIP);
        return HostIP;
    }

    // If no then Add port
    FString FullAddress = FString::Printf(TEXT("%s:%d"), *HostIP, Port);
    EM_LOG_INFO(TEXT("Server address with port: %s"), *FullAddress);

    return FullAddress;
}

void UEOSSessionManager::OnCreateSessionComplete(const EOS_Sessions_UpdateSessionCallbackInfo* Data)
{
    UEOSSessionManager* SessionManager = static_cast<UEOSSessionManager*>(Data->ClientData);

    if (!IsValid(SessionManager))
    {
        EM_LOG_INFO(TEXT("Invalid Session Manager in OnCreateSessionComplete"));
        return;
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        SessionManager->CurrentSessionId = UTF8_TO_TCHAR(Data->SessionId);
        EM_LOG_INFO(TEXT("Session created successfully! SessionId: %s"), *SessionManager->CurrentSessionId);
    }
}

void UEOSSessionManager::OnDestroySessionComplete(const EOS_Sessions_DestroySessionCallbackInfo* Data)
{
    UEOSSessionManager* SessionManager = static_cast<UEOSSessionManager*>(Data->ClientData);

    if (!IsValid(SessionManager))
    {
        return;
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        SessionManager->CurrentSessionId.Empty();
        EM_LOG_INFO(TEXT("Session destroyed successfully"));
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to destroy session: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

