(*
 * Fib — Fibonacci 计算（纯函数，不需要 stdlib）
 *
 * 编译: bash build.sh
 *
 * 可与 ocamlrun 差分比对:
 *   ocamlc -o fib.cmo fib.ml && ocamlrun fib.cmo
 *)
let rec fib n = if n < 2 then n else fib (n-1) + fib (n-2)
let _ = fib 10
