# Serenity Toolchain

This directory contains all toolchain related files. E.g. build scripts for
the cross compilation toolchain and build toolchain for ports.

This document previously contained outdated build instructions. [Click here for the latest build instructions.](https://github.com/SerenityOS/serenity/blob/master/Documentation/BuildInstructions.md)

For early 32-bit PowerPC kernel bring-up, build the freestanding SerenityOS
cross compiler and libgcc with:

```sh
env ARCH=powerpc MAKEJOBS=8 ./Toolchain/BuildGNU.sh --bootstrap-only
make -C Tests/Toolchain/PowerPC check
```

This selects the big-endian PowerPC SysV ABI and disables multilibs. The
regular `BuildGNU.sh` path also builds libstdc++. A single-ABI libstdc++ build
succeeds with the current PowerPC headers, while a bootable userland still
needs the rest of LibC and the dynamic loader port.
