#include <server/WorldSim.hpp>
#include <client/ClientPacket.hpp>

#include <AsteroidGenerator.hpp>

namespace vasteroids {
namespace server {

using client::ClientPacket;

Napi::Function WorldSim::GetClassInstance(Napi::Env env) {
  return DefineClass(env, "WorldSim", {
    InstanceMethod("GetChunkDims", &WorldSim::GetChunkDims),
    InstanceMethod("HandleClientPacket", &WorldSim::HandleClientPacket)
  });
}

WorldSim::WorldSim(const Napi::CallbackInfo& info) : ObjectWrap(info) {
  id_max_ = 0;
  Napi::Env env = info.Env();
  Napi::Value chunks = info[0];
  if (!chunks.IsNumber()) {
    TYPEERROR(env, "Param used to construct worldsim is not a number!");
  }

  chunk_dims_ = chunks.As<Napi::Number>().Int32Value();

  Napi::Value asteroidsObj = info[1];
  if (!asteroidsObj.IsNumber()) {
    TYPEERROR(env, "Number of asteroids initially spawned not specified.");   
  }

  int asteroids = asteroidsObj.As<Napi::Number>().Int32Value();
  
  // generating asteroids initially?
  // use a gaussian distribution to place our asteroids in the world
  // use GenerateNewAsteroid to place them
  // gaussian a chunk, choose a random point inside that chunk.
  std::random_device dev;
  gen = std::mt19937(dev());
  chunk_gen = std::normal_distribution<>(chunk_dims_ / 2.0, chunk_dims_ / 4.0);
  coord_gen = std::uniform_real_distribution<float>(0.0f, 128.0f);
  velo_gen = std::uniform_real_distribution<float>(-3.0, 3.0);

  WorldPosition temp;
  for (int i = 0; i < asteroids; i++) {
    while (temp.chunk.x >= 0 && temp.chunk.y < chunk_dims_
        && temp.chunk.y >= 0 && temp.chunk.y < chunk_dims_) {
      temp.chunk.x = static_cast<int>(chunk_gen(gen));
      temp.chunk.y = static_cast<int>(chunk_gen(gen));
    }

    temp.position.x = coord_gen(gen);
    temp.position.y = coord_gen(gen);

    SpawnNewAsteroid(temp);
  }
}

Napi::Value WorldSim::GetChunkDims(const Napi::CallbackInfo& info) {
  return Napi::Number::New(info.Env(), chunk_dims_);
}

Napi::Value WorldSim::HandleClientPacket(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  Napi::Value packetObj = info[0];
  if (!packetObj.IsObject()) {
    TYPEERROR(env, "argument to `HandleClientPacket` is not a ClientPacket!");
  }

  ClientPacket packet(packetObj.As<Napi::Object>());

  // note: there's currently no security on ships -- if you know another player's ship ID, you can just snag it.
  // a solution to this would be to return a tuple which contains a private identifier for each player
  // when we receive a ClientPacket, we verify that the ID they provide matches the one we have, and then let it pass if so.
  
  // find old ship record
  auto ship_old = ships_.find(packet.client_ship.id);
  if (ship_old == ships_.end()) {
    Napi::Error::New(env, "Updated ship does not exist!").ThrowAsJavaScriptException();
  }

  Point2D<int> chunk = ship_old->second;

  auto c = chunks_.find(chunk);
  if (c == chunks_.end()) {
    Napi::Error::New(env, "Invariant not maintained -- ship does not exist in chunk!").ThrowAsJavaScriptException();
  }

  auto& ship_new = packet.client_ship;

  {
    // update ver number
    Ship* ship_last = c->second.GetShip(packet.client_ship.id);
    ship_new.ver = ship_last->ver + 1;
  }
  
  // remove old ship from old chunk
  c->second.RemoveInstance(packet.client_ship.id);

  // insert new ship into new chunk
  Point2D<int> new_chunk = ship_new.position.chunk;
  if (chunks_.find(new_chunk) == chunks_.end()) {
    CreateChunk(new_chunk);
  }

  ship_new.last_update = std::chrono::high_resolution_clock::now();
  chunks_.at(new_chunk).InsertShip(ship_new);

  // update ships list to match new chunk
  ships_.insert(std::make_pair(ship_new.id, new_chunk));
  return env.Undefined();
  // ret.
}

Napi::Value WorldSim::UpdateSim(const Napi::CallbackInfo& info) {
  // update all components
  // figure out which chunks we need to update
  Napi::Env env = info.Env();
  Napi::Object obj_ret = Napi::Object::New(env);
  std::unordered_set<Point2D<int>> update_chunks;
  for (auto ship : ships_) {
    for (int x = ship.second.x - 1; x <= ship.second.x + 1; x++) {
      for (int y = ship.second.y - 1; y <= ship.second.y + 1; y++) {
        if (x < 0 || x >= chunk_dims_) {
          continue;
        }

        if (y < 0 || y >= chunk_dims_) {
          continue;
        }

        update_chunks.insert(Point2D<int>(x, y));
      }
    }
  }

  // update the chunks we have to update
  ServerPacket collate;
  for (auto point : update_chunks) {
    if (!chunks_.count(point)) {
      continue;
    }

    chunks_.at(point).UpdateChunk(collate);
  }

  // collate now contains all of the elements which have been displaced, properly simulated.
  // place remaining elements in their proper places!
  for (auto a : collate.asteroids) {
    Point2D<int> chunk_coord = a.position.chunk;
    if (!chunks_.count(chunk_coord)) {
      CreateChunk(chunk_coord);
    }

    chunks_.at(chunk_coord).InsertAsteroid(a);
  }

  for (auto s : collate.ships) {
    Point2D<int> chunk_coord = s.position.chunk;
    if (!chunks_.count(chunk_coord)) {
      CreateChunk(chunk_coord);
    }

    chunks_.at(chunk_coord).InsertShip(s);
  }

  // now, we would check relevant chunks for collisions
  // im gonna skip this for now

  // lastly, we need to figure out which entities to expose to which instances
  
  // for each chunk nearby:
  // create a map to contain new knowns
  // if known and old version: delta only
  // if known and new version: send nothing
  // if unknown: send full

  // note: we can really easily multithread this  
  for (auto& ship : ships_) {
    uint64_t id = ship.first;
    std::unordered_map<uint64_t, uint32_t>* knowns = nullptr;
    if (known_ids_.count(id)) {
      knowns = &known_ids_.at(id);
    }

    std::unordered_map<uint64_t, uint32_t> knowns_new;
    
    ServerPacket res;
    for (int x = ship.second.x - 1; x <= ship.second.x + 1; x++) {
      for (int y = ship.second.y - 1; y <= ship.second.y + 1; y++) {
        if (x < 0 || x >= chunk_dims_) {
          continue;
        }

        if (y < 0 || y >= chunk_dims_) {
          continue;
        }

        chunks_.at(Point2D<int>(x, y)).GetContents(res);
      }
    }

    Instance delta_pkt;
    // res now contains all nearby objects -- trim it down based on `knowns`
    auto itr_a = res.asteroids.begin();
    while (itr_a != res.asteroids.end()) {
      knowns_new.insert(std::make_pair(itr_a->id, itr_a->ver));
      if (knowns && knowns->count(itr_a->id)) {
        // known
        if (knowns->at(itr_a->id) != itr_a->ver) {
          // known, but out of date
          // remove from itr, add to delta
          delta_pkt.id = itr_a->id;
          delta_pkt.position = itr_a->position;
          delta_pkt.velocity = itr_a->velocity;
          delta_pkt.rotation = itr_a->rotation;
          delta_pkt.rotation_velocity = itr_a->rotation_velocity;
          res.deltas.push_back(std::move(delta_pkt));
        }

        itr_a = res.asteroids.erase(itr_a);
      } // unknown -- send the whole packet!
    }

    auto itr_s = res.ships.begin();
    while (itr_s != res.ships.end()) {
      knowns_new.insert(std::make_pair(itr_s->id, itr_s->ver));
      if (knowns && knowns->count(itr_s->id)) {
        if (knowns->at(itr_s->id) != itr_s->ver) {
          delta_pkt.id = itr_s->id;
          delta_pkt.position = itr_s->position;
          delta_pkt.velocity = itr_s->velocity;
          delta_pkt.rotation = itr_s->rotation;
          delta_pkt.rotation_velocity = itr_s->rotation_velocity;
          res.deltas.push_back(std::move(delta_pkt));
        }

        itr_s = res.ships.erase(itr_s);
      }
    }

    known_ids_.insert(std::make_pair(id, std::move(knowns_new)));
    // res still contains our server packet for this ship
    // map from ID to that packet!
    // key: id -- value: server packet
    obj_ret.Set(std::to_string(id), res.ToNodeObject(env));
  }

  // obj_ret returns
  // keys: IDs as strings -- values: serverpackets
  return obj_ret;
}

Napi::Value WorldSim::AddShip(const Napi::CallbackInfo& info) {
  Napi::Env env =  info.Env();
  Napi::Value val = info[0];
  if (!val.IsObject()) {
    TYPEERROR(env, "`name` is not a string!");
  }

  Ship s;
  // get name for this ship
  s.name = val.As<Napi::String>().Utf8Value();
  // add entries for our new ship
  while (s.position.chunk.x >= 0 && s.position.chunk.y < chunk_dims_
        && s.position.chunk.y >= 0 && s.position.chunk.y < chunk_dims_) {
    s.position.chunk.x = static_cast<int>(chunk_gen(gen));
    s.position.chunk.y = static_cast<int>(chunk_gen(gen));
  }

  if (!chunks_.count(s.position.chunk)) {
    CreateChunk(s.position.chunk);
  }

  s.position.position.x = coord_gen(gen);
  s.position.position.y = coord_gen(gen);
  s.id = id_max_++;
  s.velocity = {0.0f, 0.0f};
  s.rotation = 0.0f;
  s.rotation_velocity = 0.0f;
  s.ver = 0;
  s.last_update = std::chrono::high_resolution_clock::now();
  // create the new ship and give it an id
  // find a random position for it to roam
  // return the new position of this ship

  ships_.insert(std::make_pair(s.id, s.position.chunk));
  known_ids_.insert(std::make_pair(s.id, std::unordered_map<uint64_t, uint32_t>()));

  return s.ToNodeObject(env);
}

Napi::Value WorldSim::DeleteShip(const Napi::CallbackInfo& info) {
  // find the ship in our map and remove it
  // remove all entries associated with it
  // move on :)
  Napi::Env env = info.Env();
  Napi::Value val = info[0];
  if (!val.IsNumber()) {
    TYPEERROR(env, "param is not a number!");
  }

  uint64_t id = static_cast<uint64_t>(val.As<Napi::Number>().Int64Value());
  // remove from chunk
  if (!ships_.count(id)) {
    return Napi::Boolean::New(env, false);
  }

  if (!chunks_.at(ships_.at(id)).RemoveInstance(id)) {
    Napi::Error::New(env, "invariant broken: ship not present in chunk!").ThrowAsJavaScriptException();
  }

  // remove from class
  ships_.erase(id);
  known_ids_.erase(id);
  return Napi::Boolean::New(env, true);
}

// private funcs
void WorldSim::CreateChunk(Point2D<int> chunk_coord) {
  chunks_.insert(std::make_pair(chunk_coord, Chunk()));
}

void WorldSim::SpawnNewAsteroid(WorldPosition coord) {
  SpawnNewAsteroid(coord, 1.5f, 12);
}

void WorldSim::SpawnNewAsteroid(WorldPosition coord, float radius, int points) {
  if (!chunks_.count(coord.chunk)) {
    CreateChunk(coord.chunk);
  }

  auto& chunk = chunks_.at(coord.chunk);
  auto ast = GenerateAsteroid(radius, points);
  // random velocity
  ast.velocity = { coord_gen(gen), coord_gen(gen) };
  ast.rotation_velocity = coord_gen(gen);
  ast.position = coord;
  ast.ver = 0;
  ast.id = id_max_++;
  ast.last_update = std::chrono::high_resolution_clock::now();

  chunk.InsertAsteroid(ast);
}


}
}