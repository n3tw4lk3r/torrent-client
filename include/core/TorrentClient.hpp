#pragma once

#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <vector>

#include "core/HttpTracker.hpp"
#include "core/PieceStorage.hpp"
#include "core/TorrentFile.hpp"
#include "core/TorrentTask.hpp"
#include "net/PeerConnection.hpp"
#include "net/PeerManager.hpp"
#include "utils/Timer.hpp"

class TorrentClient {
public:
    explicit TorrentClient(const std::string& peer_id = "TESTAPPDONTWORRY");
    ~TorrentClient() = default;

    void DownloadTorrent(
        const std::filesystem::path& torrent_file_path,
        const std::filesystem::path& output_directory
    );

    const std::string& GetPeerId() const { return peer_id; }
    void SetPeerId(const std::string& new_peer_id) { peer_id = new_peer_id; }

    TorrentTask GetCurrentTask() const;
    std::vector<std::string> GetLogMessages(size_t max_count = 50) const;
    
    bool IsDownloading() const;
    bool IsFinished() const;
    
    std::chrono::seconds ElapsedTime() const;

private:
    static constexpr int kPiecesLeftToEnterEndgame = 20;

    std::string peer_id;
    std::atomic<bool> is_terminated{false};

    mutable std::mutex task_mutex;
    TorrentTask current_task;

    mutable std::mutex log_mutex;
    std::vector<std::string> log_messages;

    mutable std::mutex peer_connections_mutex;
    std::vector<std::shared_ptr<PeerConnection>> peer_connections;
    
    Timer timer;
    PeerManager peer_manager;

    static constexpr std::array<const char*, 4> kDefaultTrackers = {
        "udp://tracker.opentrackr.org:1337/announce",
        "udp://open.stealth.si:80/announce",
        "udp://exodus.desync.com:6969/announce",
        "udp://tracker.torrent.eu.org:451/announce"
    };

private:
    void AddLogMessage(const std::string& message);
    void UpdateTaskStatus(TorrentStatus status);
    void UpdateTaskFromPieceStorage(const PieceStorage& storage);
    void UpdateTaskFromTracker(const HttpTracker& tracker);

    std::string GenerateRandomSuffix(size_t length = 4);

    bool RunDownloadMultithread(
        PieceStorage& pieces,
        const TorrentFile& torrent_file,
        const HttpTracker& tracker
    );

    void DownloadFromTracker(
        const TorrentFile& torrent_file,
        PieceStorage& pieces
    );
};

