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
    static int const MAX_EVENTS = 16;
    static int const EPOLL_TIMEOUT_MS = 10 * 1000;

    class EpollClient
    {
    public:
        EpollClient();
        ~EpollClient();

        int Init( unsigned short localPort );
        void ThreadLoop( char const* ip, unsigned short remotePort );
        void Shutdown();

        void Send( uint8_t* data, size_t size );

    private:
        void Update( char const* ip, unsigned short remotePort );
    private:
        int sockfd_{-1};
        int epollfd_{-1};
        epoll_event* events;
        std::thread thread_;
        std::vector<uint8_t> sendBuffer_;
        std::vector<uint8_t> receiveBuffer_;
    };
}
