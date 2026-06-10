add_library(core STATIC
    core/HttpTracker.cpp
    core/Piece.cpp
    core/PieceStorage.cpp
    core/TorrentClient.cpp
    core/TorrentFile.cpp
    core/TorrentTask.cpp
    core/UdpTracker.cpp
    net/HttpClient.cpp
    net/Message.cpp
    net/PeerConnection.cpp
    net/PeerManager.cpp
    net/TcpConnection.cpp
    net/UdpConnection.cpp
    utils/BencodeParser.cpp
    utils/byte_tools.cpp
    utils/Sha1.cpp
    utils/Timer.cpp
)

target_include_directories(core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(torrent-client
    main.cpp
    ui/TorrentUi.cpp
    ui/UiFormatter.cpp
    ui/UiRenderer.cpp
)

target_include_directories(torrent-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(torrent-client
    core
)

install(TARGETS torrent-client
    DESTINATION bin
)

