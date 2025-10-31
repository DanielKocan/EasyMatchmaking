// EOSManager.cpp
#include "EOSManager.h"
#include "EOSLobbyManager.h"  
#include "IEOSSDKManager.h"
#include "EasyMatchmakingLog.h"
#include "EasyMatchmakingSettings.h"
#include <eos_sdk.h>

#include "EasyMatchmaking.h"
#include "eos_auth.h"

void UEOSManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

	// Check if EOS SDK is initialized and if yes, then authenticate user (and load lobby manager)
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (SDKManager && SDKManager->IsInitialized())
    {
        FString DefaultConfig = SDKManager->GetDefaultPlatformConfigName();
        const FEOSSDKPlatformConfig* Config = SDKManager->GetPlatformConfig(DefaultConfig);
        if (Config)
        {
            EM_LOG_INFO(TEXT("Active Config - ProductId: %s"), *Config->ProductId);
            EM_LOG_INFO(TEXT("Active Config - SandboxId: %s"), *Config->SandboxId);
            EM_LOG_INFO(TEXT("Active Config - DeploymentId: %s"), *Config->DeploymentId);
            EM_LOG_INFO(TEXT("Active Config - ClientId: %s"), *Config->ClientId);
            EM_LOG_INFO(TEXT("Active Config - ClientSecret: %s"), *Config->ClientSecret);
        }

        IEOSPlatformHandlePtr CachedPlatform = FEasyMatchmakingModule::GetCachedEOSPlatform();

        if (CachedPlatform.IsValid())
        {
            EOS_HPlatform PlatformHandle = *CachedPlatform;
            EOS_HLobby LobbyHandle = EOS_Platform_GetLobbyInterface(PlatformHandle);

            if (IsRunningDedicatedServer())
            {
                // Server only needs session manager, no auth or lobby
                EOS_HSessions SessionHandle = EOS_Platform_GetSessionsInterface(PlatformHandle);

                LobbyManager = NewObject<UEOSLobbyManager>(this);
                SessionManager = NewObject<UEOSSessionManager>(this);
                SessionManager->Init(PlatformHandle, SessionHandle, nullptr, this); // No LocalUserId needed

                EM_LOG_INFO(TEXT("Dedicated server initialized - session manager ready"));
            }
            else
            {
                // Create Lobby manager (initialize later)
                LobbyManager = NewObject<UEOSLobbyManager>(this);
                // Create Session manager (initialize later)
                SessionManager = NewObject<UEOSSessionManager>(this);

                AuthenticateUser();

                EM_LOG_INFO(TEXT("EOSManager initialized with lobby support for client"));
            }
        }
    }
    else
    {
		EM_LOG_ERROR(TEXT("EOS SDK Manager not available or not initialized"));
    }
}

void UEOSManager::Deinitialize()
{
    LobbyManager->LeaveLobby(); // TODO: add destructor
	Super::Deinitialize();
}

bool UEOSManager::TestEOSInitialization()
{
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (SDKManager && SDKManager->IsInitialized())
    {
        UE_LOG(LogTemp, Log, TEXT("EOS SDK Manager is initialized!"));

        TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
        if (Platforms.Num() > 0)
        {
            IEOSPlatformHandlePtr Platform = Platforms[0];
            EOS_HPlatform RawHandle = *Platform;

            UE_LOG(LogTemp, Log, TEXT("Got EOS Platform Handle!"));
        }

        FString DefaultConfig = SDKManager->GetDefaultPlatformConfigName();
        const FEOSSDKPlatformConfig* Config = SDKManager->GetPlatformConfig(DefaultConfig);

        if (Config == nullptr)
        {
            UE_LOG(LogTemp, Error, TEXT("Config is empty!"));
        }

        if (Config)
        {
            UE_LOG(LogTemp, Log, TEXT("ProductId: %s"), *Config->ProductId);
            UE_LOG(LogTemp, Log, TEXT("SandboxId: %s"), *Config->SandboxId);
            UE_LOG(LogTemp, Log, TEXT("DeploymentId: %s"), *Config->DeploymentId);
        }

        return true;
    }

	UE_LOG(LogTemp, Error, TEXT("EOS SDK Manager not available or not initialized"));
    return false;
}

FString UEOSManager::GetCurrentUserId() const
{
    char UserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
    int32 BufferSize = sizeof(UserIdStr);

    if (EOS_ProductUserId_ToString(LocalUserId, UserIdStr, &BufferSize) == EOS_EResult::EOS_Success)
    {
        return FString(UTF8_TO_TCHAR(UserIdStr));
    }

    return FString( "Connecting or Failed to connect.");
}

