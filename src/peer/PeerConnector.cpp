#include "peer/PeerConnector.hpp"

#include <algorithm>
#include <thread>

#include "peer/PeerSession.hpp"
#include "utils/Logger.hpp"

using namespace std::chrono_literals;

namespace tclient {

PeerConnector::PeerConnector(
    std::string_view self_peer_id,
    PeerManager& peer_manager,
    PieceStorage& piece_storage
) :
    self_peer_id(self_peer_id),
    peer_manager(peer_manager),
    piece_storage(piece_storage)
{}

PeerConnector::~PeerConnector() {
    Stop();
}

bool PeerConnector::ConnectToPeer(
    const Peer& peer,
    const TorrentFile& torrent_file
) {
    size_t actual_connected = ActiveConnectionCount();
    if (actual_connected >= kMaxConnections) {
        Logger::LogUi(
            "Max connections reached (" +
            std::to_string(actual_connected) +
            "/" +
            std::to_string(kMaxConnections) +
            "), not connecting to " +
            peer.ip +
            ":" +
            std::to_string(peer.port)
        );
        return false;
    }

    try {
        auto session = std::make_shared<PeerSession>(
            peer,
            torrent_file,
            self_peer_id,
            piece_storage
        );

        session->Start();

        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions.emplace_back(session);
        }

        peer_manager.MarkConnected(peer);
        return true;

    } catch (const std::exception& error) {
        peer_manager.MarkConnectionFailed(peer);
        Logger::LogError(
            "Failed to connect to peer " +
            peer.ip +
            ":" +
            std::to_string(peer.port) +
            " - " +
            std::string(error.what())
        );
        return false;
    }
}

void PeerConnector::CleanupTerminatedConnections() {
    std::lock_guard<std::mutex> lock(sessions_mutex);

    for (auto& session : sessions) {
        if (session && session->IsTerminated()) {
            Peer peer = session->GetPeer();
            if (!peer.ip.empty() && peer.port > 0) {
                peer_manager.MarkConnectionFailed(peer);
            }
        }
    }

    sessions.erase(
        std::remove_if(
            sessions.begin(),
            sessions.end(),
            [](const auto& session) {
                return !session || session->IsTerminated();
            }
        ),
        sessions.end()
    );
}

void PeerConnector::DiscoveryLoop(TorrentFile torrent_file) {
    while (!is_stopped) {
        CleanupTerminatedConnections();

        size_t actual_connected = ActiveConnectionCount();
        size_t total_sessions = sessions.size();

        if (actual_connected < kMaxConnections) {
            size_t available_slots = kMaxConnections - actual_connected;
            auto ready_peers = peer_manager.TakeNewPeers();

            size_t to_connect = std::min(ready_peers.size(), available_slots);

            Logger::LogUi(
                "Trying to connect " +
                std::to_string(to_connect) +
                " new peers (slots available: " +
                std::to_string(available_slots) +
                ")"
            );

            for (size_t i = 0; i < to_connect && !is_stopped; ++i) {
                ConnectToPeer(ready_peers[i], torrent_file);
            }
        } else {
            Logger::LogUi(
                "Max connections reached (" +
                std::to_string(actual_connected) +
                "/" +
                std::to_string(kMaxConnections) +
                "), waiting..."
            );
        }

        auto deadline = std::chrono::steady_clock::now() + kPeerDiscoveryInterval;
        while (std::chrono::steady_clock::now() < deadline && !is_stopped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void PeerConnector::StartDiscovery(const TorrentFile& torrent_file) {
    is_stopped = false;
    discovery_thread = std::thread(
        &PeerConnector::DiscoveryLoop,
        this,
        torrent_file
    );
}

void PeerConnector::Stop() {
    is_stopped = true;

    if (discovery_thread.joinable()) {
        discovery_thread.join();
    }

    std::vector<std::shared_ptr<PeerSession>> sessions_copy;
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        sessions_copy = sessions;
        sessions.clear();
    }

    for (auto& session : sessions_copy) {
        if (session) {
            session->Stop();
        }
    }
}

size_t PeerConnector::ActiveConnectionCount() const {
    std::lock_guard<std::mutex> lock(sessions_mutex);

    size_t count = 0;
    for (const auto& session : sessions) {
        if (session && session->IsConnected() && !session->IsTerminated()) {
            ++count;
        }
    }
    return count;
}

size_t PeerConnector::GetMaxConnections() const {
    return kMaxConnections;
}

std::vector<std::string> PeerConnector::GetActivePeerIds() const {
    std::lock_guard<std::mutex> lock(sessions_mutex);

    std::vector<std::string> ids;
    ids.reserve(sessions.size());
    for (const auto& session : sessions) {
        if (session && session->IsConnected() && !session->IsTerminated()) {
            ids.push_back(session->GetPeerId());
        }
    }
    return ids;
}

} // namespace tclient

