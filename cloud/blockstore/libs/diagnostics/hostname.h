#pragma once

#include "public.h"

#include <util/generic/string.h>

namespace NCloud::NBlockStore {

////////////////////////////////////////////////////////////////////////////////

enum class EHostService
{
    Kikimr,
    Nbs
};

////////////////////////////////////////////////////////////////////////////////

TString GetShortHostName(const TString& fullName);
TString GetShortHostName();

TString GetExternalHostUrl(
    const TString& hostName,
    EHostService serviceType,
    const TDiagnosticsConfig& config);

TString GetMonitoringNBSAlertsUrl(const TDiagnosticsConfig& config);

TString GetMonitoringNBSOverviewToTVUrl(const TDiagnosticsConfig& config);

TString GetMonitoringVolumeUrl(
    const TDiagnosticsConfig& config,
    const TString& diskId);

TString GetMonitoringPartitionUrl(const TDiagnosticsConfig& config);

TString GetMonitoringYDBGroupUrl(
    const TDiagnosticsConfig& config,
    ui32 groupId,
    const TString& storagePool);

}   // namespace NCloud::NBlockStore
