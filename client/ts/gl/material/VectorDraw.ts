import { VectorMesh2D } from "../VectorMesh2D";
import { CompileShaders } from "./CompileShaders";

export class VectorDraw {
  prog: WebGLProgram;

  loc: number;
  locColor: number;

  compilePromise: Promise<void>;

  constructor(gl: WebGLRenderingContext) {
    let base = window.location.protocol + "//" + window.location.host;
    let respFrag = fetch(base + "/glsl/vectordraw.frag");
    let respVert = fetch(base + "/glsl/vectordraw.vert");
    this.compilePromise = Promise.all([respVert, respFrag])
      .then(async (res) => {
        this.prog = CompileShaders(gl, await res[0].text(), await res[1].text());
        this.prepareAttributes(gl);
      }).catch(err => {
        console.error(err);
      });
  }

  prepareAttributes(gl: WebGLRenderingContext) : void {
    this.loc = gl.getAttribLocation(this.prog, "aPosition");
    this.locColor = gl.getAttribLocation(this.prog, "aColor");
  }

  async waitUntilCompiled() {
    await this.compilePromise;
  }

  drawMaterial(gl: WebGLRenderingContext, mesh: VectorMesh2D) {
    gl.useProgram(this.prog);
    mesh.draw(gl, this.loc, this.locColor);
  }
}