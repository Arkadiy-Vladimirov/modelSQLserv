#include <string> 
#include <iostream> 
#include <string.h>
#include <stdlib.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h> 
#include <netdb.h>
#include <sys/io.h>
#include "sock.hpp"

#define BUF_SIZE 1024
#define NUMBER_OF_COMMUNICATION_REQUESTS 5

namespace Sockets {

    std::string SocketException::m_Message[] = { 
        "Unlinking path_name attempt failed", 
        "Creating socket attempt failed", 
        "Connection socket attempt failed", 
        "There is no socket yet", 
        "Sending data attempt failed", 
        "Receiving data attempt failed", 
        "Binding socket attempt failed", 
        "Listening socket attempt failed", 
        "Accepting socket attempt failed" 
    }; 

    std::string SocketException::GetMessage() {
        return m_Message[m_ErrCode] + "\n";
    }

    void Exception::Report() {
        std::cout << GetMessage() << std::endl;
    }

    SocketAddress::~SocketAddress() {
        if (m_pAddr) delete m_pAddr;
    }
    int SocketAddress::GetLength() {
        return 0;
    }

    SocketAddress::operator struct sockaddr * () {
        return (struct sockaddr *) m_pAddr;
    }

    const char * SocketAddress::GetPath() {
        if (m_pAddr == NULL) {
            throw SocketException(SocketException::ESE_SUCCESS);
        }
        return m_pAddr->sun_path;
    }

    UnSocketAddress::UnSocketAddress(const char * SockName) {
        m_pAddr = new struct sockaddr_un;
        m_pAddr->sun_family = AF_UNIX;
        strcpy(m_pAddr->sun_path, SockName);
    }

    int UnSocketAddress::GetLength() {
        return m_pAddr ? sizeof(m_pAddr->sun_family) + strlen(m_pAddr->sun_path) : 0;
    }

    /*SocketAddress * UnSocketAddress::Clone() {
        clone_pAddr = new struct sockaddr_un;
        clone_pAddr->sun_family = AF_UNIX;
        strcpy(clone_pAddr->sun_path, m_pAddr->sun_path);
        return;
    }*/

    void BaseSocket::PutString(const std::string& s) {
        if (send(m_Socket, s.c_str(), strlen(s.c_str()), 0) == -1) {
            throw SocketException(SocketException::ESE_SOCKSEND);
        }    
    }
    
    std::string BaseSocket::GetString() {
        char buf[BUF_SIZE]; 
        int i;
        if ((i = recv(m_Socket, buf, BUF_SIZE, 0)) == -1) {
            throw SocketException(SocketException::ESE_SOCKRECV);
        }
        buf[i] = '\0';
        return buf;
    }

    int BaseSocket::GetSockDescriptor() {
        if (m_Socket == -1) {
            throw SocketException(SocketException::ESE_SOCKILLEGAL);
        }
        return m_Socket;         
    }

    BaseSocket::~BaseSocket() {
        if (m_pAddr) delete m_pAddr;
        close(m_Socket);
    }

    void ClientSocket::Connect() {
        if (connect(m_Socket, *m_pAddr, m_pAddr->GetLength()) == -1) {
            throw SocketException(SocketException::ESE_SOCKCONN);
        }
    }

    BaseSocket * ServerSocket::Accept() {
        int new_sd;
        SocketAddress * client_pAddr = new SocketAddress();
        unsigned int client_pAddr_len = sizeof(struct sockaddr_un);
        if ((new_sd = accept(m_Socket, *client_pAddr, &client_pAddr_len)) == -1) {
            throw SocketException(SocketException::ESE_SOCKACCEPT);
        } 
        BaseSocket pConn(new_sd, client_pAddr);
        OnAccept(&pConn);
        return this;
    }

    void ServerSocket::Bind() {
        unlink(m_pAddr->GetPath());
        if (bind(m_Socket, *m_pAddr, m_pAddr->GetLength()) == -1) {
            throw SocketException(SocketException::ESE_SOCKBIND);
        }
    }

    void ServerSocket::Listen(int BackLog) {
        if (listen(m_Socket, BackLog) == -1) {
            throw SocketException(SocketException::ESE_SOCKLISTEN);
        }
    }

    UnClientSocket::UnClientSocket(const char * Address) {
        UnSocketAddress * pAddr = new UnSocketAddress(Address);
        m_pAddr = pAddr;
        int sd;
        if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw SocketException(SocketException::ESE_SOCKCREATE);
        } 
        m_Socket = sd;
    }

    UnServerSocket::UnServerSocket(const char * Address) {
        UnSocketAddress * pAddr = new UnSocketAddress(Address);
        m_pAddr = pAddr;
        int sd;
        if ((sd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw SocketException(SocketException::ESE_SOCKCREATE);
        } 
        m_Socket = sd;
        Bind();
        Listen(NUMBER_OF_COMMUNICATION_REQUESTS);
    }

}

