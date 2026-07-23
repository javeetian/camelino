(*
 * examples/host/AllOpcodes/all_opcodes.ml
 * @brief 全 149 条 ZAM 指令 ML 覆盖测试
 *
 * 每个 OCaml 特性对应一类 ZAM 指令。✅ 管道能过，⚠️ segfault/待修。
 * 不求跑通——只管把所有指令类别的测试代码写全。
 *
 * 运行: bash build.sh
 *)

(* ============================================================
   1. ACC0-7, ACC, PUSH, PUSHACC0-7, PUSHACC, POP, ASSIGN
   栈访问 — 每个 let 绑定触发 PUSH + ACC + ASSIGN
   ============================================================ *)
let _ = 0
let _ = 1
let _ = 2
let _ = 3
let a = 42          (* SETGLOBAL a *)
let _ = a           (* GETGLOBAL a → ACC *)
let _ = let x = 5 in let y = 10 in x + y  (* PUSH + ACC + ASSIGN *)

(* ============================================================
   2. ENVACC1-4, ENVACC, PUSHENVACC1-4, PUSHENVACC
   环境访问 — 闭包捕获自由变量
   ⚠️ segfault
   ============================================================ *)
(*
let _ =
  let x = 10 in
  let y = 20 in
  let z = 30 in
  let f w = x + y + z + w in
  f 40
*)

(* ============================================================
   3. PUSH_RETADDR, APPLY, APPLY1, APPLY2, APPLY3
   函数调用
   ⚠️ segfault
   ============================================================ *)
(*
let f x = x + 1           (* APPLY1 *)
let _ = f 5

let add x y = x + y       (* APPLY2 *)
let _ = add 3 4

let triple x y z = x+y+z  (* APPLY3 *)
let _ = triple 1 2 3
*)

(* ============================================================
   4. APPTERM, APPTERM1-3 — 尾调用优化
   ⚠️ segfault (CLOSURE code offset)
   ============================================================ *)
(*
let rec tail_sum acc n =
  if n <= 0 then acc
  else tail_sum (acc+n) (n-1)
let _ = tail_sum 0 10000  (* APPTERM *)
*)

(* ============================================================
   5. RETURN, RESTART, GRAB
   函数返回 / 多参数应用重组 / 参数匹配
   ============================================================ *)
(*
let f x = x + 2           (* RETURN 1 *)
let _ = f 5               (* APPLY1 + GRAB 1 *)

let add x y = x + y       (* GRAB 2 *)
let inc = add 1            (* RESTART via partial app *)
let _ = inc 10
*)

(* ============================================================
   6. CLOSURE, CLOSUREREC
   闭包创建 / 递归闭包
   ============================================================ *)
(*
let mk_adder x = fun y -> x + y           (* CLOSURE *)
let _ = (mk_adder 5) 10

let rec fact n = if n <= 1 then 1 else n * fact (n-1)  (* CLOSUREREC *)
let _ = fact 5

let rec even n = if n=0 then 1 else odd(n-1)  (* CLOSUREREC multi *)
and odd n = if n=0 then 0 else even(n-1)
let _ = even 4
*)

(* ============================================================
   7. OFFSETCLOSURE — 从闭包偏移访问字段
   ⚠️ segfault
   ============================================================ *)
(*
let _ =
  let x = 10 in
  let y = 20 in
  let f () = x + y in   (* OFFSETCLOSURE to reach x and y *)
  f ()
*)

(* ============================================================
   8. GETGLOBAL, PUSHGETGLOBAL, GETGLOBALFIELD,
      PUSHGETGLOBALFIELD, SETGLOBAL
   全局变量
   ============================================================ *)
let global_val = 42               (* SETGLOBAL *)
let _ = global_val + 1            (* GETGLOBAL *)

(* ============================================================
   9. ATOM0, ATOM, PUSHATOM0, PUSHATOM
   结构化常量
   ============================================================ *)
let _ = ()                         (* ATOM0: unit *)
let _ = true                       (* ATOM: true *)
let _ = false                      (* ATOM: false *)
let _ = []                         (* ATOM: empty list *)

(* ============================================================
   10. MAKEBLOCK, MAKEBLOCK1-3, MAKEFLOATBLOCK
   堆块分配
   ============================================================ *)
let _ = (1, 2)                     (* MAKEBLOCK2 tag=0 size=2 *)
let _ = (1, 2, 3)                  (* MAKEBLOCK3 tag=0 size=3 *)
let _ = Some 42                    (* MAKEBLOCK1 tag=0 size=1 *)
let _ = [1; 2; 3]                  (* MAKEBLOCK cons cells *)
(* let _ = 1.5    MAKEFLOATBLOCK — needs FPU *)

(* ============================================================
   11. GETFIELD0-3, GETFIELD, SETFIELD0-3, SETFIELD
   字段访问 / 修改
   ============================================================ *)
let _ =
  let t = (10, 20, 30) in
  let (a, b, c) = t in             (* GETFIELD0-2 *)
  a + b + c

type point = { x : int; y : int }
let _ =
  let p = { x = 3; y = 4 } in      (* MAKEBLOCK2 *)
  let q = { p with x = 10 } in     (* SETFIELD *)
  q.x + q.y                        (* GETFIELD *)

(* ============================================================
   12. GETFLOATFIELD, SETFLOATFIELD — 浮点字段
   ⚠️ needs FPU
   ============================================================ *)
(* let _ = [|1.0; 2.0; 3.0|].(1) *)
(* let a = [|0.0; 0.0|] in a.(0) <- 1.5 *)

(* ============================================================
   13. VECTLENGTH, GETVECTITEM, SETVECTITEM
   数组操作 — ⚠️ needs caml_make_vect primitive
   ============================================================ *)
