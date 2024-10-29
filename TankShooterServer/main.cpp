#include <SFML/Network.hpp>
#include <iostream>
#include <cstdint>
#include <vector>
#include <atomic>
#include <memory>
#include <cstring>
#include <string>
#include <mutex>


enum RequestType : char 
{
    CONNECT,
    PLAYER_JOIN,
    PLAYER_LEFT,
    PLAYER_MOVE,
    PLAYER_ATTACK,
    PLAYER_COUNT
};

enum Direction : char 
{
    NORTH,
    NORTHEAST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
    NORTHWEST
};

struct Position 
{
    int32_t x = 0;
    int32_t y = 0;

    void serialize(char* buffer) const 
    {
        std::memcpy(buffer, this, sizeof(Position));
    }
};

struct Player 
{
    int64_t id{0};
    char ip[15]{0};
    unsigned short port{};
    char name[32]{};
    Position pos{};
    Direction direction{};
    float scale{};
    float traverseAngle{};
    int64_t point{};
    bool isActive{false};
    Player() = default;
    Player(int64_t id) : id {id}
    {}
    static consteval int getSerializeSize() noexcept 
    {
        return sizeof(id) + sizeof(ip) + sizeof(port) + sizeof(name) + sizeof(pos)
            + sizeof(direction) + sizeof(scale) + sizeof(traverseAngle) + sizeof(point) + sizeof(isActive);
    }

    void serialize(char* buffer) const noexcept
    {
        std::size_t offset = 0;

        std::memcpy(buffer + offset, &id, sizeof(id));
        offset += sizeof(id);

        std::memcpy(buffer + offset, ip, sizeof(ip));
        offset += sizeof(ip);

        std::memcpy(buffer + offset, &port, sizeof(port));
        offset += sizeof(port);

        std::memcpy(buffer + offset, name, sizeof(name));
        offset += sizeof(name);

        std::memcpy(buffer + offset, &pos, sizeof(pos));
        offset += sizeof(pos);

        std::memcpy(buffer + offset, &direction, sizeof(direction));
        offset += sizeof(direction);

        std::memcpy(buffer + offset, &scale, sizeof(scale));
        offset += sizeof(scale);

        std::memcpy(buffer + offset, &traverseAngle, sizeof(traverseAngle));
        offset += sizeof(traverseAngle);

        std::memcpy(buffer + offset, &point, sizeof(point));
        offset += sizeof(point);

        std::memcpy(buffer + offset, &isActive, sizeof(isActive));
    }

    void deserialize(const char* buffer) noexcept
    {
        std::size_t offset = 0;

        std::memcpy(&id, buffer + offset, sizeof(id));
        offset += sizeof(id);

        std::memcpy(ip, buffer + offset, sizeof(ip));
        offset += sizeof(ip);

        std::memcpy(&port, buffer + offset, sizeof(port));
        offset += sizeof(port);

        std::memcpy(name, buffer + offset, sizeof(name));
        offset += sizeof(name);

        std::memcpy(&pos, buffer + offset, sizeof(pos));
        offset += sizeof(pos);

        std::memcpy(&direction, buffer + offset, sizeof(direction));
        offset += sizeof(direction);

        std::memcpy(&scale, buffer + offset, sizeof(scale));
        offset += sizeof(scale);

        std::memcpy(&traverseAngle, buffer + offset, sizeof(traverseAngle));
        offset += sizeof(traverseAngle);

        std::memcpy(&point, buffer + offset, sizeof(point));
        offset += sizeof(point);

        std::memcpy(&isActive, buffer + offset, sizeof(isActive));
    }
};

class GameServer 
{
public:
    GameServer(unsigned short port) : uid(1) 
    {
        for (int64_t i = 0; i < 100; ++i) {
            playerContainer.push_back(std::make_unique<Player>(i));
        }
        if (socket.bind(port) != sf::Socket::Done) 
        {
            throw std::runtime_error("Error binding socket!");
        }
        std::cout << "Server running on port " << port << std::endl;
    }

    void run() 
    {
        while (true) 
        {
            receiveData();
        }
    }

private:
    sf::UdpSocket socket;
    std::atomic_int uid{101};
    std::vector<std::unique_ptr<Player>> playerContainer{};
    std::mutex mt;

    void receiveData() 
    {
        char data[1024];
        sf::IpAddress sender;
        unsigned short senderPort;

        std::size_t received = 0;
        if (socket.receive(data, sizeof(data), received, sender, senderPort) == sf::Socket::Done) 
        {
            std::lock_guard<std::mutex> lock{ mt };
            std::cout << "Received " << received << " bytes from " << sender << ":" << senderPort << std::endl;
            handleRequest(data, sender, senderPort);
        }
    }

    void handleRequest(const char* data, sf::IpAddress& sender, const unsigned short senderPort) 
    {
        switch (static_cast<RequestType>(data[0])) 
        {
            case CONNECT:
                handleConnect(sender, senderPort);
                break;
            case PLAYER_JOIN:
                handlePlayerJoin(data + 1, sender, senderPort);
                break;
            case PLAYER_LEFT:
                handlePlayerLeft();
                break;
            case PLAYER_MOVE:
                handlePlayerMove();
                break;
            case PLAYER_ATTACK:
                handlePlayerAttack();
                break;
            default:
                std::cout << "Unknown request type: " << data << std::endl;
                break;
        }
    }

