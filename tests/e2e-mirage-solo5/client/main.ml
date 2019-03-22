open Lwt.Infix

let sockaddr_of_string s =
  let open Unix in ADDR_INET (inet_addr_of_string s, 23)

let try_netcat addr =
  Lwt.try_bind
    (fun () -> (* do this task *)
      Lwt_io.with_connection (sockaddr_of_string addr)
            (fun (in_ch, _out_ch) ->
               Lwt_io.read_line in_ch >>= fun line ->
               Lwt_io.write_line Lwt_io.stdout line))
    (fun () -> (* and then this if the task succeeded *)
      Lwt.return true)
    (fun e  -> (* or this if the task failed *)
      Lwt_io.printf "Failed with: %s\n" (Printexc.to_string e)
      >>= fun () ->
      Lwt.return false)

let () =
  let addr = Sys.argv.(1)
  in
  (* We can get all sorts of transient errors while the unikernel is starting,
   * so try to connect up to three times with an increasing timeout, and fail
   * if the entire operation takes more than 10 seconds.
   *)
  let fail_on_timeout =
    Lwt_unix.sleep 10.0 >>= fun () ->
    Lwt.fail_with "Timed out"
  in
  let netcat =
    try_netcat addr
    >>= function
    | true  -> Lwt.return_unit
    | false -> Lwt_unix.sleep 0.5 >>= fun () -> try_netcat addr
    >>= function
    | true  -> Lwt.return_unit
    | false -> Lwt_unix.sleep 1.0 >>= fun () -> try_netcat addr
    >>= function
    | true  -> Lwt.return_unit
    | false -> Lwt.fail_with "Failed"
  in
  Lwt_main.run (Lwt.pick [netcat; fail_on_timeout])
