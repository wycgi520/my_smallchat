#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <memory>

const unsigned short PORT = 7711;
const int MAX_BUFFER = 1024;

class Client final
{
private:
    int clientSocket_; // 客户端socket
    std::string nick_;

public:
    Client(int socket) : clientSocket_(socket) { nick_ = "User:" + std::to_string(socket); }
    ~Client() {
        close(clientSocket_);
    }

    Client (const Client &other) = default;
    Client &operator= (const Client &other) = default;

    int getSocket() const
    {
        return clientSocket_;
    }

    const std::string &getNick() const
    {
        return nick_;
    }

    void setNick(const std::string &nick)
    {
        nick_ = nick;
    }

    // 读取客户端数据
    int readFrom(char *buffer, const int bufferSize)
    {
        return read(clientSocket_, buffer, bufferSize);
    }

    // 向客户端写入数据
    int writeTo(const char *message)
    {
        return write(clientSocket_, message, strlen(message));
    }
};

class ChatManager final
{
private:
    int serverSocket_;                            // 服务端socket
    int maxClient_;                               // 最大客户端
    std::vector<std::unique_ptr<Client>> clients_; // 存储客户端类实例的数组

public:
    ChatManager() : serverSocket_(-1), maxClient_(-1) {}
    ~ChatManager() {
        if (serverSocket_ != -1) {
            close(serverSocket_);
        }
    }

    ChatManager (const ChatManager &other) = default;
    ChatManager &operator= (const ChatManager &other) = default;

    bool init()
    {
        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ == -1) {
            std::cerr << "Error creating server socket\n";
            return false;
        }

        int yes = 1;
        setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        serverAddr.sin_port = htons(PORT);

        if (bind(serverSocket_, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) == -1) {
            std::cerr << "Error binding server socket\n";
            return false;
        }

        if (listen(serverSocket_, 511) == -1) {
            std::cerr << "Error listening on server socket\n";
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

        int clientSocket = accept(serverSocket_, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientAddrLen);
        if (clientSocket == -1) {
            std::cerr << "Error accepting client connection\n";
            return;
        }

        if (clientSocket > maxClient_) {
            maxClient_ = clientSocket;
        }

        auto client = std::make_unique<Client>(clientSocket);
        const char *welcome_msg =
            "Welcome to Simple Chat! "
            "Use /nick <nick> to set your nick_.\n";
        if (client->writeTo(welcome_msg) == -1) {
            std::cerr << "Error writing to client " << clientSocket << std::endl;
            return;
        }
        
        clients_.push_back(std::move(client));
        std::cout << "Client connected. Total clients: " << clients_.size() << std::endl;
    }

    int selectRead(fd_set &readfds)
    {
        FD_ZERO(&readfds);
        FD_SET(serverSocket_, &readfds);

        for (const auto &client : clients_) {
            FD_SET(client->getSocket(), &readfds);
        }

        int maxFd = maxClient_;
        if (maxFd < serverSocket_)
            maxFd = serverSocket_;

        struct timeval timeout;
        timeout.tv_sec = 1; // 设置超时时间为1秒
        timeout.tv_usec = 0;
        return select(maxFd + 1, &readfds, nullptr, nullptr, &timeout);
    }

    void setNick(const std::string &msg, std::unique_ptr<Client> &client)
    {
        std::string nick;
        for (const auto &c : msg.substr(6)) {
            if (c != '\r' and c != '\n') {
                nick.push_back(c);
            }
        }

        client->setNick(nick);
    }

    void sendMsgToClients(const int &clientSocket, const std::string &msg)
    {
        for (const auto &otherClient : clients_) {
            if (otherClient->getSocket() != clientSocket) {
                otherClient->writeTo(msg.c_str());
            }
        }
        std::cout << msg;
    }

    void processRead(fd_set &readfds)
    {
        if (FD_ISSET(serverSocket_, &readfds)) {
            acceptClient();
        }

        char buffer[MAX_BUFFER]{};
        for (auto it = clients_.begin(); it != clients_.end();) {
            int clientSocket = (*it)->getSocket();
            if (FD_ISSET(clientSocket, &readfds)) {
                int bytesRead = (*it)->readFrom(buffer, sizeof(buffer));
                if (bytesRead <= 0) {
                    it = clients_.erase(it);
                    std::cout << "Client disconnected. Total clients: " << clients_.size() << std::endl;
                    continue;
                }

                buffer[bytesRead] = 0;

                const std::string msg{buffer};
                if (msg.find("/nick ") == 0) {
                    setNick(msg, *it);
                } else {
                    sendMsgToClients(clientSocket, (*it)->getNick() + " > " + msg);
                }
            }
            ++it;
        }
    }

    void run()
    {
        fd_set readfds;
        while (true) {
            // select阻塞监听各个客户端fd和自身fd
            int activity = selectRead(readfds);
            if (activity < 0) {
                std::cerr << "Error in select\n";
                break;
            } else if (activity > 0) {
                processRead(readfds);
            }
        }
    }
};

int main()
{
    auto chatManager = std::make_unique<ChatManager>();
    if (chatManager->init()) {
        chatManager->run();
    }

    return 0;
}
