(*
 * SerialHello — 串口输出示例
 *
 * 编译: bash build.sh
 *)

let message = "Hello from Camelino on RP2350!\n"

let () =
  serial_write 0 message
