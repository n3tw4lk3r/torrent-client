#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include "core/TorrentTask.hpp"

namespace tclient {

class TorrentSession;

class TorrentClient {
private:
    static constexpr size_t kPeerIdLength = 20;
    static constexpr std::string kBaseSelfPeerId = "TCLIENT-";
    static constexpr size_t kSelfPeerIdSuffixLength =
        kPeerIdLength - kBaseSelfPeerId.size();

public:
    explicit TorrentClient();
    ~TorrentClient();

    void DownloadTorrent(
        const std::filesystem::path& torrent_file_path,
        const std::filesystem::path& output_directory,
        const std::filesystem::path& config_directory
    );

    const std::string& GetPeerId() const;
    TorrentTask GetCurrentTask() const;
    std::vector<std::string> GetLogMessages(size_t max_count = 50) const;

    bool IsFinished() const;
    std::chrono::seconds ElapsedTime() const;

private:
    static constexpr int kListenPort = 77777;

    std::mt19937 random_engine;
    std::uniform_int_distribution<int> char_dist =
        std::uniform_int_distribution<int>('a', 'z');

    std::string peer_id;

    mutable std::mutex session_mutex;

    std::unique_ptr<TorrentSession> active_session;

    std::atomic<bool> is_terminated = false;

private:
    std::string GenerateRandomSuffix(size_t length);
};

} // namespace tclient

