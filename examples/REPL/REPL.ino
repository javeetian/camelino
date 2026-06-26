#include <Camelino.h>

void setup() {
  Camelino_begin();
  Serial.println("Camelino REPL: load bytecode via serial");
}

void loop() {
  Camelino_repl_task();
}
