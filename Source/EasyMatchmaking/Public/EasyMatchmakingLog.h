#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEasyMatchmaking, Log, All);

// Simple logging macros (EM stand for EasyMatchmaking)
#define EM_LOG_ERROR(Format, ...) UE_LOG(LogEasyMatchmaking, Error, Format, ##__VA_ARGS__)
#define EM_LOG_WARNING(Format, ...) UE_LOG(LogEasyMatchmaking, Warning, Format, ##__VA_ARGS__)
#define EM_LOG_INFO(Format, ...) UE_LOG(LogEasyMatchmaking, Log, Format, ##__VA_ARGS__)