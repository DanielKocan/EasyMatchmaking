#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EOSLobbyManager.h"
#include "MainMenuWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
// DEPRICATED, was used at the beginning of the project
class EASYMATCHMAKING_API UMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    // UI Update events (implement in Blueprint)
    UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
    void UpdateLobbyState(bool bInLobby, const FString& LobbyId, int32 PlayerCount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
    void UpdatePlayerList(const TArray<FString>& PlayerNames);

    UFUNCTION(BlueprintImplementableEvent, Category = "Menu")
    void ShowError(const FString& ErrorMessage);

protected:
    UPROPERTY()
    UEOSLobbyManager* LobbyManager;

    // Event handlers
    UFUNCTION()
    void OnLobbyCreated(const FString& LobbyId);

    UFUNCTION()
    void OnLobbyJoined(const FString& LobbyId);

    UFUNCTION()
    void OnLobbyLeft();

    UFUNCTION()
    void OnLobbyError(const FString& ErrorMessage);
};