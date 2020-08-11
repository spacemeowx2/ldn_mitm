#pragma once
#include <stratosphere.hpp>
#include <cstring>
#include <atomic>
#include "../ldn_icommunication.hpp"
#include "../ldnmitm_config.hpp"
#include "../debug.hpp"

namespace ams::mitm::ldn {
    #define AMS_ILDN_MITM_SERVICE(C, H)                                                                                                          \
        AMS_SF_METHOD_INFO(C, H, 0, Result, CreateUserLocalCommunicationService, (sf::Out<std::shared_ptr<ILdnCommunication>> out))   \
        AMS_SF_METHOD_INFO(C, H, 65000, Result, CreateLdnMitmConfigService, (sf::Out<std::shared_ptr<ILdnConfig>> out))                      \

    AMS_SF_DEFINE_MITM_INTERFACE(ILdnMitMService, AMS_ILDN_MITM_SERVICE)
}