(*
 * camelino-embed — .cmo bytecode extractor → .camel binary
 *
 * Phase 1 MVP: parse .cmo, extract code[], write .camel header + CRC32.
 * Usage: camelino-embed input.cmo -o output.camel
 *)

let magic_len = String.length Config.cmo_magic_number

(* CRC-32/ISO-HDLC *)
let crc32_table =
  Array.init 256 (fun i ->
    let rec loop j crc =
      if j = 0 then crc
      else loop (j - 1)
        (if (Int32.logand crc 1l) <> 0l
         then Int32.logxor 0xEDB88320l (Int32.shift_right_logical crc 1)
         else Int32.shift_right_logical crc 1)
    in
    loop 8 (Int32.of_int i))

let crc32 buf ofs len =
  let crc = ref 0xFFFFFFFFl in
  for i = ofs to ofs + len - 1 do
    let byte_val = Int32.of_int (Char.code buf.[i]) in
    let idx = Int32.to_int (Int32.logxor !crc byte_val) land 0xFF in
    crc := Int32.logxor crc32_table.(idx)
                        (Int32.shift_right_logical !crc 8)
  done;
  Int32.logxor !crc 0xFFFFFFFFl

(* write uint32 LE *)
let output_int32_le oc v =
  output_byte oc (Int32.to_int (Int32.logand v 0xFFl));
  output_byte oc (Int32.to_int (Int32.logand (Int32.shift_right_logical v 8) 0xFFl));
  output_byte oc (Int32.to_int (Int32.logand (Int32.shift_right_logical v 16) 0xFFl));
  output_byte oc (Int32.to_int (Int32.logand (Int32.shift_right_logical v 24) 0xFFl))

(* write uint16 LE *)
let output_uint16_le oc v =
  output_byte oc (v land 0xFF);
  output_byte oc ((v lsr 8) land 0xFF)

