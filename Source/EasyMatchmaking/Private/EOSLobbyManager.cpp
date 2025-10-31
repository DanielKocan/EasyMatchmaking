#include "EOSLobbyManager.h"

#include <eos_connect.h>
#include <eos_sdk.h>
#include <eos_userinfo.h>
#include <eos_lobby.h>

#include "EasyMatchmakingLog.h"
#include "EOSManager.h"
#include "IEOSSDKManager.h"

void UEOSLobbyManager::Init(void* InPlatformHandle, void* InLobbyHandle, void* InLocalUserId, UEOSManager* InManager)
{
    PlatformHandle = static_cast<EOS_HPlatform>(InPlatformHandle);
    LobbyHandle = static_cast<EOS_HLobby>(InLobbyHandle);
    LocalUserId = static_cast<EOS_ProductUserId>(InLocalUserId);
    EOSManager = InManager;

    LocalEpicAccountId = EOSManager->GetCurrentEpicAccountId();

    if (PlatformHandle && LobbyHandle)
    {
        EM_LOG_INFO(TEXT("EOSLobbyManager initialized successfully"));
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to initialize EOSLobbyManager - invalid handles"));
    }

    RegisterP2PNotifications();

    // Log the exact user ID being used
    char UserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
    int32 BufferSize = sizeof(UserIdStr);
    EOS_ProductUserId_ToString(LocalUserId, UserIdStr, &BufferSize);

    EM_LOG_INFO(TEXT("Initializing lobby with ProductUserId: %s"), UTF8_TO_TCHAR(UserIdStr));

    SetPlayerReady(false);
}

void UEOSLobbyManager::BeginDestroy()
{
    UnregisterLobbyNotifications();

    // Clear all cached data
    LobbyMembers.Empty();
    FoundLobbies.Empty();

    // Cancel any pending async operations if possible
    if (CurrentLobbySearchHandle)
    {
        EOS_LobbySearch_Release(CurrentLobbySearchHandle);
        CurrentLobbySearchHandle = nullptr;
    }

	UObject::BeginDestroy();
}

void UEOSLobbyManager::RegisterLobbyNotifications()
{
    if (!LobbyHandle)
    {
        EM_LOG_ERROR(TEXT("Cannot register notifications - invalid lobby handle"));
        return;
    }

    // Unregister existing notifications first
    UnregisterLobbyNotifications();

    // Register for lobby updates (when owner changes lobby properties)
    EOS_Lobby_AddNotifyLobbyUpdateReceivedOptions LobbyUpdateOptions = {};
    LobbyUpdateOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYUPDATERECEIVED_API_LATEST;

    LobbyUpdateNotificationId = EOS_Lobby_AddNotifyLobbyUpdateReceived(
        LobbyHandle,
        &LobbyUpdateOptions,
        this,
        OnLobbyUpdateReceived
    );

    EM_LOG_INFO(TEXT("Registered for lobby update notifications (ID: %llu)"), LobbyUpdateNotificationId);

    // Register for member updates (when any member changes their attributes)
    EOS_Lobby_AddNotifyLobbyMemberUpdateReceivedOptions MemberUpdateOptions = {};
    MemberUpdateOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERUPDATERECEIVED_API_LATEST;

    LobbyMemberUpdateNotificationId = EOS_Lobby_AddNotifyLobbyMemberUpdateReceived(
        LobbyHandle,
        &MemberUpdateOptions,
        this,
        OnLobbyMemberUpdateReceived
    );

    EM_LOG_INFO(TEXT("Registered for member update notifications (ID: %llu)"), LobbyMemberUpdateNotificationId);

    // Register for member status (join/leave)
    EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions StatusOptions = {};
    StatusOptions.ApiVersion = EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST;

    LobbyMemberStatusReceivedNotificationId = EOS_Lobby_AddNotifyLobbyMemberStatusReceived(
        LobbyHandle,
        &StatusOptions,
        this,
        OnLobbyMemberStatusReceived
    );

    EM_LOG_INFO(TEXT("Registered for member status notifications (ID: %llu)"), LobbyMemberStatusReceivedNotificationId);
}

void UEOSLobbyManager::UnregisterLobbyNotifications()
{
    if (!LobbyHandle)
    {
        EM_LOG_ERROR(TEXT("Cannot register notifications - invalid lobby handle"));
        return;
    }

    if (LobbyUpdateNotificationId != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_Lobby_RemoveNotifyLobbyUpdateReceived(LobbyHandle, LobbyUpdateNotificationId);
        LobbyUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
        EM_LOG_INFO(TEXT("Unregistered lobby update notifications"));
    }

    if (LobbyMemberUpdateNotificationId != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_Lobby_RemoveNotifyLobbyMemberUpdateReceived(LobbyHandle, LobbyMemberUpdateNotificationId);
        LobbyMemberUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
        EM_LOG_INFO(TEXT("Unregistered member update notifications"));
    }

    if (LobbyMemberStatusReceivedNotificationId != EOS_INVALID_NOTIFICATIONID)
    {
        EOS_Lobby_RemoveNotifyLobbyMemberStatusReceived(LobbyHandle, LobbyMemberStatusReceivedNotificationId);
        LobbyMemberStatusReceivedNotificationId = EOS_INVALID_NOTIFICATIONID;
        EM_LOG_INFO(TEXT("Unregistered member status notifications"));
    }
}

void UEOSLobbyManager::CreateLobby(const FLobbySettings& Settings)
{
    if (!LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot create lobby - invalid handles or user not authenticated"));
        return;
    }

    if (bIsInLobby)
    {
        EM_LOG_WARNING(TEXT("Already in a lobby. Leave current lobby first."));
        return;
    }

    // Store settings
    CurrentSettings = Settings;

    // Log the exact user ID being used
    char UserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
    int32 BufferSize = sizeof(UserIdStr);
    EOS_ProductUserId_ToString(LocalUserId, UserIdStr, &BufferSize);
    EM_LOG_INFO(TEXT("Initializing lobby with ProductUserId: %s"), UTF8_TO_TCHAR(UserIdStr));

    EOS_Lobby_CreateLobbyOptions CreateOptions = {};
    CreateOptions.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    CreateOptions.LocalUserId = LocalUserId;
    CreateOptions.MaxLobbyMembers = Settings.MaxPlayers;
    CreateOptions.PermissionLevel = EOS_ELobbyPermissionLevel::EOS_LPL_PUBLICADVERTISED;
    CreateOptions.bPresenceEnabled = EOS_FALSE;
    CreateOptions.bAllowInvites = EOS_TRUE;
    CreateOptions.bDisableHostMigration = EOS_FALSE;
    CreateOptions.LobbyId = nullptr; 
    CreateOptions.bEnableRTCRoom = EOS_FALSE;  
    CreateOptions.LocalRTCOptions = nullptr;  

    FTCHARToUTF8 BucketIdConverter(*Settings.BucketId);
    CreateOptions.BucketId = BucketIdConverter.Get();

    EM_LOG_INFO(TEXT("=== EOS PLATFORM DEBUG INFO ==="));

    // Get the current platform config
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (SDKManager)
    {
        FString DefaultConfig = SDKManager->GetDefaultPlatformConfigName();
        const FEOSSDKPlatformConfig* Config = SDKManager->GetPlatformConfig(DefaultConfig);

        if (Config)
        {
            EM_LOG_INFO(TEXT("Config Name: %s"), *Config->Name);
            EM_LOG_INFO(TEXT("ProductId: %s"), *Config->ProductId);
            EM_LOG_INFO(TEXT("SandboxId: %s"), *Config->SandboxId);
            EM_LOG_INFO(TEXT("ClientId: %s"), *Config->ClientId);
            EM_LOG_INFO(TEXT("ClientSecret: %s"), *Config->ClientSecret);
            EM_LOG_INFO(TEXT("DeploymentId: %s"), *Config->DeploymentId);
            EM_LOG_INFO(TEXT("bIsServer: %s"), Config->bIsServer ? TEXT("true") : TEXT("false"));
            EM_LOG_INFO(TEXT("bDisableOverlay: %s"), Config->bDisableOverlay ? TEXT("true") : TEXT("false"));
        }
    }

    EOS_Lobby_CreateLobby(LobbyHandle, &CreateOptions, this, OnCreateLobbyComplete);
}

