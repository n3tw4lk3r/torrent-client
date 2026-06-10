#include "ui/TorrentUi.hpp"

#include <thread>

TorrentUi::TorrentUi(TorrentClient& client) :
    client(client)
{}

void TorrentUi::Run() {
    using namespace std::chrono_literals;

    while (running) {
        renderer.Render(
            client.GetCurrentTask(),
            client.ElapsedTime(),
            client.GetLogMessages(30)
        );
        
        if (client.IsFinished()) {
            break;
        }

        std::this_thread::sleep_for(500ms);
    }
}

