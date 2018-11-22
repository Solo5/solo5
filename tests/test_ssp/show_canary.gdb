target remote:1234
p /x __stack_chk_guard
b solo5_app_main
c
p /x __stack_chk_guard
c
q

