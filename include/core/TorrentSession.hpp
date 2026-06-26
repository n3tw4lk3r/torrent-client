#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "core/TorrentFile.hpp"
#include "core/TorrentTask.hpp"
#include "peer/PeerManager.hpp"
#include "utils/Timer.hpp"

namespace tclient {

class PieceStorage;
class TrackerManager;
class PeerConnector;
class DownloadMonitor;

class TorrentSession {
public:
    TorrentSession(
        const std::filesystem::path& torrent_file_path,
        const std::filesystem::path& output_directory,
        const std::filesystem::path& config_directory,
        std::string_view peer_id,
        int listen_port
    );

    ~TorrentSession();

    TorrentSession(const TorrentSession&) = delete;
    TorrentSession& operator=(const TorrentSession&) = delete;

    void Start();
    void Stop();
    bool IsFinished() const;

    TorrentTask GetCurrentTask() const;
    std::chrono::seconds ElapsedTime() const;

private:
    const std::filesystem::path kConfigDirectory;

    std::string peer_id;
    int listen_port;
    TorrentFile torrent_file;

    Timer timer;
    PeerManager peer_manager;

    mutable std::mutex task_mutex;
    TorrentTask current_task;

    std::unique_ptr<PieceStorage> piece_storage;
    std::unique_ptr<TrackerManager> tracker_manager;
    std::unique_ptr<PeerConnector> peer_connector;
    std::unique_ptr<DownloadMonitor> download_monitor;

    std::atomic<bool> is_finished = false;
    std::atomic<bool> is_terminated = false;

    std::thread session_thread;

private:
    void RunSession();
    void InitializeComponents(const TorrentFile& torrent_file);
    void CleanupComponents();
    void UpdateTaskStatus(TorrentStatus status);
};

} // namespace tclient

