@echo off
del sms.exe
cl /MD /Os main.c psg.c ym2413.c /link /out:temp.exe
upx -9 -o sms.exe temp.exe
del temp.exe main.obj psg.obj ym2413.obj
pause
