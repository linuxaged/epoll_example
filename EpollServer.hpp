//
// Created by tracy on 3/21/18.
//

#pragma once

#include <vector>
#include <cstdint>
#include <thread>

struct epoll_event;

namespace mj2
{
    static unsigned short const SERVER_PORT = 17133;
    static int const MAX_EVENTS = 100;
    static int const EPOLL_TIMEOUT_MS = 10 * 1000;

    class EpollServer
    {
    public:
        int Init();
        void Shutdown();
        void ThreadLoop();
        void Update();

        void Send( uint8_t* data, size_t size );
    private:
        int epollfd_ {-1};
        int sockfd_ {-1};
        epoll_event* events {nullptr};
        std::thread thread_;
        std::vector<uint8_t> sendBuffer_;
        std::vector<uint8_t> receiveBuffer;
    };
}
