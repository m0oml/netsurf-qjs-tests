import { b, bump } from "./b.js";
import { c } from "./sub/c.js";
export const a = "a" + b + c;
export const count = bump();