void UEOSLobbyManager::JoinLobby(const FString& LobbyId)
{
    if (!LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot join lobby - invalid handles or user not authenticated"));
        return;
    }

    if (bIsInLobby)
    {
        EM_LOG_WARNING(TEXT("Already in a lobby. Leave current lobby first."));
        return;
    }

    if (LobbyId.IsEmpty())
    {
        EM_LOG_ERROR(TEXT("Cannot join lobby - LobbyId is empty"));
        return;
    }

    // Search for the specific lobby to get its details handle
    EOS_Lobby_CreateLobbySearchOptions SearchOptions = {};
    SearchOptions.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    SearchOptions.MaxResults = 1;

    EOS_EResult Result = EOS_Lobby_CreateLobbySearch(LobbyHandle, &SearchOptions, &CurrentLobbySearchHandle);

    if (Result == EOS_EResult::EOS_Success && CurrentLobbySearchHandle)
    {
        // Set the specific lobby ID to search for
        FTCHARToUTF8 LobbyIdConverter(*LobbyId);
        EOS_LobbySearch_SetLobbyIdOptions SetLobbyIdOptions = {};
        SetLobbyIdOptions.ApiVersion = EOS_LOBBYSEARCH_SETLOBBYID_API_LATEST;
        SetLobbyIdOptions.LobbyId = LobbyIdConverter.Get();

        EOS_LobbySearch_SetLobbyId(CurrentLobbySearchHandle, &SetLobbyIdOptions);

        // Execute search to get lobby details
        EOS_LobbySearch_FindOptions FindOptions = {};
        FindOptions.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
        FindOptions.LocalUserId = LocalUserId;

        EM_LOG_INFO(TEXT("Searching for lobby to join: %s"), *LobbyId);
        EOS_LobbySearch_Find(CurrentLobbySearchHandle, &FindOptions, this, OnFindLobbyToJoinComplete);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create lobby search for joining"));
    }
}

void UEOSLobbyManager::LeaveLobby()
{
    if (!LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot leave lobby - invalid handles"));
        return;
    }

    if (!bIsInLobby || CurrentLobbyId.IsEmpty())
    {
        EM_LOG_WARNING(TEXT("Not currently in a lobby"));
        return;
    }

    EOS_Lobby_LeaveLobbyOptions LeaveOptions = {};
    LeaveOptions.ApiVersion = EOS_LOBBY_LEAVELOBBY_API_LATEST;
    LeaveOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    LeaveOptions.LobbyId = LobbyIdConverter.Get();

    EM_LOG_INFO(TEXT("Leaving lobby: %s"), *CurrentLobbyId);

    EOS_Lobby_LeaveLobby(LobbyHandle, &LeaveOptions, this, OnLeaveLobbyComplete);
}

void UEOSLobbyManager::DestroyLobby()
{
    if (!LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot destroy lobby - invalid handles"));
        return;
    }

    if (!bIsInLobby || CurrentLobbyId.IsEmpty())
    {
        EM_LOG_WARNING(TEXT("Not currently in a lobby to destroy"));
        return;
    }

    EOS_Lobby_DestroyLobbyOptions DestroyOptions = {};
    DestroyOptions.ApiVersion = EOS_LOBBY_DESTROYLOBBY_API_LATEST;
    DestroyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    DestroyOptions.LobbyId = LobbyIdConverter.Get();

    EM_LOG_INFO(TEXT("Destroying lobby: %s"), *CurrentLobbyId);

    EOS_Lobby_DestroyLobby(LobbyHandle, &DestroyOptions, this, OnDestroyLobbyComplete);
}

void UEOSLobbyManager::SearchLobbies(const FString& BucketId)
{
    if (!LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot search lobbies - invalid handles"));
        return;
    }

    FoundLobbies.Empty();

    // Create lobby search handle
    EOS_Lobby_CreateLobbySearchOptions SearchOptions = {};
    SearchOptions.ApiVersion = EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST;
    SearchOptions.MaxResults = 50;

    EOS_EResult Result = EOS_Lobby_CreateLobbySearch(LobbyHandle, &SearchOptions, &CurrentLobbySearchHandle);

    if (Result == EOS_EResult::EOS_Success && CurrentLobbySearchHandle)
    {
        if (!BucketId.IsEmpty())
        {
            FTCHARToUTF8 BucketIdConverter(*BucketId);

            EOS_Lobby_AttributeData BucketAttribute = {};
            BucketAttribute.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
            BucketAttribute.Key = "bucket";
            BucketAttribute.Value.AsUtf8 = BucketIdConverter.Get();
            BucketAttribute.ValueType = EOS_ELobbyAttributeType::EOS_AT_STRING;

            EOS_LobbySearch_SetParameterOptions SetParamOptions = {};
            SetParamOptions.ApiVersion = EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST;
            SetParamOptions.Parameter = &BucketAttribute;
            SetParamOptions.ComparisonOp = EOS_EComparisonOp::EOS_CO_EQUAL;

            EOS_LobbySearch_SetParameter(CurrentLobbySearchHandle, &SetParamOptions);
        }
        // Execute search
        EOS_LobbySearch_FindOptions FindOptions = {};
        FindOptions.ApiVersion = EOS_LOBBYSEARCH_FIND_API_LATEST;
        FindOptions.LocalUserId = LocalUserId;

        EM_LOG_INFO(TEXT("Executing lobby search..."));
        EOS_LobbySearch_Find(CurrentLobbySearchHandle, &FindOptions, this, OnFindLobbiesComplete);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create lobby search"));
    }
}

const FLobbyInfo UEOSLobbyManager::UpdateLobbyInfoData()
{
    if (!bIsInLobby || CurrentLobbyId.IsEmpty())
    {
        EM_LOG_ERROR(TEXT("Not in a lobby to refresh"));
        return FLobbyInfo();
    }

    // Get current lobby details
    EOS_Lobby_CopyLobbyDetailsHandleOptions CopyOptions = {};
    CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    CopyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    CopyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyDetails LobbyDetails = nullptr;
    EOS_EResult Result = EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle, &CopyOptions, &LobbyDetails);

    FLobbyInfo UpdatedInfo;
    if (Result == EOS_EResult::EOS_Success && LobbyDetails)
    {
        // Get updated lobby info
        EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
        InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

        EOS_LobbyDetails_Info* LobbyInfo = nullptr;
		
        if (EOS_LobbyDetails_CopyInfo(LobbyDetails, &InfoOptions, &LobbyInfo) == EOS_EResult::EOS_Success)
        {
            EM_LOG_INFO(TEXT("Current lobby refreshed: %s (Players: %d/%d)"),
                *CurrentLobbyId,
                LobbyInfo->MaxMembers - LobbyInfo->AvailableSlots,
                LobbyInfo->MaxMembers);

            UpdatedInfo.LobbyId = CurrentLobbyId;
            UpdatedInfo.CurrentPlayers = LobbyInfo->MaxMembers - LobbyInfo->AvailableSlots;
            UpdatedInfo.MaxPlayers = LobbyInfo->MaxMembers;
            UpdatedInfo.BucketId = LobbyInfo->BucketId;
            // Convert owner ProductUserId to string
            if (LobbyInfo->LobbyOwnerUserId)
            {
                char OwnerIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
                int32_t BufferSize = sizeof(OwnerIdStr);
                if (EOS_ProductUserId_ToString(LobbyInfo->LobbyOwnerUserId, OwnerIdStr, &BufferSize) == EOS_EResult::EOS_Success)
                {
                    UpdatedInfo.OwnerUserId = UTF8_TO_TCHAR(OwnerIdStr);
                }
                else
                {
                    UpdatedInfo.OwnerUserId = TEXT("Unknown");
                }
            }
            else
            {
                UpdatedInfo.OwnerUserId = TEXT("No Owner");
            }

            // Update current lobby settings
            CurrentSettings.MaxPlayers = LobbyInfo->MaxMembers;

            EOS_LobbyDetails_Info_Release(LobbyInfo);
        }

        // Check for session address attribute
        EOS_LobbyDetails_GetAttributeCountOptions AttrCountOptions = {};
        AttrCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST;
        uint32_t AttributeCount = EOS_LobbyDetails_GetAttributeCount(LobbyDetails, &AttrCountOptions);

        for (uint32_t i = 0; i < AttributeCount; i++)
        {
            EOS_LobbyDetails_CopyAttributeByIndexOptions AttrOptions = {};
            AttrOptions.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST;
            AttrOptions.AttrIndex = i;

            EOS_Lobby_Attribute* Attribute = nullptr;
            EOS_EResult AttrResult = EOS_LobbyDetails_CopyAttributeByIndex(LobbyDetails, &AttrOptions, &Attribute);

            if (AttrResult == EOS_EResult::EOS_Success && Attribute)
            {
                FString AttributeKey = UTF8_TO_TCHAR(Attribute->Data->Key);

                if (AttributeKey == TEXT("session_address"))
                {
                    FString SessionAddress = UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8);
                    EM_LOG_INFO(TEXT("Session address available: %s"), *SessionAddress);

                    // Broadcast to all members
                    OnSessionAddressUpdated.Broadcast(SessionAddress);
                }

                EOS_Lobby_Attribute_Release(Attribute);
            }
        }

        EOS_LobbyDetails_Release(LobbyDetails);
    }
    return UpdatedInfo;
}

