#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

UCLASS()
// DEPRICATED, was used at the beginning of the project
class EASYMATCHMAKING_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
    AMainMenuGameMode();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
    TSubclassOf<class UUserWidget> MainMenuWidgetClass;

private:
    UPROPERTY()
    UUserWidget* MainMenuWidget;
};
