#include "extra_sf_packet_op.hpp"

#include <SFML/Network.hpp>
#include <iostream>
#include <cstdint>
#include <vector>
#include <atomic>
#include <memory>
#include <cstring>
#include <string>
#include <mutex>


enum  RequestType : char 
{
    CONNECT,
    PLAYER_JOIN,
    PLAYER_LEFT,
    PLAYER_MOVE,
    PLAYER_ATTACK,
    PLAYER_COUNT
};

enum  Direction : char 
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
    sf::Int64 id{0};
    std::string ip{};
    unsigned short port{};
    std::string name{};
    Position pos{};
    Direction direction{};
    float scale{};
    float traverseAngle{};
    sf::Int64 point{};
    bool isActive{false};
    Player() = default;
    Player(int64_t id) : id {id}
    {}
};
sf::Packet& operator <<(sf::Packet& packet,  Player& player)
{
    return packet << player.id << player.name << player.pos.x << player.pos.y << player.direction << player.scale << player.traverseAngle << player.point;
}

sf::Packet& operator >>(sf::Packet& packet,  Player& player)
{
    return packet >> player.id >> player.name >> player.pos.x >> player.pos.y >> player.direction >> player.scale >> player.traverseAngle >> player.point;
}

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
        sf::Packet data;
        sf::IpAddress sender;
        unsigned short senderPort;

        std::size_t received = 0;
        if (socket.receive(data, sender, senderPort) == sf::Socket::Done) 
        {
            std::lock_guard<std::mutex> lock{ mt };
            std::cout << "Received " << received << " bytes from " << sender << ":" << senderPort << std::endl;
            handleRequest(data, sender, senderPort);
        }
    }

    void handleRequest(sf::Packet& data, sf::IpAddress& sender, const unsigned short senderPort) 
    {
        sf::Int8 reqType{};
        data >> reqType;
        switch (static_cast<RequestType>(reqType)) 
        {
            case CONNECT:
                handleConnect(sender, senderPort);
                break;
            case PLAYER_JOIN:
                handlePlayerJoin(data, sender, senderPort);
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

    void handlePlayerJoin(sf::Packet& packet, const sf::IpAddress& sender,const unsigned short senderPort)
    {
        bool isEmptySlot = false;
        int64_t newPlayerId = -1;
        for (auto& player : playerContainer) 
        {
            if (!player->isActive) 
            {

                newPlayerId = player->id;
                player->isActive = true;
                packet >> player->name;
                std::cout << "Player name: " << player->name << "\n";
                player->ip = sender.toString();
                std::cout << "PLayerIp: " << player->ip << "\n";

                player->port = senderPort;
                isEmptySlot = true;
                break;
            }
        }

        if (newPlayerId == -1) 
        {
            newPlayerId = uid.load();
            uid++;
            auto newPlayer = std::make_unique<Player>();
            packet >> newPlayer->name;
            newPlayer->ip = sender.toString();
            newPlayer->id = newPlayerId;
            newPlayer->isActive = true;
            newPlayer->port = senderPort;
            playerContainer.push_back(std::move(newPlayer));
        }
        std::cout << sender << " " << senderPort << " NewPlayerId: " << newPlayerId << " PlayerName: " << playerContainer[newPlayerId]->name <<"\n";
        sendToNewPLayer(sender, senderPort, newPlayerId);
        broadcastPlayerJoin(newPlayerId);
    }

    void sendToNewPLayer(const sf::IpAddress& sender, const unsigned short senderPort, const int64_t newPlayerId)
    {
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

        sf::Packet playerCountPacket;
        playerCountPacket << static_cast<char>(PLAYER_COUNT) << static_cast<sf::Int64>(activePlayerNum);

        if (socket.send(playerCountPacket, sender, senderPort) != sf::Socket::Done)
        {
            std::cerr << "Cannot send player number to new player!" << std::endl;
            return;
        }

        sf::Packet playersPacket;
        for (const auto& player : playerContainer)
        {
            if (player->isActive)
            {
                playersPacket << *player; 
            }
        }

        if (socket.send(playersPacket, sf::IpAddress(playerContainer[newPlayerId]->ip), playerContainer[newPlayerId]->port) != sf::Socket::Done)
        {
            std::cerr << "Cannot send player container to new player!" << std::endl;
            return;
        }
    }

    void broadcastPlayerJoin(int64_t newPlayerId)
    {
        sf::Packet broadcastPacket;
        broadcastPacket << static_cast<char>(PLAYER_JOIN);
        broadcastPacket << *playerContainer[newPlayerId]; 

        for (const auto& player : playerContainer)
        {
            if (player->isActive && player->id != newPlayerId)
            {
                if (socket.send(broadcastPacket, sf::IpAddress(player->ip), player->port) != sf::Socket::Done)
                {
                    std::cerr << "Cant send broadcast data to ip: " << player->ip << "\n";
                }
            }
        }
    }

    void handlePlayerLeft() 
    {
    }

    void handlePlayerMove() 
    {
    }

    void handlePlayerAttack() 
    {
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