void UEOSLobbyManager::RegisterP2PNotifications()
{
    EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
    if (!P2PHandle)
    {
        EM_LOG_ERROR(TEXT("Failed to get P2P interface"));
        return;
    }

    // Initialize chat socket ID
    ChatSocketId = {};
    ChatSocketId.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;    
    FCStringAnsi::Strncpy(ChatSocketId.SocketName, "CHAT", sizeof(ChatSocketId.SocketName) - 1); 

    EM_LOG_INFO(TEXT("ChatSocketId initialized: Name=%s, ApiVersion=%d"),
        UTF8_TO_TCHAR(ChatSocketId.SocketName),
        ChatSocketId.ApiVersion);

    EOS_P2P_AddNotifyPeerConnectionRequestOptions RequestOptions = {};
    RequestOptions.ApiVersion = EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST;
    RequestOptions.LocalUserId = LocalUserId;
    RequestOptions.SocketId = &ChatSocketId;

    P2PConnectionRequestNotificationId = EOS_P2P_AddNotifyPeerConnectionRequest(
        P2PHandle,
        &RequestOptions,
        this,
        OnIncomingConnectionRequest
    );

    if (P2PConnectionRequestNotificationId != EOS_INVALID_NOTIFICATIONID)
    {
        EM_LOG_INFO(TEXT("Registered P2P connection notifications (ID: %llu)"), P2PConnectionRequestNotificationId);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to register P2P notifications!"));
    }
}

void UEOSLobbyManager::RegisterTimerForTickP2PMessages(UEOSLobbyManager* LobbyManager)
{
    if (UWorld* World = GEngine->GetWorldFromContextObject(LobbyManager, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->GetTimerManager().SetTimer(
            LobbyManager->ChatTickTimer,
            LobbyManager,
            &UEOSLobbyManager::TickP2PMessages,
            0.1f,
            true
        );
    }
}

void UEOSLobbyManager::UnregisterTimerForTickP2PMessages(UEOSLobbyManager* LobbyManager)
{
    if (UWorld* World = GEngine->GetWorldFromContextObject(LobbyManager, EGetWorldErrorMode::LogAndReturnNull))
    {
        World->GetTimerManager().ClearTimer(LobbyManager->ChatTickTimer);
    }
}

bool UEOSLobbyManager::IsLobbyOwner() const
{
    if (!bIsInLobby || CurrentLobbyId.IsEmpty() || !LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("IsLobbyOwner returned false because something was missing"));
        return false;
    }

    EOS_Lobby_CopyLobbyDetailsHandleOptions CopyOptions = {};
    CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    CopyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    CopyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyDetails LobbyDetails = nullptr;
    EOS_EResult Result = EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle, &CopyOptions, &LobbyDetails);

    bool bIsOwner = false;
    if (Result == EOS_EResult::EOS_Success && LobbyDetails)
    {
        EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
        InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

        EOS_LobbyDetails_Info* LobbyInfo = nullptr;
        if (EOS_LobbyDetails_CopyInfo(LobbyDetails, &InfoOptions, &LobbyInfo) == EOS_EResult::EOS_Success)
        {
            // Check if we're the owner
            bIsOwner = (LobbyInfo->LobbyOwnerUserId == LocalUserId);

            EOS_LobbyDetails_Info_Release(LobbyInfo);
        }
        else
        {
			EM_LOG_ERROR(TEXT("Failed to copy lobby info to check owner"));
        }

        EOS_LobbyDetails_Release(LobbyDetails);
    }

    return bIsOwner;
}

bool UEOSLobbyManager::AreAllPlayersReady() const
{
    if (!bIsInLobby || LobbyMembers.Num() == 0)
    {
        return false;
    }

    // Check if all members are ready
    for (const TPair<FString, FLobbyMemberInfo>& Member : LobbyMembers)
    {
        if (!Member.Value.bIsReady)
        {
            EM_LOG_INFO(TEXT("Player %s is not ready"), *Member.Value.DisplayName);
            return false;
        }
    }

    EM_LOG_INFO(TEXT("All %d players are ready!"), LobbyMembers.Num());
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1,5.0f,FColor::Green,TEXT("All players in lobby are ready!"));
 
    return true;
}

int32 UEOSLobbyManager::GetReadyPlayerCount() const
{
    int32 ReadyCount = 0;

    for (const TPair<FString, FLobbyMemberInfo>& Member : LobbyMembers)
    {
        if (Member.Value.bIsReady)
        {
            ReadyCount++;
        }
    }

    return ReadyCount;
}

void UEOSLobbyManager::SetPlayerReady(bool bReady)
{
    if (!bIsInLobby || CurrentLobbyId.IsEmpty() || !LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot set ready status - not in lobby"));
        return;
    }

    EOS_Lobby_UpdateLobbyModificationOptions ModifyOptions = {};
    ModifyOptions.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
    ModifyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    ModifyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyModification LobbyModificationHandle = nullptr;
    EOS_EResult ModifyResult = EOS_Lobby_UpdateLobbyModification(LobbyHandle, &ModifyOptions, &LobbyModificationHandle);

    if (ModifyResult == EOS_EResult::EOS_Success && LobbyModificationHandle)
    {
        // Set ready status as member attribute
        EOS_Lobby_AttributeData ReadyAttribute = {};
        ReadyAttribute.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        ReadyAttribute.Key = "ready";
        ReadyAttribute.Value.AsBool = bReady ? EOS_TRUE : EOS_FALSE;
        ReadyAttribute.ValueType = EOS_ELobbyAttributeType::EOS_AT_BOOLEAN;

        EOS_LobbyModification_AddMemberAttributeOptions AddMemberAttrOptions = {};
        AddMemberAttrOptions.ApiVersion = EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST;
        AddMemberAttrOptions.Attribute = &ReadyAttribute;
        AddMemberAttrOptions.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;

        EOS_EResult AddAttrResult = EOS_LobbyModification_AddMemberAttribute(LobbyModificationHandle, &AddMemberAttrOptions);

        if (AddAttrResult == EOS_EResult::EOS_Success)
        {
            // Apply the modification
            EOS_Lobby_UpdateLobbyOptions UpdateOptions = {};
            UpdateOptions.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
            UpdateOptions.LobbyModificationHandle = LobbyModificationHandle;

            EM_LOG_INFO(TEXT("Setting ready status to: %s"), bReady ? TEXT("Ready") : TEXT("Not Ready"));
            EOS_Lobby_UpdateLobby(LobbyHandle, &UpdateOptions, this, OnUpdateReadyStatusLobbyComplete);
        }
        else
        {
            EM_LOG_ERROR(TEXT("Failed to add member attribute: %s"),
                UTF8_TO_TCHAR(EOS_EResult_ToString(AddAttrResult)));
        }

        EOS_LobbyModification_Release(LobbyModificationHandle);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create lobby modification: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(ModifyResult)));
    }
}

