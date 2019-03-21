open Lib
open Shexp_process

(* See commentary in lib.ml for what the various sub-stages do. In general,
 * when developing this script or fixing failures found by it, you can comment
 * out earlier stages which you are not interested in running in order to save
 * iteration time.
 *
 * The default behaviour is to always do everything from scratch, with the
 * exception of some limited caching of prep_setup_switch. *)

let () =
    Logged.eval prep_setup_switch;
    Logged.eval prep_install_packages;
    Logged.eval build_smoketest;
    Logged.eval run_setup_net;
    Logged.eval run_setup_block;
    Logged.eval run_init_smoketest;
    let expected = [ 1; 2; 3 ] in
    Logged.eval (List.iter ~f:run_smoketest expected)
