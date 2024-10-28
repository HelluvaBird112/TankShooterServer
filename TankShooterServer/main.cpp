﻿#include <SFML/Network.hpp>
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
    PLAYER_ATTACK
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
    int64_t id;
    char ip[15];
    unsigned short port;
    char name[32];
    Position pos;
    Direction direction;
    float scale;
    float traverseAngle;
    int64_t point;
    bool isActive;

    static constexpr int getSerializeSize() noexcept 
    {
        return sizeof(id) + sizeof(ip) + sizeof(port) + sizeof(name) + sizeof(pos)
            + sizeof(direction) + sizeof(scale) + sizeof(traverseAngle) + sizeof(point) + sizeof(isActive);
    }

    void serialize(char* buffer) const noexcept 
    {
        std::size_t offset = 0;
        std::memcpy(buffer + offset, this, getSerializeSize());
    }

    void deserialize(const char* buffer) noexcept 
    {
        std::memcpy(this, buffer, getSerializeSize());
    }
};

class GameServer 
{
public:
    GameServer(unsigned short port) : uid(1) 
    {
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
    std::atomic_int uid;
    std::vector<std::unique_ptr<Player>> playerContainer{ 1000 };
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

    void handleRequest(const char* data, sf::IpAddress& sender, unsigned short senderPort) 
    {
        switch (data[0]) 
        {
            case CONNECT:
                handleConnect(sender, senderPort);
                break;
            case PLAYER_JOIN:
                handlePlayerJoin(data + 1, sender);
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
                std::cout << "Unknown request type: " << data[0] << std::endl;
                break;
        }
    }

    void handleConnect(sf::IpAddress& sender, unsigned short senderPort) 
    {
    }

    void handlePlayerJoin(const char* name, sf::IpAddress& sender) 
    {
        bool isEmptySlot = false;
        int64_t newPlayerId = -1;

        for (auto& player : playerContainer) 
        {
            if (!player->isActive) 
            {
                newPlayerId = player->id;
                player->isActive = true;
                std::strncpy(player->ip, sender.toString().c_str(), sizeof(player->ip) - 1);
                std::strncpy(player->name, name, sizeof(player->name) - 1);
                isEmptySlot = true;
                break;
            }
        }

        if (newPlayerId == -1) 
        {
            newPlayerId = uid++;
            auto newPlayer = std::make_unique<Player>();
            newPlayer->id = newPlayerId;
            newPlayer->isActive = true;
            std::strncpy(newPlayer->ip, sender.toString().c_str(), sizeof(newPlayer->ip) - 1);
            std::strncpy(newPlayer->name, name, sizeof(newPlayer->name) - 1);
            playerContainer.push_back(std::move(newPlayer));
        }

        broadcastPlayerJoin(newPlayerId);
    }

    void broadcastPlayerJoin(int64_t newPlayerId) 
    {
        char responseBuffer[1024]{};
        char broadcastMessage[Player::getSerializeSize() + 1]{};
        broadcastMessage[0] = PLAYER_JOIN;

        playerContainer[newPlayerId]->serialize(broadcastMessage + 1);

        std::size_t offset = 1;
        for (const auto& player : playerContainer) 
        {
            if (player->isActive && player->id != newPlayerId) 
            {
                player->serialize(responseBuffer + offset);
                offset += Player::getSerializeSize();
                socket.send(broadcastMessage, sizeof(broadcastMessage), sf::IpAddress(player->ip), player->port);
            }
        }

        socket.send(responseBuffer, offset, sf::IpAddress(playerContainer[newPlayerId]->ip), playerContainer[newPlayerId]->port);
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