void UEOSManager::AuthenticateUser()
{
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (!SDKManager) return;

    TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
    if (Platforms.Num() == 0) return;

    EOS_HPlatform PlatformHandle = *Platforms[0];
    EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);

    if (!AuthHandle)
    {
        EM_LOG_ERROR(TEXT("Auth interface not available"));
        return;
    }

    EOS_Auth_Credentials Credentials = {};
    Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

    // Check for command line credentials FIRST
    FString DevAuthHost;
    FString DevAuthToken;
    if (FParse::Value(FCommandLine::Get(), TEXT("DevAuthHost="), DevAuthHost) &&
        FParse::Value(FCommandLine::Get(), TEXT("DevAuthToken="), DevAuthToken))
    {
        // Use developer auth from command line
        EM_LOG_INFO(TEXT("Using Developer Auth from command line"));
        EM_LOG_INFO(TEXT("Host: %s, Token: %s"), *DevAuthHost, *DevAuthToken);

        Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;

        DevAuthHostStorage = DevAuthHost;
        DevAuthTokenStorage = DevAuthToken;

        Credentials.Id = TCHAR_TO_UTF8(*DevAuthHostStorage);
        Credentials.Token = TCHAR_TO_UTF8(*DevAuthTokenStorage);
    }
	// Below code is for debugging to be able to run multiple PIE (game editor in engine) instances with different users
#if WITH_EDITOR
    else if (GIsEditor && GetWorld() && GetWorld()->IsPlayInEditor())
    {
        // Use PIE instance ID to determine auth type
        int32 PIEInstance = GetWorld()->GetOutermost()->GetPIEInstanceID();
        const UEasyMatchmakingSettings* Settings = UEasyMatchmakingSettings::Get();

        if (PIEInstance == 0)
        {
            // First PIE instance -> Epic Account login via website
            Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
            Credentials.Id = nullptr;
            Credentials.Token = nullptr;

            EM_LOG_INFO(TEXT("PIE Instance 0: Using Epic Account login via website"));
        }
        else
        {
            // Additional PIE instances -> use temporary credentials from settings
            int32 TempIndex = PIEInstance - 1; // array index for PIEInstance > 0
            if (Settings->TemporaryAccounts.IsValidIndex(TempIndex))
            {
                const FEOSDevTestAccounts& TempAccount = Settings->TemporaryAccounts[TempIndex];
                Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;
                Credentials.Id = TCHAR_TO_UTF8(*TempAccount.Host);
                Credentials.Token = TCHAR_TO_UTF8(*TempAccount.Token);

                EM_LOG_INFO(TEXT("PIE Instance %d using DevAuthTool credentials (Host: %s, Token: %s)"),
                    PIEInstance, *TempAccount.Host, *TempAccount.Token);
            }
            else
            {
                // Fallback: Epic Account login
                Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
                Credentials.Id = nullptr;
                Credentials.Token = nullptr;

                EM_LOG_ERROR(TEXT("PIE Instance %d has no temporary account configured. Using Epic Account login."), PIEInstance);
            }
        }
    }
#endif
    else
    {
        // Normal editor = Epic Account
        Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
        Credentials.Id = nullptr;
        Credentials.Token = nullptr;

        EM_LOG_INFO(TEXT("Editor mode using Epic Account auth"));
    }

    EOS_Auth_LoginOptions LoginOptions = {};
    LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;
    LoginOptions.Credentials = &Credentials;

    EM_LOG_INFO(TEXT("Starting Epic Account authentication..."));
    EOS_Auth_Login(AuthHandle, &LoginOptions, this, OnAuthLoginComplete);
}

