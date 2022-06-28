#ifndef __SOCK_HPP__ 
#define __SOCK_HPP__

namespace Sockets {
    // Exception --- base exception class
    class Exception { 
    protected:
        int m_ErrCode; 
    public:
        Exception(int errcode) : m_ErrCode(errcode) {} 
        void Report();
        virtual std::string GetMessage() = 0;
    };
    
    // SocketException --- socket exception class
    class SocketException : public Exception { 
        static std::string m_Message[];
    public:
        enum SocketExceptionCode {
            ESE_SUCCESS, 
            ESE_SOCKCREATE, 
            ESE_SOCKCONN, 
            ESE_SOCKILLEGAL, 
            ESE_SOCKHOSTNAME, 
            ESE_SOCKSEND, 
            ESE_SOCKRECV, 
            ESE_SOCKBIND, 
            ESE_SOCKLISTEN, 
            ESE_SOCKACCEPT,
        };
        SocketException (SocketExceptionCode errcode) : Exception(errcode) {} 
        std::string GetMessage();
    };
    
    // SocketAddress --- базовый класс для представления
    // сетевых адресов 
    class SocketAddress { 
    protected:
        struct sockaddr_un * m_pAddr;
    public:
        SocketAddress() : m_pAddr(NULL) {}
        virtual ~SocketAddress();
        virtual int GetLength();
        operator struct sockaddr * ();
        const char * GetPath();
    };

    // UnSocketAddress --- представление адреса семейства AF_UNIX
    class UnSocketAddress : public SocketAddress { 
    public:
        UnSocketAddress(const char * SockName); 
        int GetLength();
    };

    // BaseSocket --- базовый класс для сокетов
    class BaseSocket {
    public:
        explicit BaseSocket(int sd = -1, SocketAddress *pAddr = NULL) :
            m_Socket(sd), m_pAddr(pAddr) {}
        virtual ~BaseSocket(); 
        void PutString(const std::string& s);
        std::string GetString();
        int GetSockDescriptor();
    protected: 
        int m_Socket;
        SocketAddress * m_pAddr;
    };

    // ClientSocket --- базовый класс для клиентских сокетов
    class ClientSocket: public BaseSocket { 
    public:
        void Connect();
    };

    // ServerSocket --- базовый класс для серверных сокетов
    class ServerSocket: public BaseSocket {
    public:
        BaseSocket * Accept();
    protected:
        void Bind();
        void Listen(int BackLog);
        virtual void OnAccept(BaseSocket * pConn) {}
    };
    
    // UnClientSocket --- представление клиентского сокета семейства
    // AF_UNIX
    class UnClientSocket: public ClientSocket { 
    public:
        UnClientSocket(const char * Address); 
    };

    // UnServerSocket --- представление серверного сокета семейства
    // AF_UNIX
    class UnServerSocket: public ServerSocket { 
    public:
        UnServerSocket(const char * Address); 
    };

} // конец namespace Sockets

#endif
