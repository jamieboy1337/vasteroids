
import { ClientPacket } from "../client/ClientPacket"
import { Point2D } from "../instances/GameTypes";
import { ClientShip } from "../instances/Ship";
import { ServerPacket } from "./ServerPacket";
/**
 *  Represents the component which handles and updates the state of the game world. 
 */
interface WorldSim {

  /**
   * @returns the number of chunks in the X/Y directions.
   */
  GetChunkDims() : number;

  /**
   * Handles updates from clients.
   * @param packet - The packet being updated.
   */
  HandleClientPacket(packet: ClientPacket) : void;

  /**
   * Updates the simulation. Should be done once per server tick.
   *   (we could JSONify this for NODE?)
   * @returns a map containing the data to be sent to each client.
   */
  UpdateSim() : Map<number, ServerPacket>;

  /**
   * Adds a new ship to the world sim.
   * @param name - the name associated with the new ship.
   * @returns a new object representing the current state of the ship.
   */
  AddShip(name: string) : ClientShip;

  /**
   * Removes a ship from this WorldSim.
   * @param id - the ID of the ship we wish to remove.
   * @returns true if the ship can be removed, false otherwise.
   */
  DeleteShip(id: number) : boolean;

  /**
   * Returns the relative amount of activity in nearby chunks.
   * @param origin - the top-left chunk we wish to fetch.
   * @param dims - the number of chunks fetched along x and y axes.
   * @returns a 2D array containing the relative activity in each chunk.
   */
  // GetLocalChunkActivity(origin: Point2D, dims: Point2D) : Array<Array<number>>;
}