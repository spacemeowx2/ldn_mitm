#pragma once
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

Result ldnMitmSaveLogToFile(LdnMitmConfigService *s);
Result ldnMitmGetVersion(LdnMitmConfigService *s, char *version);
Result ldnMitmGetLogging(LdnMitmConfigService *s, u32 *enabled);
Result ldnMitmSetLogging(LdnMitmConfigService *s, u32 enabled);
Result ldnMitmGetEnabled(LdnMitmConfigService *s, u32 *enabled);
Result ldnMitmSetEnabled(LdnMitmConfigService *s, u32 enabled);
Result ldnMitmGetConfig(LdnMitmConfigService *out);
Result ldnMitmGetConfigFromService(Service* s, LdnMitmConfigService *out);

#ifdef __cplusplus
}
#endif
