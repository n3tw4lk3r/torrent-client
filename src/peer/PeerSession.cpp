#include "peer/PeerSession.hpp"

#include <thread>

#include "peer/Message.hpp"
#include "storage/PieceStorage.hpp"
#include "utils/byte_tools.hpp"
#include "utils/Logger.hpp"

using namespace std::chrono_literals;

namespace tclient {

PeerSession::PeerPiecesAvailability::PeerPiecesAvailability(
    std::string bitfield,
    size_t size
) :
    bitfield(std::move(bitfield)),
    size(size)
{}

bool PeerSession::PeerPiecesAvailability::IsPieceAvailable(size_t index) const {
    if (index >= (size << 3)) {
        return false;
    }
    return (bitfield[index >> 3] >> (7 - (index & 7))) & 1;
}

void PeerSession::PeerPiecesAvailability::SetPieceAvailable(size_t index) {
    if (index < size * 8) {
        bitfield[index >> 3] |= (1 << (7 - (index & 7)));
    }
}

PeerSession::PeerSession(
    const Peer& peer,
    const TorrentFile& torrent_file,
    std::string self_peer_id,
    PieceStorage& piece_storage
) :
    torrent_file(torrent_file),
    tcp_connection(peer.ip, peer.port),
    self_peer_id(std::move(self_peer_id)),
    piece_storage(piece_storage)
{}

PeerSession::~PeerSession() {
    Stop();
}

void PeerSession::Start() {
    if (worker_thread && worker_thread->joinable()) {
        return;
    }

    is_terminated = false;
    is_connected = false;
    worker_thread = std::make_unique<std::thread>([this]() {
        RunLoop();
    });
}

void PeerSession::Stop() {
    bool expected = false;

    if (!is_terminated.compare_exchange_strong(expected, true)) {
        if (worker_thread && worker_thread->joinable()) {
            worker_thread->join();
        }
        worker_thread.reset();
        return;
    }

    is_connected = false;
    tcp_connection.CloseConnection();

    if (worker_thread && worker_thread->joinable()) {
        worker_thread->join();
    }

    worker_thread.reset();
}

bool PeerSession::IsTerminated() const {
    return is_terminated.load();
}

bool PeerSession::IsConnected() const {
    return is_connected.load() && !is_terminated.load();
}

std::string PeerSession::GetPeerId() const {
    return peer_id;
}

std::string PeerSession::GetPeerAddress() const {
    return tcp_connection.GetIp() + ":" + std::to_string(tcp_connection.GetPort());
}

Peer PeerSession::GetPeer() const {
    Peer peer;
    peer.ip = tcp_connection.GetIp();
    peer.port = tcp_connection.GetPort();
    return peer;
}

void PeerSession::RunLoop() {
    size_t failures_count = 0;

    while (!is_terminated && failures_count < kMaxFailures) {
        is_connected = false;
        try {
            if (EstablishConnection()) {
                failures_count = 0;
                is_connected = true;
                Logger::LogUi("Successfully connected to " + GetPeerAddress());
                MainLoop();
            } else {
                ++failures_count;
            }
        } catch (const std::exception& error) {
            ++failures_count;
            Logger::LogError(
                "Peer " + GetPeerAddress() + " error: " + error.what()
            );
            HandleError();
        }

        is_connected = false;

        if (failures_count > 0 && !is_terminated) {
            for (size_t i = 0; i < 50 && !is_terminated; ++i) {
                std::this_thread::sleep_for(100ms);
            }
        }
    }

    is_terminated = true;
    is_connected = false;
}

bool PeerSession::EstablishConnection() {
    tcp_connection.EstablishConnection();
    PerformHandshake();
    ReceiveBitfield();
    SendInterested();
    return true;
}

void PeerSession::PerformHandshake() {
    std::string msg;
    msg.reserve(68);
    msg += char(19);
    msg += "BitTorrent protocol";
    msg += std::string(8, '\0');
    msg += torrent_file.info_hash;
    msg += self_peer_id;

    tcp_connection.SendData(msg);
    auto resp = tcp_connection.ReceiveData(68);
    peer_id = resp.substr(48, 20);
}

void PeerSession::ReceiveBitfield() {
    auto data = tcp_connection.ReceiveData();
    if (data.size() < 5) {
        return;
    }

    if (data[4] == static_cast<char>(MessageId::kBitField)) {
        pieces_availability = PeerPiecesAvailability(
            data.substr(5), ((torrent_file.piece_hashes.size() + 7) >> 3)
        );
    } else if (data[4] == static_cast<char>(MessageId::kUnchoke)) {
        is_choked = false;
    }
}

void PeerSession::SendInterested() {
    tcp_connection.SendData(Message::Init(MessageId::kInterested, "").ToString());
}

void PeerSession::MainLoop() {
    while (!is_terminated) {
        if (!piece_in_progress) {
            piece_in_progress = GetNextAvailablePiece();
            inflight_offsets.clear();
        }

        if (!piece_in_progress) {
            auto msg = tcp_connection.ReceiveData();
            if (!msg.empty()) {
                ProcessMessage(msg);
            }
            continue;
        }

        if (!is_choked) {
            while (inflight_offsets.size() < kMaxInflightBlocks) {
                Block* block = piece_in_progress->GetFirstMissingBlock();
                if (!block) break;

                RequestBlock(block);
                inflight_offsets.insert(block->offset);
            }
        }

        auto msg = tcp_connection.ReceiveData();
        if (!msg.empty()) {
            ProcessMessage(msg);
        }
    }
}

PiecePtr PeerSession::GetNextAvailablePiece() {
    for (size_t i = 0; i < 100 && !is_terminated; ++i) {
        auto piece = piece_storage.GetNextPieceToDownload();
        if (!piece) return nullptr;

        if (pieces_availability.IsPieceAvailable(piece->GetIndex())) {
            return piece;
        }

        piece_storage.ReturnPiece(piece);
    }
    return nullptr;
}

void PeerSession::ProcessMessage(std::string_view data) {
    auto msg = Message::Parse(std::string(data));

    switch (msg.id) {
    case MessageId::kUnchoke: {
        is_choked = false;
        break;
    }

    case MessageId::kChoke: {
        is_choked = true;
        ReturnCurrentPiece();
        break;
    }

    case MessageId::kHave: {
        size_t index = static_cast<size_t>(bytes_to_int32(msg.payload));
        pieces_availability.SetPieceAvailable(index);
        break;
    }

    case MessageId::kPiece: {
        size_t index = static_cast<size_t>(
            bytes_to_int32(std::string_view(msg.payload).substr(0, 4))
        );
        size_t offset = static_cast<size_t>(
            bytes_to_int32(std::string_view(msg.payload).substr(4, 4))
        );

        if (piece_in_progress && piece_in_progress->GetIndex() == index) {
            piece_in_progress->SaveBlock(
                offset, std::string_view(msg.payload).substr(8)
            );
            inflight_offsets.erase(offset);

            if (piece_in_progress->AllBlocksRetrieved()) {
                piece_storage.PieceProcessed(piece_in_progress);
                piece_in_progress.reset();
                inflight_offsets.clear();
            }
        }
        break;
    }

    default:
        break;
    }
}

void PeerSession::RequestBlock(const Block* block) {
    std::string payload;
    payload.reserve(12);
    payload += int32_to_bytes(static_cast<int32_t>(block->piece));
    payload += int32_to_bytes(static_cast<int32_t>(block->offset));
    payload += int32_to_bytes(static_cast<int32_t>(block->length));
    tcp_connection.SendData(Message::Init(MessageId::kRequest, payload).ToString());
}

void PeerSession::HandleError() {
    ReturnCurrentPiece();
}

void PeerSession::ReturnCurrentPiece() {
    if (piece_in_progress) {
        piece_storage.ReturnPiece(piece_in_progress);
        piece_in_progress.reset();
    }
    inflight_offsets.clear();
}

} // namespace tclient

