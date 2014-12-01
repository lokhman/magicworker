Magic Worker
===========

Magic Worker is a small WinAPI application written in C that keeps
your account online in active messaging clients (e.g. Skype, Lync,
etc) if you don't have enough permissions to change timeout setting
for the "away" mode.

Application simply triggers a low level mouse event with the given
time interval. Additionally it blocks system display power off and
screen saver functionality. Useful interface features include
"worker" timer, system tray status, and invisible mode. Magic Worker
is a 42KB standalone executable compatible with almost all versions
of Microsoft Windows. It does not require installation and does not
keep any settings on your machine.

Since the low level system event fired in Magic Worker does not
change anything on your screen, you may run the application in
background using "Invisible mode" setting. This will make you always
online and active when you are working or are away from the computer.

Usage hints:
- To run in invisible mode check corresponding setting in window menu.
- To stop the worker press ESC key shortly at least two times.

Magic Worker is available under the MIT license. The included LICENSE
file describes this in detail.
