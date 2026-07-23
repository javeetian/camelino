(*
 * Blink — Camelino Arduino 示例
 *
 * 编译: bash build.sh
 * 烧录: 在 Arduino IDE 中打开 Blink/ 目录，编译并上传
 *
 * 依赖: lib/camelino/ 中的 external 声明 (pin_mode, digital_write, delay_ms, etc.)
 *)

let led = 25

let rec loop () =
  digital_write led 1;
  delay_ms 500;
  digital_write led 0;
  delay_ms 500;
  loop ()

let () =
  pin_mode led 1;  (* 1 = OUTPUT *)
  loop ()
