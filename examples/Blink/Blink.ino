#include <Camelino.h>

// 用 OCaml 语法写的闪烁逻辑
const char* blink_script = R"(
let led_pin = 25 in
let rec loop () =
  digital_write led_pin true;
  delay 500;
  digital_write led_pin false;
  delay 500;
  loop ()
in
pin_mode led_pin 1;  (* 1 = OUTPUT *)
loop ()
;;)";

void setup() {
    Camelino_begin();
    
    // 直接执行 OCaml 脚本
    int ret = Camelino_eval(blink_script);
    if (ret != 0) {
        Serial.println("Failed to run blink script");
    }
}

void loop() {
    // REPL 交互（可选）
    Camelino_repl_task();
}