void UEOSLobbyManager::SetLobbySessionAddress(const FString& SessionAddress)
{
    if (!bIsInLobby || CurrentLobbyId.IsEmpty() || !LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot set session address - not in lobby"));
        return;
    }

    EM_LOG_INFO(TEXT("Setting lobby session address: %s"), *SessionAddress);

    EOS_Lobby_UpdateLobbyModificationOptions ModifyOptions = {};
    ModifyOptions.ApiVersion = EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST;
    ModifyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    ModifyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyModification LobbyModificationHandle = nullptr;
    EOS_EResult ModifyResult = EOS_Lobby_UpdateLobbyModification(LobbyHandle, &ModifyOptions, &LobbyModificationHandle);

    if (ModifyResult == EOS_EResult::EOS_Success && LobbyModificationHandle)
    {
        // Set session address as lobby attribute
        FTCHARToUTF8 SessionAddressConverter(*SessionAddress);

        EOS_Lobby_AttributeData SessionAttribute = {};
        SessionAttribute.ApiVersion = EOS_LOBBY_ATTRIBUTEDATA_API_LATEST;
        SessionAttribute.Key = "session_address";
        SessionAttribute.Value.AsUtf8 = SessionAddressConverter.Get();
        SessionAttribute.ValueType = EOS_ELobbyAttributeType::EOS_AT_STRING;

        EOS_LobbyModification_AddAttributeOptions AddAttrOptions = {};
        AddAttrOptions.ApiVersion = EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST;
        AddAttrOptions.Attribute = &SessionAttribute;
        AddAttrOptions.Visibility = EOS_ELobbyAttributeVisibility::EOS_LAT_PUBLIC;

        EOS_EResult AddAttrResult = EOS_LobbyModification_AddAttribute(LobbyModificationHandle, &AddAttrOptions);

        if (AddAttrResult == EOS_EResult::EOS_Success)
        {
            // Apply the modification
            EOS_Lobby_UpdateLobbyOptions UpdateOptions = {};
            UpdateOptions.ApiVersion = EOS_LOBBY_UPDATELOBBY_API_LATEST;
            UpdateOptions.LobbyModificationHandle = LobbyModificationHandle;

            EM_LOG_INFO(TEXT("Updating lobby with session address"));
            EOS_Lobby_UpdateLobby(LobbyHandle, &UpdateOptions, this, OnSetSessionAddressComplete);
        }
        else
        {
            EM_LOG_ERROR(TEXT("Failed to add session address attribute: %s"),
                UTF8_TO_TCHAR(EOS_EResult_ToString(AddAttrResult)));
        }

        EOS_LobbyModification_Release(LobbyModificationHandle);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create lobby modification: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(ModifyResult)));
    }
}

// ---------------------------------------------------------------
// Static callback functions implementations
// ---------------------------------------------------------------

void UEOSLobbyManager::OnLobbyUpdateReceived(const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return;
    }

    FString LobbyId = UTF8_TO_TCHAR(Data->LobbyId);
    EM_LOG_INFO(TEXT("Lobby Update Received for: %s"), *LobbyId);

    // Check if it's our current lobby
    if (LobbyId == LobbyManager->CurrentLobbyId)
    {
        // Check for session address update
        FString SessionAddress = LobbyManager->GetLobbySessionAddress();

        if (!SessionAddress.IsEmpty() && SessionAddress != LobbyManager->LastKnownSessionAddress)
        {
            EM_LOG_INFO(TEXT("Session address updated: %s"), *SessionAddress);
            LobbyManager->LastKnownSessionAddress = SessionAddress;

            // Broadcast to Blueprint!
            LobbyManager->OnSessionAddressUpdated.Broadcast(SessionAddress);

            if (LobbyManager->EOSManager)
            {
                if (UEOSSessionManager* SessionManager = LobbyManager->EOSManager->GetSessionManager())
                {
                    if (LobbyManager->IsLobbyOwner())
                    {
                        EM_LOG_INFO(TEXT("Already in this session (we're the owner) - skipping auto-join. Owners can start the game only and they should be already joining the session"));
                    }
                    else
                    {
                        // Add timer for member to stagger connection
						// This is strange, but that solves an issue when multiple clients try to join the session at the same time on one PC. (there is some kind of bug)
                        if (UWorld* World = LobbyManager->GetWorld())
                        {
                            FTimerHandle MemberJoinTimer;
                            float Delay = FMath::RandRange(0.5f, 2.0f); // Random (0.5;2.0) second delay

                            World->GetTimerManager().SetTimer(
                                MemberJoinTimer,
                                [LobbyManager, SessionManager, SessionAddress, Delay]()
                                {
                                    if (IsValid(LobbyManager) && IsValid(SessionManager))
                                    {
                                        EM_LOG_INFO(TEXT("[MEMBER] Auto-joining after %.1fs delay..."), Delay);
                                        SessionManager->JoinSessionById(SessionAddress);
                                    }
                                },
                                Delay,
                                false // no loop
                            );

                            EM_LOG_INFO(TEXT("[MEMBER] Will auto-join in %.1f seconds..."), Delay);
                        }
                    }
                }
                else
                {
                    EM_LOG_ERROR(TEXT("SessionManager not available!"));
                }
            }
        }

        // Refresh lobby info
        LobbyManager->UpdateLobbyInfoData();
    }
    else
    {
        EM_LOG_ERROR(TEXT("Wrong lobby ID: %s, should be %s"), *LobbyId, *LobbyManager->CurrentLobbyId);
    }
}

void UEOSLobbyManager::OnLobbyMemberUpdateReceived(const EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return;
    }

    FString LobbyId = UTF8_TO_TCHAR(Data->LobbyId);
    FString MemberId = LobbyManager->UserIdToString(Data->TargetUserId);

    EM_LOG_INFO(TEXT("Member Update Received - Lobby: % s, Member : % s"), *LobbyId, *MemberId);

    // Update member list
    if (LobbyId == LobbyManager->CurrentLobbyId)
    {
        LobbyManager->UpdateLobbyMembersData();

        LobbyManager->OnLobbyMembersChanged.Broadcast();

        // Check if everyone is ready
        if (LobbyManager->AreAllPlayersReady())
        {
            EM_LOG_WARNING(TEXT("ALL PLAYERS READY!"));
            LobbyManager->OnAllPlayersReady.Broadcast();
        }

    }
    else
    {
		EM_LOG_ERROR(TEXT("Wrong lobby ID: %s, should be %s"), *LobbyId, *LobbyManager->CurrentLobbyId);
    }
}

void UEOSLobbyManager::OnLobbyMemberStatusReceived(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return;
    }

    FString LobbyId = UTF8_TO_TCHAR(Data->LobbyId);
    FString MemberId = LobbyManager->UserIdToString(Data->TargetUserId);

    const char* StatusStr = "";
    switch (Data->CurrentStatus)
    {
    case EOS_ELobbyMemberStatus::EOS_LMS_JOINED:
        StatusStr = "JOINED";
        break;
    case EOS_ELobbyMemberStatus::EOS_LMS_LEFT:
        StatusStr = "LEFT";
        break;
    case EOS_ELobbyMemberStatus::EOS_LMS_DISCONNECTED:
        StatusStr = "DISCONNECTED";
        break;
    case EOS_ELobbyMemberStatus::EOS_LMS_KICKED:
        StatusStr = "KICKED";
        break;
    case EOS_ELobbyMemberStatus::EOS_LMS_PROMOTED:
        StatusStr = "PROMOTED";
        break;
    case EOS_ELobbyMemberStatus::EOS_LMS_CLOSED:
        StatusStr = "CLOSED";
        break;
    }

    EM_LOG_INFO(TEXT("Member Status - Lobby: %s, Member: %s, Status: %s"),
        *LobbyId, *MemberId, UTF8_TO_TCHAR(StatusStr));

    // Update member list
    if (LobbyId == LobbyManager->CurrentLobbyId)
    {
        LobbyManager->UpdateLobbyMembersData();

        LobbyManager->OnLobbyMembersChanged.Broadcast();
    }
}

