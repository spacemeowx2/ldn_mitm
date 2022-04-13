#pragma once
#include <stratosphere.hpp>
#include <cstring>
#include <atomic>
#include "../ldn_icommunication.hpp"
#include "../ldnmitm_config.hpp"
#include "../debug.hpp"

#define AMS_ILDN_MITM_SERVICE(C, H)                                                                                                          								\
    AMS_SF_METHOD_INFO(C, H, 0, 	Result, CreateUserLocalCommunicationService, 	(ams::sf::Out<ams::sf::SharedPointer<ams::mitm::ldn::ICommunicationInterface>> out), 	(out))	\
    AMS_SF_METHOD_INFO(C, H, 65000, Result, CreateLdnMitmConfigService, 			(ams::sf::Out<ams::sf::SharedPointer<ams::mitm::ldn::ILdnConfig>> out), 				(out))	\

AMS_SF_DEFINE_MITM_INTERFACE(ams::mitm::ldn, ILdnMitMService, AMS_ILDN_MITM_SERVICE, 0x1D8F875E)
