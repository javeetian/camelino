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
    let nxt8 () = emit_byte (next_op ()) in
    let nxt16 () = let v = next_op () in emit_byte (v land 0xFF); emit_byte ((v lsr 8) land 0xFF) in
    let nxt32 () = emit_int32_le (next_op ()) in

    (* Dispatch table: operand format for each opcode.
       OCaml 4.14 ZAM instructions have at most one operand size category,
       except CLOSURE/CLOSUREREC/BEQ-family which have compound operands. *)
    let fmt = match op with
      (* ---- 0-operand ---- *)
      | 0|1|2|3|4|5|6|7              (* ACC0-7 *)
      | 9                             (* PUSH *)
      | 10|11|12|13|14|15|16|17      (* PUSHACC0-7 *)
      | 19                            (* POP *)
      | 21|22|23|24                   (* ENVACC1-4 *)
      | 25|26|27|28                   (* PUSHENVACC1-4 *)
      | 29                            (* PUSH_RETADDR *)
      | 31                            (* APPLY1 *)
      | 33                            (* APPLY3 *)
      | 36                            (* RESTART *)
      | 58                            (* ATOM0 *)
      | 60                            (* PUSHATOM0 *)
      | 83|84|85                      (* BOOLNOT BOOLAND BOOLOR *)
      | 86                            (* PUSHTRAP *)
      | 87                            (* POPTRAP *)
      | 88                            (* RAISE *)
      | 89                            (* CHECK_SIGNALS *)
      | 99|100|101|102                (* CONST0-3 *)
      | 104|105|106|107               (* PUSHCONST0-3 *)
      | 109|110|111|112|113|114       (* NEGINT ADDINT SUBINT MULINT DIVINT MODINT *)
      | 115|116|117|118|119|120       (* ANDINT ORINT XORINT LSLINT LSRINT ASRINT *)
      | 121|122|123|124|125|126       (* EQ NEQ LTINT LEINT GTINT GEINT *)
      | 128                           (* OFFSETREF *)
      | 129                           (* ISINT *)
      | 143                           (* STOP *)
      | 144|145 ->                    (* EVENT BREAK *)
        `ZERO
      (* ---- 1-byte operand ---- *)
      | 8                             (* ACC n *)
      | 18                            (* PUSHACC n *)
      | 20                            (* ASSIGN n *)
      | 30                            (* APPLY nargs *)
      | 32                            (* APPLY2 *)
      | 34                            (* APPTERM nargs, slotsize *)
      | 35                            (* RETURN n *)
      | 37                            (* GRAB n *)
      | 48                            (* PUSHATOM — wait, PUSHATOM is 59/61 *)
      | 127 ->                        (* OFFSETINT *)
        `BYTE1
      (* ---- 2-byte operand (little-endian) ---- *)
      | 52|67|68|69|70|71|72|73|74|75|76|77|78|79 ->
        `SHORT2
      (* ---- 4-byte operand ---- *)
      | 53                            (* PUSHGETGLOBAL slot *)
      | 54                            (* GETGLOBAL slot *)
      | 57                            (* SETGLOBAL slot *)
      | 59                            (* ATOM tag *)
      | 61                            (* PUSHATOM tag *)
      | 80|81|82                      (* BRANCH BRANCHIF BRANCHIFNOT *)
      | 90|91|92|93|94|95             (* C_CALL1-5,N *)
      | 103                           (* CONSTINT *)
      | 108                           (* PUSHCONSTINT *)
      | 146|147 ->                    (* GETPUBMET GETDYNMET *)
        `INT32
      (* ---- CLOSURE/CLOSUREREC: 1-byte + 4-byte ---- *)
      | 38|39 ->                      (* CLOSURE CLOSUREREC *)
        `CLOSURE
      (* ---- GETGLOBALFIELD/PUSHGETGLOBALFIELD: 4-byte + 1-byte ---- *)
      | 55|56 ->                      (* PUSHGETGLOBALFIELD GETGLOBALFIELD *)
        `GLOBALFIELD
      (* ---- BEQ-family branch-compare: branch offset only (opcode encodes comparison kind and size) ---- *)
      | 40|41|42|43|44|45|46|47 ->
        `BRANCHCOMP
      (* ---- MAKEBLOCK/MAKEBLOCK1-3: 1-byte tag + 2-byte size ---- *)
      | 62|63|64|65|66 ->             (* MAKEBLOCK MAKEBLOCK1-3 MAKEFLOATBLOCK *)
        `MAKEBLOCK
      (* ---- APPTERM: 1-byte nargs + 1-byte slotsize ---- *)
      (* already handled as BYTE1 above *)
      (* ---- default: emit opcode only, skip ---- *)
      | _ ->
        `UNKNOWN
    in
    (match fmt with
     | `ZERO -> emit_byte op
     | `BYTE1 -> emit_byte op; nxt8 ()
     | `SHORT2 -> emit_byte op; nxt16 ()
     | `INT32 -> emit_byte op; nxt32 ()
     | `CLOSURE -> emit_byte op; nxt8 (); nxt32 ()
     | `GLOBALFIELD -> emit_byte op; nxt32 (); nxt8 ()
     | `BRANCHCOMP -> emit_byte op; nxt32 ()
     | `MAKEBLOCK -> emit_byte op; nxt8 (); nxt16 ()
     | `UNKNOWN -> emit_byte op
    );
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