void UEOSLobbyManager::OnCreateLobbyComplete(const EOS_Lobby_CreateLobbyCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return; // Don't access destroyed object
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        LobbyManager->bIsInLobby = true;
        LobbyManager->CurrentLobbyId = FString(UTF8_TO_TCHAR(Data->LobbyId));

        LobbyManager->RegisterTimerForTickP2PMessages(LobbyManager);
        LobbyManager->RegisterLobbyNotifications();

        LobbyManager->OnLobbyCreated.Broadcast(LobbyManager->CurrentLobbyId);

        LobbyManager->RegisterLobbyNotifications();

        EM_LOG_INFO(TEXT("Lobby created successfully! Lobby ID: %s"), *LobbyManager->CurrentLobbyId);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create lobby. Error: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));

        FString ErrorMsg = FString(UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
        LobbyManager->OnLobbyError.Broadcast(ErrorMsg);
    }
}

void UEOSLobbyManager::OnJoinLobbyComplete(const EOS_Lobby_JoinLobbyCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return; // Don't access destroyed object
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        LobbyManager->bIsInLobby = true;

        // Start P2P message polling
		LobbyManager->RegisterTimerForTickP2PMessages(LobbyManager);
        LobbyManager->RegisterLobbyNotifications();

        LobbyManager->CurrentLobbyId = FString(UTF8_TO_TCHAR(Data->LobbyId));
        LobbyManager->OnLobbyJoined.Broadcast(LobbyManager->CurrentLobbyId);


        EM_LOG_INFO(TEXT("Successfully joined lobby: %s"), *LobbyManager->CurrentLobbyId);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to join lobby. Error: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSLobbyManager::OnLeaveLobbyComplete(const EOS_Lobby_LeaveLobbyCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return; // Don't access destroyed object
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        
        // Stop taking p2p requestss
		LobbyManager->UnregisterTimerForTickP2PMessages(LobbyManager);
        LobbyManager->UnregisterLobbyNotifications();

        LobbyManager->bIsInLobby = false;
        LobbyManager->CurrentLobbyId.Empty();
        
		LobbyManager->OnLobbyLeft.Broadcast();

        EM_LOG_INFO(TEXT("Successfully left lobby"));
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to leave lobby. Error: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSLobbyManager::OnDestroyLobbyComplete(const EOS_Lobby_DestroyLobbyCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return;
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        LobbyManager->bIsInLobby = false;
        LobbyManager->CurrentLobbyId.Empty();

        EM_LOG_INFO(TEXT("Successfully destroyed lobby"));
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to destroy lobby. Error: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSLobbyManager::OnFindLobbiesComplete(const EOS_LobbySearch_FindCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return; // Don't access destroyed object
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        LobbyManager->FoundLobbies.Empty();

        EOS_LobbySearch_GetSearchResultCountOptions CountOptions = {};
        CountOptions.ApiVersion = EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST;
        uint32_t LobbyCount = EOS_LobbySearch_GetSearchResultCount(LobbyManager->CurrentLobbySearchHandle, &CountOptions);

        // Go through each lobby
        for (uint32_t i = 0; i < LobbyCount; i++)
        {
            EOS_LobbySearch_CopySearchResultByIndexOptions CopyOptions = {};
            CopyOptions.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
            CopyOptions.LobbyIndex = i;

            EOS_HLobbyDetails LobbyDetails = nullptr;
            EOS_EResult CopyResult = EOS_LobbySearch_CopySearchResultByIndex(LobbyManager->CurrentLobbySearchHandle, &CopyOptions, &LobbyDetails);

            if (CopyResult == EOS_EResult::EOS_Success && LobbyDetails)
            {
                EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
                InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

                EOS_LobbyDetails_Info* LobbyInfo = nullptr;
                if (EOS_LobbyDetails_CopyInfo(LobbyDetails, &InfoOptions, &LobbyInfo) == EOS_EResult::EOS_Success)
                {
                    // Create lobby info struct
                    FLobbyInfo LobbyInfoStruct;
                    LobbyInfoStruct.LobbyId = UTF8_TO_TCHAR(LobbyInfo->LobbyId);
                    LobbyInfoStruct.MaxPlayers = LobbyInfo->MaxMembers;
                    LobbyInfoStruct.CurrentPlayers = LobbyInfo->MaxMembers - LobbyInfo->AvailableSlots;
                    LobbyInfoStruct.BucketId = UTF8_TO_TCHAR(LobbyInfo->BucketId ? LobbyInfo->BucketId : "");

                    // Convert owner ProductUserId to string
                    if (LobbyInfo->LobbyOwnerUserId)
                    {
                        char OwnerIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
                        int32_t BufferSize = sizeof(OwnerIdStr);
                        if (EOS_ProductUserId_ToString(LobbyInfo->LobbyOwnerUserId, OwnerIdStr, &BufferSize) == EOS_EResult::EOS_Success)
                        {
                            LobbyInfoStruct.OwnerUserId = UTF8_TO_TCHAR(OwnerIdStr);
                        }
                        else
                        {
                            LobbyInfoStruct.OwnerUserId = TEXT("Unknown");
                        }
                    }
                    else
                    {
                        LobbyInfoStruct.OwnerUserId = TEXT("No Owner");
                    }

                    LobbyInfoStruct.LobbyName = TEXT("Unnamed Lobby"); // Default
                    LobbyManager->FoundLobbies.Add(LobbyInfoStruct);

                    EM_LOG_INFO(TEXT("Found lobby: %s (Owner: %s, Players: %d/%d)"),
                        *LobbyInfoStruct.LobbyId,
                        *LobbyInfoStruct.OwnerUserId,
                        LobbyInfoStruct.CurrentPlayers,
                        LobbyInfoStruct.MaxPlayers);

                    EOS_LobbyDetails_Info_Release(LobbyInfo);
                }

                EOS_LobbyDetails_Release(LobbyDetails);
            }
        }

        if (LobbyCount == 0)
        {
            EM_LOG_WARNING(TEXT("No lobbies found matching the search criteria"));
		}

        LobbyManager->OnLobbiesFound.Broadcast(LobbyManager->FoundLobbies);
    }

    // Clean up the search handle
    if (LobbyManager->CurrentLobbySearchHandle)
    {
        EOS_LobbySearch_Release(LobbyManager->CurrentLobbySearchHandle);
        LobbyManager->CurrentLobbySearchHandle = nullptr;
    }

    EM_LOG_INFO(TEXT("Lobby search was completed"));
}

void UEOSLobbyManager::OnFindLobbyToJoinComplete(const EOS_LobbySearch_FindCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
		EM_LOG_ERROR(TEXT("LobbyManager is no longer valid in OnFindLobbyToJoinComplete"));
        return; // Don't access destroyed objects
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        // Get the lobby details handle from search results
        EOS_LobbySearch_CopySearchResultByIndexOptions CopyOptions = {};
        CopyOptions.ApiVersion = EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST;
        CopyOptions.LobbyIndex = 0; // First (and only) result

        EOS_HLobbyDetails LobbyDetails = nullptr;
        EOS_EResult CopyResult = EOS_LobbySearch_CopySearchResultByIndex(LobbyManager->CurrentLobbySearchHandle, &CopyOptions, &LobbyDetails);

        if (CopyResult == EOS_EResult::EOS_Success && LobbyDetails)
        {
            // Now we can join with the proper lobby details handle
            EOS_Lobby_JoinLobbyOptions JoinOptions = {};
            JoinOptions.ApiVersion = EOS_LOBBY_JOINLOBBY_API_LATEST;
            JoinOptions.LocalUserId = LobbyManager->LocalUserId;
            JoinOptions.LobbyDetailsHandle = LobbyDetails;

            EM_LOG_INFO(TEXT("Joining lobby with valid details handle"));
            EOS_Lobby_JoinLobby(LobbyManager->LobbyHandle, &JoinOptions, LobbyManager, OnJoinLobbyComplete);
        }
        else
        {
            EM_LOG_ERROR(TEXT("Failed to get lobby details for joining"));
        }
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to find lobby to join"));
    }

    // Clean up search handle
    if (LobbyManager->CurrentLobbySearchHandle)
    {
        EOS_LobbySearch_Release(LobbyManager->CurrentLobbySearchHandle);
        LobbyManager->CurrentLobbySearchHandle = nullptr;
    }
}

// Check if still in lobby, with EOS (not just by comparing bool)
bool UEOSLobbyManager::CheckWithEOSIsInLobby()
{
    if (CurrentLobbyId.IsEmpty())
    {
        EM_LOG_INFO(TEXT("CheckingIsInLobby: CurrentLobbyId == null"));
        return false;
    }

    // Try to get current lobby details to verify we are still a member
    EOS_Lobby_CopyLobbyDetailsHandleOptions CopyOptions = {};
    CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    CopyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    CopyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyDetails LobbyDetails = nullptr;
    EOS_EResult Result = EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle, &CopyOptions, &LobbyDetails);

    if (Result == EOS_EResult::EOS_Success && LobbyDetails)
    {
        EOS_LobbyDetails_Release(LobbyDetails);
        bIsInLobby = true;
        return true; // Successfully got lobby details, so we're still in it
    }

    // Failed to get lobby details - we might have been kicked or lobby was destroyed
    bIsInLobby = false;
    CurrentLobbyId.Empty();
    return false;
}

