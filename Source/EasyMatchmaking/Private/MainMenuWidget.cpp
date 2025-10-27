#include "MainMenuWidget.h"
#include "EOSManager.h"
#include "EasyMatchmakingLog.h"

void UMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Get lobby manager
    if (UEOSManager* EOSManager = GetGameInstance()->GetSubsystem<UEOSManager>())
    {
        LobbyManager = EOSManager->GetLobbyManager();
        if (LobbyManager)
        {
            // Bind events
            LobbyManager->OnLobbyCreated.AddDynamic(this, &UMainMenuWidget::OnLobbyCreated);
            LobbyManager->OnLobbyJoined.AddDynamic(this, &UMainMenuWidget::OnLobbyJoined);
            LobbyManager->OnLobbyLeft.AddDynamic(this, &UMainMenuWidget::OnLobbyLeft);
            LobbyManager->OnLobbyError.AddDynamic(this, &UMainMenuWidget::OnLobbyError);

            EM_LOG_INFO(TEXT("Main Menu connected to Lobby Manager"));
        }
        else
        {
            EM_LOG_ERROR(TEXT("Lobby Manager not available"));
        }
    }
}

void UMainMenuWidget::OnLobbyCreated(const FString& LobbyId)
{
    EM_LOG_INFO(TEXT("Lobby created: %s"), *LobbyId);

    // Update UI via Blueprint event
    TArray<FString> Players = LobbyManager->GetCurrentLobbyPlayers();
    UpdateLobbyState(true, LobbyId, Players.Num());
    UpdatePlayerList(Players);
}

void UMainMenuWidget::OnLobbyJoined(const FString& LobbyId)
{
    EM_LOG_INFO(TEXT("Lobby joined: %s"), *LobbyId);

    TArray<FString> Players = LobbyManager->GetCurrentLobbyPlayers();
    UpdateLobbyState(true, LobbyId, Players.Num());
    UpdatePlayerList(Players);
}

void UMainMenuWidget::OnLobbyLeft()
{
    EM_LOG_INFO(TEXT("Left lobby"));
    UpdateLobbyState(false, TEXT(""), 0);
    UpdatePlayerList(TArray<FString>());
}

void UMainMenuWidget::OnLobbyError(const FString& ErrorMessage)
{
    ShowError(ErrorMessage);
}