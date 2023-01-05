disconnect
target extended-remote :3333
set remote hardware-watchpoint-limit 2
mon reset halt
thb app_main
flushregs