    void handleConnect(const sf::IpAddress& sender, const unsigned short senderPort) 
    {
    }

    void handlePlayerJoin(const char* name, const sf::IpAddress& sender,const unsigned short senderPort)
    {
        bool isEmptySlot = false;
        int64_t newPlayerId = -1;
        for (auto& player : playerContainer) 
        {
            if (!player->isActive) 
            {

                newPlayerId = player->id;
                player->isActive = true;
                strncpy_s(player->name, name, sizeof(player->name));
                std::cout << "Player name: " << player->name << "\n";
                strncpy_s(player->ip, sender.toString().c_str(), sizeof(player->ip));
                std::cout << "PLayerIp: " << player->ip << "\n";

                player->port = senderPort;
                strncpy_s(player->name, name, sizeof(player->name));
                isEmptySlot = true;
                break;
            }
        }

        if (newPlayerId == -1) 
        {
            newPlayerId = uid.load();
            uid++;
            auto newPlayer = std::make_unique<Player>();
            strncpy_s(newPlayer->name, name, sizeof(newPlayer->name));

            newPlayer->id = newPlayerId;
            newPlayer->isActive = true;
            strncpy_s(newPlayer->ip, sender.toString().c_str(), sizeof(newPlayer->ip));
            newPlayer->port = senderPort;
            strncpy_s(newPlayer->name, name, sizeof(newPlayer->name) - 1);
            playerContainer.push_back(std::move(newPlayer));
        }
        std::cout << sender << " " << senderPort << " NewPlayerId: " << newPlayerId << " PlayerName: " << name <<"\n";
        sendToNewPLayer(sender, senderPort, newPlayerId);
        broadcastPlayerJoin(newPlayerId);
    }

    void sendToNewPLayer(const sf::IpAddress& sender, const unsigned short senderPort, const int64_t newPlayerId)
    {
        std::size_t offset{ 0 };

        if (newPlayerId < 0 || newPlayerId >= playerContainer.size() || !playerContainer[newPlayerId]->isActive)
        {
            std::cerr << "Error: Invalid newPlayerId: " << newPlayerId << std::endl;
            return;
        }
           
        size_t activePlayerNum = 0;
        for (const auto& player : playerContainer)
                {
                    if (player->isActive)
                    {
                        activePlayerNum++;
                    }
                }
         
        std::vector<char> responseBuffer(activePlayerNum * sizeof(Player));

        for (const auto& player : playerContainer)
        {
            if (player->isActive)
            {
                memcpy_s(responseBuffer.data() + offset, sizeof(Player), (void*)&player, sizeof(player));
                offset += sizeof(Player);
            }
        }

        char* playerNumPacketData = new char[sizeof(activePlayerNum) + 1];
        char playerCntCmd = static_cast<char>(PLAYER_COUNT);
        memcpy_s(playerNumPacketData, sizeof(playerCntCmd), (void*)&playerCntCmd, sizeof(playerCntCmd)); 
        memcpy_s(playerNumPacketData + sizeof(playerCntCmd), sizeof(activePlayerNum), (void*)&activePlayerNum, sizeof(activePlayerNum));
        
        if (socket.send(playerNumPacketData, sizeof(activePlayerNum) + 1, sender, senderPort) != sf::Socket::Done)
        {
            std::cerr << "Cannot send player number to new player!" << std::endl;
            delete[] playerNumPacketData;
            return;
        }

        try {
            if (socket.send(responseBuffer.data(), activePlayerNum * sizeof(Player), sf::IpAddress(playerContainer[newPlayerId]->ip), playerContainer[newPlayerId]->port) != sf::Socket::Done)
            {
                std::cerr << "Cannot send player container to new player!" << std::endl;
                delete[] playerNumPacketData;
                return;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Exception occurred while sending data: " << e.what() << std::endl;
        }

        delete[] playerNumPacketData;
    }

    void broadcastPlayerJoin(int64_t newPlayerId) 
    {
        char broadcastMessage[Player::getSerializeSize() + 1]{};
        broadcastMessage[0] = PLAYER_JOIN;

        playerContainer[newPlayerId]->serialize(broadcastMessage + 1);

        for (const auto& player : playerContainer) 
        {
            if (player->isActive && player->id != newPlayerId) 
            {
                if (socket.send(broadcastMessage, sizeof(broadcastMessage), sf::IpAddress(player->ip), player->port) != sf::Socket::Done)
                {
                    std::cerr << "Cant send broadcast data to ip: " << player->ip << "\n";
                }
            }
        }

        
    }

    void handlePlayerLeft() 
    {
        // Handle player left request here
    }

    void handlePlayerMove() 
    {
        // Handle player move request here
    }

    void handlePlayerAttack() 
    {
        // Handle player attack request here
    }
};

int main() 
{
    try 
    {
        GameServer server(54000);
        server.run();
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}