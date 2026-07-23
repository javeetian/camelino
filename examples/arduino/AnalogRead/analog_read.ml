(*
 * AnalogRead — ADC 读取 + 串口输出示例
 *
 * 编译: bash build.sh
 *)

let sensor_pin = 0  (* A0 on Arduino *)

let rec loop () =
  let value = analog_read sensor_pin in
  serial_write_int 0 value;
  serial_write 0 "\n";
  delay_ms 500;
  loop ()

let () = loop ()
