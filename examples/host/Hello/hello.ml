(*
 * Hello — 主机端 Camelino 示例
 *
 * 编译: bash build.sh
 * 运行: ./hello
 *
 * 注意: 当前 camelino-embed 尚未完整实现 .cmo Marshal 反序列化 (Design.md §3.2.1 任务 1.6.1)。
 * 待 Marshal 解析完成后，含 Printf/函数/模块的程序即可端到端运行。
 * 当前仅纯算术常量程序可在 VM 中执行 (参见 tools/test_suite/ 中的差分测试)。
 *)

let () = Printf.printf "Hello from Camelino on host!\n"
