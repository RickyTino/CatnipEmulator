# legacy test assets

Obsolete, kept only for reference - nothing here is loaded by the tests.

## Hand-converted hex dumps

The tests used to run from hand-converted hex dumps of the test programs. They
now load the ELF build output directly (`NSCSCC_*_ELF` in `tb/nscscc_tests.h`),
so these dumps are no longer needed:

- `perf_ram.txt` - converted from `tb/soft/perf_func/...` (identical content to
  the ELF anyway)
- `tlb_ram.txt` - converted from `tlb_test/` below
- `func_ram.txt` - deleted (it was from an older revision of the func test: its
  reserved-instruction case is `6f76e210`, while the release's
  `tb/soft/func/obj/main.elf` and its `inst/n76_ri_ex.S` both use `5a8d78ce`)

## Disassembly listings

- `func_test.s` / `perf_test.s` - disassembly listings of the func and perf
  test programs, kept as reading material.  They are redundant copies of the
  release's `soft/func/obj/test.s` and
  `soft/perf_func/obj/allbench/test.s`, and do match the ELFs in `tb/soft/`;
  only the top-level convenience copies are here.

## tlb_test/

An alternative TLB test we wrote ourselves (own `test.S`, linker script and
`convert.py`). It is not the one the harness runs: `NSCSCC_TLB_ELF` points at
the official `tb/soft/tlb_func/obj/main.elf` from the NSCSCC2019 release. It
also terminates differently - it writes 0xFF to the virtual UART, whereas
`tlb_func` reports through LED_RG0/LED_RG1; `run_tlb()` still accepts both.

## Authoritative sources

- Test programs: the NSCSCC2019 release, copied byte-for-byte into `tb/soft/`.
- Expected trace for the functional test: `tb/soft/func/golden_trace.txt` (the
  release ships it under `func_test_v0.01/cpu132_gettrace/`, not in `soft/`).