void UEOSLobbyManager::UpdateLobbyMembersData()
{

    if (!bIsInLobby || CurrentLobbyId.IsEmpty() || !LobbyHandle || !LocalUserId)
    {
        EM_LOG_WARNING(TEXT("Not in a lobby or invalid handles"));
    }

    EOS_Lobby_CopyLobbyDetailsHandleOptions CopyOptions = {};
    CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    CopyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    CopyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyDetails LobbyDetails = nullptr;
    EOS_EResult Result = EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle, &CopyOptions, &LobbyDetails);

    if (Result == EOS_EResult::EOS_Success && LobbyDetails)
    {
        // Get lobby info to find the owner
        EOS_LobbyDetails_CopyInfoOptions InfoOptions = {};
        InfoOptions.ApiVersion = EOS_LOBBYDETAILS_COPYINFO_API_LATEST;

        EOS_LobbyDetails_Info* LobbyInfo = nullptr;
        if (EOS_LobbyDetails_CopyInfo(LobbyDetails, &InfoOptions, &LobbyInfo) == EOS_EResult::EOS_Success)
        {
            EOS_ProductUserId LobbyOwnerId = LobbyInfo->LobbyOwnerUserId;

            // Get member count
            EOS_LobbyDetails_GetMemberCountOptions MemberCountOptions = {};
            MemberCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST;

            uint32_t MemberCount = EOS_LobbyDetails_GetMemberCount(LobbyDetails, &MemberCountOptions);
            EM_LOG_INFO(TEXT("Lobby has %d members"), MemberCount);

            // Get each member's info
            for (uint32_t i = 0; i < MemberCount; i++)
            {
                EOS_LobbyDetails_GetMemberByIndexOptions MemberOptions = {};
                MemberOptions.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST;
                MemberOptions.MemberIndex = i;

                EOS_ProductUserId MemberUserId = EOS_LobbyDetails_GetMemberByIndex(LobbyDetails, &MemberOptions);

                if (MemberUserId)
                {
                    FLobbyMemberInfo& MemberInfoInMap = LobbyMembers.FindOrAdd(UserIdToString(MemberUserId));

                    // Convert ProductUserId to string
                	MemberInfoInMap.UserId = MemberUserId;
                    // Check if this member is the lobby owner
                    MemberInfoInMap.bIsLobbyOwner = (MemberUserId == LobbyOwnerId);

                    // Read member attributes to get ready status
                    EOS_LobbyDetails_GetMemberAttributeCountOptions AttrCountOptions = {};
                    AttrCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETMEMBERATTRIBUTECOUNT_API_LATEST;
                    AttrCountOptions.TargetUserId = MemberUserId;

                    // Check each attribute for ready status
                    uint32_t AttributeCount = EOS_LobbyDetails_GetMemberAttributeCount(LobbyDetails, &AttrCountOptions);
                    for (uint32_t AttrIndex = 0; AttrIndex < AttributeCount; AttrIndex++)
                    {
                        EOS_LobbyDetails_CopyMemberAttributeByIndexOptions AttrOptions = {};
                        AttrOptions.ApiVersion = EOS_LOBBYDETAILS_COPYMEMBERATTRIBUTEBYINDEX_API_LATEST;
                        AttrOptions.TargetUserId = MemberUserId;
                        AttrOptions.AttrIndex = AttrIndex;

                        EOS_Lobby_Attribute* Attribute = nullptr; // Single pointer
                        EOS_EResult AttrResult = EOS_LobbyDetails_CopyMemberAttributeByIndex(LobbyDetails, &AttrOptions, &Attribute);

                        if (AttrResult == EOS_EResult::EOS_Success && Attribute)
                        {
                            // Access the attribute data correctly
                            FString AttributeKey = FString(UTF8_TO_TCHAR(Attribute->Data->Key));
                            if (AttributeKey == TEXT("ready"))
                            {
                                MemberInfoInMap.bIsReady = (Attribute->Data->Value.AsBool == EOS_TRUE);
                                EM_LOG_INFO(TEXT("Member %s ready status: %s"),
                                    *UserIdToString(MemberUserId),
                                    MemberInfoInMap.bIsReady ? TEXT("Ready") : TEXT("Not Ready"));
                            }

                            // Release the attribute as per documentation
                            EOS_Lobby_Attribute_Release(Attribute);
                        }
                    }
                }
            }

            EOS_LobbyDetails_Info_Release(LobbyInfo);
        }

        EOS_LobbyDetails_Release(LobbyDetails);

        for (const TPair<FString, FLobbyMemberInfo>& Elem : LobbyMembers)
        {
            const FString& ProductUserIdString = Elem.Key;
            const FLobbyMemberInfo& MemberInfo = Elem.Value;

            UE_LOG(LogTemp, Log, TEXT("Member [%s]: DisplayName=%s, Owner=%s"),
                *ProductUserIdString,
                *MemberInfo.DisplayName,
                MemberInfo.bIsLobbyOwner ? TEXT("Yes") : TEXT("No"));

            if (MemberInfo.DisplayName.IsEmpty())
            {
                GetUserDisplayName(MemberInfo.UserId);
            }
        }

    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to get lobby details for member list"));
    }
}

void UEOSLobbyManager::GetUserDisplayName(EOS_ProductUserId UserId)
{

    // Query the mapping from ProductUserId to external accounts
    EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);

    EOS_Connect_QueryProductUserIdMappingsOptions QueryOptions = {};
    QueryOptions.ApiVersion = EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST;
    QueryOptions.LocalUserId = LocalUserId; // Your ProductUserId
    QueryOptions.ProductUserIds = &UserId;
    QueryOptions.ProductUserIdCount = 1;
    
    FUserQueryContext* Context = new FUserQueryContext;
    Context->LobbyManager = this;
    Context->TargetUserId = UserId;

    EM_LOG_INFO(TEXT("Querying ProductUserId mapping for display name"));
    EOS_Connect_QueryProductUserIdMappings(ConnectHandle, &QueryOptions, Context, OnQueryProductUserIdMappingsComplete);
}

