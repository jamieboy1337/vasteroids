import * as WebSocket from "ws";
import { CreateWorldSim, WorldSim } from "./WorldSim";
import { generateID } from "./IDGen";
import { ServerPacket } from "./ServerPacket";
import { ConnectionPacket } from "./ConnectionPacket";
import { ClientPacket } from "./ClientPacket";
import { BiMap } from "./BiMap";
import now = require("performance-now");
import { ClientShip } from "../instances/Ship";
import { Point2D } from "../instances/GameTypes";
import { BiomeInfo } from "../instances/Biome";
import { ServerPacketDecoder } from "../packet/ServerPacketDecoder";

class SocketManager {
  game: WorldSim;

  // map from a player token to an ID
  players: Map<string, number>;

  // maps connections to their respective IDs
  sockets: BiMap<WebSocket, number>;

  timeouts: Map<WebSocket, NodeJS.Timeout>;

  update: NodeJS.Timeout;

  // todo: alow sockets to reconnect with a connection packet

  constructor(chunks: number, asts: number) {
    this.game = CreateWorldSim(chunks, asts);
    this.players = new Map();
    this.sockets = new BiMap();
    this.timeouts = new Map();
    // start some regular update event
    this.update = setInterval(this.handleUpdates.bind(this), 30);
  }

  async addSocket(socket: WebSocket, name: string) : Promise<void> {
    let ship_new = this.game.AddShip(name);
    this.sockets.insert(socket, ship_new.id);
    let token = await this.createPlayerToken();
    this.players.set(token, ship_new.id);
    let packet = {} as ConnectionPacket;
    packet.ship = ship_new;
    packet.playerToken = token;
    packet.chunkDims = this.game.GetChunkDims();
    packet.serverTime = this.game.GetServerTime();
    socket.send(JSON.stringify(packet));
    console.log(packet.playerToken);

    let timeout = setTimeout(() => {
      this.timeoutFunc_(socket)
    }, 15000);
    this.timeouts.set(socket, timeout);
    // set up msg listener and close listener.
    socket.addEventListener("message", (e) => { this.socketOnMessage_(socket, e.data); });
    socket.addEventListener("close", () => { this.socketOnClose_(socket); });
  }

  respawnShip(token: string) : ClientShip | undefined {
    // just respawn it! you asked after all :)
    // we could place a respawn token on the ship -- that way, we can set "destroyed" to false
    // and just place it.
    let id = this.players.get(token);
    if (!id) {
      // invalid token
      return;      
    }

    // respawn ship associated with the given token
    console.log("respawn request: " + id);
    return this.game.RespawnShip(id);
  }

  getBiomeInfo(origin: Point2D, dims: Point2D) : Array<BiomeInfo> {
    return this.game.GetLocalBiomeInfo(origin, dims);
  }

  private socketOnMessage_(socket: WebSocket, message: any) {
    // get packet
    let packet = JSON.parse(message) as ClientPacket;
    // match packet to socket id
    let id = this.sockets.getEntryT(socket);
    if (!id) {
      let errmsg = "incoming message does not have associated ID!";
      console.error(errmsg);
      return;
    }

    if (id !== packet.ship.id) {
      console.error("Bad socket id -- client sent " + packet.ship.id + ", server records " + id);
      socket.close();
      return;
    }

    let id_verify = this.players.get(packet.playerToken);
    if (id_verify !== id) {
      console.error("Socket was rejected because its token and stored ID did not agree.");
      socket.close();
      return;
    }

    let timeout = this.timeouts.get(socket);
    if (!timeout) {
      console.warn("timeout was not created for ID " + id);
    } else {
      // clear old timeout
      clearTimeout(timeout);
    }

    timeout = setTimeout(() => {
      this.timeoutFunc_(socket)
    }, 15000);
    this.timeouts.set(socket, timeout);

    // to do: add some more error prevention pertaining to individual values here.

    this.game.HandleClientPacket(packet);
  }

  private timeoutFunc_(socket: WebSocket) {
    socket.close();
  }

  private socketOnClose_(socket: WebSocket) {
    let id = this.sockets.getEntryT(socket);
    if (!id) {
      console.error("Closing a socket with no associated ID");
    }

    this.timeouts.delete(socket);
    this.sockets.removeT(socket);
    this.game.DeleteShip(id);
  }

  handleUpdates() {
    let res: { [x: string]: ServerPacket; };

    try {
      res = this.game.UpdateSim();
    } catch (e) {
      console.error(e);
      return;
    }

    for (let socket of this.sockets) {
      let id = socket[1];
      let pkt = res[id.toString()] as ServerPacket;
      if (!pkt) {
        console.error("connected socket ID " + id + " not in server packet object!");
      } else {
        let msg = new ServerPacketDecoder(pkt);
        socket[0].send(msg.encode());
      }
    }
  }

  async createPlayerToken() : Promise<string> {
    let str : string;
    do {
      str = await generateID(32);
    } while (this.players.get(str));
      
    return str;
  }
}

export { SocketManager };