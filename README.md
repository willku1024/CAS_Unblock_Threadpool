# CAS_Unblock_Threadpool
Unblock Threadpool based on lock-free ringQueue(CAS). Use c++11 lib: thread , future and so on.



#### Project Tree

> |-- LICENSE
> |— README.md
> |-- RingQueue.hpp							  cas free-lock queue, include by `Threadpool.casQueue.hpp`
> |— Threadpool.casQueue.hpp	  task queue use cas ringQueue
> |— Threadpool.stlQueue.hpp 	  task  queue use stl list
> |-- main.cpp
> `-- testlog
>
> 0 directories, 11 files



#### Compare two Threadpool

```bash
willku@vm01:~/hgfs/Files/CAS_Unblock_Threadpool$ time bash -c "for i in {1..50};do ./stllist >/dev/null;done"

real    0m49.621s
user    0m0.122s
sys     0m0.363s
willku@vm01:~/hgfs/Files/CAS_Unblock_Threadpool$ time bash -c "for i in {1..50};do ./casQ >/dev/null;done"

real    0m36.573s
user    0m0.115s
sys     0m0.339s


willku@vm01:~/CAS_Unblock_Threadpool$ \time -v bash -c "for i in {1..50};do ./casQ >/dev/null;done"
        Command being timed: "bash -c for i in {1..50};do ./cas >/dev/null;done"
        User time (seconds): 0.12
        System time (seconds): 0.32
        Percent of CPU this job got: 1%
        Elapsed (wall clock) time (h:mm:ss or m:ss): 0:29.54
        Average shared text size (kbytes): 0
        Average unshared data size (kbytes): 0
        Average stack size (kbytes): 0
        Average total size (kbytes): 0
        Maximum resident set size (kbytes): 5700
        Average resident set size (kbytes): 0
        Major (requiring I/O) page faults: 50
        Minor (reclaiming a frame) page faults: 34086
        Voluntary context switches: 13397
        Involuntary context switches: 2506
        Swaps: 0
        File system inputs: 20000
        File system outputs: 0
        Socket messages sent: 0
        Socket messages received: 0
        Signals delivered: 0
        Page size (bytes): 4096
        Exit status: 0
        
willku@vm01:~/CAS_Unblock_Threadpool$ \time -v bash -c "for i in {1..50};do ./stlQ >/dev/null;done"
        Command being timed: "bash -c for i in {1..50};do ./stllist >/dev/null;done"
        User time (seconds): 0.12
        System time (seconds): 0.38
        Percent of CPU this job got: 1%
        Elapsed (wall clock) time (h:mm:ss or m:ss): 0:47.67
        Average shared text size (kbytes): 0
        Average unshared data size (kbytes): 0
        Average stack size (kbytes): 0
        Average total size (kbytes): 0
        Maximum resident set size (kbytes): 4648
        Average resident set size (kbytes): 0
        Major (requiring I/O) page faults: 50
        Minor (reclaiming a frame) page faults: 20973
        Voluntary context switches: 16554
        Involuntary context switches: 3749
        Swaps: 0
        File system inputs: 20000
        File system outputs: 0
        Socket messages sent: 0
        Socket messages received: 0
        Signals delivered: 0
        Page size (bytes): 4096
        Exit status: 0

```





