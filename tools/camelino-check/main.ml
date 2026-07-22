(*
 * camelino-check — Static analysis of OCaml bytecode for Camelino compatibility
 *
 * Analyzes .cmo/.cma files to check if they will run on a target platform.
 * Reports: required primitives, heap estimates, word-size risks, platform gaps.
 *
 * Usage: camelino-check [options] input.cmo
 *)

open Printf

(* ---- Configuration ------------------------------------------------------ *)

type config = {
  mutable heap_limit    : int;       (* target heap in KB *)
  mutable flash_limit   : int;       (* target flash in KB *)
  mutable target_word   : int;       (* 32 or 64 *)
  mutable target_platform : string;  (* arduino, host, picosdk, ... *)
  mutable big_endian    : bool;
  mutable input_file    : string;
}

(* ---- Known primitive database ------------------------------------------- *)

type prim_status = Implemented | Stub | Missing

type prim_info = {
  p_name   : string;
  p_phase  : string;
  p_status : prim_status;
  p_hal_caps : string list;  (* HAL capabilities required *)
}

let known_primitives : prim_info list = [
  (* Phase 1 — Core *)
  {p_name="caml_equal";                p_phase="1"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_notequal";             p_phase="1"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_compare";              p_phase="1"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_int_compare";          p_phase="1"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_greaterequal";         p_phase="1"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_lessequal";            p_phase="1"; p_status=Missing;    p_hal_caps=[]};
  (* Phase 2 — Exceptions *)
  {p_name="caml_failwith";             p_phase="2"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_invalid_argument";     p_phase="2"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_array_bound_error";    p_phase="2"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_raise_with_arg";       p_phase="2"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_raise_with_string";    p_phase="2"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_install_signal_handler"; p_phase="2"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_sys_exit";             p_phase="2"; p_status=Implemented; p_hal_caps=[]};
  (* Phase 3 — String / Bytes *)
  {p_name="caml_string_length";        p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_string_get";           p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_create_string";        p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_blit_string";          p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_string_equal";         p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_string_notequal";      p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_bytes_length";         p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_bytes_get";            p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_create_bytes";         p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_blit_bytes";           p_phase="3"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_make_vect";            p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_array_get";            p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_array_set";            p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_obj_tag";              p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_obj_size";             p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_obj_field";            p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_obj_set_field";        p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_obj_block";            p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_register_global_root"; p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  {p_name="caml_remove_global_root";   p_phase="3"; p_status=Missing;    p_hal_caps=[]};
  (* Phase 4 — Format / Conversion *)
  {p_name="caml_format_int";           p_phase="4"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_format_float";         p_phase="4"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_int_of_string";        p_phase="4"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_float_of_string";      p_phase="4"; p_status=Stub;       p_hal_caps=[]};
  (* Phase 4 — HAL / GPIO / UART / Time *)
  {p_name="caml_camellino_digital_write"; p_phase="4"; p_status=Implemented; p_hal_caps=["GPIO"]};
  {p_name="caml_camellino_digital_read";  p_phase="4"; p_status=Implemented; p_hal_caps=["GPIO"]};
  {p_name="caml_camellino_pin_mode";      p_phase="4"; p_status=Implemented; p_hal_caps=["GPIO"]};
  {p_name="caml_camellino_delay_ms";      p_phase="4"; p_status=Implemented; p_hal_caps=["TIME_MS"]};
  {p_name="caml_camellino_millis";        p_phase="4"; p_status=Implemented; p_hal_caps=["TIME_MS"]};
  {p_name="caml_camellino_serial_write";  p_phase="4"; p_status=Implemented; p_hal_caps=["UART"]};
  {p_name="caml_camellino_serial_read";   p_phase="4"; p_status=Stub;       p_hal_caps=["UART"]};
  {p_name="caml_camellino_analog_read";   p_phase="4"; p_status=Stub;       p_hal_caps=["ADC"]};
  {p_name="caml_camellino_analog_write";  p_phase="4"; p_status=Stub;       p_hal_caps=["PWM"]};
  (* Phase 5 — Channel I/O *)
  {p_name="caml_ml_output_char";         p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_ml_input_char";          p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_ml_output";              p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_ml_flush";               p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_ml_input";               p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_ml_open_descriptor_out"; p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_ml_open_descriptor_in";  p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  (* Phase 5 — Random / Hash / Lazy *)
  {p_name="caml_random_init";            p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_random_int";             p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_hash_variant";           p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_hash_univ";              p_phase="5"; p_status=Implemented; p_hal_caps=[]};
  {p_name="caml_update_dummy";           p_phase="5"; p_status=Stub;       p_hal_caps=[]};
  {p_name="caml_lazy_make_forward";      p_phase="5"; p_status=Stub;       p_hal_caps=[]};
]

let prim_table = Hashtbl.create 100
let () = List.iter (fun p -> Hashtbl.add prim_table p.p_name p) known_primitives

(* ---- Platform capability database --------------------------------------- *)

type platform_caps = {
  pc_name  : string;
  pc_caps  : string list;  (* HAL capabilities supported *)
}

let platforms = [
  {pc_name="host";     pc_caps=["GPIO"; "UART"; "TIME_MS"; "TIME_US"; "ADC"; "PWM"; "FLASH"]};
  {pc_name="arduino";  pc_caps=["GPIO"; "UART"; "TIME_MS"; "TIME_US"; "ADC"; "PWM"]};
  {pc_name="picosdk";  pc_caps=["GPIO"; "UART"; "TIME_MS"; "TIME_US"; "ADC"; "PWM"; "FLASH"]};
  {pc_name="espidf";   pc_caps=["GPIO"; "UART"; "TIME_MS"; "TIME_US"; "ADC"; "PWM"; "FLASH"]};
  {pc_name="zephyr";   pc_caps=["GPIO"; "UART"; "TIME_MS"; "TIME_US"; "ADC"; "PWM"; "FLASH"]};
  {pc_name="baremetal"; pc_caps=["GPIO"]};
]

(* ---- ZAM Opcode Constants (OCaml 4.14) ---------------------------------- *)

(* These match the enum in src/core/opcodes.h, 0-based *)
let op_ACC0=0 and op_ACC1=1 and op_ACC2=2 and op_ACC3=3
let op_ACC4=4 and op_ACC5=5 and op_ACC6=6 and op_ACC7=7
let op_ACC=8 and op_PUSH=9
let op_PUSHACC0=10 and op_PUSHACC1=11 and op_PUSHACC2=12 and op_PUSHACC3=13
let op_PUSHACC4=14 and op_PUSHACC5=15 and op_PUSHACC6=16 and op_PUSHACC7=17
let op_PUSHACC=18 and op_POP=19 and op_ASSIGN=20
let op_ENVACC0=21 and op_ENVACC1=22 and op_ENVACC2=23 and op_ENVACC3=24
let op_ENVACC4=25 and op_ENVACC=26 and op_PUSHENVACC0=27
let op_PUSHENVACC1=28 and op_PUSHENVACC2=29 and op_PUSHENVACC3=30
let op_PUSHENVACC4=31 and op_PUSHENVACC=32
let op_OFFSETCLOSUREM2=33 and op_OFFSETCLOSURE0=34 and op_OFFSETCLOSURE2=35
let op_OFFSETCLOSURE=36 and op_PUSHOFFSETCLOSUREM2=37
let op_PUSHOFFSETCLOSURE0=38 and op_PUSHOFFSETCLOSURE2=39
let op_PUSHOFFSETCLOSURE=40 and op_OFFSETCLOSUREM3=41
let op_OFFSETCLOSURE3=42 and op_PUSHOFFSETCLOSUREM3=43
let op_PUSHOFFSETCLOSURE3=44
let op_PUSHRETADDR=45 and op_APPLY=46 and op_APPLY1=47
let op_APPLY2=48 and op_APPLY3=49 and op_APPTERM=50
let op_APPTERM1=51 and op_APPTERM2=52 and op_APPTERM3=53
let op_RETURN=54 and op_RESTART=55 and op_GRAB=56
let op_CLOSURE=57 and op_CLOSUREREC=58
let op_PUSHCONST0=59 and op_PUSHCONST1=60 and op_PUSHCONST2=61
let op_PUSHCONST3=62 and op_PUSHCONSTINT=63
let op_CONST0=64 and op_CONST1=65 and op_CONST2=66
let op_CONST3=67 and op_CONSTINT=68
let op_NEGINT=69 and op_ADDINT=70 and op_SUBINT=71
let op_MULINT=72 and op_DIVINT=73 and op_MODINT=74
let op_ANDINT=75 and op_ORINT=76 and op_XORINT=77
let op_LSLINT=78 and op_LSRINT=79 and op_ASRINT=80
let op_EQ=81 and op_NEQ=82 and op_LTINT=83
let op_LEINT=84 and op_GTINT=85 and op_GEINT=86
let op_OFFSETINT=87 and op_OFFSETREF=88 and op_ISINT=89
let op_GETMETHOD=90 and op_GETDYNMET=91
let op_BEQ=92 and op_BNEQ=93 and op_BLTINT=94
let op_BLEINT=95 and op_BGTINT=96 and op_BGEINT=97
let op_ULTINT=98 and op_UGEINT=99 and op_BULTINT=100
let op_BUGEINT=101
let op_GETFIELD0=102 and op_GETFIELD1=103 and op_GETFIELD2=104
let op_GETFIELD3=105 and op_GETFIELD=106
let op_SETFIELD0=107 and op_SETFIELD1=108 and op_SETFIELD2=109
let op_SETFIELD3=110 and op_SETFIELD=111
let op_GETFLOATFIELD=112 and op_SETFLOATFIELD=113
let op_VECTLENGTH=114 and op_GETVECTITEM=115 and op_SETVECTITEM=116
let op_GETBYTESCHAR=117 and op_SETBYTESCHAR=118
let op_GETSTRINGCHAR=119 and op_SETSTRINGCHAR=120
let op_BRANCH=121 and op_BRANCHIF=122 and op_BRANCHIFNOT=123
let op_SWITCH=124
let op_BOOLNOT=125 and op_BOOLAND=126 and op_BOOLOR=127
let op_PUSHTRAP=128 and op_POPTRAP=129 and op_RAISE=130
let op_RAISE_NOTRACE=131 and op_RERAISE=132
let op_CHECK_SIGNALS=133
let op_C_CALL1=134 and op_C_CALL2=135 and op_C_CALL3=136
let op_C_CALL4=137 and op_C_CALL5=138 and op_C_CALLN=139
let op_MAKEBLOCK=140 and op_MAKEBLOCK1=141 and op_MAKEBLOCK2=142
let op_MAKEBLOCK3=143
let op_MAKEFLOATBLOCK=144
let op_GETGLOBAL=145 and op_SETGLOBAL=146
let op_GETGLOBALFIELD=147 and op_SETGLOBALFIELD=148
let op_PUSHGETGLOBAL=149 and op_PUSHGETGLOBALFIELD=150
let op_ATOM0=151 and op_ATOM=152 and op_PUSHATOM0=153 and op_PUSHATOM=154
let op_MAKEBLOCK4=155
let op_STOP=156 and op_EVENT=157 and op_BREAK=158
let op_PUBLISHGETGLOBAL=159
let op_GETPUBMET=160 and op_SETPUBMET=161
let op_PUSH_GETGLOBAL=162 and op_PUSH_GETGLOBALFIELD=163

(* Total: 164 opcodes 0..163 (OCaml 4.14 has exactly 164 instructions) *)
let total_opcodes = 164

(* ---- .cmo Parsing ------------------------------------------------------- *)

let magic_len = String.length Config.cmo_magic_number

(* Read 4 bytes big-endian (OCaml input_binary_int format) *)
let read_be32 s off =
  let b0 = Char.code s.[off] in
  let b1 = Char.code s.[off+1] in
  let b2 = Char.code s.[off+2] in
  let b3 = Char.code s.[off+3] in
  (b0 lsl 24) lor (b1 lsl 16) lor (b2 lsl 8) lor b3

(* Extract primitive names from the compilation unit via proper Marshal parsing.
   Opens the file and seeks to cu_pos, then uses input_value to deserialize. *)
let extract_primitives filename cu_pos =
  if cu_pos <= 0 then [] else
  try
    let ic = open_in_bin filename in
    seek_in ic cu_pos;
    let cu = (input_value ic : Cmo_format.compilation_unit) in
    close_in ic;
    cu.Cmo_format.cu_primitives
  with
  | End_of_file -> []
  | Failure _ -> []
  | _ -> []

(* ---- Bytecode Analysis -------------------------------------------------- *)

type bc_stats = {
  mutable total_instructions : int;
  mutable heap_alloc_words   : int;     (* estimated heap words allocated *)
  mutable int_constants      : int list; (* CONSTINT values seen *)
  mutable prim_refs          : (int * string) list;  (* (index, op_name) pairs *)
  mutable prim_indices       : int list; (* raw C_CALL indices *)
  mutable used_opcodes       : (int, int) Hashtbl.t;  (* opcode → count *)
}

let analyze_bytecode raw code_start code_end =
  let st = {
    total_instructions = 0;
    heap_alloc_words = 0;
    int_constants = [];
    prim_refs = [];
    prim_indices = [];
    used_opcodes = Hashtbl.create 200;
  } in

  (* Read bytecode as 32-bit words *)
  let byte_size = code_end - code_start in
  let nwords = byte_size / 4 in
  if nwords <= 0 then st else

  let w i =
    let off = code_start + i * 4 in
    (Char.code raw.[off])
    lor (Char.code raw.[off+1] lsl 8)
    lor (Char.code raw.[off+2] lsl 16)
    lor (Char.code raw.[off+3] lsl 24)
  in

  let i = ref 0 in
  while !i < nwords do
    let word = w !i in
    let op = word land 0xFF in
    incr i;
    st.total_instructions <- st.total_instructions + 1;
    (try Hashtbl.replace st.used_opcodes op
       (1 + try Hashtbl.find st.used_opcodes op with Not_found -> 0)
     with _ -> ());

    (* 0-operand instructions *)
    if (op >= op_ACC0 && op <= op_ACC7)
      || (op >= op_PUSHACC0 && op <= op_PUSHACC7)
      || op = op_POP || op = op_PUSHRETADDR
      || op = op_APPLY1 || op = op_APPLY2 || op = op_APPLY3
      || op = op_APPTERM1 || op = op_APPTERM2 || op = op_APPTERM3
      || op = op_RETURN || op = op_RESTART || op = op_GRAB
      || op = op_CONST0 || op = op_CONST1 || op = op_CONST2 || op = op_CONST3
      || (op >= op_NEGINT && op <= op_GEINT)
      || op = op_BOOLNOT
      || (op >= op_GETFIELD0 && op <= op_SETFIELD3)
      || op = op_VECTLENGTH || op = op_BREAK || op = op_EVENT
      || op = op_STOP || op = op_CHECK_SIGNALS
      || op = op_POPTRAP || op = op_RAISE || op = op_RAISE_NOTRACE
      || op = op_RERAISE || op = op_GETPUBMET
      || op = op_ATOM0 || op = op_PUSHATOM0
      || op = op_ISINT
    then
      ()  (* 0-operand: nothing to consume *)

    (* 1-byte operand in next word *)
    else if op = op_ACC || op = op_PUSH || op = op_PUSHACC || op = op_ASSIGN
         || op = op_ENVACC || op = op_PUSHENVACC
         || op = op_OFFSETCLOSURE || op = op_PUSHOFFSETCLOSURE
         || op = op_GETFIELD || op = op_SETFIELD
         || op = op_GETVECTITEM || op = op_SETVECTITEM
         || op = op_OFFSETINT || op = op_OFFSETREF
         || op = op_APPLY || op = op_APPTERM
         || op = op_CLOSURE || op = op_CLOSUREREC
         || op = (op_OFFSETCLOSURE3) || op = (op_OFFSETCLOSUREM3)
         || op = (op_OFFSETCLOSURE0) || op = (op_OFFSETCLOSURE2)
         || op = (op_PUSHOFFSETCLOSURE3) || op = (op_PUSHOFFSETCLOSUREM3)
         || op = (op_PUSHOFFSETCLOSURE0) || op = (op_PUSHOFFSETCLOSURE2)
         || op = (op_ENVACC0) || op = (op_ENVACC1) || op = (op_ENVACC2)
         || op = (op_ENVACC3) || op = (op_ENVACC4)
         || op = (op_PUSHENVACC0) || op = (op_PUSHENVACC1) || op = (op_PUSHENVACC2)
         || op = (op_PUSHENVACC3) || op = (op_PUSHENVACC4)
         || (op >= op_BEQ && op <= op_ULTINT)
    then begin
      if !i < nwords then incr i  (* consume operand *)
    end

    (* CONSTINT / C_CALL / BRANCH / GETGLOBAL / SETGLOBAL etc: 4-byte operand *)
    else if op = op_CONSTINT || op = op_PUSHCONSTINT
         || op = op_GETGLOBAL || op = op_SETGLOBAL
         || op = op_ATOM || op = op_PUSHATOM
         || op = op_GETGLOBALFIELD || op = op_SETGLOBALFIELD
         || op = op_PUSHGETGLOBAL || op = op_PUSHGETGLOBALFIELD
         || op = op_PUBLISHGETGLOBAL
         || op = op_PUSH_GETGLOBAL || op = op_PUSH_GETGLOBALFIELD
         || op = op_BRANCH || op = op_BRANCHIF || op = op_BRANCHIFNOT
         || op = op_SWITCH
         || (op >= op_C_CALL1 && op <= op_C_CALLN)
         || (op >= op_BGEINT && op <= op_BUGEINT)
    then begin
      if !i < nwords then begin
        let operand = w !i in
        incr i;
        if op = op_CONSTINT then
          st.int_constants <- operand :: st.int_constants
        else if op >= op_C_CALL1 && op <= op_C_CALLN then
          st.prim_indices <- operand :: st.prim_indices;
      end
    end

    (* MAKEBLOCK: 1-byte tag in next word + 2-byte size (low 16 bits) *)
    else if (op >= op_MAKEBLOCK && op <= op_MAKEBLOCK3) || op = op_MAKEBLOCK4 then begin
      if !i < nwords then begin
        let tag_and_size = w !i in
        incr i;
        let size = tag_and_size land 0xFFFF in
        st.heap_alloc_words <- st.heap_alloc_words + size + 1  (* +1 for header *)
      end
    end

    (* MAKEFLOATBLOCK: 1-byte op + 2-byte size *)
    else if op = op_MAKEFLOATBLOCK then begin
      if !i < nwords then incr i
    end

    (* PUSHCONST0-3: no operand in bytecode *)
    else if op >= op_PUSHCONST0 && op <= op_PUSHCONST3 then
      ()

    (* GETMETHOD, GETDYNMET, SETPUBMET, etc: skip for now *)
    else
      ()  (* unknown – skip *)

  done;
  st

(* ---- Report Generation -------------------------------------------------- *)

let parse_size s =
  let s = String.lowercase_ascii s in
  let num = ref "" in
  let i = ref 0 in
  while !i < String.length s && (s.[!i] >= '0' && s.[!i] <= '9') do
    num := !num ^ String.make 1 s.[!i];
    incr i
  done;
  let n = if !num = "" then 0 else int_of_string !num in
  let suffix = String.sub s !i (String.length s - !i) in
  match suffix with
  | "k" | "kb" -> n
  | "m" | "mb" -> n * 1024
  | "" -> n / 1024
  | _ -> n

let check_int_truncation value target_word =
  if target_word = 32 then
    let v = Int64.of_int value in
    let max31 = Int64.shift_left 1L 30 in
    let min31 = Int64.neg max31 in
    v > max31 || v < min31
  else
    false

let report cfg st prim_names =
  let open Printf in

  (* Header *)
  printf "\n╔══════════════════════════════════════════════════════════╗\n";
  printf   "║        Camelino Static Analysis Report                   ║\n";
  printf   "╠══════════════════════════════════════════════════════════╣\n";
  printf   "║ File:       %-45s ║\n" (Filename.basename cfg.input_file);
  printf   "║ Platform:   %-45s ║\n" cfg.target_platform;
  printf   "║ Word size:  %d-bit%-40s ║\n" cfg.target_word
    (if cfg.big_endian then " (big-endian)" else " (little-endian)");
  printf   "╚══════════════════════════════════════════════════════════╝\n\n";

  (* 1. General stats *)
  printf "─── Bytecode Statistics ───────────────────────────────\n";
  printf "  Instructions (approx):  %d\n" st.total_instructions;
  let code_bytes = (List.length prim_names) * 4 in
  printf "  Code size (approx):     %d bytes\n" (st.total_instructions * 4);
  printf "  Unique opcodes used:    %d / %d\n"
    (Hashtbl.length st.used_opcodes) total_opcodes;

  (* Heap estimate *)
  let heap_estimate_bytes = st.heap_alloc_words * (cfg.target_word / 8) in
  let heap_estimate_kb = heap_estimate_bytes / 1024 in
  printf "  Heap alloc (static):    ~%d KB (%d blocks)\n"
    heap_estimate_kb st.heap_alloc_words;
  if cfg.heap_limit > 0 then begin
    let pct = if cfg.heap_limit = 0 then 0.0
      else float_of_int heap_estimate_kb /. float_of_int cfg.heap_limit *. 100.0 in
    printf "  Heap limit:             %d KB (%.1f%% used)\n" cfg.heap_limit pct;
    if heap_estimate_kb > cfg.heap_limit then
      printf "  ⚠ WARNING: Static heap estimate exceeds limit!\n"
  end;
  printf "\n";

  (* 2. Primitive analysis *)
  printf "─── Primitive Analysis ─────────────────────────────────\n";
  let found_prims = List.filter (fun s -> s <> "" && String.length s > 0) prim_names in
  printf "  Primitives referenced:  %d\n" (List.length found_prims);

  let implemented = ref 0 in
  let stubs = ref 0 in
  let missing = ref 0 in
  let unknown = ref 0 in
  let hal_caps_needed = Hashtbl.create 10 in

  List.iter (fun name ->
    match Hashtbl.find_opt prim_table name with
    | Some pi ->
        (match pi.p_status with
         | Implemented -> incr implemented
         | Stub -> incr stubs
         | Missing -> incr missing);
        List.iter (fun cap -> Hashtbl.replace hal_caps_needed cap ()) pi.p_hal_caps
    | None ->
        incr unknown
  ) found_prims;

  printf "  ├─ Implemented:          %d\n" !implemented;
  printf "  ├─ Stub (placeholder):   %d\n" !stubs;
  printf "  ├─ Missing (unimpl):     %d\n" !missing;
  printf "  └─ Unknown (not in DB):  %d\n" !unknown;

  if !stubs > 0 || !missing > 0 || !unknown > 0 then begin
    printf "\n  ⚠ Primitive details:\n";
    List.iter (fun name ->
      match Hashtbl.find_opt prim_table name with
      | Some pi ->
          if pi.p_status = Stub then
            printf "    ◐ %s (Phase %s) — STUB: runtime will call stub\n" name pi.p_phase
          else if pi.p_status = Missing then
            printf "    ✗ %s (Phase %s) — MISSING: not yet implemented\n" name pi.p_phase
      | None ->
          printf "    ? %s — UNKNOWN: not in Camelino primitive database\n" name
    ) found_prims;
  end;
  printf "\n";

  (* 3. Word-size truncation check *)
  if cfg.target_word = 32 && st.int_constants <> [] then begin
    printf "─── Word-size Truncation Check (64→32) ──────────────\n";
    let risky = List.filter (fun v -> check_int_truncation v 32) st.int_constants in
    if risky = [] then
      printf "  ✓ No truncation risks detected\n"
    else begin
      printf "  ⚠ WARNING: %d integer constant(s) exceed 31-bit range:\n"
        (List.length risky);
      List.iter (fun v ->
        printf "    CONSTINT %d — out of range for 32-bit target\n" v
      ) (List.sort compare risky);
    end;
    printf "\n";
  end;

  (* 4. Platform capability gaps *)
  printf "─── Platform Capability Check ─────────────────────────\n";
  let pc = List.find_opt (fun p -> p.pc_name = cfg.target_platform) platforms in
  (match pc with
   | None -> printf "  Platform '%s' not in capability database\n" cfg.target_platform
   | Some plat ->
       let platform_has cap = List.mem cap plat.pc_caps in
       let caps = Hashtbl.fold (fun cap () acc -> cap :: acc) hal_caps_needed [] in
       if caps = [] then
         printf "  ✓ No HAL capabilities required\n"
       else begin
         printf "  Required HAL capabilities:\n";
         List.iter (fun cap ->
           let ok = platform_has cap in
           printf "    %s %s\n" (if ok then "✓" else "✗") cap;
           if not ok then printf "      → Not supported on '%s'\n" cfg.target_platform;
         ) (List.sort String.compare caps);
       end;
  );
  printf "\n";

  (* 5. Summary *)
  printf "─── Summary ───────────────────────────────────────────\n";
  let issues = !stubs + !missing + !unknown in
  let trunc_issues = if cfg.target_word = 32 then
    List.length (List.filter (fun v -> check_int_truncation v 32) st.int_constants)
  else 0 in
  let cap_gaps =
    match List.find_opt (fun p -> p.pc_name = cfg.target_platform) platforms with
    | None -> 0
    | Some plat ->
        let caps = Hashtbl.fold (fun cap () acc -> cap :: acc) hal_caps_needed [] in
        List.length (List.filter (fun c -> not (List.mem c plat.pc_caps)) caps)
  in
  let total_issues = issues + (if trunc_issues > 0 then 1 else 0) + cap_gaps in

  printf "  Primitives: %d impl, %d stub, %d missing, %d unknown\n"
    !implemented !stubs !missing !unknown;
  printf "  Heap:       ~%d KB static estimate\n" heap_estimate_kb;
  printf "  Truncation: %d risky constant(s)\n" trunc_issues;
  printf "  Platform:   %d capability gap(s)\n" cap_gaps;
  printf "  ───────────────────────────────────────────\n";
  if total_issues = 0 then
    printf "  ✓ READY — No issues detected\n"
  else
    printf "  ⚠ %d issue(s) found — review details above\n" total_issues;
  printf "\n"

(* ---- Argument Parsing --------------------------------------------------- *)

let parse_args () =
  let cfg = {
    heap_limit = 0; flash_limit = 0;
    target_word = 32; target_platform = "arduino";
    big_endian = false; input_file = "";
  } in
  let args = Array.to_list Sys.argv in
  let rec loop = function
    | [] -> ()
    | "--heap" :: v :: rest -> cfg.heap_limit <- parse_size v; loop rest
    | "--flash" :: v :: rest -> cfg.flash_limit <- parse_size v; loop rest
    | "--word-size" :: v :: rest -> cfg.target_word <- int_of_string v; loop rest
    | "--platform" :: v :: rest -> cfg.target_platform <- v; loop rest
    | "--big-endian" :: rest -> cfg.big_endian <- true; loop rest
    | f :: rest when not (String.starts_with ~prefix:"--" f) ->
        cfg.input_file <- f; loop rest
    | unknown :: rest ->
        eprintf "Unknown option: %s\n" unknown; exit 1
  in
  loop (List.tl args);  (* skip program name *)
  if cfg.input_file = "" then begin
    eprintf "Usage: camelino-check [options] input.cmo\n";
    eprintf "Options:\n";
    eprintf "  --heap SIZE         Target heap size (e.g., 192k)\n";
    eprintf "  --flash SIZE        Target flash size (e.g., 512k)\n";
    eprintf "  --word-size N       Target word size (32 or 64, default: 32)\n";
    eprintf "  --platform NAME     Target platform (arduino, host, picosdk, ...)\n";
    eprintf "  --big-endian        Target is big-endian\n";
    exit 1
  end;
  if not (Sys.file_exists cfg.input_file) then begin
    eprintf "Error: file not found: %s\n" cfg.input_file; exit 1
  end;
  cfg

(* ---- Main --------------------------------------------------------------- *)

let () =
  let cfg = parse_args () in

  (* Read file *)
  let ic = open_in_bin cfg.input_file in
  let file_len = in_channel_length ic in
  let raw = really_input_string ic file_len in
  close_in ic;

  (* Validate magic *)
  if String.length raw < magic_len then begin
    eprintf "Error: file too small for .cmo format (need at least %d bytes)\n" magic_len;
    exit 1
  end;
  let magic = String.sub raw 0 magic_len in
  if magic <> Config.cmo_magic_number then begin
    eprintf "Error: bad magic — not a valid OCaml .cmo file\n";
    eprintf "  Expected: %S\n" Config.cmo_magic_number;
    eprintf "  Got:      %S\n" magic;
    exit 1
  end;
  if String.length raw < magic_len + 4 then begin
    eprintf "Error: file too small for .cmo cu_pos field\n"; exit 1
  end;

  (* Parse cu_pos *)
  let cu_pos = read_be32 raw magic_len in
  let code_start = magic_len + 4 in
  let code_end = if cu_pos > code_start then cu_pos else String.length raw in

  (* Extract primitives from compilation unit *)
  let prim_names = extract_primitives cfg.input_file cu_pos in

  (* Analyze bytecode *)
  let stats = analyze_bytecode raw code_start code_end in

  (* Cross-reference C_CALL indices with primitive names *)
  (* (For now, indices are positional — we use the extracted prim_names list) *)

  (* Generate report *)
  report cfg stats prim_names
