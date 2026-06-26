# tclient

A BitTorrent client written in C++20 that supports downloading **single-file torrents** using **HTTP and UDP trackers**.  
The project features a **multi-threaded architecture** and a self-written **text-based user interface (TUI)**.

No external libraries are required — all networking is implemented using plain POSIX sockets.

This project originally started as a university assignment and is now being refactored and improved.

## Notes

- If you keep getting few or no peers, try updating `config/tracker-list.txt` with **UDP trackers list** from [here](https://github.com/ngosang/trackerslist).
- It's best to use "fresh" torrent files. Note that only **single-file** torrents are supported.

## Screenshots

<details>
<summary>Show screenshots</summary>

![Download In Progress](assets/images/downloading.png)
![Download Completed](assets/images/finished.png)

</details>

[Example Log File](assets/tclient.log)

## Features

- Single-file torrent downloads
- Multi-threaded peer connections (up to 50 concurrent)
- HTTP and UDP tracker support
- Endgame mode for faster completion
- Text User Interface (TUI) with live progress and log
- Text log file

## Build

Requirements:

- CMake 3.14+
- C++20-compatible compiler

```bash
git clone https://github.com/ch1ldzero/tclient
cd tclient
mkdir build && cd build
cmake ..
cmake --build .
```

## Usage

```bash
# inside build/
src/tclient <torrent-file> <output-directory>
```

### Example

```bash
# inside build/
src/tclient ../resources/archlinux-2026.06.01-x86_64.iso.torrent downloads
```

Log file `tclient.log` will appear inside `build/`.

## Architecture

Project structure:

- `core/`
  - Client entry point, file parsing, download orchestration and state snapshotting
- `download/`
  - Piece picking logic and download progress monitoring
- `net/`
  - TCP, UDP and HTTP connection abstractions
- `peer/`
  - Peer protocol messages, peer connection management and session handling
- `storage/`
  - Piece and block management, persistent storage to disk
- `tracker/`
  - Tracker interface, HTTP/UDP tracker implementations and factory
- `ui/`
  - Terminal UI rendering with live progress bars and log display
- `utils/`
  - SHA-1 hashing, Bencode parser, byte utilities, timer and logger

## Limitations

- Supports only single-file torrents (no multi-file/directory structure)
- No seeding/upload capability
- No DHT support
- No magnet link support

## Planned

### Near Future
- Calculate and display download speed

### Someday
- Multi-file torrent support
- Seeding/upload support