void UEOSManager::OnAuthLoginComplete(const EOS_Auth_LoginCallbackInfo* Data)
{
    UEOSManager* Manager = static_cast<UEOSManager*>(Data->ClientData);

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        Manager->EpicAccountId = Data->LocalUserId;

        // Get platform handle and auth token
        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
        TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
        EOS_HPlatform PlatformHandle = *Platforms[0];
        EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);

        EOS_Auth_CopyUserAuthTokenOptions TokenOptions = {};
        TokenOptions.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

        EOS_Auth_Token* AuthToken = nullptr;
        EOS_EResult TokenResult = EOS_Auth_CopyUserAuthToken(AuthHandle, &TokenOptions, Data->LocalUserId, &AuthToken);

        if (TokenResult == EOS_EResult::EOS_Success && AuthToken)
        {
            EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);

            EOS_Connect_Credentials ConnectCredentials = {};
            ConnectCredentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
            ConnectCredentials.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC;
            ConnectCredentials.Token = AuthToken->AccessToken;

            EOS_Connect_LoginOptions ConnectLoginOptions = {};
            ConnectLoginOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
            ConnectLoginOptions.Credentials = &ConnectCredentials;
            ConnectLoginOptions.UserLoginInfo = nullptr;

            // Store the token for potential user creation
            Manager->StoredAuthToken = FString(AuthToken->AccessToken);

            EM_LOG_INFO(TEXT("Attempting Connect login..."));
            EOS_Connect_Login(ConnectHandle, &ConnectLoginOptions, Manager, OnConnectLoginFromEpicComplete);

            EOS_Auth_Token_Release(AuthToken);
        }
    }
    else
    {
        EM_LOG_ERROR(TEXT("Auth login failed: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSManager::OnConnectLoginFromEpicComplete(const EOS_Connect_LoginCallbackInfo* Data)
{
    UEOSManager* Manager = static_cast<UEOSManager*>(Data->ClientData);

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        // Login succeeded = user already exists
        Manager->LocalUserId = Data->LocalUserId;
        Manager->bIsUserAuthenticated = true;

        // Log the actual ProductUserId to see if they are different
        char UserIdStr[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
        int32 BufferSize = sizeof(UserIdStr);
        EOS_ProductUserId_ToString(Data->LocalUserId, UserIdStr, &BufferSize);

        EM_LOG_INFO(TEXT("Authenticated with ProductUserId: %s"), UTF8_TO_TCHAR(UserIdStr));

        Manager->InitializeManagers();

        Manager->OnUserAuthenticated.Broadcast();
    }
    else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
    { 
        // User doesn not exist = create
        EM_LOG_ERROR(TEXT("User doesn't exist in Connect, creating..."));

        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
        TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
        EOS_HPlatform PlatformHandle = *Platforms[0];
        EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);

        EOS_Connect_CreateUserOptions CreateOptions = {};
        CreateOptions.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
        CreateOptions.ContinuanceToken = Data->ContinuanceToken;

        EOS_Connect_CreateUser(ConnectHandle, &CreateOptions, Manager, OnCreateUserComplete);
    }
    else
    {
        EM_LOG_ERROR(TEXT("Connect login failed: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSManager::OnCreateUserComplete(const EOS_Connect_CreateUserCallbackInfo* Data)
{
    UEOSManager* Manager = static_cast<UEOSManager*>(Data->ClientData);

    if (Data->ResultCode == EOS_EResult::EOS_Success)
    {
        Manager->LocalUserId = Data->LocalUserId;

        // Print ProductUserId as string
        char UserIdBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
        int32_t BufferLen = sizeof(UserIdBuffer);
        EOS_EResult IdResult = EOS_ProductUserId_ToString(
            Data->LocalUserId,
            UserIdBuffer,
            &BufferLen
        );
        EM_LOG_INFO(TEXT("ProductUserId: "), UTF8_TO_TCHAR(UserIdBuffer));

        Manager->bIsUserAuthenticated = true;
        Manager->InitializeManagers();
        EM_LOG_INFO(TEXT("Connect user created and authenticated!"));
    }
    else
    {
        EM_LOG_ERROR(TEXT("Failed to create Connect user: %s"),
            UTF8_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
    }
}

void UEOSManager::InitializeManagers()
{
    if (LobbyManager)
    {
        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
        TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
        EOS_HPlatform PlatformHandle = *Platforms[0];
        EOS_HLobby LobbyHandle = EOS_Platform_GetLobbyInterface(PlatformHandle);

        LobbyManager->Init(PlatformHandle, LobbyHandle, LocalUserId, this);
    }

    if (SessionManager)
    {
        IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
        TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
        EOS_HPlatform PlatformHandle = *Platforms[0];
        EOS_HSessions SessionHandle = EOS_Platform_GetSessionsInterface(PlatformHandle);

        SessionManager->Init(PlatformHandle, SessionHandle, LocalUserId, this);
	}
}

EOS_HPlatform UEOSManager::GetPlatformHandle()
{
    IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
    if (SDKManager && SDKManager->IsInitialized())
    {
        TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
        if (Platforms.Num() > 0)
        {
            return *Platforms[0];
        }
    }
    return nullptr;
}
