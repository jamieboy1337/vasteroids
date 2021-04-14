#include <server/ServerPacket.hpp>

namespace vasteroids {
namespace server {

void ServerPacket::ConcatPacket(const ServerPacket& packet) {
  asteroids.insert(asteroids.end(), packet.asteroids.begin(), packet.asteroids.end());
  ships.insert(ships.end(), packet.ships.begin(), packet.ships.end());
}

Napi::Object ServerPacket::ToNodeObject(Napi::Env env) {
  Napi::Object obj = Napi::Object::New(env);
  Napi::Array instances = Napi::Array::New(env);

  {
    Napi::Object asteroid_info = Napi::Object::New(env);
    std::string type = InstanceToString(InstanceType::ASTEROID);
    asteroid_info.Set("type", type);
    Napi::Array instance_arr = Napi::Array::New(env);
    uint32_t i = 0;
    for (const auto& asteroid : asteroids) {
      instance_arr[i++] = asteroid.ToNodeObject(env);
    }

    asteroid_info.Set("data", instance_arr);
    instances[(uint32_t)0] = asteroid_info;
  }

  {
    Napi::Object ship_info = Napi::Object::New(env);
    std::string type = InstanceToString(InstanceType::SHIP);
    ship_info.Set("type", type);
    Napi::Array instance_arr = Napi::Array::New(env);
    uint32_t i = 0;
    for (const auto& ship : ships) {
      instance_arr[i++] = ship.ToNodeObject(env);
    }

    ship_info.Set("data", instance_arr);
    instances[(uint32_t)1] = ship_info;

    obj.Set("instances", instances);
    return obj;
  }
  
}

}
}