import { Asteroid } from "../instances/Asteroid";
import { InstanceType, Instance } from "../instances/GameTypes";
import { Projectile } from "../instances/Projectile";
import { ClientShip } from "../instances/Ship";

interface ServerPacket {
  asteroids: Array<Asteroid>,
  ships: Array<ClientShip>,
  deltas: Array<Instance>,
  projectiles: Array<Projectile>,
  projectilesLocal: Array<Projectile>,
  deleted: Array<number>,
  deletedLocal: Array<number>,

  // seconds since server creation
  serverTime: number

  // client's current score
  score: number;
}

export { ServerPacket };