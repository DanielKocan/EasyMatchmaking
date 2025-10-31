#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "eos_sessions.h"
#include <eos_types.h>
#include <eos_sessions_types.h>
#include "EOSSessionManager.generated.h"

//Forwad declaration
class UEOSManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionsFound, const TArray<FString>&, SessionIds);

UCLASS()
class EASYMATCHMAKING_API UEOSSessionManager : public UObject
{
    GENERATED_BODY()

public:
    void Init(void* InPlatformHandle, void* InSessionHandle, void* InLocalUserId, UEOSManager* InEOSManager);
    virtual void BeginDestroy() override;

	// Call to create a dedicatd server
    UFUNCTION(BlueprintCallable, Category = "Matchmaking")
    void InitServer();

    // Server creates session
    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void CreateSession(const FString& SessionName, int32 MaxPlayers);

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void SearchSessions(const FString& BucketId = TEXT("GameSession"));

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void DestroySession();

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    void JoinSessionById(const FString& SessionId);

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnSessionsFound OnSessionsFound;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EasyMatchmaking")
    FString GetCurrentSessionId() const
    {
        return PendingJoinSessionId.IsEmpty() ? CurrentSessionId : PendingJoinSessionId;
    }

private:
    FString CurrentSessionId;

    EOS_HPlatform PlatformHandle = nullptr;
    EOS_HSessions SessionHandle = nullptr;
    EOS_ProductUserId LocalUserId = nullptr;
    EOS_HSessionSearch CurrentSessionSearchHandle = nullptr;
    // session details for joining, so we dont have to call callbacks again
    TMap<FString, EOS_HSessionDetails> CachedSessionDetails;

    FString PendingJoinSessionId;

    // reference to EOSManager
    UPROPERTY()
    TObjectPtr<UEOSManager> EOSManager = nullptr; 

    // Helper functions
    FString GetServerAddressFromSessionDetails(EOS_HSessionDetails SessionDetails);

    static void OnCreateSessionComplete(const EOS_Sessions_UpdateSessionCallbackInfo* Data);
    static void OnJoinSessionComplete(const EOS_Sessions_JoinSessionCallbackInfo* Data);
	static void OnFindSessionComplete(const EOS_SessionSearch_FindCallbackInfo* Data);
    static void OnSessionSearchComplete(const EOS_SessionSearch_FindCallbackInfo* Data);
    static void OnDestroySessionComplete(const EOS_Sessions_DestroySessionCallbackInfo* Data);
};
