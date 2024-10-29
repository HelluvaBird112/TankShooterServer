#pragma once
// source: https://en.sfml-dev.org/forums/index.php?topic=17075.0
#include <type_traits>
#include <SFML/Network/Packet.hpp>
template<typename T, typename = typename std::enable_if<std::is_enum<T>::value>::type>
sf::Packet& operator<<(sf::Packet& roPacket, const T& rkeMsgType)
{
	return roPacket << static_cast<std::underlying_type<T>::type>(rkeMsgType);
}

template<typename T, typename = typename std::enable_if<std::is_enum<T>::value>::type>
sf::Packet& operator>>(sf::Packet& roPacket, T& reMsgType)
{
	typename std::underlying_type<T>::type xMsgType;
	roPacket >> xMsgType;
	reMsgType = static_cast<T>(xMsgType);
	return roPacket;
}