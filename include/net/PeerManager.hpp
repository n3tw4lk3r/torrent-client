#pragma once

#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

#include "net/Peer.hpp"

class PeerManager {
public:
    void AddPeers(const std::vector<Peer>& peers);
    std::vector<Peer> TakeNewPeers();

    std::vector<Peer> GetAllPeers() const;
    std::vector<Peer> GetUnconnectedPeers();

    void MarkConnected(const Peer& peer);

    size_t Count() const;

    void Clear();

private:
    static std::string MakeKey(const Peer& peer);

private:
    mutable std::mutex mutex;

    std::vector<Peer> peers;

    std::unordered_set<std::string> known_peers;
    std::unordered_set<std::string> connected_peers;
};

