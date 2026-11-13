CampusCompute OS Mini-Project

Requirements
Ubuntu/Linux, GCC and GNU Make.

Build
make

Run commands from the project directory
./campuscompute help
./campuscompute fcfs tests/jobs.txt
./campuscompute rr tests/jobs.txt 2
./campuscompute priority tests/jobs.txt
./campuscompute compare tests/jobs.txt 2
./campuscompute banker tests/banker_safe.txt
./campuscompute banker tests/banker_unsafe.txt
./campuscompute logger
./campuscompute ipc
./campuscompute worker normal
./campuscompute worker fail
./campuscompute limit
./campuscompute demo

Logs
Events are appended to activity.log.

Cleanup
Logger threads are joined and child processes are collected.
The IPC demonstration closes and unlinks its shared memory
and named semaphores.
The resource-limit demonstration removes its temporary file.
make clean removes generated project objects and executable.

Resource limit
The limit demonstration applies RLIMIT_FSIZE to a child.
It attempts to write 2048 bytes with a 1024-byte limit and
checks the write error and actual file size.

The worker fail command demonstrates a deliberate simulated
failure with exit status 7.
