#include "core/TorrentSession.hpp"

#include "core/TorrentFile.hpp"
#include "download/DownloadMonitor.hpp"
#include "peer/PeerConnector.hpp"
#include "storage/PieceStorage.hpp"
#include "tracker/TrackerManager.hpp"
#include "utils/Logger.hpp"

namespace tclient {

TorrentSession::TorrentSession(
    const std::filesystem::path& torrent_file_path,
    const std::filesystem::path& output_directory,
    std::string_view peer_id,
    int listen_port
) :
    peer_id(peer_id),
    listen_port(listen_port)
{
    current_task.start_time = std::chrono::system_clock::now();
    current_task.filename = torrent_file_path.filename().string();
    current_task.output_file_path = output_directory.string();
    current_task.status = TorrentStatus::kNoTorrent;

    torrent_file = TorrentFile::Load(torrent_file_path.string());

    current_task.total_size = torrent_file.length;
    current_task.info_hash = torrent_file.info_hash;
    current_task.announce_url = torrent_file.announce;
    current_task.total_pieces_count = torrent_file.piece_hashes.size();
}

TorrentSession::~TorrentSession() {
    if (session_thread.joinable()) {
        session_thread.join();
    }
}

void TorrentSession::Start() {
    if (is_terminated.load()) {
        return;
    }

    is_terminated = false;
    is_finished = false;
    timer.Start();

    session_thread = std::thread(&TorrentSession::RunSession, this);
}

void TorrentSession::RunSession() {
    UpdateTaskStatus(TorrentStatus::kLoading);
    Logger::LogUi("Loading torrent file: " + current_task.filename);

    Logger::LogUi(
        "File: " +
        torrent_file.name +
        " (" +
        std::to_string(torrent_file.length) +
        " bytes, " +
        std::to_string(torrent_file.piece_hashes.size()) +
        " pieces)"
    );

    InitializeComponents(torrent_file);

    UpdateTaskStatus(TorrentStatus::kConnected);
    Logger::LogUi("Connecting to trackers...");

    if (!tracker_manager->FetchInitialPeers(torrent_file)) {
        Logger::LogUi("No peers found, unable to start download");
        UpdateTaskStatus(TorrentStatus::kError);
        timer.Stop();
        CleanupComponents();
        is_finished = true;
        return;
    }

    {
        std::lock_guard<std::mutex> lock(task_mutex);
        current_task.total_peers_count = static_cast<int>(peer_manager.Count());
    }

    Logger::LogUi(
        "Starting download with " +
        std::to_string(peer_manager.Count()) +
        " peers"
    );

    UpdateTaskStatus(TorrentStatus::kDownloading);
    tracker_manager->StartBackgroundUpdates(torrent_file);

    for (const auto& peer : peer_manager.TakeNewPeers()) {
        peer_connector->ConnectToPeer(peer, torrent_file);
    }

    peer_connector->StartDiscovery(torrent_file);

    try {
        download_monitor->WaitForCompletion();
    } catch (const std::exception& error) {
        UpdateTaskStatus(TorrentStatus::kError);
        Logger::LogUi("Download error: " + std::string(error.what()));
    }

    timer.Stop();

    if (peer_connector) {
        peer_connector->Stop();
    }
    if (tracker_manager) {
        tracker_manager->Stop();
    }

    download_monitor->UpdateTask();

    if (piece_storage->IsDownloadComplete()) {
        UpdateTaskStatus(TorrentStatus::kCompleted);
        Logger::LogUi("Download completed successfully");
    } else {
        UpdateTaskStatus(TorrentStatus::kError);
        Logger::LogUi("Download incomplete - missing pieces");
    }

    if (piece_storage) {
        piece_storage->CloseOutputFile();
    }

    is_finished = true;

    Logger::LogUi("Session finished");
}

void TorrentSession::InitializeComponents(const TorrentFile& tf) {
    piece_storage = std::make_unique<PieceStorage>(
        tf,
        current_task.output_file_path
    );

    tracker_manager = std::make_unique<TrackerManager>(
        peer_id,
        listen_port,
        peer_manager
    );

    peer_connector = std::make_unique<PeerConnector>(
        peer_id,
        peer_manager,
        *piece_storage
    );

    tracker_manager->SetPeerConnector(peer_connector.get());

    download_monitor = std::make_unique<DownloadMonitor>(
        current_task,
        *piece_storage,
        *peer_connector
    );
}

void TorrentSession::CleanupComponents() {
    if (peer_connector) {
        peer_connector->Stop();
    }

    if (tracker_manager) {
        tracker_manager->Stop();
    }

    if (piece_storage) {
        piece_storage->CloseOutputFile();
    }
}

void TorrentSession::Stop() {
    is_terminated = true;

    if (session_thread.joinable()) {
        session_thread.join();
    }

    is_finished = true;
}

bool TorrentSession::IsFinished() const {
    return is_finished.load();
}

TorrentTask TorrentSession::GetCurrentTask() const {
    std::lock_guard<std::mutex> lock(task_mutex);
    return current_task;
}

std::chrono::seconds TorrentSession::ElapsedTime() const {
    return std::chrono::duration_cast<std::chrono::seconds>(timer.Elapsed());
}

void TorrentSession::UpdateTaskStatus(TorrentStatus status) {
    std::lock_guard<std::mutex> lock(task_mutex);
    current_task.status = status;
    current_task.last_update = std::chrono::system_clock::now();
}

} // namespace tclient

