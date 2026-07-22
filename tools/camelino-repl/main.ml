(*
 * camelino-repl — Interactive OCaml REPL for Camelino
 *
 * Reads OCaml expressions, compiles to bytecode, runs on host simulator.
 *
 * Usage:
 *   camelino-repl --local      Run locally via host simulator
 *   camelino-repl --port DEV   Run via serial port to device (future)
 *)

open Printf

let camelino_embed = "tools/camelino-embed/camelino-embed"
let camelino_host  = "build/camelino-host"
let temp_dir = Filename.get_temp_dir_name ()

(* Counter for unique temp files *)
let counter = ref 0
let next_id () = incr counter; !counter

(* Run a command and capture its stdout *)
let run_cmd cmd =
  let ic = Unix.open_process_in cmd in
  let buf = Buffer.create 256 in
  (try while true do Buffer.add_char buf (input_char ic) done
   with End_of_file -> ());
  match Unix.close_process_in ic with
  | Unix.WEXITED 0 -> String.trim (Buffer.contents buf)
  | Unix.WEXITED n -> failwith (sprintf "command failed (exit %d): %s" n cmd)
  | _ -> failwith ("command killed: " ^ cmd)

(* Compile OCaml expression → .cmo *)
let compile expr =
  let id = next_id () in
  let ml_file = sprintf "%s/cml_repl_%d.ml" temp_dir id in
  let cmo_file = sprintf "%s/cml_repl_%d.cmo" temp_dir id in
  (* Wrap expression to produce a value *)
  let oc = open_out ml_file in
  output_string oc expr;
  close_out oc;
  (try
    let _ = run_cmd (sprintf "ocamlc -c -o %s %s 2>&1" cmo_file ml_file) in
    Unix.unlink ml_file;
    cmo_file
  with e ->
    (try Unix.unlink ml_file with _ -> ());
    raise e)

(* Embed .cmo → .camel *)
let embed cmo_file =
  let id = next_id () in
  let camel_file = sprintf "%s/cml_repl_%d.camel" temp_dir id in
  let _ = run_cmd (sprintf "%s %s -o %s 2>&1" camelino_embed cmo_file camel_file) in
  Unix.unlink cmo_file;
  camel_file

(* Execute .camel via host simulator *)
let execute camel_file =
  let result = run_cmd (sprintf "%s --acc-only %s 2>/dev/null" camelino_host camel_file) in
  Unix.unlink camel_file;
  result

(* REPL loop *)
let repl_loop () =
  printf "Camelino REPL (Phase 6.5) — type OCaml expressions, Ctrl+D to exit\n";
  printf "Example: 2 + 3\n\n";
  let rec loop () =
    printf "> %!";
    match input_line stdin with
    | line ->
      let line = String.trim line in
      if line = "" then loop ()
      else begin
        (try
          let expr = if String.starts_with ~prefix:"let" line then line
                     else "let _ = " ^ line in
          let cmo = compile expr in
          let camel = embed cmo in
          let result = execute camel in
          printf "%s\n\n%!" result
        with
        | Failure msg ->
          printf "Error: %s\n\n%!" msg
        | e ->
          printf "Error: %s\n\n%!" (Printexc.to_string e));
        loop ()
      end
    | exception End_of_file ->
      printf "\nBye.\n"
  in
  loop ()

(* Main *)
let () =
  let local_mode = ref false in
  let args = Array.to_list Sys.argv |> List.tl in
  let rec parse = function
    | "--local" :: rest -> local_mode := true; parse rest
    | "--port" :: _ :: rest -> parse rest  (* future *)
    | [] -> ()
    | unknown :: _ ->
      eprintf "Unknown option: %s\n" unknown;
      eprintf "Usage: camelino-repl [--local] [--port DEV]\n";
      exit 1
  in
  parse args;
  if not !local_mode then begin
    eprintf "camelino-repl: use --local for host-simulator mode\n";
    eprintf "  --port DEV will enable serial device mode in a future release\n";
    exit 1
  end;
  (* Check tools exist *)
  if not (Sys.file_exists camelino_embed) then begin
    eprintf "Error: camelino-embed not found at %s\n" camelino_embed;
    eprintf "Build it first: cd tools/camelino-embed && ocamlfind ocamlc -linkpkg -package compiler-libs.common -o camelino-embed main.ml\n";
    exit 1
  end;
  repl_loop ()
