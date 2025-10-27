// Copyright Epic Games, Inc. All Rights Reserved.

#include "EasyMatchmaking.h"

#if WITH_EDITOR
#include "EasyMatchmakingStyle.h"
#include "EasyMatchmakingCommands.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Interfaces/IPluginManager.h"
#endif

#include "EOSManager.h"
#include "EasyMatchmakingSettings.h"
#include "IEOSSDKManager.h"
#include "EasyMatchmakingLog.h"
#include <eos_sdk.h>

DEFINE_LOG_CATEGORY(LogEasyMatchmaking);

static const FName EasyMatchmakingTabName("EasyMatchmaking");

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "FEasyMatchmakingModule"
#endif

#if WITH_EDITOR
void FEasyMatchmakingModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FEasyMatchmakingStyle::Initialize();
	FEasyMatchmakingStyle::ReloadTextures();

	FEasyMatchmakingCommands::Register();

	EM_LOG_INFO(TEXT("=== PLUGIN LOADING - CHECKING EXISTING EOS STATE ==="));

	// Check if EOSShared module is loaded
	bool bIsEOSSharedLoaded = FModuleManager::Get().IsModuleLoaded("EOSShared");
	EM_LOG_INFO(TEXT("EOSShared module loaded: %s"), bIsEOSSharedLoaded ? TEXT("Yes") : TEXT("No"));

	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (SDKManager)
	{
		TArray<IEOSPlatformHandlePtr> ExistingPlatforms = SDKManager->GetActivePlatforms();
		EM_LOG_INFO(TEXT("Found %d existing EOS platforms"), ExistingPlatforms.Num());

		FString DefaultConfigName = SDKManager->GetDefaultPlatformConfigName();
		EM_LOG_INFO(TEXT("Current default config: %s"), *DefaultConfigName);

		const FEOSSDKPlatformConfig* ExistingConfig = SDKManager->GetPlatformConfig(DefaultConfigName);
		if (ExistingConfig)
		{
			EM_LOG_INFO(TEXT("Existing ClientId: %s"), *ExistingConfig->ClientId);
			EM_LOG_INFO(TEXT("Existing DeploymentId: %s"), *ExistingConfig->DeploymentId);
		}
	}

	// Initialize EOS with settings
	InitializeEOSUserSettings();
	
	// Button code
	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FEasyMatchmakingCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FEasyMatchmakingModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FEasyMatchmakingModule::RegisterMenus));
}
#else
void FEasyMatchmakingModule::StartupModule()
{
	// Server-specific initialization
	if (IsRunningDedicatedServer())
	{
		InitializeEOSForDedicatedServer();
	}
	else
	{
		InitializeEOSUserSettings(); // Client
	}
}
#endif

void FEasyMatchmakingModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Close plugin window if it's open and delete pointers
#if WITH_EDITOR
	if (PluginWindow.IsValid())
	{
		TSharedPtr<SWindow> Window = PluginWindow.Pin();
		if (Window.IsValid())
		{
			Window->RequestDestroyWindow();
		}
		PluginWindow.Reset();
	}

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FEasyMatchmakingStyle::Shutdown();
	FEasyMatchmakingCommands::Unregister();
#endif
}

#if WITH_EDITOR
void FEasyMatchmakingModule::PluginButtonClicked()
{
	// Check if window is already open
	if (PluginWindow.IsValid())
	{
		// Window exists, just bring it to front
		TSharedPtr<SWindow> Window = PluginWindow.Pin();
		if (Window.IsValid())
		{
			Window->BringToFront();
			return; // Don't create a new window
		}
	}

	// Path to image
	FString PluginDir = IPluginManager::Get().FindPlugin(TEXT("EasyMatchmaking"))->GetBaseDir();
	FString ImagePath = FPaths::Combine(PluginDir, TEXT("Resources/LogoBUas_RGB.png"));

	ImageBrush = MakeShareable(
		new FSlateDynamicImageBrush(FName(ImagePath), FVector2D(600, 207)) // image size
	);

	// Create a new window
	TSharedRef<SWindow> NewWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("My New Window")))
		.ClientSize(FVector2D(800, 400)) // size of the window
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	// Add some content to the window
	NewWindow->SetContent(
		SNew(SBorder)
		.Padding(10)
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
						.Text(FText::FromString(TEXT("Hello from my custom window!")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
						.Image(ImageBrush.Get())
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0, 20, 0, 0)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("Test EOS Initialization")))
						.OnClicked_Lambda([this]() -> FReply
							{
								bool bTestResult = UEOSManager::TestEOSInitialization();

								FText ResultText = bTestResult ?
									FText::FromString(TEXT("EOS Test PASSED! Check Output Log for details.")) :
									FText::FromString(TEXT("EOS Test FAILED! Check Output Log for details."));

								FMessageDialog::Open(EAppMsgType::Ok, ResultText);

								return FReply::Handled();
							})
				]
		]
	);

	// Store weak reference to the window
	PluginWindow = NewWindow;

	// Show the window
	FSlateApplication::Get().AddWindow(NewWindow);
}


void FEasyMatchmakingModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FEasyMatchmakingCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FEasyMatchmakingCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}
#endif

void FEasyMatchmakingModule::InitializeEOSUserSettings()
{
	// Get header settings
	const UEasyMatchmakingSettings* Settings = UEasyMatchmakingSettings::Get();
	if (!Settings)
	{
		EM_LOG_ERROR(TEXT("EasyMatchmaking: Could not get settings!"));
		return;
	}

	// Check if credentials are set
	if (Settings->ProductId == TEXT("YOUR_PRODUCT_ID_HERE") || Settings->ProductId.IsEmpty())
	{
		EM_LOG_ERROR(TEXT("EasyMatchmaking: EOS credentials not configured"));
		return;
	}

	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		EM_LOG_ERROR(TEXT("EasyMatchmaking: EOS SDK Manager not available!"));
		return;
	}

	// Create custom config
	FEOSSDKPlatformConfig CustomConfig;
	CustomConfig.Name = TEXT("EasyMatchmakingConfig");
	CustomConfig.ProductId = Settings->ProductId;
	CustomConfig.SandboxId = Settings->SandboxId;
	CustomConfig.ClientId = Settings->ClientId;
	CustomConfig.ClientSecret = Settings->ClientSecret;
	CustomConfig.DeploymentId = Settings->DeploymentId;
	CustomConfig.bDisableOverlay = Settings->DisableOverlay;

	// Add and set as default - this should override the existing config
	bool bSuccess = SDKManager->AddPlatformConfig(CustomConfig, true);
	if (bSuccess)
	{
		SDKManager->SetDefaultPlatformConfigName(CustomConfig.Name);

		// CREATE the platform - this was missing
		IEOSPlatformHandlePtr Platform = SDKManager->CreatePlatform(CustomConfig.Name);
		if (Platform.IsValid())
		{
			InstanceCachedEOSPlatform = Platform; // Store it immediately
			EM_LOG_INFO(TEXT("EasyMatchmaking: EOS platform created successfully!"));
		}
		else
		{
			EM_LOG_ERROR(TEXT("EasyMatchmaking: Failed to create EOS platform!"));
		}
	}
	else
	{
		EM_LOG_ERROR(TEXT("EasyMatchmaking: Failed to add platform config!"));
	}
}

void FEasyMatchmakingModule::InitializeEOSForDedicatedServer()
{
	EM_LOG_INFO(TEXT("Initializing EOS for Dedicated Server"));

	const UEasyMatchmakingSettings* Settings = UEasyMatchmakingSettings::Get();
	if (!Settings)
	{
		EM_LOG_ERROR(TEXT("Could not get settings for server"));
		return;
	}

	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		EM_LOG_ERROR(TEXT("EOS SDK Manager not available"));
		return;
	}

	// Create server-specific config
	FEOSSDKPlatformConfig ServerConfig;
	ServerConfig.Name = TEXT("DedicatedServer");
	ServerConfig.ProductId = Settings->ProductId;
	ServerConfig.SandboxId = Settings->SandboxId;
	ServerConfig.DeploymentId = Settings->DeploymentId;

	// Use server client credentials (should be TrustedServer policy in dev.epicgames.com)
	ServerConfig.ClientId = Settings->ServerClientId;
	ServerConfig.ClientSecret = Settings->ServerClientSecret;

	ServerConfig.bIsServer = true;
	ServerConfig.bDisableOverlay = true;
	ServerConfig.bDisableSocialOverlay = true;

	bool bSuccess = SDKManager->AddPlatformConfig(ServerConfig, true);
	if (bSuccess)
	{
		SDKManager->SetDefaultPlatformConfigName(ServerConfig.Name);

		IEOSPlatformHandlePtr Platform = SDKManager->CreatePlatform(ServerConfig.Name);
		if (Platform.IsValid())
		{
			InstanceCachedEOSPlatform = Platform;
			EM_LOG_INFO(TEXT("Dedicated server EOS platform created successfully"));
		}
	}
}

// Static getter
IEOSPlatformHandlePtr FEasyMatchmakingModule::GetCachedEOSPlatform()
{
	FEasyMatchmakingModule* Module = FModuleManager::GetModulePtr<FEasyMatchmakingModule>("EasyMatchmaking");
	return Module ? Module->InstanceCachedEOSPlatform : nullptr;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FEasyMatchmakingModule, EasyMatchmaking)