FString UEOSLobbyManager::GetLobbySessionAddress()
{
    if (!bIsInLobby || CurrentLobbyId.IsEmpty() || !LobbyHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot get session address - not in lobby"));
        return FString();
    }

    EOS_Lobby_CopyLobbyDetailsHandleOptions CopyOptions = {};
    CopyOptions.ApiVersion = EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST;
    CopyOptions.LocalUserId = LocalUserId;

    FTCHARToUTF8 LobbyIdConverter(*CurrentLobbyId);
    CopyOptions.LobbyId = LobbyIdConverter.Get();

    EOS_HLobbyDetails LobbyDetails = nullptr;
    EOS_EResult Result = EOS_Lobby_CopyLobbyDetailsHandle(LobbyHandle, &CopyOptions, &LobbyDetails);

    FString SessionAddress;
    if (Result == EOS_EResult::EOS_Success && LobbyDetails)
    {
        // Get attribute count
        EOS_LobbyDetails_GetAttributeCountOptions AttrCountOptions = {};
        AttrCountOptions.ApiVersion = EOS_LOBBYDETAILS_GETATTRIBUTECOUNT_API_LATEST;

        uint32_t AttributeCount = EOS_LobbyDetails_GetAttributeCount(LobbyDetails, &AttrCountOptions);

        // Search for session_address attribute
        for (uint32_t i = 0; i < AttributeCount; i++)
        {
            EOS_LobbyDetails_CopyAttributeByIndexOptions AttrOptions = {};
            AttrOptions.ApiVersion = EOS_LOBBYDETAILS_COPYATTRIBUTEBYINDEX_API_LATEST;
            AttrOptions.AttrIndex = i;

            EOS_Lobby_Attribute* Attribute = nullptr;
            EOS_EResult AttrResult = EOS_LobbyDetails_CopyAttributeByIndex(LobbyDetails, &AttrOptions, &Attribute);

            if (AttrResult == EOS_EResult::EOS_Success && Attribute)
            {
                FString AttributeKey = UTF8_TO_TCHAR(Attribute->Data->Key);

                if (AttributeKey == TEXT("session_address"))
                {
                    SessionAddress = UTF8_TO_TCHAR(Attribute->Data->Value.AsUtf8);
                    EM_LOG_INFO(TEXT("Found session address in lobby: %s"), *SessionAddress);

                    EOS_Lobby_Attribute_Release(Attribute);
                    break;
                }

                EOS_Lobby_Attribute_Release(Attribute);
            }
        }

        EOS_LobbyDetails_Release(LobbyDetails);
    }

    return SessionAddress;
}

void UEOSLobbyManager::OnQueryProductUserIdMappingsComplete(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data)
{
	// Heap memory allocation (because functions can be called several times at once, to prevent data override we use heap memory allocation)
    FUserQueryContext* Context = static_cast<FUserQueryContext*>(Data->ClientData);
    UEOSLobbyManager* LobbyManager = Context->LobbyManager;

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(LobbyManager->PlatformHandle);
        EOS_Connect_GetProductUserIdMappingOptions GetMappingOptions = {};
        GetMappingOptions.ApiVersion = EOS_CONNECT_GETPRODUCTUSERIDMAPPING_API_LATEST;
        GetMappingOptions.LocalUserId = LobbyManager->LocalUserId;
        GetMappingOptions.TargetProductUserId = Context->TargetUserId;
        GetMappingOptions.AccountIdType = EOS_EExternalAccountType::EOS_EAT_EPIC;

        char ExternalAccountId[EOS_CONNECT_EXTERNAL_ACCOUNT_ID_MAX_LENGTH + 1];
        int32_t BufferSize = sizeof(ExternalAccountId);

        EOS_EResult MappingResult = EOS_Connect_GetProductUserIdMapping(ConnectHandle, &GetMappingOptions, ExternalAccountId, &BufferSize);

        if (MappingResult == EOS_EResult::EOS_Success)
        {
            // Now we have the EpicAccountId, query user info
            EOS_EpicAccountId TargetEpicAccountId = EOS_EpicAccountId_FromString(ExternalAccountId);

            EOS_HUserInfo UserInfoHandle = EOS_Platform_GetUserInfoInterface(LobbyManager->PlatformHandle);

            EOS_UserInfo_QueryUserInfoOptions UserInfoQueryOptions = {};
            UserInfoQueryOptions.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST;
            UserInfoQueryOptions.LocalUserId = LobbyManager->LocalEpicAccountId;
            UserInfoQueryOptions.TargetUserId = TargetEpicAccountId;

            EOS_UserInfo_QueryUserInfo(UserInfoHandle, &UserInfoQueryOptions, Context, OnQueryUserInfoComplete);
        }
        else
        {
            EM_LOG_INFO(TEXT("No Epic account mapping found for ProductUserId"));
            delete Context;
        }
    }
    else
    {
        delete Context;
    }
}

