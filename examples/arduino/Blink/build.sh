#!/bin/bash
# ============================================================
# Blink 示例 — 编译 OCaml 源码 → 生成 bytecode.h
#
# 用法: bash build.sh
# 输出: bytecode.h (放在当前目录，供 Blink.ino #include)
# ============================================================
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(cd ../../.. && pwd)"

echo "=== Camelino Blink: .ml → bytecode.h ==="

# 1. 编译 camelino-embed 工具（如未编译）
EMBED="$ROOT/tools/camelino-embed/camelino-embed"
if [ ! -x "$EMBED" ]; then
    echo "Building camelino-embed..."
    (cd "$ROOT/tools/camelino-embed" && \
     ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml)
fi

# 2. 编译 OCaml → .cmo
echo "Compiling blink.ml → blink.cmo..."
ocamlc -I "$ROOT/lib/camelino" -c -o blink.cmo blink.ml

# 3. 嵌入 → bytecode.h
echo "Embedding → bytecode.h..."
"$EMBED" blink.cmo -o bytecode.h

echo "=== Done: bytecode.h generated ==="
echo "Now open Blink.ino in Arduino IDE, verify with RP2350 board, and upload."
