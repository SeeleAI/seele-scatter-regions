// Copyright 2026 QUANLING SHENZHEN NETWORK CO., LTD. All Rights Reserved.

#include "ScatterRegionEditorSubsystem.h"

#include "ScatterRegionJsonAdapter.h"

bool UScatterRegionEditorSubsystem::ExecuteScatterRegionJsonCommand(const FString& CommandType, const FString& ParamsJson, FString& OutResultJson)
{
	return FScatterRegionJsonAdapter::ExecuteJsonCommand(CommandType, ParamsJson, OutResultJson);
}
