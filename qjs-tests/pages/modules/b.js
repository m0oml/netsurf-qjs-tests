import { a } from "./a.js";
window.__evals = (window.__evals || 0) + 1;
export const b = "b";
export function bump() { return window.__evals; }
export function late() { return a; }
