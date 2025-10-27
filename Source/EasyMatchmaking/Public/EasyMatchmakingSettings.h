#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EasyMatchmakingSettings.generated.h"

USTRUCT(BlueprintType)
struct FEOSDevTestAccounts
{
    GENERATED_BODY()

    UPROPERTY(Config, EditAnywhere, Category = "Temporary Account")
    FString Host = TEXT("localhost:6547");

    UPROPERTY(Config, EditAnywhere, Category = "Temporary Account")
    FString Token = TEXT("DefaultToken");

    UPROPERTY(Config, EditAnywhere, Category = "Temporary Account")
    FString DisplayName = TEXT("PIEUser");
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Easy Matchmaking"))
class EASYMATCHMAKING_API UEasyMatchmakingSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UEasyMatchmakingSettings()
    {
        CategoryName = TEXT("Game");
        SectionName = TEXT("Easy Matchmaking");
    }

	// Basic EOS Credentials

    UPROPERTY(Config, EditAnywhere, Category = "EOS Credentials", meta = (DisplayName = "Product ID"))
    FString ProductId = TEXT("YOUR_PRODUCT_ID_HERE");

    UPROPERTY(Config, EditAnywhere, Category = "EOS Credentials", meta = (DisplayName = "Sandbox ID"))
    FString SandboxId = TEXT("YOUR_SANDBOX_ID_HERE");

    UPROPERTY(Config, EditAnywhere, Category = "EOS Credentials", meta = (DisplayName = "Deployment ID"))
    FString DeploymentId = TEXT("YOUR_DEPLOYMENT_ID_HERE");

	// Client Credentials

    UPROPERTY(Config, EditAnywhere, Category = "EOS User Credentials", meta = (DisplayName = "Client ID"))
    FString ClientId = TEXT("YOUR_CLIENT_ID_HERE");

    UPROPERTY(Config, EditAnywhere, Category = "EOS User Credentials", meta = (DisplayName = "Client Secret", PasswordField = true))
    FString ClientSecret = TEXT("YOUR_CLIENT_SECRET_HERE");

	// For Dedicated servers:
    UPROPERTY(Config, EditAnywhere, Category = "Server Credentials")
    FString ServerClientId = TEXT("YOUR_SERVER_CLIENT_ID");

    UPROPERTY(Config, EditAnywhere, Category = "Server Credentials", meta = (PasswordField = true))
    FString ServerClientSecret = TEXT("YOUR_SERVER_CLIENT_SECRET");

    // PIE accounts for local testing
    UPROPERTY( Config, EditAnywhere, Category = "EOS User Credentials",
        meta = (
            DisplayName = "Temporary PIE Accounts",
            AdvancedDisplay,
            ToolTip = "PIE Instance 0 automatically opens Epic Account login via web. \n\nPIE Instances 1+ use the credentials here (Host/Token) from EOS DevAuthTool. \n\nAdd one entry per additional PIE instance for local testing."
            )
    )
    TArray<FEOSDevTestAccounts> TemporaryAccounts;

    UPROPERTY(Config, EditAnywhere, Category = "Overall EOS Settings", meta = (ToolTip = "Overlay dosent work properly while testing in Editor, so it is suggested to disable it (web browser will be used for log-in)"))
    bool DisableOverlay = true;

    // Helper to get settings instance
    static const UEasyMatchmakingSettings* Get()
    {
        return GetDefault<UEasyMatchmakingSettings>();
    }
};