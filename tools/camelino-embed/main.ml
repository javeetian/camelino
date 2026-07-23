(*
 * camelino-embed — .cmo bytecode extractor → .camel binary
 *
 * Phase 1.6: proper .cmo Marshal parsing + relocation application.
 * Usage: camelino-embed input.cmo -o output.camel
 *)

let magic_len = String.length Config.cmo_magic_number

(* ---- CRC-32/ISO-HDLC ---- *)
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

(* ---- Output helpers ---- *)
let output_int32_le oc v =
  let open Int32 in
  output_byte oc (to_int (logand v 0xFFl));
  output_byte oc (to_int (logand (shift_right_logical v 8) 0xFFl));
  output_byte oc (to_int (logand (shift_right_logical v 16) 0xFFl));
  output_byte oc (to_int (logand (shift_right_logical v 24) 0xFFl))

let output_uint16_le oc v =
  output_byte oc (v land 0xFF);
  output_byte oc ((v lsr 8) land 0xFF)

(* ---- Patch a 32-bit LE word at byte offset in a Bytes buffer ---- *)
let patch_int32_le buf off v =
  Bytes.set buf off       (Char.chr (v land 0xFF));
  Bytes.set buf (off + 1) (Char.chr ((v lsr 8) land 0xFF));
  Bytes.set buf (off + 2) (Char.chr ((v lsr 16) land 0xFF));
  Bytes.set buf (off + 3) (Char.chr ((v lsr 24) land 0xFF))

(* ---- Extract Ident.t stamp via Obj (Ident.t is {name; stamp; flags; scope}) ---- *)
let ident_stamp id =
  let o = Obj.repr id in
  (Obj.magic (Obj.field o 1) : int)

(* ---- Encode OCaml 32-bit-word bytecode to compact byte stream ----
   Two-pass: first build word→offset map, then encode with branch remapping.
   [use_word_index]: if true, branch operands are word indices (in .cmo).
                     if false, branch operands are byte offsets (in .byte). ---- *)
