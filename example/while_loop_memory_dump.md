## While-loop process
```c++
#include <iostream>
#include <unistd.h>
#include <bits/this_thread_sleep.h>

int main() {
    std::cout << "current process PID=" << getpid() << std::endl;
    int i = 0, summa = 0;
    while (i < 100) {
        summa += i;
        i++;
        std::cout << summa << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}
```

## While-loop process memory dump (/proc/PID/maps)
```text
Using: process_vm_readv (Scatter-Gather I/O)
Memory Reader: process_vm_readv
Target PID: 10936
Output: dump.bin
========================================
Dumping process 10936 using process_vm_readv
Found 39 regions, 38 readable
[1] 55958e5b1000-55958e5b2000 (4096 bytes) /home/user/dir/programming/MemoryDumper/cmake-build-debug/test_binary/TestBinary
[2] 55958e5b2000-55958e5b3000 (4096 bytes) /home/user/dir/programming/MemoryDumper/cmake-build-debug/test_binary/TestBinary
[3] 55958e5b3000-55958e5b4000 (4096 bytes) /home/user/dir/programming/MemoryDumper/cmake-build-debug/test_binary/TestBinary
[4] 55958e5b4000-55958e5b5000 (4096 bytes) /home/user/dir/programming/MemoryDumper/cmake-build-debug/test_binary/TestBinary
[5] 55958e5b5000-55958e5b6000 (4096 bytes) /home/user/dir/programming/MemoryDumper/cmake-build-debug/test_binary/TestBinary
[6] 5595b39c5000-5595b39f8000 (208896 bytes) [heap]
[7] 7f7345ae4000-7f7345b08000 (147456 bytes) /usr/lib/libc.so.6
[8] 7f7345b08000-7f7345c79000 (1511424 bytes) /usr/lib/libc.so.6
[9] 7f7345c79000-7f7345cc7000 (319488 bytes) /usr/lib/libc.so.6
[10] 7f7345cc7000-7f7345ccb000 (16384 bytes) /usr/lib/libc.so.6
[11] 7f7345ccb000-7f7345ccd000 (8192 bytes) /usr/lib/libc.so.6
[12] 7f7345ccd000-7f7345cd5000 (32768 bytes)
[13] 7f7345cd5000-7f7345cd9000 (16384 bytes) /usr/lib/libgcc_s.so.1
[14] 7f7345cd9000-7f7345cfc000 (143360 bytes) /usr/lib/libgcc_s.so.1
[15] 7f7345cfc000-7f7345d00000 (16384 bytes) /usr/lib/libgcc_s.so.1
[16] 7f7345d00000-7f7345d01000 (4096 bytes) /usr/lib/libgcc_s.so.1
[17] 7f7345d01000-7f7345d02000 (4096 bytes) /usr/lib/libgcc_s.so.1
[18] 7f7345d02000-7f7345d11000 (61440 bytes) /usr/lib/libm.so.6
[19] 7f7345d11000-7f7345d9c000 (569344 bytes) /usr/lib/libm.so.6
[20] 7f7345d9c000-7f7345dfe000 (401408 bytes) /usr/lib/libm.so.6
[21] 7f7345dfe000-7f7345dff000 (4096 bytes) /usr/lib/libm.so.6
[22] 7f7345dff000-7f7345e00000 (4096 bytes) /usr/lib/libm.so.6
[23] 7f7345e00000-7f7345e97000 (618496 bytes) /usr/lib/libstdc++.so.6.0.34
[24] 7f7345e97000-7f7345fec000 (1396736 bytes) /usr/lib/libstdc++.so.6.0.34
[25] 7f7345fec000-7f734607e000 (598016 bytes) /usr/lib/libstdc++.so.6.0.34
[26] 7f734607e000-7f734608f000 (69632 bytes) /usr/lib/libstdc++.so.6.0.34
[27] 7f734608f000-7f7346090000 (4096 bytes) /usr/lib/libstdc++.so.6.0.34
[28] 7f7346090000-7f7346094000 (16384 bytes)
[29] 7f73460b3000-7f73460b9000 (24576 bytes)
Skipping: [vvar]
[30] 7f73460e9000-7f73460eb000 (8192 bytes) [vdso]
[31] 7f73460eb000-7f73460ec000 (4096 bytes) /usr/lib/ld-linux-x86-64.so.2
[32] 7f73460ec000-7f7346116000 (172032 bytes) /usr/lib/ld-linux-x86-64.so.2
[33] 7f7346116000-7f7346121000 (45056 bytes) /usr/lib/ld-linux-x86-64.so.2
[34] 7f7346121000-7f7346123000 (8192 bytes) /usr/lib/ld-linux-x86-64.so.2
[35] 7f7346123000-7f7346124000 (4096 bytes) /usr/lib/ld-linux-x86-64.so.2
[36] 7f7346124000-7f7346125000 (4096 bytes)
[37] 7ffc3d1f3000-7ffc3d214000 (135168 bytes) [stack]

Stats:
Total requested: 6598656 bytes
Total read: 6598656 bytes
Success: 37/37 regions
========================================
Dump completed successfully!

Final Stats:
Bytes requested: 6598656
Bytes read: 6598656
Regions: 37/37 successful
```