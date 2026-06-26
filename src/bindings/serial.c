#include <Arduino.h>

void serial_bindings_init(void) {
}

void serial_begin(unsigned long baud) {
  Serial.begin(baud);
}

int serial_available(void) {
  return Serial.available();
}

int serial_read(void) {
  return Serial.read();
}

size_t serial_write(const char *data, size_t len) {
  return Serial.write(reinterpret_cast<const uint8_t *>(data), len);
}

void serial_print(const char *data) {
  Serial.print(data);
}
