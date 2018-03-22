//
// Created by tracy on 3/21/18.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include "EpollClient.hpp"
#include "sys_utils.h"

namespace mj2
{

EpollClient::EpollClient()
{
}

EpollClient::~EpollClient()
{
}

int EpollClient::Init( unsigned short localPort )
{
    // create socket
    sockfd_ = socket( AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0 );
    if ( -1 == sockfd_ )
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

void EpollClient::Shutdown()
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

void EpollClient::ThreadLoop( char const* ip, unsigned short remotePort )
{
    thread_ = std::thread{ &EpollClient::Update, this, ip, remotePort };
    thread_.detach();
}

void EpollClient::Update( char const* ip, unsigned short remotePort )
{
    // connect
    bool connected = true;

    sockaddr_in remoteAddr{};
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_addr.s_addr = inet_addr( ip );
    remoteAddr.sin_port = htons( remotePort );
    int error_conn = connect( sockfd_, (sockaddr*) &remoteAddr, sizeof( remoteAddr ) );
    if ( -1 == error_conn && errno == EINPROGRESS )
    {
        Print("--tcp client connecting...\n");
        connected = false;
    }

    // edge-triggered, sock fd should be non-blocking
    epoll_event ev{};
    ev.events = EPOLLIN | EPOLLET;
    if ( !connected )
    {
        ev.events |= EPOLLOUT; // connection established is a write event
    }
    ev.data.fd = sockfd_;
    if ( epoll_ctl( epollfd_, EPOLL_CTL_ADD, sockfd_, &ev ) == -1 )
    {
        Print( "error epoll_ctl\n" );
    }

    int nfds, n;
    for (;;)
    {
        nfds = epoll_wait( epollfd_, events, MAX_EVENTS, -1 );
        if ( nfds == -1 )
        {
            Print("--tcp client, epoll_wait error: %s\n", strerror(errno) );
            break;
        }

        Print( "--tcp epoll waiting %d ...\n", nfds );
        for ( n = 0; n < nfds; ++n )
        {
            if ( events[n].data.fd == sockfd_ )
            {

                switch ( events[n].events )
                {
                    case EPOLLIN:
                    {
                        char buffer[1024];
                        for(;;)
                        {
                            ssize_t n_read = read( events[n].data.fd, buffer, sizeof(buffer) );
                            if ( n_read == -1 )
                            {
                                if ( errno == EWOULDBLOCK || errno == EAGAIN )
                                {
                                    Print("--tcp client, read finished\n");
                                    continue;
                                }
                                else
                                {
                                    Print("--tcp client read error!\n");
                                    break;
                                }
                            }
                            else if ( n_read == 0 )
                            {
                                Print("--tcp client read EOF\n");
                                break;
                            }
                            else
                            {
                                Print("--tcp client continue reading");
                                continue;
                            }
                        }

                        break;
                    }
                    case EPOLLOUT:
                    {
                        if ( !connected )
                        {
                            int sock_err = 0;
                            socklen_t socklen = sizeof(int);
                            int n_sockopt = getsockopt( sockfd_, SOL_SOCKET, SO_ERROR, &sock_err, &socklen );
                            if ( n_sockopt == 0 )
                            {
                                if ( sock_err == 0 )
                                {
                                    connected = true;
                                    Print( "--tcp client, connected to %s success!\n", ip );
                                }
                                else
                                {
                                    Print("--tcp client, failed to connect to %s\n", ip );
                                }
                            }
                            else
                            {
                                Print("--tcp client, failed to getsockopt %s\n", strerror( errno ) );
                            }
                        }
                        else
                        {
                            Print("--tcp client, ok to write to server\n");
                            if ( !sendBuffer_.empty() )
                            {
                                ssize_t written = 0;
                                ssize_t n_write = 0;
                                for(;;)
                                {
                                    n_write = write( sockfd_, sendBuffer_.data() + written, sendBuffer_.size() );
                                    if ( -1 == n_write )
                                    {
                                        if ( errno == EAGAIN || errno == EWOULDBLOCK )
                                        {
                                            Print("--tcp client, write finished\n");
                                            break;
                                        }
                                        else
                                        {
                                            Print("--tcp client failed to write\n");
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        written += n_write;
                                        continue;
                                    }
                                }
                            }
                        }

                        break;
                    }
                    case EPOLLERR:
                        Print("--tcp client epoll error\n");
                        break;
                    case EPOLLHUP:
                        Print("--tcp epoll hup\n");
                        break;
                    default:
                        Print("--tcp client unkown event %04x\n", events[n].events );
                        break;
                }

            }
            else // remote fd
            {
                Print("--tcp client remote fd\n");
                switch ( events[n].events )
                {
                    case EPOLLIN:
                    {
                        break;
                    }
                    case EPOLLOUT:
                    {
                        break;
                    }
                    default:
                        break;
                }
            }
        }
    }
}

void EpollClient::Send(uint8_t *data, size_t size)
{
    Print("--tcp client insert %d bytes to sendBuffer\n", size );
    sendBuffer_.insert( sendBuffer_.end(), data, data + size );
}

}