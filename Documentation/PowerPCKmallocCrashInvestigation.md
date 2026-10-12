# PowerPC kmalloc and HPT alias crash

## Reproduction

In a 512 MiB QEMU Pegasos 2 guest with a 750cxe CPU, open Terminal, run
`ping google.com`, wait for two replies, and press Ctrl+C. Open `google.com`
in Browser, then search for `user agent` with a space.

The reported failure was a deadly KUBSAN misaligned `FreelistEntry` access at
`Kernel/Heap/kmalloc.cpp:80`. The bogus pointer was `0x00101193`; the stack
included `NetworkTask_main` while it allocated a timer from the 128-byte
slab. That allocation exposed damaged allocator state. It did not identify
the writer.

## Cause

The loader reserved physical `0x04800000–0x04bfffff` for its 4 MiB hash
page table and mapped it at virtual `0xc4800000–0xc4bfffff`. The slab block
reported at `0xc4810000` was inside this mapping. Reading it in QEMU showed
alternating PowerPC hash page table upper and lower words. The bogus pointer
`0x00101193` also has the form of a hash page table lower word: a page
address combined with flags.

The PowerPC memory manager registered the kernel image in its virtual region
tree but omitted the hash page table alias. Before the fix, the virtual range
chosen for later kmalloc heap expansion was
`0xc4700000–0xc86fffff`, overlapping the alias. Heap expansion could
therefore map allocator storage over the hash page table.

## Fix and regression

PowerPC memory initialization now registers the loader's hash page table
alias as an unbacked virtual region before placing other regions. It also
checks that the selected kmalloc expansion range does not intersect the
alias.

The `PowerPCKmallocHPTCheckQuick` and `PowerPCKmallocHPTCheck` targets boot
the full kernel under QEMU and compare the actual selected ranges. The quick
target failed on the old code. Both targets pass after the fix on 750 and
750cxe CPUs, with the kmalloc expansion range moved to
`0xc4c00000–0xc8bfffff`.

`PowerPCBrowserCheckQuick` and `PowerPCBrowserCheck` also pass with the
rebuilt loader. They cover local page rendering, DHCP, DNS, and guest time.
The exact interactive Google search has not been replayed after the fix.

Run the checks with:

```sh
cmake --build Build/powerpc --target PowerPCKmallocHPTCheck
cmake --build Build/powerpc --target PowerPCBrowserCheck
```
