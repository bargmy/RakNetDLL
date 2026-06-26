/**
 * raknet_c.cpp — C wrapper implementation for RakNet.
 * Compiled into RakNetDLL. Do NOT include from C client code.
 */

#include "raknet_c.h"

/* Pull in ALL RakNet types we use */
#include "RakNetTypes.h"       /* Packet, SystemAddress, StartupResult, etc. */
#include "RakNetSocket2.h"     /* SocketDescriptor                           */
#include "RakPeerInterface.h"
#include "PacketPriority.h"
#include "GetTime.h"

#include <cstring>
#include <cstdio>

#if defined(_WIN32)
#  include <winsock2.h>        /* ntohl */
#else
#  include <arpa/inet.h>       /* ntohl */
#endif

/* ------------------------------------------------------------------ */
/*  Internal type casts                                                 */
/* ------------------------------------------------------------------ */
static inline RakNet::RakPeerInterface* toCpp(RakPeerHandle* h)
{
    return reinterpret_cast<RakNet::RakPeerInterface*>(h);
}

static inline RakNet::Packet* toPacket(RakPacket* p)
{
    return reinterpret_cast<RakNet::Packet*>(p);
}

/* ================================================================== */
/*  Lifecycle                                                           */
/* ================================================================== */

extern "C" RAKNET_C_API
RakPeerHandle* raknet_peer_create(void)
{
    return reinterpret_cast<RakPeerHandle*>(
        RakNet::RakPeerInterface::GetInstance());
}

extern "C" RAKNET_C_API
void raknet_peer_destroy(RakPeerHandle* peer)
{
    if (peer)
        RakNet::RakPeerInterface::DestroyInstance(toCpp(peer));
}

extern "C" RAKNET_C_API
int raknet_peer_startup(RakPeerHandle*  peer,
                        unsigned int    maxConnections,
                        unsigned short  localPort)
{
    if (!peer) return RN_ERR_STARTUP_FAILURE;

    RakNet::SocketDescriptor sd(localPort, 0);
    RakNet::StartupResult r = toCpp(peer)->Startup(maxConnections, &sd, 1);

    switch (r) {
        case RakNet::RAKNET_STARTED:            return RN_STARTED;
        case RakNet::RAKNET_ALREADY_STARTED:    return RN_ERR_ALREADY_STARTED;
        case RakNet::SOCKET_PORT_ALREADY_IN_USE:return RN_ERR_PORT_IN_USE;
        case RakNet::INVALID_SOCKET_DESCRIPTORS:return RN_ERR_INVALID_SOCKET;
        default:                                return RN_ERR_STARTUP_FAILURE;
    }
}

extern "C" RAKNET_C_API
void raknet_peer_set_max_incoming(RakPeerHandle* peer, unsigned int count)
{
    if (peer)
        toCpp(peer)->SetMaximumIncomingConnections((unsigned short)count);
}

extern "C" RAKNET_C_API
int raknet_peer_connect(RakPeerHandle*  peer,
                        const char*     host,
                        unsigned short  remotePort,
                        const char*     password,
                        int             passwordLen)
{
    if (!peer || !host) return RN_ERR_CONNECT_FAILED;

    RakNet::ConnectionAttemptResult r =
        toCpp(peer)->Connect(host, remotePort, password, passwordLen);

    switch (r) {
        case RakNet::CONNECTION_ATTEMPT_STARTED:           return RN_CONNECT_STARTED;
        case RakNet::ALREADY_CONNECTED_TO_ENDPOINT:        return RN_ERR_ALREADY_CONNECTED;
        case RakNet::CONNECTION_ATTEMPT_ALREADY_IN_PROGRESS: return RN_ERR_CONNECT_IN_PROGRESS;
        default:                                           return RN_ERR_CONNECT_FAILED;
    }
}

extern "C" RAKNET_C_API
void raknet_peer_shutdown(RakPeerHandle* peer, unsigned int blockDurationMs)
{
    if (peer)
        toCpp(peer)->Shutdown(blockDurationMs);
}