let () =
  if Array.length Sys.argv < 2 then begin
    Printf.eprintf "Usage: %s input.cmo [-o output.camel]\n" Sys.argv.(0);
    exit 1
  end;
  let input_file = Sys.argv.(1) in
  let output_file =
    if Array.length Sys.argv >= 4 && Sys.argv.(2) = "-o" then Sys.argv.(3)
    else (Filename.chop_extension input_file) ^ ".camel"
  in

  (* read entire .cmo file *)
  let ic = open_in_bin input_file in
  let file_len = in_channel_length ic in
  let raw = really_input_string ic file_len in
  close_in ic;

  (* parse *)
  if String.length raw < magic_len + 4 then
    failwith "file too small for .cmo magic + cu_pos";

  let magic = String.sub raw 0 magic_len in
  if magic <> Config.cmo_magic_number then
    failwith (Printf.sprintf "bad magic: expected %s" Config.cmo_magic_number);

  (* cu_pos: 4 bytes BIG-ENDIAN at offset magic_len (OCaml input_binary_int) *)
  let cu_pos =
    let b0 = Char.code raw.[magic_len] in
    let b1 = Char.code raw.[magic_len + 1] in
    let b2 = Char.code raw.[magic_len + 2] in
    let b3 = Char.code raw.[magic_len + 3] in
    (b0 lsl 24) + (b1 lsl 16) + (b2 lsl 8) + b3
  in

  let code_start = magic_len + 4 in
  (* bytecode is at offset code_start, extends to cu_pos *)
  let code_size = cu_pos - code_start in
  if code_size <= 0 || code_start + code_size > String.length raw then
    failwith (Printf.sprintf "invalid code region: start=%d size=%d file_len=%d"
                code_start code_size (String.length raw));

  let bytecode_raw = String.sub raw code_start code_size in

  (* Normalize: OCaml bytecode is 32-bit words → compact byte encoding.
     Each word's low byte = opcode. Multi-word instructions have operands
     in subsequent words' low bytes (or full words for 32-bit operands). *)
  let words = Array.init (code_size / 4) (fun i ->
    let o = i * 4 in
    let b0 = Char.code bytecode_raw.[o] in
    let b1 = Char.code bytecode_raw.[o+1] in
    let b2 = Char.code bytecode_raw.[o+2] in
    let b3 = Char.code bytecode_raw.[o+3] in
    b0 + (b1 lsl 8) + (b2 lsl 16) + (b3 lsl 24)
  ) in
  let w i = if i < Array.length words then words.(i) else 0 in

  let compact = Buffer.create code_size in
  let emit_byte b = Buffer.add_char compact (Char.chr (b land 0xFF)) in
  let emit_int32_le n =
    emit_byte (n land 0xFF);
    emit_byte ((n lsr 8) land 0xFF);
    emit_byte ((n lsr 16) land 0xFF);
    emit_byte ((n lsr 24) land 0xFF)
  in

  let i = ref 0 in
  let next_op () = let v = w !i in incr i; v in
  let is_op n = (w !i land 0xFF) = n in
  (* is current word one of the given opcodes? *)
  let any ops = List.exists (fun n -> w !i land 0xFF = n) ops in

  while !i < Array.length words do
    let op = next_op () in
    (* 0-operand: ACC0-7, PUSHACC0-7, PUSH_RETADDR, APPLY1-3, APPTERM, RETURN,
       RESTART, GRAB, CONST0-3, NEGINT..GEINT, STOP, PUSH, POP (handled as 1-op below) *)
    if op = 9 || op = 19 || op = 31 || op = 84 || op = 88 || op = 143
       || (op >= 0 && op <= 7) || (op >= 10 && op <= 17)
       || (op >= 33 && op <= 42) || (op >= 99 && op <= 102)
       || (op >= 109 && op <= 126) then
      emit_byte op
    (* 1-byte operand from next word: OFFSETINT, ACC, PUSHACC, ASSIGN, APPLY, etc *)
    else if op = 127 || op = 8 || op = 18 || op = 20 || op = 25 || op = 30 || op = 32
            || op = 43 || op = 44 || op = 48 || op = 52
            || (op >= 67 && op <= 78) then begin
      emit_byte op; emit_byte (next_op ())
    end
    (* ATOM0 / PUSHATOM0: 0 operand *)
    else if op = 58 || op = 60 then
      emit_byte op
    (* ATOM / PUSHATOM / GETGLOBAL / SETGLOBAL / CONSTINT / BRANCH family / C_CALL: 4-byte operand *)
    else if op = 59 || op = 61 || (op >= 53 && op <= 57) || op = 103 || op = 108
            || (op >= 84 && op <= 86) || (op >= 89 && op <= 91) || (op >= 93 && op <= 98)
            || (op >= 146 && op <= 147) then begin
      emit_byte op; emit_int32_le (next_op ())
    end
    (* MAKEBLOCK: 1-byte tag + 2-byte size *)
    else if op >= 62 && op <= 66 then begin
      emit_byte op; emit_byte (next_op ());  (* tag *)
      let sz = next_op () in
      emit_byte (sz land 0xFF); emit_byte ((sz lsr 8) land 0xFF)  (* size LE *)
    end
    else
      emit_byte op  (* unknown: emit opcode only *)
  done;
  (* OCaml bytecode may not end with STOP — append one *)
  emit_byte 143;  (* STOP *)

  let bytecode = Buffer.contents compact in
  let code_size = String.length bytecode in
  let entry = 0 in  (* Phase 1: entry is offset 0 *)

  (* write .camel *)
  let oc = open_out_bin output_file in

  (* header: magic + format_ver + word_size + big_endian + reserved + stdlib_crc *)
  output_string oc "CAMELINO01";           (* magic 10 bytes *)
  output_uint16_le oc 1;                   (* format_ver = 1 *)
  output_byte oc (Sys.word_size / 8);      (* word_size = sizeof(value) *)
  output_byte oc 0;                        (* big_endian = 0 = LE *)
  output_byte oc 0; output_byte oc 0;      (* reserved[2] *)
  output_int32_le oc 0l;                   (* stdlib_crc = 0 *)
  output_int32_le oc (Int32.of_int code_size);  (* code_size *)
  output_int32_le oc 0l;                        (* data_size = 0 *)
  output_int32_le oc (Int32.of_int entry);      (* entry_offset *)

  (* code[] *)
  output_string oc bytecode;

  (* CRC32 of code[] *)
  let crc = crc32 bytecode 0 code_size in
  output_int32_le oc crc;

  close_out oc;

  Printf.printf "camelino-embed: %s (%d bytecode) → %s (%d bytes total)\n"
    input_file code_size output_file
    (10 + 2 + 1 + 1 + 2 + 4 + 4 + 4 + 4 + code_size + 4)
