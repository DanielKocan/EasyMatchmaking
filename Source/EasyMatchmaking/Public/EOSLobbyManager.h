#pragma once

#include <eos_types.h>
#include <eos_userinfo_types.h>
#include <eos_connect_types.h>

#include <eos_p2p.h>
#include <eos_p2p_types.h>

#include "CoreMinimal.h"
#include "eos_lobby.h"
#include "EOSLobbyManager.generated.h"

class UEOSManager;

USTRUCT(BlueprintType)
struct FLobbyMemberInfo
{
    GENERATED_BODY()

    EOS_ProductUserId UserId;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby Member")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby Member")
    bool bIsLobbyOwner;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby Member")
    bool bIsReady = false;

    EOS_EpicAccountId EpicAccountId;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby Member")
    FString EpicAccountIdString;
};

USTRUCT(BlueprintType)
struct FLobbySettings
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Lobby")
    FString LobbyName = TEXT("DefaultLobby");

    UPROPERTY(BlueprintReadWrite, Category = "Lobby")
    int32 MaxPlayers = 4;

    UPROPERTY(BlueprintReadWrite, Category = "Lobby")
    bool bIsPrivate = false;

    UPROPERTY(BlueprintReadWrite, Category = "Lobby")
    FString BucketId = TEXT("DefaultBucket");
};

USTRUCT(BlueprintType)
struct FLobbyInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Lobby")
    FString LobbyId;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby")
    FString LobbyName;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby")
    FString OwnerUserId;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby")
    int32 CurrentPlayers;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby")
    int32 MaxPlayers;

    UPROPERTY(BlueprintReadOnly, Category = "Lobby")
    FString BucketId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyCreated, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbiesFound, const TArray<FLobbyInfo>&, FoundLobbies);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyJoined, const FString&, LobbyId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllPlayersReady);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyMemberEpicGameNicknameGot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyMembersChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionAddressUpdated, const FString&, SessionAddress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChatMessageReceived, FString, PlayerName, FString, Message);

UCLASS(BlueprintType)
// Don't forget to call initialize with proper settings
class EASYMATCHMAKING_API UEOSLobbyManager : public UObject
{
    GENERATED_BODY()
public:
    // --- Initialize with personal EOS mmanager and seperate destructor ---
    void Init(void* InPlatformHandle, void* InLobbyHandle, void* InLocalUserId, UEOSManager* InManager);

    virtual void BeginDestroy() override;

    void RegisterLobbyNotifications();
    void UnregisterLobbyNotifications();

    // --- Lobby operations ---
    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void CreateLobby(const FLobbySettings& Settings);

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void JoinLobby(const FString& LobbyId);

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void LeaveLobby();

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void DestroyLobby();

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void SearchLobbies(const FString& BucketId = TEXT("DefaultBucket"));

    // --- Other usefull functions ---
    UFUNCTION(BlueprintPure, Category = "EasyMatchmaking")
    bool CheckWithEOSIsInLobby();

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
	// Function to refresh the list of current lobby members and their info
    void UpdateLobbyMembersData();

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    const FLobbyInfo UpdateLobbyInfoData();

    // --- Chat opperations ---

    void RegisterP2PNotifications();
	void RegisterTimerForTickP2PMessages(UEOSLobbyManager* LobbyManager);
	void UnregisterTimerForTickP2PMessages(UEOSLobbyManager* LobbyManager);
    void TickP2PMessages();

    UFUNCTION(BlueprintCallable, Category = "Chat")
    void SendChatMessage(const FString& Message);

    UPROPERTY(BlueprintAssignable, Category = "Chat Events")
    FOnChatMessageReceived OnChatMessageReceived;

    // --- Callback blueprint events for UI ---
    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbyCreated OnLobbyCreated;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbiesFound OnLobbiesFound;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbyJoined OnLobbyJoined;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbyLeft OnLobbyLeft;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbyError OnLobbyError;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnSessionAddressUpdated OnSessionAddressUpdated;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnAllPlayersReady OnAllPlayersReady;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbyMembersChanged OnLobbyMembersChanged;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnLobbyMemberEpicGameNicknameGot OnLobbyMemberEpicGameNicknameGot;

    // --- Getters ---
    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    TArray<FString> GetCurrentLobbyPlayers() const { return CurrentPlayers; }

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    FLobbySettings GetCurrentLobbySettings() const { return CurrentSettings; }

    UFUNCTION(BlueprintPure, Category = "EasyMatchmaking")
    FString GetCurrentLobbyId() const { return CurrentLobbyId; }

