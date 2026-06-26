add_library(tclient_lib STATIC
    core/TorrentClient.cpp
    core/TorrentFile.cpp
    core/TorrentSession.cpp
    core/TorrentSnapshot.cpp
    core/TorrentTask.cpp

    download/DownloadMonitor.cpp
    download/PiecePicker.cpp

    net/HttpConnection.cpp
    net/TcpConnection.cpp
    net/UdpConnection.cpp

    peer/Message.cpp
    peer/PeerConnector.cpp
    peer/PeerManager.cpp
    peer/PeerSession.cpp

    storage/Piece.cpp
    storage/PieceStorage.cpp

    tracker/HttpTracker.cpp
    tracker/ITracker.cpp
    tracker/TrackerFactory.cpp
    tracker/TrackerManager.cpp
    tracker/UdpTracker.cpp

    utils/BencodeParser.cpp
    utils/Logger.cpp
    utils/Sha1.cpp
    utils/Timer.cpp
    utils/byte_tools.cpp
)

target_include_directories(tclient_lib
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_definitions(tclient_lib
    PRIVATE
        PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_executable(tclient
    main.cpp
    ui/TorrentUi.cpp
    ui/UiFormatter.cpp
    ui/UiRenderer.cpp
)

target_link_libraries(tclient
    PRIVATE
        tclient_lib
)

target_include_directories(tclient
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}
)

target_compile_options(tclient
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

target_compile_definitions(tclient
    PRIVATE
        PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

install(TARGETS tclient
    DESTINATION bin
)