/* ================================================================== */
/*  Send / Receive                                                      */
/* ================================================================== */

extern "C" RAKNET_C_API
int raknet_peer_send(RakPeerHandle*  peer,
                     const char*     data,
                     int             length,
                     int             priority,
                     int             reliability,
                     int             channel,
                     const char*     host,
                     unsigned short  port,
                     int             broadcast)
{
    if (!peer || !data || length <= 0) return -1;

    PacketPriority    prio = (PacketPriority)   priority;
    PacketReliability rel  = (PacketReliability) reliability;

    RakNet::SystemAddress target = RakNet::UNASSIGNED_SYSTEM_ADDRESS;
    if (host && !broadcast)
        target.FromStringExplicitPort(host, port);

    return toCpp(peer)->Send(data, length, prio, rel,
                             (char)channel, target, broadcast != 0);
}

extern "C" RAKNET_C_API
RakPacket* raknet_peer_receive(RakPeerHandle* peer)
{
    if (!peer) return nullptr;
    return reinterpret_cast<RakPacket*>(toCpp(peer)->Receive());
}

extern "C" RAKNET_C_API
void raknet_packet_get_info(RakPacket* packet, RakPacketInfo* out)
{
    if (!packet || !out) return;
    RakNet::Packet* p = toPacket(packet);

    out->data        = p->data;
    out->length      = p->length;
    out->systemPort  = p->systemAddress.GetPort();
    out->guid        = p->guid.g;

    /* Textual IP (without port suffix) */
    const char* addrStr = p->systemAddress.ToString(false);
    if (addrStr)
        snprintf(out->systemAddress, sizeof(out->systemAddress), "%s", addrStr);
    else
        out->systemAddress[0] = '\0';

    /* Raw IPv4 as host-byte-order uint32 */
    uint32_t raw = p->systemAddress.address.addr4.sin_addr.s_addr;
#if defined(_WIN32)
    out->systemAddressRaw = ntohl(raw);
#else
    out->systemAddressRaw = ntohl(raw);
#endif
}

extern "C" RAKNET_C_API
void raknet_peer_deallocate_packet(RakPeerHandle* peer, RakPacket* packet)
{
    if (peer && packet)
        toCpp(peer)->DeallocatePacket(toPacket(packet));
}

/* ================================================================== */
/*  Status / Diagnostics                                                */
/* ================================================================== */

extern "C" RAKNET_C_API
int raknet_peer_is_active(RakPeerHandle* peer)
{
    return (peer && toCpp(peer)->IsActive()) ? 1 : 0;
}

extern "C" RAKNET_C_API
unsigned short raknet_peer_get_bound_port(RakPeerHandle* peer,
                                          unsigned int   socketIndex)
{
    if (!peer) return 0;
    return toCpp(peer)->GetInternalID(
        RakNet::UNASSIGNED_SYSTEM_ADDRESS, (int)socketIndex).GetPort();
}

extern "C" RAKNET_C_API
unsigned int raknet_peer_get_connection_count(RakPeerHandle* peer)
{
    if (!peer) return 0;
    unsigned short count = 0;
    toCpp(peer)->GetConnectionList(nullptr, &count);
    return (unsigned int)count;
}

extern "C" RAKNET_C_API
int raknet_peer_get_average_ping(RakPeerHandle* peer,
                                 const char*    host,
                                 unsigned short port)
{
    if (!peer || !host) return -1;
    RakNet::SystemAddress addr;
    addr.FromStringExplicitPort(host, port);
    return toCpp(peer)->GetAveragePing(addr);
}

extern "C" RAKNET_C_API
const char* raknet_peer_get_local_ip(RakPeerHandle* peer,
                                     unsigned int   index,
                                     char*          buf,
                                     unsigned int   bufSize)
{
    if (!peer || !buf || bufSize == 0) return nullptr;
    const char* ip = toCpp(peer)->GetLocalIP(index);
    if (!ip) return nullptr;
    snprintf(buf, bufSize, "%s", ip);
    return buf;
}
