#include <filesystem>
#include <future>
#include <iostream>
#include <thread>

#include "core/TorrentClient.hpp"
#include "ui/TorrentUi.hpp"

void DownloadThreadFunction(
    TorrentClient* client,
    const std::filesystem::path& torrent_file_path,
    const std::filesystem::path& output_directory,
    std::promise<bool>& download_promise
) {
    try {
        client->DownloadTorrent(torrent_file_path, output_directory);
        if (client->IsStopRequested()) {
            download_promise.set_value(false);
        } else {
            download_promise.set_value(true);
        }
    } catch (const std::exception& error) {
        std::cerr << "Download error: " << error.what() << std::endl;
        download_promise.set_exception(std::current_exception());
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr
            << "Usage: "
            << argv[0]
            << " <torrent-file> <output-directory>"
            << std::endl;
        return EXIT_FAILURE;
    }
    
    std::filesystem::path torrent_file_path = argv[1];
    std::filesystem::path output_directory = argv[2];
    
    if (!std::filesystem::exists(torrent_file_path)) {
        std::cerr
            << "Error: Torrent file not found: "
            << torrent_file_path
            << std::endl;
        return EXIT_FAILURE;
    }
    
    if (!std::filesystem::exists(output_directory)) {
        std::filesystem::create_directories(output_directory);
    }
    
    try {
        auto client = std::make_unique<TorrentClient>();
        TorrentClient* client_raw = client.get();
        
        std::promise<bool> download_promise;
        std::future<bool> download_future =
            download_promise.get_future();
        
        std::thread download_thread(
            DownloadThreadFunction, 
            client_raw, 
            torrent_file_path, 
            output_directory,
            std::ref(download_promise)
        );
        
        TorrentUi torrent_ui(std::move(client));
        torrent_ui.Run();
        
        client_raw->RequestStop();
        
        if (download_thread.joinable()) {
            download_thread.join();
        }
        
        try {
            bool download_success = download_future.get();
            if (download_success) {
                std::cout
                    << "Download completed successfully!"
                    << std::endl;
                return EXIT_SUCCESS;
            } else {
                std::cerr
                    << "Download stopped by user (Q was pressed)"
                    << std::endl;
                return EXIT_FAILURE;
            }
        } catch (const std::exception& error) {
            std::cerr
                << "Download failed with error: "
                << error.what()
                << std::endl;
            return EXIT_FAILURE;
        }
        
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }
}

