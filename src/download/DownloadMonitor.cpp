#include "download/DownloadMonitor.hpp"

#include <thread>
#include <unordered_set>

#include "peer/PeerConnector.hpp"
#include "storage/PieceStorage.hpp"
#include "utils/Logger.hpp"

namespace tclient {

DownloadMonitor::DownloadMonitor(
    TorrentTask& task,
    PieceStorage& piece_storage,
    PeerConnector& peer_connector
) :
    task(task),
    piece_storage(piece_storage),
    peer_connector(peer_connector)
{}

void DownloadMonitor::UpdateTask() {
    size_t piece_length = 0;
    if (task.total_pieces_count > 0) {
        piece_length = task.total_size / task.total_pieces_count;
    }

    task.UpdateFromPieceStorage(piece_storage, piece_length);

    auto active_ids = peer_connector.GetActivePeerIds();
    std::unordered_set<std::string> unique_active(
        active_ids.begin(),
        active_ids.end()
    );

    task.SetConnectedPeers(static_cast<int>(unique_active.size()));
    task.total_peers_count = static_cast<int>(peer_connector.peer_manager.Count());
    task.last_update = std::chrono::system_clock::now();
}

void DownloadMonitor::ManageEndgameMode(
    bool& endgame_mode,
    std::chrono::steady_clock::time_point& last_requeue_time
) {
    size_t missing_count = piece_storage.GetMissingPieces().size();

    if (!endgame_mode && missing_count <= kEndgameThreshold) {
        endgame_mode = true;
        Logger::LogUi(
            "Entering endgame mode - " +
            std::to_string(missing_count) +
            " pieces remaining"
        );
    }

    if (endgame_mode && piece_storage.QueueIsEmpty()) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_requeue_time > kRequeueInterval) {
            Logger::LogUi(
                "Endgame: requeuing " +
                std::to_string(missing_count) +
                " missing pieces"
            );
            piece_storage.ForceRequeueMissingPieces();
            last_requeue_time = now;
        }
    }
}

void DownloadMonitor::WaitForCompletion() {
    using namespace std::chrono_literals;

    bool endgame_mode = false;
    auto last_requeue_time = std::chrono::steady_clock::now();
    auto last_status_update = std::chrono::steady_clock::now();

    while (!piece_storage.IsDownloadComplete()) {
        auto now = std::chrono::steady_clock::now();

        if (now - last_status_update > kStatusUpdateInterval) {
            UpdateTask();
            last_status_update = now;
        }

        ManageEndgameMode(endgame_mode, last_requeue_time);

        if (!piece_storage.HasActiveWork()) {
            std::this_thread::sleep_for(500ms);
        } else {
            std::this_thread::sleep_for(50ms);
        }
    }

    UpdateTask();
    is_finished = true;
}

bool DownloadMonitor::IsFinished() const {
    return is_finished.load();
}

} // namespace tclient

