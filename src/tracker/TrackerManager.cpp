#include "tracker/TrackerManager.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "peer/PeerConnector.hpp"
#include "tracker/TrackerFactory.hpp"
#include "utils/Logger.hpp"

namespace tclient {

TrackerManager::TrackerManager(
    std::string_view self_peer_id,
    int listen_port,
    PeerManager& peer_manager
) :
    self_peer_id(self_peer_id),
    listen_port(listen_port),
    peer_manager(peer_manager),
    peer_connector(nullptr)
{}

TrackerManager::~TrackerManager() {
    Stop();
}

void TrackerManager::SetPeerConnector(PeerConnector* connector) {
    peer_connector = connector;
}

std::vector<std::string> TrackerManager::CollectTrackerUrls(
    const TorrentFile& torrent_file
) {
    std::vector<std::string> urls;
    urls.reserve(kDefaultTrackers.size() + 1);
    if (!torrent_file.announce.empty()) {
        urls.push_back(torrent_file.announce);
    }

    for (const auto& tracker : kDefaultTrackers) {
        urls.emplace_back(tracker);
    }

    std::sort(urls.begin(), urls.end());
    urls.erase(std::unique(urls.begin(), urls.end()), urls.end());
    return urls;
}

bool TrackerManager::TryAnnounceToTracker(
    std::string_view url,
    const TorrentFile& torrent_file
) {
    try {
        auto tracker = TrackerFactory::CreateTracker(std::string(url));
        Logger::LogUi("Requesting peers from " + std::string(url) + "...");

        auto peers = tracker->Announce(
            torrent_file,
            self_peer_id,
            0, // downloaded
            torrent_file.length, // left
            0, // uploaded
            2, // event
            static_cast<uint16_t>(listen_port)
        );

        peer_manager.AddPeers(peers);

        Logger::LogUi(
            "Got " +
            std::to_string(peers.size()) +
            " peers from " + std::string(url)
        );

        return !peers.empty();
    } catch (const std::exception& error) {
        Logger::LogUi("Tracker " + std::string(url) + " error: " + error.what());
        return false;
    }
}

bool TrackerManager::FetchInitialPeers(const TorrentFile& torrent_file) {
    tracker_urls = CollectTrackerUrls(torrent_file);
    Logger::LogUi(
        "Using " +
        std::to_string(tracker_urls.size()) +
        " trackers"
    );

    for (
        size_t i = 0;
        i < tracker_urls.size() && peer_manager.Count() == 0 && !is_stopped;
        ++i
    ) {
        TryAnnounceToTracker(tracker_urls[i], torrent_file);
    }

    return peer_manager.Count() > 0;
}

void TrackerManager::BackgroundUpdateLoop(TorrentFile torrent_file) {
    while (!is_stopped) {
        size_t active_connections = 0;
        if (peer_connector) {
            active_connections = peer_connector->ActiveConnectionCount();
        }

        size_t known_peers = peer_manager.Count();

        if (active_connections < 50 && known_peers < 200) {
            Logger::LogUi(
                "Requesting more peers (active: " +
                std::to_string(active_connections) + "/50)..."
            );

            for (size_t i = 0; i < tracker_urls.size() && !is_stopped; ++i) {
                if (peer_connector) {
                    active_connections = peer_connector->ActiveConnectionCount();
                }
                if (active_connections >= 50) {
                    Logger::LogUi(
                        "Reached 50 active connections, stopping tracker requests"
                    );
                    break;
                }

                try {
                    auto tracker = TrackerFactory::CreateTracker(tracker_urls[i]);
                    auto peers = tracker->Announce(
                        torrent_file,
                        self_peer_id,
                        0, // downloaded
                        torrent_file.length, // left
                        0, // uploaded
                        0, // event (0 = none, for updates)
                        static_cast<uint16_t>(listen_port)
                    );

                    auto before = peer_manager.Count();
                    peer_manager.AddPeers(peers);
                    auto after = peer_manager.Count();

                    if (after > before) {
                        Logger::LogUi(
                            "Discovered " +
                            std::to_string(after - before) +
                            " new peers from " +
                            tracker_urls[i] +
                            " (total known: " + std::to_string(after) + ")"
                        );
                    }
                } catch (const std::exception& error) {
                    Logger::LogUi(
                        "Background tracker update error: " +
                        std::string(error.what())
                    );
                }
            }
        } else if (active_connections >= 50) {
            Logger::LogUi(
                "Have " +
                std::to_string(active_connections) +
                " active connections, skipping tracker update"
            );
        } else {
            Logger::LogUi(
                "Have " +
                std::to_string(known_peers) +
                " known peers, enough in pool"
            );
        }

        auto deadline = std::chrono::steady_clock::now() + kUpdateInterval;
        while (std::chrono::steady_clock::now() < deadline && !is_stopped) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void TrackerManager::StartBackgroundUpdates(const TorrentFile& torrent_file) {
    is_stopped = false;
    background_thread = std::thread([this, torrent_file]() {
        BackgroundUpdateLoop(torrent_file);
    });
}

void TrackerManager::Stop() {
    is_stopped = true;

    if (background_thread.joinable()) {
        background_thread.join();
    }
}

size_t TrackerManager::TrackerCount() const {
    return tracker_urls.size();
}

} // namespace tclient