    // Get the session address from the lobby
    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    FString GetLobbySessionAddress();

    UFUNCTION(BlueprintPure, Category = "EasyMatchmaking")
    const TMap<FString, FLobbyMemberInfo>& GetLobbyMembers() const { return LobbyMembers; }

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    FString GetLocalPlayerDisplayName() const { return LocalPlayerDisplayName; }

    UFUNCTION(BlueprintPure, Category = "EasyMatchmaking")
    bool IsInLobby() const { return bIsInLobby; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EasyMatchmaking")
    bool IsLobbyOwner() const;

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    bool AreAllPlayersReady() const;

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    int32 GetReadyPlayerCount() const;

    // --- Setters ---
    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void SetPlayerReady(bool bReady);

    // Set the session address in the lobby (owner only)
    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void SetLobbySessionAddress(const FString& SessionAddress);

private:
    EOS_HPlatform PlatformHandle = nullptr;
    EOS_HLobby LobbyHandle = nullptr;
    EOS_ProductUserId LocalUserId = nullptr;
    EOS_HLobbySearch CurrentLobbySearchHandle = nullptr;
    EOS_EpicAccountId AuthenticatedEpicAccountId = nullptr;
    UEOSManager* EOSManager;

    // Notification IDs (to unregister later)
    EOS_NotificationId LobbyUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId LobbyMemberUpdateNotificationId = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId LobbyMemberStatusReceivedNotificationId = EOS_INVALID_NOTIFICATIONID;

    // Notification callbacks
    static void EOS_CALL OnLobbyUpdateReceived(const EOS_Lobby_LobbyUpdateReceivedCallbackInfo* Data);
    static void EOS_CALL OnLobbyMemberUpdateReceived(const EOS_Lobby_LobbyMemberUpdateReceivedCallbackInfo* Data);
    static void EOS_CALL OnLobbyMemberStatusReceived(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* Data);

    // --- Callbacks ---
    static void OnCreateLobbyComplete(const EOS_Lobby_CreateLobbyCallbackInfo* Data);
    static void OnJoinLobbyComplete(const EOS_Lobby_JoinLobbyCallbackInfo* Data);
    static void OnLeaveLobbyComplete(const EOS_Lobby_LeaveLobbyCallbackInfo* Data);
    static void OnDestroyLobbyComplete(const EOS_Lobby_DestroyLobbyCallbackInfo* Data);
    static void OnFindLobbiesComplete(const EOS_LobbySearch_FindCallbackInfo* Data);
    static void OnFindLobbyToJoinComplete(const EOS_LobbySearch_FindCallbackInfo* Data);
    static void OnQueryUserInfoComplete(const EOS_UserInfo_QueryUserInfoCallbackInfo* Data);
    static void OnUpdateReadyStatusLobbyComplete(const EOS_Lobby_UpdateLobbyCallbackInfo* Data);
    static void OnQueryProductUserIdMappingsComplete(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data);
    static void OnSetSessionAddressComplete(const EOS_Lobby_UpdateLobbyCallbackInfo* Data);
    static void EOS_CALL OnIncomingConnectionRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* Data);
    static void EOS_CALL OnRemoteConnectionClosed(const EOS_P2P_OnRemoteConnectionClosedInfo* Data);

    // --- Helper Functions ---
    void GetUserDisplayName(EOS_ProductUserId UserId);
    FString UserIdToString(EOS_ProductUserId UserId) const;

    // --- Holding data ---
    bool bIsInLobby = false;
    FString CurrentLobbyId;
    EOS_EpicAccountId LocalEpicAccountId;
    FString LocalPlayerDisplayName;
    FLobbySettings CurrentSettings;
    TArray<FString> CurrentPlayers; // Track current lobby members
    TArray<FLobbyInfo> FoundLobbies;
    TMap<FString, FLobbyMemberInfo> LobbyMembers;
    FString LastKnownSessionAddress;

    // For chat
    EOS_NotificationId P2PConnectionRequestNotificationId = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId P2PConnectionClosedNotificationId = EOS_INVALID_NOTIFICATIONID;
    FTimerHandle ChatTickTimer;
    EOS_P2P_SocketId ChatSocketId;

    //Structure used to query info from lobby members (for example to display name)
    struct FUserQueryContext 
    {
        UEOSLobbyManager* LobbyManager;
        EOS_ProductUserId TargetUserId;
    };
};
