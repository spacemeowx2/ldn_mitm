#include "ipinfo.hpp"
#include "debug.hpp"

Result nifmSetLocalNetworkMode(NifmRequest *r, bool isLocalNetworkMode)
{
    u8 option = isLocalNetworkMode ? 2 : 4;

    serviceAssumeDomain(&r->s);

    return serviceDispatchIn(&r->s, 11, option);
}
