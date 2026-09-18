
  

  

# QChat


**A peer-to-peer, end-to-end encrypted chat client with a custom binary protocol, NAT traversal, and a self-hosted local web UI - built from scratch in C++.**

> Final project for *Programming II* - a decentralized messenger that talks directly, peer-to-peer, using a hand-rolled UDP protocol instead of routing messages through a central server.


---

## Highlights

  
QChat isn't a wrapper around a chat SDK - nearly every layer of the stack is implemented manually:

  
  

-  **Custom cryptographic handshake** - X25519 (ECDH) key exchange used to securely wrap and exchange per-friend AES-256-CBC session keys, with SHA-256 key derivation. No plaintext key ever touches the wire.

-  **Hand-designed binary UDP protocol** - a fixed 512-byte packet format (`F | type | quantity | messageID | packetID | payload[495] | CRC32`) supporting text messages, file transfers, key exchange, and username sync, complete with a self-implemented CRC32 checksum table for integrity verification.

-  **NAT traversal via UDP hole punching** - continuous keep-alive "punch" packets maintain bidirectional peer connections without port forwarding.

-  **Chunked, encrypted file transfer** - arbitrary files are split into ~461-byte chunks, individually AES-encrypted, streamed over UDP, and reassembled out-of-order on the receiving end using packet IDs.

-  **A coordinating rendezvous server** - a lightweight TCP client component registers the instance with a central "introducer" server (length-prefixed binary frames, AES-encrypted payloads) purely to help peers discover each other's IP/port - the server never sees message content.

-  **Self-hosted HTTP server + web frontend** - QChat runs its own minimal HTTP/1.1 server (built on Asio) that serves a browser-based UI (HTML/CSS/JS) as the actual chat interface, turning the C++ backend into a local web app.

-  **Full friend management** - add/remove friends, live online/offline presence detection, per-conversation unread badges, custom profile pictures broadcast to peers, and persistent local message history.

-  **Fully asynchronous architecture** - built on `asio`, with independent async UDP sessions per contact, timers for hole-punching and file streaming, and non-blocking TCP request handling.

  

  

## What this project demonstrates

- Low-level network programming (raw UDP framing, checksums, custom protocols) instead of relying on existing chat libraries

- Applied cryptography (asymmetric key exchange + symmetric encryption, correct IV handling, key derivation)

- Concurrency and asynchronous I/O design patterns in modern C++

- Practical NAT traversal / peer discovery, a topic usually abstracted away by existing tools

- Building a full-stack application (C++ backend acting as both a P2P node *and* a local web server) from first principles

  

  

##  Architecture Overview

  

  

```
 ┌─────────────┐                      ┌────────────────┐                     ┌─────────────────────┐
 │  Browser    │◄──HTTP (127.0.0.1)──►│   QChat Core   │◄──TCP (encrypted)──►│ Coordinating Server │
 │  (Frontend) │                      │   (C++ / Asio) │                     └─────────────────────┘
 └─────────────┘                      └────────────────┘                             ▲                   
                                              ▲                                      |            
                                              │ UDP (AES-256, hole-punched)          |            
                                              ▼                                      |                   
                                       ┌───────────────┐                             |     
                                       │   Peer QChat  │◄──TCP (encrypted)───────────┘
                                       │    Instance   │
                                       └───────────────┘
```

-  **Frontend ↔ Core**: plain HTTP on `localhost:80`, JSON over REST-like endpoints (`/friend_list`, `/messages/{uuid}`, `/send/{uuid}`, `/upload/file/{uuid}`, etc.)
-  **Core ↔ Core (peer-to-peer)**: direct UDP, X25519-negotiated AES-256 session keys, custom framed packets
-  **Core ↔ Coordinating server**: TCP, used only to exchange public IP/port info so peers can find each other

  
## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| Networking | [Asio](https://think-async.com/Asio/) (standalone, non-Boost) |
| Cryptography | OpenSSL (EVP API: X25519, AES-256-CBC, SHA-256) |
| Protocol | Custom binary UDP format + CRC32 |
| Backend | C++17/20, `std::filesystem`, async callbacks |
| Frontend | HTML / CSS / JavaScript, served locally by the app itself |

## Project Status

QChat is a **student/learning project built to explore protocol design, cryptography, and async networking - not a production-ready application.** Before it could be considered usable day-to-day, it would still need:

 
- Proper authentication and identity verification (no protection today against key/UUID spoofing on first contact)
- Hardened input parsing (several fields are extracted with manual string searches rather than a real parser)
- Reliability features for UDP: retransmission/ACKs for text messages, ordered delivery guarantees
- Persistent storage beyond flat text/JSON files (no database)
- Cross-platform support (currently Windows-oriented, e.g. `system("start ...")`, `chcp 65001`)

- Proper error handling/logging instead of console prints, and general security review of the file-serving endpoints

Treat this as a **proof of concept and learning exercise** in building a P2P encrypted messenger from the ground up - not as software to trust with real conversations yet.
