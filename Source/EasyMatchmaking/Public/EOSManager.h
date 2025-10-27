#pragma once

#include <eos_auth_types.h>
#include <eos_connect_types.h>

#include "CoreMinimal.h"
#include "EOSLobbyManager.h"
#include "Session/EOSSessionManager.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUserAuthenticated);

UCLASS()
// It is GameInstanceSubsytem, so it loads when game starts. It
// coordinates all Easy Matchmaking functionality (with EOS)
class EASYMATCHMAKING_API UEOSManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category = "EasyMatchmaking")
    FOnUserAuthenticated OnUserAuthenticated;

    EOS_EpicAccountId GetCurrentEpicAccountId() const { return EpicAccountId; }

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    UEOSLobbyManager* GetLobbyManager() const { return LobbyManager; }

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    UEOSSessionManager* GetSessionManager() const { return SessionManager; }

    UFUNCTION(BlueprintPure, Category = "EasyMatchmaking")
    FString GetCurrentUserId() const;

    UFUNCTION(BlueprintCallable, Category = "EasyMatchmaking")
    static bool TestEOSInitialization();

private:

    // Authentication functions
    void InitializeManagers();
    void AuthenticateUser();

    // Callback functionsq
    static void OnAuthLoginComplete(const EOS_Auth_LoginCallbackInfo* Data);
    static void OnConnectLoginFromEpicComplete(const EOS_Connect_LoginCallbackInfo* Data);
    static void OnCreateUserComplete(const EOS_Connect_CreateUserCallbackInfo* Data);

    // Helper function
    EOS_HPlatform GetPlatformHandle();

	// --- Holding data ---
	// Make these UPROPERTY so they're properly managed by Unreal's garbage collector
    UPROPERTY()
    UEOSLobbyManager* LobbyManager = nullptr;

    UPROPERTY()
	UEOSSessionManager* SessionManager = nullptr;

    FString DevAuthHostStorage;
    FString DevAuthTokenStorage;

    EOS_ProductUserId LocalUserId = nullptr;
    EOS_EpicAccountId EpicAccountId = nullptr;
    FString StoredAuthToken; // Store auth token temporarily
    bool bIsUserAuthenticated = false;
};