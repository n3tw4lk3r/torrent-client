#include "core/UdpTracker.hpp"

#include <cstring>
#include <stdexcept>

#include "utils/byte_tools.hpp"

UdpTracker::UdpTracker(
    const std::string& host,
    int port,
    int timeout_sec
) :
    host(host),
    port(port),
    timeout_sec(timeout_sec),
    udp_client(host, port, timeout_sec)
{}

UdpTracker::TrackerResponse UdpTracker::Announce(
    const std::string& info_hash,
    const std::string& peer_id,
    uint64_t downloaded,
    uint64_t left,
    uint64_t uploaded,
    int event,
    int num_want,
    uint16_t port
) {
    uint64_t connection_id = Connect();

    return AnnounceWithConnection(
        connection_id,
        info_hash,
        peer_id,
        downloaded,
        left,
        uploaded,
        event,
        num_want,
        port
    );
}

uint64_t UdpTracker::Connect() {
    uint64_t protocol_id = 0x41727101980;
    uint32_t action = 0; // connect = 0
    uint32_t transaction_id = utils::bytes_to_int32_t(
        utils::int32_t_to_bytes(rand())
    );

    std::string request;
    request.reserve(16);

    request += utils::uint64_t_to_bytes(protocol_id);
    request += utils::int32_t_to_bytes(action);
    request += utils::int32_t_to_bytes(transaction_id);

    std::string response = udp_client.SendReceive(request);

    if (response.size() < 16) {
        throw std::runtime_error(
            "CONNECT response too small: " +
            std::to_string(response.size())
        );
    }

    uint32_t resp_action = utils::bytes_to_int32_t(response.substr(0, 4));
    uint32_t resp_trans = utils::bytes_to_int32_t(response.substr(4, 4));

    if (resp_action != 0) {
        throw std::runtime_error(
            "CONNECT failed: action=" +
            std::to_string(resp_action)
        );
    }

    if (resp_trans != transaction_id) {
        throw std::runtime_error("CONNECT transaction_id mismatch");
    }

    uint64_t connection_id = utils::bytes_to_uint64_t(response.substr(8, 8));

    return connection_id;
}

UdpTracker::TrackerResponse UdpTracker::AnnounceWithConnection(
    uint64_t connection_id,
    const std::string& info_hash,
    const std::string& peer_id,
    uint64_t downloaded,
    uint64_t left,
    uint64_t uploaded,
    int event,
    int num_want,
    uint16_t port
) {
    if (info_hash.size() != 20) {
        throw std::runtime_error("Info_hash must be 20 bytes");
    }
    if (peer_id.size() != 20) {
        throw std::runtime_error("Peer_id must be 20 bytes");
    }

    uint32_t action = 1; // announce
    uint32_t transaction_id = utils::bytes_to_int32_t(
        utils::int32_t_to_bytes(rand())
    );

    std::string request;
    request.reserve(98);

    request += utils::uint64_t_to_bytes(connection_id);
    request += utils::int32_t_to_bytes(action);
    request += utils::int32_t_to_bytes(transaction_id);
    request += info_hash;
    request += peer_id;
    request += utils::uint64_t_to_bytes(downloaded);
    request += utils::uint64_t_to_bytes(left);
    request += utils::uint64_t_to_bytes(uploaded);
    
    // 0=none,1=completed,2=started,3=stopped
    request += utils::int32_t_to_bytes(event);
    
    request += utils::int32_t_to_bytes(0); // IP = default
    request += utils::int32_t_to_bytes(rand()); // key
    request += utils::int32_t_to_bytes(num_want);
    request += utils::int32_t_to_bytes(port << 16 | (port & 0xFFFF));

    std::string response = udp_client.SendReceive(request);

    if (response.size() < 20) {
        throw std::runtime_error(
            "ANNOUNCE response too small: " +
            std::to_string(response.size())
        );
    }

    uint32_t resp_action = utils::bytes_to_int32_t(response.substr(0, 4));
    uint32_t resp_trans  = utils::bytes_to_int32_t(response.substr(4, 4));

    if (resp_action == 3) { // error
        std::string err_msg = response.substr(8);
        throw std::runtime_error("Tracker error: " + err_msg);
    }

    if (resp_action != 1) {
        throw std::runtime_error(
            "ANNOUNCE failed: wrong action=" +
            std::to_string(resp_action)
        );
    }

    if (resp_trans != transaction_id) {
        throw std::runtime_error("ANNOUNCE transaction_id mismatch");
    }

    TrackerResponse tracker_response;
    tracker_response.interval = utils::bytes_to_int32_t(response.substr(8, 4));
    tracker_response.leechers = utils::bytes_to_int32_t(response.substr(12, 4));
    tracker_response.seeders = utils::bytes_to_int32_t(response.substr(16, 4));

    size_t offset = 20;
    while (offset + 6 <= response.size()) {
        TrackerPeer peer;

        peer.ip = utils::bytes_to_int32_t(response.substr(offset, 4));
        peer.port = static_cast<uint16_t>(
            (static_cast<unsigned char>(response[offset + 4]) << 8) |
            static_cast<unsigned char>(response[offset + 5])
        );

        tracker_response.peers.push_back(peer);
        offset += 6;
    }

    return tracker_response;
}

