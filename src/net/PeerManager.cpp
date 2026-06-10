#include "net/PeerManager.hpp"

std::string PeerManager::MakeKey(const Peer& peer) {
    return peer.ip + ":" + std::to_string(peer.port);
}

void PeerManager::AddPeers(const std::vector<Peer>& prs) {
    std::lock_guard<std::mutex> lock(mutex);

    for (const auto& peer : prs) {
        const auto key = MakeKey(peer);

        if (known_peers.insert(key).second) {
            peers.push_back(peer);
        }
    }
}

std::vector<Peer> PeerManager::TakeNewPeers() {
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<Peer> result;
    for (const auto& peer : peers) {
        const auto key = MakeKey(peer);

        if (!connected_peers.contains(key)) {
            connected_peers.insert(key);
            result.push_back(peer);
        }
    }

    return result;
}

std::vector<Peer> PeerManager::GetAllPeers() const {
    std::lock_guard<std::mutex> lock(mutex);
    return peers;
}

std::vector<Peer> PeerManager::GetUnconnectedPeers() {
    std::lock_guard<std::mutex> lock(mutex);

    std::vector<Peer> result;
    for (const auto& peer : peers) {
        const auto key = MakeKey(peer);

        if (!connected_peers.contains(key)) {
            result.push_back(peer);
        }
    }

    return result;
}

void PeerManager::MarkConnected(const Peer& peer) {
    std::lock_guard<std::mutex> lock(mutex);

    connected_peers.insert(MakeKey(peer));
}

size_t PeerManager::Count() const {
    std::lock_guard<std::mutex> lock(mutex);
    return peers.size();
}

void PeerManager::Clear() {
    std::lock_guard<std::mutex> lock(mutex);

    peers.clear();
    known_peers.clear();
    connected_peers.clear();
}

