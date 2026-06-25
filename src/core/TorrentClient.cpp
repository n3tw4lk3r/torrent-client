#include "core/TorrentClient.hpp"

#include <chrono>
#include <random>

#include "core/TorrentSession.hpp"
#include "utils/Logger.hpp"

namespace tclient {

TorrentClient::TorrentClient() :
    random_engine(std::random_device()()),
    peer_id(kBaseSelfPeerId + GenerateRandomSuffix(kSelfPeerIdSuffixLength))
{
    Logger::LogUi("Torrent client initialized");
}

TorrentClient::~TorrentClient() {
    std::lock_guard<std::mutex> lock(session_mutex);

    if (active_session) {
        active_session->Stop();
        active_session.reset();
    }
}

std::string TorrentClient::GenerateRandomSuffix(size_t length) {
    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result.push_back(static_cast<char>(char_dist(random_engine)));
    }
    return result;
}

void TorrentClient::DownloadTorrent(
    const std::filesystem::path& torrent_file_path,
    const std::filesystem::path& output_directory
) {
    std::lock_guard<std::mutex> lock(session_mutex);

    is_terminated = false;

    active_session = std::make_unique<TorrentSession>(
        torrent_file_path,
        output_directory,
        peer_id,
        kListenPort
    );

    active_session->Start();
}

const std::string& TorrentClient::GetPeerId() const {
    return peer_id;
}

TorrentTask TorrentClient::GetCurrentTask() const {
    std::lock_guard<std::mutex> lock(session_mutex);

    if (active_session) {
        return active_session->GetCurrentTask();
    }

    TorrentTask empty;
    return empty;
}

std::vector<std::string> TorrentClient::GetLogMessages(size_t max_count) const {
    return Logger::GetMessages(max_count);
}

bool TorrentClient::IsFinished() const {
    std::lock_guard<std::mutex> lock(session_mutex);

    if (active_session) {
        return active_session->IsFinished();
    }
    return true;
}

std::chrono::seconds TorrentClient::ElapsedTime() const {
    std::lock_guard<std::mutex> lock(session_mutex);

    if (active_session) {
        return active_session->ElapsedTime();
    }
    return std::chrono::seconds(0);
}

} // namespace tclient

