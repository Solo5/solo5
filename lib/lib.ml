open Shexp_process
open Shexp_process.Infix

(* "Configurable" settings -------------------------------------------------- *)

let run_path = "./run"
let switch_path = run_path ^ "/switch"
let src_path = "./unikernel"
let block_path = run_path ^ "/disk.img"
let universe_path = "./universe"

(* Prepare: Base switch setup ----------------------------------------------- *)

(* setup the base switch, creating a "cache" of it in switch.tar.gz, this is
 * useful for repeated runs mainly while developing this script, to save on
 * time taken to build ocaml *)
let prep_setup_switch = 
    file_exists switch_path >>= function
    | true  ->
      call [ "rm"; "-rf"; switch_path ]
      >> call [ "mkdir"; "-p"; switch_path ]
      >> chdir switch_path (call [ "tar"; "-xzf"; "../switch.tar.gz" ])
    | false ->
      call [ "mkdir"; "-p"; switch_path ]
      >> call [ "opam"; "switch"; "create"; switch_path; "4.09.0" ]
      >> chdir switch_path (call [ "tar"; "-czf"; "../switch.tar.gz"; "." ])

(* Prepare: Install root packages ------------------------------------------- *)

type pin_action = Master | Release | Local

let universe : (string * pin_action) list =
  let u = eval (readdir universe_path) in
  Stdlib.List.map (fun pkg ->
    let pkgdir = eval (readdir (universe_path ^ "/" ^ pkg)) in
    match pkgdir with
    | [ "master" ] -> (pkg, Master)
    | [ "release" ] -> (pkg, Release)
    | [ "local" ] -> (pkg, Local)
    | _ -> failwith ("Invalid definition for package: " ^ pkg))
    u

let abspath path = call [ "readlink"; "-f"; path ] |- read_all >>| String.trim

(* the eval expression will be evaluated immediately, so we can't use (abspath
 * switch_path) here as it might not exist yet *)
let with_switch = [ "opam"; "exec";
                    "--switch=" ^ (eval (abspath ".")) ^ "/" ^ switch_path;
                    "--set-switch"; "--" ]

let opam_install args =
  set_env "OPAMYES" "true" (call (with_switch @ [ "opam"; "install" ] @ args))

let opam_pin_action (pkg, action) =
  match action with
  | Master -> (* pin package to --dev-repo *)
    set_env "OPAMYES" "true" (
      call (with_switch @ [ "opam"; "pin"; "add"; "-n"; "--dev-repo"; pkg ]))
  | Local -> (* pin package to local repo at universe/pkg/local *)
    set_env "OPAMYES" "true" (
      call (with_switch @ [ "opam"; "pin"; "add"; "-n"; pkg;
                            universe_path ^ "/" ^ pkg ^ "/local" ]))
  | Release -> (* install but don't pin, i.e. use the released version *)
    return ()

let prep_install_packages =
  Shexp_process.List.iter ~f:opam_pin_action universe
  >> opam_install (Stdlib.List.map (fun (pkg, _) -> pkg) universe)

(* Build: Build smoketest unikernel ----------------------------------------- *)

let call_ignored args = call_exit_status args >>= fun _status -> return ()

let build_smoketest =
  chdir src_path (
    call_ignored (with_switch @ [ "mirage"; "clean" ])
    >> call (with_switch @ [ "mirage"; "configure"; "-t"; "hvt" ])
    >> call (with_switch @ [ "make"; "depend" ])
    >> call (with_switch @ [ "make" ])
  )

(* Run: Setup network device ------------------------------------------------ *)

let run_setup_net =
  let ip args = call ([ "sudo"; "ip" ] @ args) in
  file_exists "/sys/class/net/tap100" >>= function
  | true  -> return ()
  | false -> ip [ "tuntap"; "add"; "tap100"; "mode"; "tap" ]
             >> ip [ "addr"; "add"; "10.0.0.1/24"; "dev"; "tap100" ]
             >> ip [ "link"; "set"; "dev"; "tap100"; "up" ]

(* Run: Setup block device -------------------------------------------------- *)

let run_setup_block =
  file_exists block_path >>= function
  | true  -> return ()
  | false ->
    call [ "dd"; "if=/dev/zero"; "of=" ^ block_path; "bs=512"; "count=1" ]

(* Run: Initialise smoketest ------------------------------------------------ *)

let run_init_smoketest =
  call (with_switch @
        [ "solo5-hvt";
          "--net:service=tap100";
          "--block:storage=" ^ block_path;
          src_path ^ "/test.hvt"; "--init" ])

(* Run: Run smoketest ------------------------------------------------------- *)

let run_smoketest_server =
  call (with_switch @
        [ "solo5-hvt";
          "--net:service=tap100";
          "--block:storage=" ^ block_path;
          src_path ^ "/test.hvt" ])

let run_smoketest_client =
  call [ "dune"; "exec";
         "client/main.exe"; "10.0.0.2" ] |- read_all >>= fun output ->
  return (int_of_string (String.trim (output)))

let run_smoketest expected =
  fork run_smoketest_server run_smoketest_client >>= fun (_s, c) ->
  if c = expected then
    echo (">>>> Passed: " ^ string_of_int expected)
  else failwith ("Failed:" ^ string_of_int c)
