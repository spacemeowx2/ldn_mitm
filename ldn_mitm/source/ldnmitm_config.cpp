#include <cstring>
#include "ldnmitm_config.hpp"
#include "debug.hpp"

std::atomic_bool LdnConfig::LdnEnabled = true;

Result LdnConfig::SaveLogToFile() {
    return ::SaveLogToFile();
}
Result LdnConfig::GetVersion(Out<LdnMitmVersion> version) {
    std::strcpy(version.GetPointer()->raw, GITDESCVER);
    return 0;
}
Result LdnConfig::GetLogging(Out<u32> enabled) {
    return ::GetLogging(enabled.GetPointer());
}
Result LdnConfig::SetLogging(u32 enabled) {
    return ::SetLogging(enabled);
}
Result LdnConfig::GetEnabled(Out<u32> enabled) {
    enabled.SetValue(LdnEnabled);
    return 0;
}
Result LdnConfig::SetEnabled(u32 enabled) {
    LdnEnabled = enabled;
    return 0;
}