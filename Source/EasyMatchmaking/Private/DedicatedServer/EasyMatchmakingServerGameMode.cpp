#include "DedicatedServer/EasyMatchmakingServerGameMode.h"
#include "EOSManager.h"
#include "EasyMatchmakingLog.h"

void AEasyMatchmakingServerGameMode::BeginPlay()
{
    Super::BeginPlay();
    InitServer();
}

void AEasyMatchmakingServerGameMode::InitServer()
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green,
            TEXT("EasyMatchmakingServerGameMode Initialized!"));
    }

    if (HasAuthority()) // Server only
    {
        EM_LOG_ERROR(TEXT("ServerHasAuthority"));
        UEOSManager* SessionManager = GetGameInstance()->GetSubsystem<UEOSManager>();
    }
}

