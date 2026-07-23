(*
 * Fib — 主机端 Fibonacci 示例
 *
 * 编译: bash build.sh
 * 运行: ./fib
 *
 * 可与 ocamlrun 差分比对:
 *   ocamlc -o fib.cmo fib.ml && ocamlrun fib.cmo
 *   ./fib
 *
 * 注意: 当前 camelino-embed 尚未完整实现 Marshal 解析。
 * 待完成后，含函数的程序即可端到端运行。
 *)

let rec fib n = if n < 2 then n else fib (n-1) + fib (n-2)
let () = Printf.printf "fib(10) = %d\n" (fib 10)
