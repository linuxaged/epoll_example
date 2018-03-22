//
// Created by tracy on 3/21/18.
//

#include "EpollServer.hpp"
#include "sys_utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <arpa/inet.h>

namespace mj2
{

int EpollServer::Init()
{
    // create socket
    sockfd_ = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 );
    if ( -1 == sockfd_ )
    {
        return -1;
    }
    // bind
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl( INADDR_ANY);
    localAddr.sin_port = htons( SERVER_PORT );
    int error_bind = bind( sockfd_, (sockaddr*) &localAddr, sizeof(localAddr) );
    if ( -1 == error_bind )
    {
        return -1;
    }
    // listen
    int error_listen = listen( sockfd_, 2 );
    if ( -1 == error_listen )
    {
        return -1;
    }
    // create epoll
    epollfd_ = epoll_create1( 0 );
    if ( -1 == epollfd_ )
    {
        return -1;
    }

    events = new epoll_event[MAX_EVENTS];

    return 0;
}

void EpollServer::Shutdown()
{
    // close socket
    if ( sockfd_ > 0 )
        close( sockfd_ );

    // TODO: remove socket from epoll


    // delete epoll
    if ( epollfd_ > 0 )
        close( epollfd_ );

    delete[] events;
}

void EpollServer::Update()
{
    // edge-triggered, sock fd should be non-blocking
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sockfd_;
    if ( epoll_ctl( epollfd_, EPOLL_CTL_ADD, sockfd_, &ev ) == -1 )
    {
        Print( "error epoll_ctl\n" );
    }

    int nfds, n, conn_sock;
    for (;;)
    {
        nfds = epoll_wait( epollfd_, events, MAX_EVENTS, EPOLL_TIMEOUT_MS );
        if ( nfds == -1 )
        {
            Print("--tcp server, epoll_wait error: %s\n", strerror(errno) );
            break;
        }

        Print( "--tcp epoll waiting %d...\n", nfds );
        for ( n = 0; n < nfds; ++n )
        {
            if ( events[n].events & EPOLLERR ||
                    events[n].events & EPOLLHUP ||
                    !(events[n].events & EPOLLIN) )
            {
                Print("--tcp server, epoll error\n");
                continue;
            }
            else if ( events[n].data.fd == sockfd_ )
            {
                sockaddr_in addr;
                socklen_t addrlen = sizeof( sockaddr_in );
                conn_sock = accept4( sockfd_, (struct sockaddr *) &addr, &addrlen, SOCK_NONBLOCK );
                if ( conn_sock == -1 )
                {
                    Print("--tcp server accept4 failed\n");
                    continue;
                }
                else
                {
                    char ipStr[INET_ADDRSTRLEN];
                    inet_ntop(PF_INET, &(addr.sin_addr), ipStr, INET_ADDRSTRLEN);
                    Print("--tcp server, accept connection from %s\n", ipStr);
                    // add remote fd to epoll interest list
                    epoll_event ee{};
                    ee.events = EPOLLIN | EPOLLET;
                    ee.data.fd = conn_sock;
                    if ( epoll_ctl( epollfd_, EPOLL_CTL_ADD, conn_sock, &ee ) == -1 )
                    {
                        Print("--tcp server, failed to epoll_ctl\n");
                        continue;
                    }
                }
            }
            else // remote fd
            {
                Print("--tcp server, read and write\n");
                char buffer[1024];
                for (;;)
                {
                    ssize_t n_read = read(events[n].data.fd, buffer, sizeof(buffer));
                    if (n_read == -1)
                    {
                        if (errno == EWOULDBLOCK || errno == EAGAIN)
                        {
                            Print("--tcp read finish\n");
                            break;
                        }
                        else
                        {
                            Print("--tcp server read error!\n");
                            break;
                        }
                    }
                    else if (n_read == 0)
                    {
                        Print("--tcp server read EOF, client closed\n");
                        break;
                    }
                    else
                    {
                        Print("--tcp server continue reading");
                        continue;
                    }
                }

                /**
                 * write data in send buffer back to client
                 */
                if (!sendBuffer_.empty())
                {
                    int written = 0;
                    for (;;)
                    {
                        ssize_t n_write = write(events[n].data.fd, sendBuffer_.data() + written,
                                                sendBuffer_.size() - written);
                        if (n_write == -1)
                        {
                            if (errno == EWOULDBLOCK || errno == EAGAIN)
                            {
                                Print("--tcp server, write finished\n");
                                break;
                            }
                            else
                            {
                                Print("--tcp server, write error %s\n", strerror(errno));
                                break;
                            }
                        } else if (n_write == 0)
                        {
                            Print("--tcp server, write finish\n");
                            break;
                        } else
                        {
                            written += n_write;
                        }
                    }

                    sendBuffer_.clear();
                }


            }
        }
    }
}

void EpollServer::ThreadLoop()
{
    thread_ = std::thread{ &EpollServer::Update, this };
    thread_.detach();
}

void EpollServer::Send(uint8_t *data, size_t size)
{
    Print("--tcp server, insert %d bytes into sendBuffer\n", size );
    sendBuffer_.insert( sendBuffer_.end(), data, data + size );
}

}