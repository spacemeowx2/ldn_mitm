#pragma once
#include <stratosphere.hpp>

namespace ams::mitm::ldn {

    struct LdnMitmVersion {
        char raw[32];
    };

}

#define AMS_LDN_CONFIG(C, H)                                                                									\
    AMS_SF_METHOD_INFO(C, H, 65000, Result, SaveLogToFile, 	(), 													())			\
    AMS_SF_METHOD_INFO(C, H, 65001, Result, GetVersion, 	(ams::sf::Out<ams::mitm::ldn::LdnMitmVersion> version), (version))  \
    AMS_SF_METHOD_INFO(C, H, 65002, Result, GetLogging, 	(ams::sf::Out<u32> enabled), 							(enabled))	\
    AMS_SF_METHOD_INFO(C, H, 65003, Result, SetLogging, 	(u32 enabled), 											(enabled))	\
    AMS_SF_METHOD_INFO(C, H, 65004, Result, GetEnabled, 	(ams::sf::Out<u32> enabled), 							(enabled))	\
    AMS_SF_METHOD_INFO(C, H, 65005, Result, SetEnabled, 	(u32 enabled), 											(enabled))

    AMS_SF_DEFINE_INTERFACE(ams::mitm::ldn, ILdnConfig, AMS_LDN_CONFIG, 0x14c8af2c)
