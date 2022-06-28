#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include "sock.hpp"

extern "C" {
    #include "table.h"
}

#include "tableint.hpp"
#include "interpreter.hpp"

using namespace Sockets;
using namespace Interpreter;
using namespace TableInterface;

/*class ShutdownException : public Exception {
public:    
    ShutdownException() : Exception(0) {}
    std::string GetMessage();
};

std::string ShutdownException::GetMessage() {
    return "Session completed.\n";
}
*/

const char * address = "mysocket"; // Socketname

class MyServerSocket : public UnServerSocket { 
public:
    MyServerSocket () : UnServerSocket (address) {} 
protected:
    void OnAccept (BaseSocket * pConn) {

        // Connection established   
        for (;;) {
            bool no_exception = true;
            try {
                std::string str = pConn->GetString();
                lexer::expr_init(str);
                parser::init_parse();
            } catch (SocketException &se) {
                throw se;
            } catch (InterpretException & ie) {
                pConn->PutString(ie.GetMessage());
                no_exception = false;
            } catch (DataBaseException & de) {
                pConn->PutString(de.GetMessage());
                no_exception = false;
            }
            if (no_exception) {
                pConn->PutString(parser::get_answer());
            }
        } 
    } 
};

int main(int argc, char ** argv) {
    try {
        MyServerSocket server_socket; 
        pid_t pid = fork();
        if (pid == -1) {
            std::cout << "Fork() caused an error;" << std::endl;
            return 1;
        }
        if (pid == (pid_t) 0) {
            server_socket.Accept(); 
        } else {
            
            // Establishing connection 

            UnClientSocket client_socket(address);
            client_socket.Connect(); 
            for (;;) {
                std::cout << "If you want to shut down, simply type \"FINISH\"" << std::endl << 
                    "Otherwise, please, enter SQL sentence:\n" << std::endl;
                std::string expr;
                getline(std::cin, expr, '\n');
                if (expr == "FINISH" || expr == "\"FINISH\"") {
                    std::cout << "\nSession completed." << std::endl;
                    unlink(address);
                    //throw ShutdownException();
                    return 0;
                } else {
                    client_socket.PutString(expr);
                    std::cout << "\nResponse to your request:\n" << client_socket.GetString() << std::endl;
                }
            }
        } 
    } catch (Exception & e) {
        
        // Exception was thrown --- printing error message 
        
        unlink(address);
        e.Report();
    }
    return 0;
}