void UEOSLobbyManager::OnQueryUserInfoComplete(const EOS_UserInfo_QueryUserInfoCallbackInfo* Data)
{
    FUserQueryContext* Context = static_cast<FUserQueryContext*>(Data->ClientData);
    UEOSLobbyManager* LobbyManager = Context->LobbyManager;
	if (!IsValid(Context->LobbyManager)) //to avoid crashes on exit
    {
        delete Context;
        return;
    }

    // Debug - Show the EpicAccountId being queried
    char EpicAccountIdStr[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
    int32_t EpicBufferSize = sizeof(EpicAccountIdStr);
    EOS_EpicAccountId_ToString(Data->TargetUserId, EpicAccountIdStr, &EpicBufferSize);
    EM_LOG_INFO(TEXT("Query completed for EpicAccountId: %s"), UTF8_TO_TCHAR(EpicAccountIdStr));

    // Debug - Show your local EpicAccountId
    char LocalEpicIdStr[EOS_EPICACCOUNTID_MAX_LENGTH + 1];
    int32_t LocalBufferSize = sizeof(LocalEpicIdStr);
    EOS_EpicAccountId_ToString(LobbyManager->LocalEpicAccountId, LocalEpicIdStr, &LocalBufferSize);
    EM_LOG_INFO(TEXT("Local EpicAccountId: %s"), UTF8_TO_TCHAR(LocalEpicIdStr));

    // Log user ID 
    char UserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
    int32 BufferSize = sizeof(UserIdStr);
    EOS_ProductUserId_ToString(LobbyManager->LocalUserId, UserIdStr, &BufferSize);
    EM_LOG_INFO(TEXT("ProductUserId: %s"), UTF8_TO_TCHAR(UserIdStr));

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        EOS_HUserInfo UserInfoHandle = EOS_Platform_GetUserInfoInterface(LobbyManager->PlatformHandle);

        EOS_UserInfo_CopyUserInfoOptions CopyOptions = {};
        CopyOptions.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST;
        CopyOptions.LocalUserId = LobbyManager->LocalEpicAccountId;
        CopyOptions.TargetUserId = Data->TargetUserId;

        EOS_UserInfo* UserInfo = nullptr;
        EOS_EResult CopyResult = EOS_UserInfo_CopyUserInfo(UserInfoHandle, &CopyOptions, &UserInfo);

        if (CopyResult == EOS_EResult::EOS_Success && UserInfo)
        {
            FString DisplayName = UTF8_TO_TCHAR(UserInfo->DisplayName);

            // Use the ProductUserId from context to update the correct map entry
            FString ProductUserIdString = LobbyManager->UserIdToString(Context->TargetUserId);

            // Update the map entry with the display name
            if (FLobbyMemberInfo* MemberInfo = LobbyManager->LobbyMembers.Find(ProductUserIdString))
            {
                MemberInfo->DisplayName = DisplayName;
                EM_LOG_INFO(TEXT("Updated display name: %s for ProductUserId: %s"),
                    *DisplayName, *ProductUserIdString);
                LobbyManager->OnLobbyMemberEpicGameNicknameGot.Broadcast();
            }

            if (Context->TargetUserId == LobbyManager->LocalUserId)
            {
                LobbyManager->LocalPlayerDisplayName = DisplayName;
                EM_LOG_INFO(TEXT("Saved local player name: %s"), *DisplayName);
            }

            EOS_UserInfo_Release(UserInfo);
        }
        else
        {
            EM_LOG_ERROR(TEXT("Failed to copy user info: %s"),
                UTF8_TO_TCHAR(EOS_EResult_ToString(CopyResult)));
        }
    }
    else
    {
        EM_LOG_ERROR(TEXT("Query user info failed: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }


    delete Context;
}

void UEOSLobbyManager::OnUpdateReadyStatusLobbyComplete(const EOS_Lobby_UpdateLobbyCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager)) return;
   

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        EM_LOG_INFO(TEXT("Ready status updated successfully"));
    }
    else if (Data->ResultCode == EOS_EResult::EOS_TimedOut)
    {
        EM_LOG_WARNING(TEXT("Ready status update timed out - lobby service may be slow"));
        // TODO: add retry logic later?
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to update ready status: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

FString UEOSLobbyManager::UserIdToString(EOS_ProductUserId UserId) const
{
    if (!UserId)
    {
        return FString();
    }

    char UserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
    int32 BufferSize = sizeof(UserIdStr);
    if (EOS_ProductUserId_ToString(UserId, UserIdStr, &BufferSize) == EOS_EResult::EOS_Success)
    {
        return FString(UTF8_TO_TCHAR(UserIdStr));
    }

    return FString();
}

void UEOSLobbyManager::OnSetSessionAddressComplete(const EOS_Lobby_UpdateLobbyCallbackInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return;
    }

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        EM_LOG_INFO(TEXT("Session address updated in lobby successfully"));

        // Broadcast to all members
        FString SessionAddress = LobbyManager->GetLobbySessionAddress();
        LobbyManager->OnSessionAddressUpdated.Broadcast(SessionAddress);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to update session address: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void EOS_CALL UEOSLobbyManager::OnIncomingConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* Data)
{
    UEOSLobbyManager* LobbyManager = static_cast<UEOSLobbyManager*>(Data->ClientData);
    if (!IsValid(LobbyManager))
    {
        return;
    }
    else
    {
        EM_LOG_INFO(TEXT("LobbyManager is valid in OnIncomingConnectionRequest"));
    }

    EM_LOG_INFO(TEXT(" P2P connection request received from remote user"));

    // Accept the connection
    EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(LobbyManager->PlatformHandle);
    if (!P2PHandle)
    {
        EM_LOG_ERROR(TEXT("Failed to get P2P interface"));
        return;
    }

    EOS_P2P_AcceptConnectionOptions AcceptOptions = {};
    AcceptOptions.ApiVersion = EOS_P2P_ACCEPTCONNECTION_API_LATEST;
    AcceptOptions.LocalUserId = Data->LocalUserId;
    AcceptOptions.RemoteUserId = Data->RemoteUserId;
    AcceptOptions.SocketId = Data->SocketId;

    EOS_EResult Result = EOS_P2P_AcceptConnection(P2PHandle, &AcceptOptions);

    if (Result == EOS_EResult::EOS_Success)
    {
        EM_LOG_INFO(TEXT("Accepted P2P connection"));
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to accept P2P connection: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Result)));
    }
}


void UEOSLobbyManager::TickP2PMessages()
{
    if (!bIsInLobby) return;

    EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
    if (!P2PHandle) return;

    // Check for incoming packets
    EOS_P2P_GetNextReceivedPacketSizeOptions SizeOptions = {};
    SizeOptions.ApiVersion = EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST;
    SizeOptions.LocalUserId = LocalUserId;
    SizeOptions.RequestedChannel = nullptr;  // All channels

    uint32_t PacketSize = 0;
    EOS_EResult SizeResult = EOS_P2P_GetNextReceivedPacketSize(P2PHandle, &SizeOptions, &PacketSize);

    if (SizeResult == EOS_EResult::EOS_Success && PacketSize > 0)
    {
        // Allocate buffer for packet
        TArray<uint8> PacketData;
        PacketData.SetNum(PacketSize);

        // Receive the packet
        EOS_P2P_ReceivePacketOptions ReceiveOptions = {};
        ReceiveOptions.ApiVersion = EOS_P2P_RECEIVEPACKET_API_LATEST;
        ReceiveOptions.LocalUserId = LocalUserId;
        ReceiveOptions.MaxDataSizeBytes = PacketSize;
        ReceiveOptions.RequestedChannel = nullptr;

        EOS_ProductUserId SenderUserId;
        EOS_P2P_SocketId SocketId;
        uint8_t Channel;
        uint32_t BytesWritten;

        EOS_EResult ReceiveResult = EOS_P2P_ReceivePacket(
            P2PHandle,
            &ReceiveOptions,
            &SenderUserId,
            &SocketId,
            &Channel,
            PacketData.GetData(),
            &BytesWritten
        );

        if (ReceiveResult == EOS_EResult::EOS_Success)
        {
            // Convert data to FString
            FString Message = UTF8_TO_TCHAR((char*)PacketData.GetData());
            FString SenderName = UserIdToString(SenderUserId);

            // Get display name from cache
            if (FLobbyMemberInfo* MemberInfo = LobbyMembers.Find(SenderName))
            {
                SenderName = MemberInfo->DisplayName;
            }

            EM_LOG_INFO(TEXT("Chat from %s: %s"), *SenderName, *Message);

            // Broadcast to Blueprint!
            OnChatMessageReceived.Broadcast(SenderName, Message);
        }
    }
}

void UEOSLobbyManager::SendChatMessage(const FString& Message)
{
    if (!bIsInLobby || !PlatformHandle || !LocalUserId)
    {
        EM_LOG_ERROR(TEXT("Cannot send chat - not in lobby or invalid handles"));
        return;
    }

    if (Message.IsEmpty())
    {
        EM_LOG_WARNING(TEXT("Cannot send empty message"));
        return;
    }

    EOS_HP2P P2PHandle = EOS_Platform_GetP2PInterface(PlatformHandle);
    if (!P2PHandle)
    {
        EM_LOG_ERROR(TEXT("Failed to get P2P interface"));
        return;
    }

    if (ChatSocketId.SocketName[0] == '\0')
    {
        EM_LOG_ERROR(TEXT("ChatSocketId is EMPTY!"));
        return;
    }

    FTCHARToUTF8 MessageConverter(*Message);

    // Send to each lobby member
    int32 SentCount = 0;
    for (const auto& Member : LobbyMembers)
    {
        // Skip sending to yourself
        if (Member.Value.UserId == LocalUserId)
        {
            continue;
        }

        EOS_P2P_SendPacketOptions SendOptions = {};
        SendOptions.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
        SendOptions.LocalUserId = LocalUserId;
        SendOptions.RemoteUserId = Member.Value.UserId;
        SendOptions.SocketId = &ChatSocketId;
        SendOptions.Channel = 0;
        SendOptions.DataLengthBytes = MessageConverter.Length() + 1; // +1 for null terminator
        SendOptions.Data = (uint8_t*)MessageConverter.Get();
        SendOptions.bAllowDelayedDelivery = EOS_TRUE;
        SendOptions.Reliability = EOS_EPacketReliability::EOS_PR_ReliableOrdered;

        EOS_EResult Result = EOS_P2P_SendPacket(P2PHandle, &SendOptions);

        if (Result == EOS_EResult::EOS_Success)
        {
            SentCount++;
            EM_LOG_INFO(TEXT("Sent to %s"), *Member.Value.DisplayName);
        }
        else
        {
            EM_LOG_WARNING(TEXT("Failed to send message to %s: %s"),
                *Member.Value.DisplayName,
                UTF8_TO_TCHAR(EOS_EResult_ToString(Result)));
        }
    }

    EM_LOG_INFO(TEXT("Chat message sent to %d members: %s"), SentCount, *Message);
}