#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace std;

const int PORT = 7711;
const int MAX_CLIENTS = 1000;

class Client
{
private:
    int clientSocket; // 客户端socket
    string nick;

public:
    Client(int socket) : clientSocket(socket) { nick = "User:" + std::to_string(socket); }

    int getSocket() const
    {
        return clientSocket;
    }

    const std::string &getNick() const
    {
        return nick;
    }

    void setNick(string &nick)
    {
        this->nick = nick;
    }

    // 读取客户端数据
    int readFrom(char *buffer, int bufferSize)
    {
        return read(clientSocket, buffer, bufferSize);
    }

    // 向客户端写入数据
    void writeTo(const char *message)
    {
        write(clientSocket, message, strlen(message));
    }

    // 关闭客户端
    void closeClient()
    {
        close(clientSocket);
    }
};

class ChatManagement
{
private:
    int serverSocket;              // 服务端socket
    int maxClient;                 // 最大客户端
    int clientCount;               // 当前连接的客户端数
    std::vector<Client *> clients; // 存储客户端类实例的数组

public:
    ChatManagement() : serverSocket(-1), maxClient(-1), clientCount(0) {}

    bool init()
    {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            std::cerr << "Error creating server socket\n";
            return false;
        }
        int yes = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)); // Best effort.

        sockaddr_in serverAddr{};
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddr.sin_port = htons(PORT); // 你可以根据需要更改端口号

        if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1) {
            std::cerr << "Error binding server socket\n";
            close(serverSocket);
            return false;
        }

        if (listen(serverSocket, 511) == -1) {
            std::cerr << "Error listening on server socket\n";
            close(serverSocket);
            return false;
        }

        std::cout << "Server initialized and listening on port " << PORT << std::endl;
        return true;
    }

    // 接受客户端连接
    void acceptClient()
    {
        sockaddr_in clientAddr{};
        socklen_t clientAddrLen = sizeof(clientAddr);
        memset(&clientAddr, 0, clientAddrLen);

        int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrLen);
        if (clientSocket == -1) {
            std::cerr << "Error accepting client connection\n";
            return;
        }

        if (clientSocket > maxClient) {
            maxClient = clientSocket;
        }
        clientCount++;

        Client *client = new Client(clientSocket);
        const char *welcome_msg =
            "Welcome to Simple Chat! "
            "Use /nick <nick> to set your nick.\n";
        client->writeTo(welcome_msg);
        clients.push_back(client);
        std::cout << "Client connected. Total clients: " << clientCount << std::endl;
    }

    // 运行循环，select阻塞监听各个客户端fd和自身fd
    void run()
    {
        fd_set readfds;
        int maxFd;

        struct timeval timeout;
        timeout.tv_sec = 1; // 设置超时时间为1秒
        timeout.tv_usec = 0;

        char buffer[1024];
        while (true) {
            FD_ZERO(&readfds);
            FD_SET(serverSocket, &readfds);
            maxFd = maxClient;

            for (const auto &client : clients) {
                int clientSocket = client->getSocket();
                FD_SET(clientSocket, &readfds);
            }

            if (maxFd < serverSocket)
                maxFd = serverSocket;
            int activity = select(maxFd + 1, &readfds, nullptr, nullptr, &timeout);
            if (activity == -1) {
                std::cerr << "Error in select\n";
                break;
            } else if (activity == 0) {
                continue;
            }

            if (FD_ISSET(serverSocket, &readfds)) {
                acceptClient();
            }

            for (auto it = clients.begin(); it != clients.end();) {
                int clientSocket = (*it)->getSocket();
                if (FD_ISSET(clientSocket, &readfds)) {
                    int bytesRead = (*it)->readFrom(buffer, sizeof(buffer));
                    if (bytesRead <= 0) {
                        // 客户端断开连接
                        (*it)->closeClient();
                        delete *it;
                        clientCount--;
                        std::cout << "Client disconnected. Total clients: " << clients.size() - 1 << std::endl;

                        // 移除断开连接的客户端
                        it = clients.erase(it);
                        continue;
                    }

                    buffer[bytesRead] = 0;

                    string msg{buffer};
                    if (msg.find("/nick ") == 0) {
                        string new_nick;
                        for (const auto &c : msg.substr(6)) {
                            if (c != '\r' and c != '\n') {
                                new_nick.push_back(c);
                            }
                        }

                        (*it)->setNick(new_nick);
                        ++it;
                        continue;
                    }

                    msg = (*it)->getNick() + " > " + msg;
                    // 向其他客户端写入信息
                    for (const auto &otherClient : clients) {
                        if (otherClient->getSocket() != clientSocket) {
                            otherClient->writeTo(msg.c_str());
                        }
                    }
                    cout << msg;
                    ++it;

                } else {
                    ++it;
                }
            }
        }

        close(serverSocket);
    }
};

int main()
{
    ChatManagement chatServer{};
    if (chatServer.init()) {
        chatServer.run();
    }

    return 0;
}
