#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "EasyMatchmakingServerGameMode.generated.h"

UCLASS()
class EASYMATCHMAKING_API AEasyMatchmakingServerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	// Before Calling it check HasAuthority()!!!
	UFUNCTION(BlueprintCallable, Category = "Matchmaking")
	void InitServer();

	void BeginPlay() override;
};
