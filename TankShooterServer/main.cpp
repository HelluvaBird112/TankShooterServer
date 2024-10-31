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
#include <algorithm>

constexpr sf::Uint32 screenWidth = 840;
constexpr sf::Uint32 screenHeight = 640;


enum  RequestType : sf::Uint8 
{
    CONNECT,
    PLAYER_JOIN,
    PLAYER_LEFT,
    PLAYER_MOVE,
    PLAYER_ATTACK,
    PLAYER_COUNT
};

enum  Direction : sf::Uint8
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
    sf::Int32 x = 0;
    sf::Int32 y = 0;
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
        //socket.setBlocking(false);
        for (int64_t i = 0; i < 100; ++i) 
        {
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

    void receiveData() 
    {
        sf::Packet data;
        sf::IpAddress sender;
        unsigned short senderPort;

        std::size_t received = 0;
        if (socket.receive(data, sender, senderPort) == sf::Socket::Done) 
        {
            std::cout << "Received " << data.getDataSize() << " bytes from " << sender << ":" << senderPort << std::endl;
            sf::Uint64 playerId{ 0 };
            for (const auto& player: playerContainer )
            {
                if (player->ip.compare(sender.toString()) && player->port == senderPort)
                {
                    playerId = player->id;
                    break;
                }
            }
            handleRequest(data,  playerId, sender, senderPort);
        }
    }

    void handleRequest(sf::Packet& data, sf::Uint64 playerId, sf::IpAddress& sender, const unsigned short senderPort) 
    {
        sf::Uint8 reqType{};
        data >> reqType;
        std::cout << static_cast<RequestType>(reqType) << "\n";
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
                handlePlayerMove(data, playerId);
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

    void sendToNewPLayer(const sf::IpAddress& sender, const unsigned short senderPort, const sf::Uint64 newPlayerId)
    {
        if (newPlayerId < 0 || newPlayerId >= playerContainer.size() || !playerContainer[newPlayerId]->isActive)
        {
            std::cerr << "Error: Invalid newPlayerId: " << newPlayerId << std::endl;
            return;
        }
        sf::Uint64 activePlayerNum = 0;
        for (const auto& player : playerContainer)
        {
            if (player->isActive)
            {
                activePlayerNum++;
            }
        }

        /*sf::Packet playerCountAndIdPacket{};
        RequestType reqType{ PLAYER_COUNT };
        playerCountAndIdPacket << reqType << activePlayerNum << newPlayerId;
        std::cout << "Active: " << activePlayerNum << " NewPlayerId: " << newPlayerId << "\n";
        std::cout << "sizeof send packet: " << playerCountAndIdPacket.getDataSize();
        if (socket.send(playerCountAndIdPacket, sender, senderPort) != sf::Socket::Done)
        {
            std::cerr << "Cannot send player number to new player!" << std::endl;
            return;
        }*/

        sf::Packet playersPacket{};
        std::cout << "Data send:\n";
        for (const auto& player : playerContainer)
        {
            /*if (player->isActive)
            {*/
                playersPacket << *player; 
                //std::cout << (*player).id << " " << (*player).name << "\n";
            /*}*/
        }
        std::cout << "Size of player vector packet :" << playersPacket.getDataSize() << " bytes\n";
        if (socket.send(playersPacket, sender, senderPort) != sf::Socket::Done)
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
                std::cout << "Broadcast :" << player->id << "\n";
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

    void handlePlayerMove(sf::Packet& data, sf::Uint64 playerId) 
    {
        Direction direction{};
        RequestType cmdType{ PLAYER_MOVE };
        data >> direction;
        sf::Packet packet{};
        Position& playerPos = playerContainer[playerId]->pos;
        switch (direction)
        {
        case NORTH:
            playerPos.y--;
            break;
        case NORTHEAST:
            playerPos.x--;
            playerPos.y--;
            break;
        case EAST:
            playerPos.x--;
            break;
        case SOUTHEAST:
            playerPos.x--;
            playerPos.y++;
            break;
        case SOUTH:
            playerPos.y++;
            break;
        case SOUTHWEST:
            playerPos.x++;
            playerPos.y++;
            break;
        case WEST:
            playerPos.x++;
            break;
        case NORTHWEST:
            playerPos.x++;
            playerPos.y--;
            break;
        default:
            break;
        }
        playerPos.x = std::clamp(playerPos.x, 0, sf::Int32(screenWidth));
        playerPos.y = std::clamp(playerPos.y, 0, sf::Int32(screenHeight));


        packet << cmdType << playerId << playerPos.x << playerPos.y;

        for (const auto& player : playerContainer)
        {
            if (player->isActive)
            {
                std::cout << "send to ip: " << player->ip << " port:" << player->port <<"\n";
                if (socket.send(packet, sf::IpAddress(player->ip), player->port) != sf::Socket::Done)
                {
                    std::cerr << "Cant send Move Packet to Client has Ip: " << player->ip << " and Port: " << player->port << "\n";
                }
            }
        }

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