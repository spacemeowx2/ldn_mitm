#pragma once
#include <switch.h>
#include <stratosphere.hpp>

Result ipinfoInit();
void ipinfoExit();
Result ipinfoGetIpConfig(u32* address);
Result ipinfoGetIpConfig(u32* address, u32* netmask);
Result nifmSubmitRequestAndWait();
Result nifmCancelRequest();
Result nifmSetLocalNetworkMode(bool isLocalNetworkMode);
