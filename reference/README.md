# Reference projects

These are pinned git submodules of other Android translation layers. They are here for study only and are **not built** as part of Lunaria. To fetch them:

    git submodule update --init --depth 1 reference/<name>

| Submodule | License | What to take |
|---|---|---|
| androidrecomp | MIT | Notes on where bionic and glibc differ in ABI (`stat`/`dirent`/`tm`, `va_list`, `z_stream`, `fopen` text mode, recursive `mkdir`). A three-layer shim design (explicit implementation → ABI-identical alias → host libc). Unicorn-based tests that compare its translated code against reference execution (`lift_verify*.py`). |
| android-compatibility-layer | MIT | `GuestLibc`/`GuestSync`, to compare against Lunaria's HLE. About 1,500 framework classes in `java/`, as a reference for filling gaps in `jni_stubs`/`dvm`. Decoder test scripts (`tools/run-*-gate.sh`). |
| pvz2-re | MIT | Also dynarmic-based: diff its `dynarmic_config*.cpp` against Lunaria's. Its GuestHeap and OBB/VFS handling. |
| stubuika | MPL-2.0 (same license as Lunaria) | `.apkm` support in `src/apk.c`. luna-browser's TLS check skipping made opt-in only (commit `cfed70d`). A fix for macOS CLI feature macros. Installer SHA-256 checks. |
| droidloom | GPL-3.0 | **Ideas only, no code.** Wayland integration: one window per app, text-input-v3, clipboard, DMA-BUF zero-copy. Its namespaces/Binder core is container-like, so it is out of scope. |

Also worth watching (not added):
- Android Translation Layer on GitLab (GPL-3.0). Its `art_standalone` is Apache-2.0 and is the option if we ever replace the dvm with real ART.
- Winedroid (GPL-3.0).
- Berberis/Digitalis (Apache-2.0), a possible alternative to dynarmic for ARM64.

## Known issue
Upstream Lunaria `master` (branch `upstream-lunaria` here) does not build, because four headers under `src/dvm` were never committed. This needs fixing first.
