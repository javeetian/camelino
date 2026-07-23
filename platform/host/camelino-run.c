/**
 * camelino-run — Camelino .camel bytecode runner (host)
 *
 * Usage: camelino-run [file.camel]
 */
#include <stdio.h>
#include <stdlib.h>
#include "vm.h"
#include "memory.h"
#include "bytecode.h"
#include "value.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s file.camel\n", argv[0]);
        return 1;
    }
    const char *path = argv[1];
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    uint8_t *buf = malloc(sz);
    if (!buf) { fclose(f); return 1; }
    fread(buf, 1, sz, f);
    fclose(f);

    int rc = caml_bytecode_load(buf, sz);
    if (rc != 0) {
        fprintf(stderr, "Error loading %s (code %d)\n", path, rc);
        free(buf);
        return 1;
    }

    caml_vm_init();
    caml_load_bytecode_buf(caml_bytecode_code(), caml_bytecode_code_size(),
                           caml_bytecode_data(), caml_bytecode_data_size());
    caml_interpret();

    if (caml_vm_halted()) {
        printf("halted, acc=%ld, instructions=%zu\n",
               (long)Long_val(caml_get_acc()),
               caml_get_instruction_count());
    } else {
        printf("yielded, acc=%ld, instructions=%zu\n",
               (long)Long_val(caml_get_acc()),
               caml_get_instruction_count());
    }

    caml_bytecode_unload();
    free(buf);
    return 0;
}
