#include "storage/PieceStorage.hpp"

#include <algorithm>
#include <stdexcept>

#include "utils/Logger.hpp"

namespace tclient {

PieceStorage::PieceStorage(
    const TorrentFile& torrent_file,
    const std::filesystem::path& output_directory
) :
    output_directory(output_directory),
    default_piece_length(torrent_file.piece_length),
    total_piece_count(torrent_file.piece_hashes.size()),
    torrent_file(torrent_file)
{
    for (size_t i = 0; i < total_piece_count; ++i) {
        size_t len;
        if (i + 1 == total_piece_count) {
            len = torrent_file.length - i * torrent_file.piece_length;
        } else {
            len = torrent_file.piece_length;
        }

        remaining_pieces_queue.push(
            std::make_shared<Piece>(i, len, std::move(torrent_file.piece_hashes[i]))
        );
    }

    InitializeOutputFile();
}

void PieceStorage::InitializeOutputFile() {
    std::filesystem::create_directories(output_directory);
    auto filename = (output_directory / torrent_file.name).string();

    output_file.open(filename, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!output_file.is_open()) {
        throw std::runtime_error("Failed to open output file");
    }

    if (torrent_file.length > 0) {
        output_file.seekp(static_cast<std::streamoff>(torrent_file.length - 1));
        char zero = '\0';
        output_file.write(&zero, 1);
        output_file.seekp(0);
    }
}

PiecePtr PieceStorage::GetNextPieceToDownload() {
    std::lock_guard<std::mutex> lock(queue_mutex);

    if (remaining_pieces_queue.empty()) {
        return nullptr;
    }

    auto piece = remaining_pieces_queue.front();
    remaining_pieces_queue.pop();
    active_pieces.insert(piece->GetIndex());
    return piece;
}

void PieceStorage::ReturnPiece(const PiecePtr& piece) {
    if (!piece) {
        return;
    }

    if (IsPieceAlreadySaved(piece->GetIndex())) {
        return;
    }

    piece->Reset();

    std::lock_guard<std::mutex> lock(queue_mutex);

    active_pieces.erase(piece->GetIndex());
    remaining_pieces_queue.push(piece);
}

void PieceStorage::PieceProcessed(const PiecePtr& piece) {
    if (!piece) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        active_pieces.erase(piece->GetIndex());
    }

    if (!piece->HashMatches()) {
        Logger::LogUi(
            "Piece " +
            std::to_string(piece->GetIndex()) +
            " hash mismatch, re-downloading"
        );
        ReturnPiece(piece);
        return;
    }

    SavePieceToDisk(piece);
}

void PieceStorage::SavePieceToDisk(const PiecePtr& piece) {
    std::lock_guard<std::mutex> lock(file_mutex);

    if (saved_pieces.contains(piece->GetIndex())) {
        return;
    }

    output_file.seekp(
        static_cast<std::streamoff>(piece->GetIndex() * default_piece_length)
    );

    const auto& data = piece->GetData();
    output_file.write(data.data(), static_cast<std::streamsize>(data.size()));

    saved_pieces.insert(piece->GetIndex());
}

bool PieceStorage::QueueIsEmpty() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return remaining_pieces_queue.empty();
}

bool PieceStorage::IsPieceAlreadySaved(size_t index) const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return saved_pieces.contains(index);
}

bool PieceStorage::IsDownloadComplete() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return saved_pieces.size() == total_piece_count;
}

bool PieceStorage::HasActiveWork() const {
    std::lock_guard<std::mutex> lock(queue_mutex);
    return !remaining_pieces_queue.empty() || !active_pieces.empty();
}

size_t PieceStorage::TotalPiecesCount() const {
    return total_piece_count;
}

size_t PieceStorage::PiecesSavedToDiskCount() const {
    std::lock_guard<std::mutex> lock(file_mutex);
    return saved_pieces.size();
}

std::vector<size_t> PieceStorage::GetMissingPieces() const {
    std::lock_guard<std::mutex> lock(file_mutex);

    std::vector<size_t> missing;
    missing.reserve(total_piece_count - saved_pieces.size());

    for (size_t i = 0; i < total_piece_count; ++i) {
        if (!saved_pieces.contains(i)) {
            missing.push_back(i);
        }
    }
    return missing;
}

void PieceStorage::ForceRequeueMissingPieces() {
    std::lock_guard<std::mutex> qlock(queue_mutex);
    std::lock_guard<std::mutex> flock(file_mutex);

    active_pieces.clear();

    std::queue<PiecePtr> empty;
    std::swap(remaining_pieces_queue, empty);

    for (size_t i = 0; i < total_piece_count; ++i) {
        if (saved_pieces.contains(i)) continue;

        size_t len;
        if (i + 1 == total_piece_count) {
            len = torrent_file.length - i * torrent_file.piece_length;
        } else {
            len = torrent_file.piece_length;
        }

        remaining_pieces_queue.push(
            std::make_shared<Piece>(i, len, torrent_file.piece_hashes[i])
        );
    }
}

void PieceStorage::CloseOutputFile() {
    std::lock_guard<std::mutex> lock(file_mutex);
    if (output_file.is_open()) {
        output_file.flush();
        output_file.close();
    }
}

} // namespace tclient

