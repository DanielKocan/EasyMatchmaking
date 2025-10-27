#include "MainMenuGameMode.h"
#include "Blueprint/UserWidget.h"
#include "UObject/ConstructorHelpers.h"

AMainMenuGameMode::AMainMenuGameMode()
{
    // default pawn to None (no player movement needed in menu)
    DefaultPawnClass = nullptr;
}

void AMainMenuGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Check if there is a Main Menu Widget Class in Game Mode and then create it
    if (MainMenuWidgetClass)
    {
        MainMenuWidget = CreateWidget<UUserWidget>(GetWorld(), MainMenuWidgetClass);
        if (MainMenuWidget)
        {
            MainMenuWidget->AddToViewport();

            // Show mouse cursor
            APlayerController* PC = GetWorld()->GetFirstPlayerController();
            if (PC)
            {
                PC->bShowMouseCursor = true;
                PC->SetInputMode(FInputModeUIOnly());
            }
        }
    }
}