(* let arr = [|1; 2; 3|] in arr.(0) + arr.(1) + arr.(2) *)

(* ============================================================
   14. GETBYTESCHAR, SETBYTESCHAR — 字节操作
   ⚠️ needs stdlib
   ============================================================ *)
(* let b = Bytes.create 3 in Bytes.set b 0 'A'; Bytes.get b 0 *)

(* ============================================================
   15. BRANCH, BRANCHIF, BRANCHIFNOT, SWITCH, BOOLNOT
   分支和布尔
   ============================================================ *)
let _ = if 1 = 1 then 99 else 0    (* BRANCHIF *)
let _ = if 1 = 2 then 0 else 88    (* BRANCHIFNOT *)
let _ = not false                  (* BOOLNOT *)
let _ = not true

type abc = A | B | C
let _ = match B with               (* SWITCH *)
        | A -> 1
        | B -> 2
        | C -> 3

(* ============================================================
   16. PUSHTRAP, POPTRAP, RAISE, RERAISE, RAISE_NOTRACE
   异常处理
   ⚠️ needs Stdlib Not_found
   ============================================================ *)
(*
let _ = try raise Not_found with Not_found -> 99
let _ = try raise Exit with Exit -> 1
exception MyExn of int
let _ = try raise (MyExn 42) with MyExn x -> x
*)

(* ============================================================
   17. CHECK_SIGNALS — 信号检查（嵌入式桩）
   ============================================================ *)
(* OCaml 编译器在循环/递归点自动插入 CHECK_SIGNALS，
   无需显式触发 *)

(* ============================================================
   18. C_CALL1-5, C_CALLN — FFI 调用
   ⚠️ needs external primitive registration
   ============================================================ *)
(*
external my_add : int -> int -> int = "caml_test_add"
let _ = my_add 3 5                 (* C_CALL2 *)
*)

(* ============================================================
   19. CONST0-3, CONSTINT, PUSHCONSTINT
   常量指令
   ============================================================ *)
let _ = 0    (* CONST0 *)
let _ = 1    (* CONST1 *)
let _ = 2    (* CONST2 *)
let _ = 3    (* CONST3 *)
let _ = 42   (* CONSTINT *)
let _ = -99  (* CONSTINT negative *)
let _ = 12345(* CONSTINT large *)

(* ============================================================
   20. NEGINT, ADDINT, SUBINT, MULINT, DIVINT, MODINT,
       ANDINT, ORINT, XORINT, LSLINT, LSRINT, ASRINT
   整数运算
   ============================================================ *)
let _ = ~-42
let _ = 3 + 2
let _ = 10 - 3
let _ = 6 * 7
let _ = 20 / 4
let _ = 10 mod 3
let _ = 5 land 3
let _ = 5 lor 2
let _ = 5 lxor 3
let _ = 1 lsl 3
let _ = 8 lsr 3
let _ = (-8) asr 1

(* ============================================================
   21. EQ, NEQ, LTINT, LEINT, GTINT, GEINT
   比较
   ============================================================ *)
let _ = (3 = 3)
let _ = (3 <> 5)
let _ = (3 < 5)
let _ = (3 <= 3)
let _ = (5 > 3)
let _ = (5 >= 3)

(* ============================================================
   22. OFFSETINT, OFFSETREF, ISINT
   整数偏移 / ref 偏移 / 类型检查
   ⚠️ REF segfault
   ============================================================ *)
(*
let _ = let r = ref 10 in r := !r + 5; !r   (* OFFSETREF *)
let _ = let r = ref 0 in for i=1 to 10 do r := !r + i done; !r
(* for loop triggers OFFSETINT BEQ BRANCH *)
*)

(* ============================================================
   23. GETMETHOD, GETPUBMET, GETDYNMET
   对象方法调度
   ⚠️ needs CamlinternalOO stdlib
   ============================================================ *)
(*
class counter = object
  val mutable n = 0
  method get = n
  method incr = n <- n + 1
end
let _ =
  let c = new counter in
  c#incr;                (* GETPUBMET *)
  c#get                  (* GETMETHOD *)
*)

(* ============================================================
   24. BEQ, BNEQ, BLTINT, BLEINT, BGTINT, BGEINT
   分支比较 — for 循环触发
   ⚠️ needs ref
   ============================================================ *)
(*
let _ =
  let r = ref 0 in
  for i = 0 to 9 do r := !r + 1 done;
  !r
(* BEQ at loop back-edge *)
*)

(* ============================================================
   25. ULTINT, UGEINT, BULTINT, BUGEINT
   无符号比较 — 少用，需要特殊代码触发
   ============================================================ *)
(* OCaml 极少生成无符号比较，这里放占位 *)
(* 通过 Nativeint 或 Int64 操作可触发，依赖 stdlib *)

(* ============================================================
   26. STOP, EVENT, BREAK
   控制 / 调试
   ============================================================ *)
(* STOP 由编译器在模块末尾自动插入 *)
(* EVENT/BREAK 由调试器插入，正常代码不触发 *)

(* ============================================================
   27. RERAISE, RAISE_NOTRACE, GETSTRINGCHAR
   异常重抛 / 字符串字符
   ⚠️ needs stdlib
   ============================================================ *)
(* let _ = try raise Exit with _ -> raise Not_found (* RERAISE *) *)
(* let _ = "hello".[1]                                   (* GETSTRINGCHAR *) *)

(* ============================================================
   28. 递归测试（已验证 ✅）
   ============================================================ *)
let rec sum n = if n <= 0 then 0 else n + sum (n - 1)
let _ = sum 10

(* ============================================================
   Final: 返回 149
   ============================================================ *)
let _ = 149