let encode_compact code_str ~use_word_index =
  let nwords = String.length code_str / 4 in
  let words = Array.init nwords (fun i ->
    let o = i * 4 in
    Char.code code_str.[o] +
    (Char.code code_str.[o+1] lsl 8) +
    (Char.code code_str.[o+2] lsl 16) +
    (Char.code code_str.[o+3] lsl 24)
  ) in
  let w i = if i < nwords then words.(i) else 0 in

  (* ---- Pass 1: compute compact offset for each word index ---- *)
  let word_map = Array.make nwords 0 in
  let i = ref 0 in
  let pos = ref 0 in
  let advance () = let v = w !i in incr i; v in
  let advance_size op =
    (* (compacted_byte_size, extra_words_to_skip_in_32bit_space) *)
    let sz, skip = match op with
      | 43 -> (6, 2)                                           (* CLOSURE: 1 + 4 remapped + 1 nvars; 3 words *)
      | 44 -> (0, 0)                                           (* CLOSUREREC: compact size and skip computed dynamically below *)
      | 36 -> (3, 2)                                           (* APPTERM: 1 + 1 nargs + 1 slotsize; 3 words *)
      | 131|132|133|134|135|136 -> (5, 1)                     (* BEQ-family branch-compare: 1 + 4; 2 words *)
      | 53|54|57|59|61 -> (5, 1)                              (* GETGLOBAL/SETGLOBAL/ATOM/PUSHATOM *)
      | 55|56 -> (6, 2)                                       (* GLOBALFIELD: 1 + 4 + 1; 3 words *)
      | 80|81|82 -> (3, 1)                                    (* GETVECTITEM/SETVECTITEM/GETBYTESCHAR *)
      | 83 -> (3, 1)                                          (* SETBYTESCHAR: 1 + 2 *)
      | 84|85|86 -> (5, 1)                                    (* BRANCH/BRANCHIF/BRANCHIFNOT: 1 + 4 *)
      | 87 -> (9, 2)                                          (* SWITCH: 1 + 4 + 4; 3 words *)
      | 93|94|95|96|97 -> (5, 1)                              (* C_CALL1-5: 1 + 4 *)
      | 98 -> (6, 2)                                          (* C_CALLN: 1 + 4 + 1; 3 words *)
      | 103|108 -> (5, 1)                                     (* CONSTINT/PUSHCONSTINT: 1 + 4 *)
      | 146|147 -> (5, 1)                                     (* GETPUBMET/GETDYNMET: 1 + 4 *)
      | 19 -> (3, 1)                                          (* POP: 1 + 2-byte count *)

      | 52|67|68|69|70|71|72|73|74|75|76|77|78|79 -> (3, 1)  (* field/vector/bytes ops: 1 + 2 *)
      | 62|63|64|65|66 -> (2, 1)                              (* MAKEBLOCK: 1 opcode + 1 tag; size=0 optimized *)
      | 8|18|20|30|32|35|37|40|42|127 -> (2, 1)                  (* 1-byte operand: 2 words; RETURN=40, GRAB=42 *)
      | _ -> (1, 0)                                           (* 0-operand: 1 word *)
    in
    (* Handle CLOSUREREC dynamically: OCaml 5.x format = nfuncs + nvars + nfuncs*offset_bytes *)
    let sz, skip = if op = 44 then
      let nfuncs = w (!i) land 0xFF in  (* read nfuncs from next word *)
      let nvars  = w (!i + 1) land 0xFF in (* read nvars *)
      let total = 3 + nfuncs in  (* opcode + nfuncs_byte + nvars_byte + nfuncs * offset_byte *)
      (total, 2 + nfuncs)                (* skip: nfuncs_word + nvars_word + nfuncs offset words *)
    else (sz, skip) in
    word_map.(!i - 1) <- !pos;
    pos := !pos + sz;
    for _ = 1 to skip do ignore (advance ()) done
  in
  while !i < nwords do
    let op = advance () in
    advance_size op
  done;

  (* ---- Pass 2: emit compact bytecode ---- *)
  let compact = Buffer.create !pos in
  let emit_byte b = Buffer.add_char compact (Char.chr (b land 0xFF)) in
  let emit_int32_le n =
    emit_byte (n land 0xFF); emit_byte ((n lsr 8) land 0xFF);
    emit_byte ((n lsr 16) land 0xFF); emit_byte ((n lsr 24) land 0xFF)
  in

  i := 0;
  let next_op () = let v = w !i in incr i; v in
  let nxt8 () = emit_byte (next_op ()) in
  let nxt16 () = let v = next_op () in emit_byte (v land 0xFF); emit_byte ((v lsr 8) land 0xFF) in
  let nxt32 () = emit_int32_le (next_op ()) in
  let nxt32_branch () =
    let old_off = next_op () in
    let target_widx = if use_word_index then old_off else old_off / 4 in
    (* Find the first mapped compact offset at or after target_widx.
       Some branch targets land on operand words which have no map entry. *)
    let rec find_map idx =
      if idx >= nwords then old_off
      else if word_map.(idx) <> 0 || idx = 0 then word_map.(idx)
      else find_map (idx + 1)
    in
    let remapped = find_map target_widx in
    emit_int32_le remapped
  in

  while !i < nwords do
    let op = next_op () in
    let fmt = match op with
      | 0|1|2|3|4|5|6|7 -> `ZERO     (* ACC0-7 *)
      | 9|29|31|33|36|58|60 -> `ZERO  (* PUSH, PUSH_RETADDR, APPLY1, APPLY3, RESTART, ATOM0, PUSHATOM0 *)
      | 19 -> `SHORT2  (* POP: 2-byte count *)
      | 10|11|12|13|14|15|16|17 -> `ZERO (* PUSHACC0-7 *)
      | 21|22|23|24|25|26|27|28 -> `ZERO (* ENVACC1-4, PUSHENVACC1-4 *)
      | 88 -> `ZERO    (* BOOLNOT: 0-operand *)
      | 89 -> `INT32    (* PUSHTRAP: 4-byte handler offset *)
      | 90|91|92 -> `ZERO   (* POPTRAP, RAISE, CHECK_SIGNALS *)
      | 99|100|101|102|104|105|106|107 -> `ZERO (* CONST0-3, PUSHCONST0-3 *)
      | 109|110|111|112|113|114|115|116|117|118|119|120 -> `ZERO
      | 121|122|123|124|125|126|128|129 -> `ZERO
      | 143|144|145 -> `ZERO
      | 8|18|20|30|32|35|37|40|42|127 -> `BYTE1  (* 1-byte operand *)
      | 36 -> `APPTERM                          (* APPTERM: 1+1 nargs+slotsize *)
      | 34 -> `BYTE1                            (* APPTERM1-3 in enum: actually 37-39 but op 34=APPTERM already handled as APPTERM above *)
      | 19 -> `SHORT2  (* POP: 2-byte count *)
      | 52|67|68|69|70|71|72|73|74|75|76|77|78|79 -> `SHORT2
      | 53|54|57|59|61|103|108|146|147 -> `INT32
      | 84|85|86 -> `BRANCH   (* BRANCH/BRANCHIF/BRANCHIFNOT: remapped 4-byte *)
      | 87 -> `SWITCH        (* SWITCH: 4+4 *)
      | 93|94|95|96|97|98 -> `C_CALL   (* C_CALL1-5,N: 4-byte index *)
      | 43 -> `CLOSURE     (* 4-byte remapped + 1-byte nvars *)
      | 44 -> `CLOSUREREC  (* 4-byte remapped + 1-byte nfuncs + 1-byte nvars *)
      | 55|56 -> `GLOBALFIELD                    (* 4-byte + 1-byte *)
      | 131|132|133|134|135|136 -> `BRANCHCOMP  (* BEQ family: remapped *)
      | 62|63|64|65|66 -> `MAKEBLOCK
      | _ -> `UNKNOWN
    in
    (match fmt with
     | `ZERO          -> emit_byte op
     | `BYTE1         -> emit_byte op; nxt8 ()
     | `SHORT2        -> emit_byte op; nxt16 ()
     | `INT32         -> emit_byte op; nxt32 ()
     | `BRANCH        -> emit_byte op; nxt32_branch ()
     | `BRANCHCOMP    -> emit_byte op; nxt32_branch ()
     | `SWITCH        -> emit_byte op; nxt32 (); nxt32_branch ()
     | `C_CALL        -> emit_byte op; nxt32 ()
     | `CLOSURE       -> emit_byte op; nxt32_branch (); nxt8 ()
     | `CLOSUREREC    ->
        let nfuncs = next_op () land 0xFF in
        let nvars = next_op () land 0xFF in
        emit_byte op; emit_byte nfuncs; emit_byte nvars;
        (* Remap relative offsets from word space to compact byte space *)
        let offset_base = !i in
        let compact_pc = Buffer.length compact in
        for k = 0 to nfuncs - 1 do
          let off = next_op () land 0xFF in
          let off_signed = if off < 128 then off else off - 256 in
          let target_widx = offset_base + k + off_signed in (* word index of target *)
          let new_off =
            if target_widx >= 0 && target_widx < nwords
            then (word_map.(target_widx) - compact_pc)
            else off_signed
          in
          emit_byte (new_off land 0xFF)
        done
     | `APPTERM       -> emit_byte op; nxt8 (); nxt8 ()
     | `GLOBALFIELD   -> emit_byte op; nxt32 (); nxt8 ()
     | `MAKEBLOCK     -> emit_byte op; nxt8 (); emit_byte 0; emit_byte 0  (* tag + sz=0: OCaml 5.x omits zero-size operand *)
     | `UNKNOWN       -> emit_byte op
    );
  done;
  emit_byte 143;  (* STOP *)
  Buffer.contents compact

(* ---- Main ---- *)
let () =
  if Array.length Sys.argv < 2 then begin
    Printf.eprintf "Usage: %s input.{cmo,byte} [-o output.camel]\n" Sys.argv.(0);
    Printf.eprintf "  .cmo:  single compilation unit (relocations auto-applied)\n";
    Printf.eprintf "  .byte: linked bytecode executable (ocamlc -o prog.byte input.ml)\n";
    exit 1
  end;
  let input_file = Sys.argv.(1) in
  let output_file =
    if Array.length Sys.argv >= 4 && Sys.argv.(2) = "-o" then Sys.argv.(3)
    else (Filename.chop_extension input_file) ^ ".camel"
  in

  (* ---- Step 1: Read file ---- *)
  let ic = open_in_bin input_file in
  let file_len = in_channel_length ic in
  let raw = really_input_string ic file_len in
  close_in ic;

  (* ---- Detect format ---- *)
  let is_exec = String.length raw > 2 && String.sub raw 0 2 = "#!" in

  let raw_code, raw_data, prims, use_word_index =
    if is_exec then begin
      (* Bytecode executable: sections at end with big-endian directory *)
      let shebang_end = String.index raw '\n' + 1 in
      let rest = String.sub raw shebang_end (String.length raw - shebang_end) in
      let len = String.length rest in

      (* Section directory at end: [name4][size4 BE]... [num_sections4 BE][exec_magic12] *)
      let read_be32 s off =
        (Char.code s.[off] lsl 24) + (Char.code s.[off+1] lsl 16)
        + (Char.code s.[off+2] lsl 8) + Char.code s.[off+3]
      in
      let num_sec = read_be32 rest (len - 16) in
      let sections = ref [] in
      let pos = ref (len - 16) in
      for _ = 1 to num_sec do
        pos := !pos - 8;
        let name = String.sub rest !pos 4 in
        let sz = read_be32 rest (!pos + 4) in
        sections := (name, sz) :: !sections
      done;

      (* Find CODE and DATA sections *)
      let code = ref "" in
      let data = ref "" in
      let sec_start = ref 0 in
      List.iter (fun (name, sz) ->
        let name_str = String.trim name in
        if name_str = "CODE" then code := String.sub rest !sec_start sz
        else if name_str = "DATA" then data := String.sub rest !sec_start sz;
        sec_start := !sec_start + sz
      ) !sections;

      (* The CODE section = [global_table (32-bit entries)] + [ZAM bytecode].
         Each global has a 4-byte entry. The number of globals = data_size / 4
         (since DATA section stores one 32-bit value per global).
         Skip the global table prefix. *)
      let data_sz = String.length !data in
      let num_globals = data_sz / 4 in
      let table_sz = num_globals * 4 in
      let code_str = String.sub !code table_sz (String.length !code - table_sz) in

      Printf.eprintf "  bytecode exec: code=%d data=%d (skipped %d-byte global table for %d globals)\n%!"
        (String.length code_str) data_sz table_sz num_globals;
      (code_str, !data, [], false)
    end else begin
      (* .cmo format: parse with Marshal *)
      if String.length raw < magic_len + 4 then
        failwith "file too small for .cmo magic + cu_pos";

      let magic = String.sub raw 0 magic_len in
      if magic <> Config.cmo_magic_number then
        failwith (Printf.sprintf "bad magic: expected %s (got %.12s)"
                    Config.cmo_magic_number (String.sub raw 0 magic_len));

      let read_be32 s off =
        (Char.code s.[off] lsl 24) + (Char.code s.[off+1] lsl 16)
        + (Char.code s.[off+2] lsl 8) + Char.code s.[off+3]
      in
      let cu_pos = read_be32 raw magic_len in
      let code_start = magic_len + 4 in
      if cu_pos <= code_start || cu_pos > String.length raw then
        failwith (Printf.sprintf "invalid cu_pos=%d (file_len=%d)" cu_pos (String.length raw));

      let raw_code = String.sub raw code_start (cu_pos - code_start) in

      (* Parse compilation unit *)
      let ic = open_in_bin input_file in
      seek_in ic cu_pos;
      let cu = (input_value ic : Cmo_format.compilation_unit) in
      close_in ic;

      Printf.eprintf "  cu_name=%s  cu_codesize=%d  cu_reloc=%d  cu_primitives=%d\n%!"
        cu.Cmo_format.cu_name cu.Cmo_format.cu_codesize
        (List.length cu.Cmo_format.cu_reloc) (List.length cu.Cmo_format.cu_primitives);

      (* Apply relocations *)
      let bytecode = Bytes.of_string raw_code in
      let module StampMap = Map.Make(struct type t = int let compare = compare end) in
      let stamp_map = ref StampMap.empty in
      let next_slot = ref 0 in
      let get_slot (id : Ident.t) =
        let stamp = ident_stamp id in
        match StampMap.find_opt stamp !stamp_map with
        | Some slot -> slot
        | None -> let slot = !next_slot in next_slot := slot + 1; stamp_map := StampMap.add stamp slot !stamp_map; slot
      in
      List.iter (fun (reloc, pos) ->
        let r = Obj.repr reloc in
        let tag = if Obj.is_int r then Obj.magic r else Obj.tag r in
        match tag with
        | 0 -> (* Reloc_literal of int *)
            let sc = (Obj.magic (Obj.field r 0) : int) in
            let v = (sc lsl 1) + 1 in
            patch_int32_le bytecode pos v
        | 1 -> (* Reloc_getglobal *)
            let id = (Obj.magic (Obj.field r 0) : Ident.t) in
            let slot = get_slot id in
            Printf.eprintf "  Reloc_getglobal %s → slot %d\n%!" (Ident.name id) slot;
            patch_int32_le bytecode pos slot
        | 2 -> (* Reloc_setglobal *)
            let id = (Obj.magic (Obj.field r 0) : Ident.t) in
            let slot = get_slot id in
            Printf.eprintf "  Reloc_setglobal %s → slot %d\n%!" (Ident.name id) slot;
            patch_int32_le bytecode pos slot
        | 3 -> (* Reloc_primitive of string *)
            Printf.eprintf "  Reloc_primitive %S (skip)\n%!" ((Obj.magic (Obj.field r 0) : string))
        | _ -> Printf.eprintf "  Reloc tag=%d (skip)\n%!" tag
      ) cu.Cmo_format.cu_reloc;
      (Bytes.to_string bytecode, "", cu.Cmo_format.cu_primitives, true)
    end
  in

  (* ---- Encode to compact format ---- *)
  let compact = encode_compact raw_code ~use_word_index in
  let code_size = String.length compact in
  let data_size = String.length raw_data in

  (* ---- Write .camel ---- *)
  let oc = open_out_bin output_file in

  output_string oc "CAMELINO01";
  output_uint16_le oc 1;
  output_byte oc (Sys.word_size / 8);
  output_byte oc 0;
  output_byte oc 0; output_byte oc 0;
  output_int32_le oc 0l;
  output_int32_le oc (Int32.of_int code_size);
  output_int32_le oc (Int32.of_int data_size);
  output_int32_le oc 0l;         (* entry_offset = 0 *)

  output_string oc compact;
  if data_size > 0 then output_string oc raw_data;

  let crc = crc32 (compact ^ raw_data) 0 (code_size + data_size) in
  output_int32_le oc crc;

  close_out oc;

  Printf.printf "camelino-embed: %s → %s (%d code + %d data = %d bytes)\n%!"
    input_file output_file code_size data_size (32 + code_size + data_size + 4)
