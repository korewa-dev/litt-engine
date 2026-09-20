# Scripting and FFI contract

## Supported contract

The release-supported foreign-function boundary is `alt/src/native/litt_c.h`.
It is a C ABI with an explicit ABI version. Consumers should compare
`litt_abi_version()` with `LITT_ABI_VERSION` before depending on optional
entry points.

The scripting surface exposed through that ABI is the built-in Litt bytecode VM.
A VM returned by `litt_script_vm_create` is owned by the caller and must be
released exactly once with `litt_script_vm_destroy`. The pointer returned by
`litt_script_vm_last_error` is borrowed and is valid only until the next call
on that VM or until the VM is destroyed.

VM execution is bounded. Source bytes, executed instructions, and stack values
have configurable non-zero limits. Limit failures are reported as
`LITT_ERROR_LIMIT`; missing scripts, compile failures, invalid arguments, and
runtime failures have distinct result codes.

## Language support

C is the only release-supported external language contract today. C++, Python,
C#, and Lua code may exist in the repository as internal, legacy, examples, or
experiments, but none is advertised as a supported language binding until it has
an independent consumer build-and-run test in CI.

A wrapper source file alone is not evidence of support. New bindings must use
the versioned C ABI rather than reaching into C++ implementation types.

## Compatibility policy

ABI major changes are breaking. ABI minor changes may add functions, enums, or
capabilities while preserving existing declarations and ownership rules.
Callers own handles returned by create functions unless a declaration explicitly
says otherwise. Borrowed strings and pointers must never be freed by callers.

## Verification

The stabilization workflow builds the packaged SDK and compiles an external C11
consumer against only the installed header and shared library. That consumer
checks the ABI version and creates, compiles, executes, and destroys a scripting
VM. The C bridge contract tests additionally cover negative inputs, missing
scripts, source limits, instruction limits, and ownership lifecycle.
