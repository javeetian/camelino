#include <Arduino.h>

void gpio_bindings_init(void) {
}

void gpio_pin_mode(uint8_t pin, uint8_t mode) {
  pinMode(pin, mode);
}

void gpio_digital_write(uint8_t pin, uint8_t value) {
  digitalWrite(pin, value);
}

uint8_t gpio_digital_read(uint8_t pin) {
  return digitalRead(pin);
}
