#pragma once

#include <atomic>
#include <filesystem>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "core/TorrentFile.hpp"
#include "peer/PeerManager.hpp"

namespace tclient {

class PeerConnector;

class TrackerManager {
public:
    TrackerManager(
        std::string_view self_peer_id,
        int listen_port,
        PeerManager& peer_manager,
        const std::filesystem::path& config_directory
    );

    ~TrackerManager();

    void SetPeerConnector(PeerConnector* connector);
    bool FetchInitialPeers(const TorrentFile& torrent_file);
    void StartBackgroundUpdates(const TorrentFile& torrent_file);
    void Stop();
    size_t TrackerCount() const;

private:
    static constexpr std::chrono::seconds kUpdateInterval =
        std::chrono::seconds(15);

    std::string self_peer_id;
    int listen_port;
    PeerManager& peer_manager;
    PeerConnector* peer_connector;

    std::vector<std::string> default_trackers;
    std::vector<std::string> tracker_urls;
    std::atomic<bool> is_stopped = false;
    std::thread background_thread;

private:
    void LoadDefaultTrackersFromConfig(const std::filesystem::path& tracker_config_path);

    std::vector<std::string> CollectTrackerUrls(const TorrentFile& torrent_file);

    bool TryAnnounceToTracker(
        std::string_view url,
        const TorrentFile& torrent_file
    );

    void BackgroundUpdateLoop(TorrentFile torrent_file);
};

} // namespace tclient

