#pragma once
#include <stratosphere.hpp>
#include "../debug.hpp"
#include "../lan_discovery.hpp"
#include "../ldn_types.hpp"
#include "../ipinfo.hpp"

    #define AMS_LDN_ICOMMUNICATION(C, H)            																																																				\
        AMS_SF_METHOD_INFO(C, H, 0,   Result, GetState, 			   (ams::sf::Out<u32> state), 																														(state))							\
        AMS_SF_METHOD_INFO(C, H, 1,   Result, GetNetworkInfo, 			   (ams::sf::Out<ams::mitm::ldn::NetworkInfo> buffer), 																							(buffer))							\
        AMS_SF_METHOD_INFO(C, H, 2,   Result, GetIpv4Address, 			   (ams::sf::Out<u32> address, ams::sf::Out<u32> mask), 																							(address, mask))					\
        AMS_SF_METHOD_INFO(C, H, 3,   Result, GetDisconnectReason, 		   (ams::sf::Out<u32> reason), 																													(reason))							\
        AMS_SF_METHOD_INFO(C, H, 4,   Result, GetSecurityParameter, 		   (ams::sf::Out<ams::mitm::ldn::SecurityParameter> out), 																							(out))								\
        AMS_SF_METHOD_INFO(C, H, 5,   Result, GetNetworkConfig, 		   (ams::sf::Out<ams::mitm::ldn::NetworkConfig> out), 																								(out))								\
        AMS_SF_METHOD_INFO(C, H, 100, Result, AttachStateChangeEvent,	          (ams::sf::Out<ams::sf::CopyHandle> handle), 																									(handle))							\
        AMS_SF_METHOD_INFO(C, H, 101, Result, GetNetworkInfoLatestUpdate,       (ams::sf::Out<ams::mitm::ldn::NetworkInfo> buffer, ams::sf::OutArray<ams::mitm::ldn::NodeLatestUpdate> pUpdates), 								(buffer, pUpdates))       			\
        AMS_SF_METHOD_INFO(C, H, 102, Result, Scan, 				   (ams::sf::Out<u32> count, ams::sf::OutAutoSelectArray<ams::mitm::ldn::NetworkInfo> buffer, u16 channel, ams::mitm::ldn::ScanFilter filter), 	(count, buffer, channel, filter))	\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 103, Result, ScanPrivate, 			   (), 																																			())									\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 104, Result, SetWirelessControllerRestriction, (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 200, Result, OpenAccessPoint, 		   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 201, Result, CloseAccessPoint, 		   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 202, Result, CreateNetwork, 			   (ams::mitm::ldn::CreateNetworkConfig data), 																									(data))								\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 203, Result, CreateNetworkPrivate, 		   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 204, Result, DestroyNetwork, 			   (), 																																			())									\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 205, Result, Reject, 				   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 206, Result, SetAdvertiseData, 		   (ams::sf::InAutoSelectBuffer data), 																											(data))								\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 207, Result, SetStationAcceptPolicy, 	   (u8 policy),																																	(policy))							\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 208, Result, AddAcceptFilterEntry, 		   (), 																																			())									\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 209, Result, ClearAcceptFilter, 		   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 300, Result, OpenStation, 			   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 301, Result, CloseStation, 			   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 302, Result, Connect, 				   (ams::mitm::ldn::ConnectNetworkData dat, const ams::mitm::ldn::NetworkInfo &data), 																	(dat, data))						\
 /*nyi*/AMS_SF_METHOD_INFO(C, H, 303, Result, ConnectPrivate, 			   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 304, Result, Disconnect, 			   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 400, Result, Initialize, 			   (const ams::sf::ClientProcessId &client_process_id), 																							(client_process_id))				\
        AMS_SF_METHOD_INFO(C, H, 401, Result, Finalize, 			   (), 																																			())									\
        AMS_SF_METHOD_INFO(C, H, 402, Result, InitializeSystem2, 		   (u64 unk, const ams::sf::ClientProcessId &client_process_id), 																					(unk, client_process_id))

    AMS_SF_DEFINE_INTERFACE(ams::mitm::ldn, ICommunicationInterface, AMS_LDN_ICOMMUNICATION)
