"""Opt-in real libc++ container execution and allocation failure controls."""

import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import (
    EUSER_808,
    ROM_808,
    locate,
    validate_manifest,
)
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk
from symbian.sdk import build_import_proxy, inspect_proxy
from symbian.status import StatusException

WORKSPACE = os.environ.get("SYMBIAN_RUNTIME_WORKSPACE")
pytestmark = pytest.mark.skipif(
    not WORKSPACE, reason="Set SYMBIAN_RUNTIME_WORKSPACE to preserved inputs"
)


@pytest.fixture(scope="module")
def artifacts(tmp_path_factory):
    root = Path(WORKSPACE).resolve()
    output = tmp_path_factory.mktemp("runtime-builds")
    selected = os.environ.get("SYMBIAN_APP_SDK")
    if selected:
        sdk = AppSdk.load(Path(selected))
        native_proxy = str(sdk.prefix / "proxies/euser/euser.dso")
        exports = {
            item["symbol"]
            for item in inspect_proxy(Path(native_proxy))["exports"]
        }
        assert {
            "_ZN10RAllocator4OpenEv",
            "_ZN10RAllocator5CloseEv",
            "_ZN4User9AllocatorEv",
            "_ZN9RFastLock11CreateLocalE10TOwnerType",
            "_ZN9RFastLock4WaitEv",
            "_ZN9RFastLock4PollEv",
            "_ZN9RFastLock6SignalEv",
            "_ZN11RHandleBase5CloseEv",
            "__e32_memory_barrier",
            "__e32_atomic_add_ord32",
            "__e32_atomic_and_ord32",
            "__e32_atomic_load_acq32",
            "__e32_atomic_ior_ord32",
            "__e32_atomic_cas_ord32",
            "__e32_atomic_cas_ord8",
            "__e32_atomic_load_acq8",
            "__e32_atomic_store_ord32",
            "__e32_atomic_store_ord8",
            "__e32_atomic_swp_ord32",
            "__e32_atomic_swp_ord8",
            "_ZN7UserHal15PageSizeInBytesERi",
            "_ZN6RChunk11CreateLocalEii10TOwnerType",
            "_ZNK6RChunk4BaseEv",
            "_ZNK6RChunk4SizeEv",
        } <= exports
    else:
        native_proxy = build_import_proxy(
            root / "research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def",
            [
                "_ZN4User11InitProcessEv",
                "_ZN4User4ExitEi",
                "_ZN4User9InvariantEv",
                "_ZN8UserHeap15SetupThreadHeapEiR24SStdEpocThreadCreateInfo",
                "_ZN10RAllocator4OpenEv",
                "_ZN10RAllocator5CloseEv",
                "_ZN4User5AllocEi",
                "_ZN4User4FreeEPv",
                "_ZN4User9AllocatorEv",
                "_ZN4User15CountAllocCellsEv",
                "memcpy",
                "memmove",
                "memset",
                "_ZN9RFastLock11CreateLocalE10TOwnerType",
                "_ZN9RFastLock4WaitEv",
                "_ZN9RFastLock4PollEv",
                "_ZN9RFastLock6SignalEv",
                "_ZN11RHandleBase5CloseEv",
                "__e32_memory_barrier",
                "__e32_atomic_add_ord32",
                "__e32_atomic_and_ord32",
                "__e32_atomic_load_acq32",
                "__e32_atomic_ior_ord32",
                "__e32_atomic_cas_ord32",
                "__e32_atomic_cas_ord8",
                "__e32_atomic_load_acq8",
                "__e32_atomic_store_ord32",
                "__e32_atomic_store_ord8",
                "__e32_atomic_swp_ord32",
                "__e32_atomic_swp_ord8",
                "_ZN4User9TickCountEv",
                "_ZN4User10NTickCountEv",
                "_ZN4User11FastCounterEv",
                "_ZN4User5AfterE27TTimeIntervalMicroSeconds32",
                "_ZN4User14WaitForRequestER14TRequestStatus",
                "_ZN4User17WaitForAnyRequestEv",
                "_ZN6RTimer11CreateLocalEv",
                "_ZN6RTimer6CancelEv",
                "_ZN6RTimer7HighResER14TRequestStatus27TTimeIntervalMicroSeconds32",
                "_ZN7RThread4OpenE9TThreadId10TOwnerType",
                "_ZNK7RThread2IdEv",
                "_ZNK7RThread13RequestSignalEv",
                "_ZN7UserHal10TickPeriodER27TTimeIntervalMicroSeconds32",
                "_ZN7UserSvr11HalFunctionEiiPvS0_",
            ],
            "euser.dll",
            output / "euser-native-primitives",
            compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
            linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
        )["artifact"]
    thread_symbols = {
        "_ZN4User5AfterE27TTimeIntervalMicroSeconds32",
        "_ZN4User14WaitForRequestER14TRequestStatus",
        "_ZN7RThread6CreateERK7TDesC16PFiPvEiiiS3_10TOwnerType",
        "_ZNK7RThread5LogonER14TRequestStatus",
        "_ZNK7RThread6ResumeEv",
        "_ZNK7RThread10ExitReasonEv",
        "_ZNK7RThread8ExitTypeEv",
    }
    native_symbols = {
        item["symbol"] for item in inspect_proxy(Path(native_proxy))["exports"]
    }
    if thread_symbols <= native_symbols:
        thread_proxy = native_proxy
    else:
        thread_proxy = build_import_proxy(
            root / "research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def",
            sorted(native_symbols | thread_symbols),
            "euser.dll",
            output / "euser-thread-primitives",
            compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
            linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
        )["artifact"]
    atomic64_proxy = thread_proxy
    native_atomic64_symbols = {
        "__e32_atomic_add_ord64",
        "__e32_atomic_cas_ord64",
        "__e32_atomic_load_acq64",
        "__e32_atomic_swp_ord64",
    }
    native_atomic64_proxy = build_import_proxy(
        root / "research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def",
        sorted(native_symbols | thread_symbols | native_atomic64_symbols),
        "euser.dll",
        output / "euser-native-atomic64-diagnostic",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    chunk_symbols = {
        "_ZN7UserHal15PageSizeInBytesERi",
        "_ZN6RChunk11CreateLocalEii10TOwnerType",
        "_ZNK6RChunk4BaseEv",
        "_ZNK6RChunk4SizeEv",
        "_ZNK6RChunk7MaxSizeEv",
        "_ZNK6RChunk6AdjustEi",
    }
    if chunk_symbols <= native_symbols:
        chunk_proxy = native_proxy
    else:
        chunk_proxy = build_import_proxy(
            root / "research/upstream/kernelhwsrv/kernel/eka/eabi/euseru.def",
            sorted(native_symbols | chunk_symbols),
            "euser.dll",
            output / "euser-chunk-primitives",
            compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
            linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
        )["artifact"]
    libpthread_proxy = build_import_proxy(
        root
        / "research/upstream/ossrv/genericopenlibs/openenvcore/libpthread"
        / "eabi/libpthreadu.def",
        [
            "pthread_create",
            "pthread_join",
            "pthread_detach",
            "pthread_self",
            "pthread_key_create",
            "pthread_key_delete",
            "pthread_setspecific",
            "pthread_getspecific",
            "pthread_mutex_init",
            "pthread_mutex_lock",
            "pthread_mutex_unlock",
            "pthread_mutex_trylock",
            "pthread_mutex_destroy",
            "pthread_cond_broadcast",
            "pthread_cond_init",
            "pthread_cond_signal",
            "pthread_cond_wait",
            "pthread_cond_timedwait",
            "pthread_cond_destroy",
            "pthread_once",
        ],
        "libpthread.dll",
        output / "libpthread-primitives",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    cxxabi_proxy = build_import_proxy(
        root
        / "research/upstream/kernelhwsrv/kernel/eka/compsupp/eabi"
        / "drtaeabiu.def",
        [
            "__cxa_guard_acquire",
            "__cxa_guard_release",
            "__cxa_pure_virtual",
            "_ZSt9terminatev",
        ],
        "drtaeabi.dll",
        output / "drtaeabi-primitives",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    if selected:
        libpthread_proxy = str(sdk.prefix / "proxies/libpthread/libpthread.dso")
        cxxabi_proxy = str(sdk.prefix / "proxies/drtaeabi/drtaeabi.dso")
        assert Path(libpthread_proxy).is_file()
        assert Path(cxxabi_proxy).is_file()
    exception_proxy = build_import_proxy(
        root
        / "research/upstream/kernelhwsrv/kernel/eka/compsupp/eabi"
        / "drtaeabiu.def",
        [
            "__cxa_allocate_exception",
            "__cxa_begin_catch",
            "__cxa_end_catch",
            "__cxa_end_cleanup",
            "__cxa_throw",
            "__aeabi_unwind_cpp_pr1",
            "_ZTIi",
        ],
        "drtaeabi.dll",
        output / "drtaeabi-exception-metadata",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    libc_proxy = build_import_proxy(
        root
        / "research/upstream/ossrv/genericopenlibs/openenvcore/libc/eabi"
        / "libcu.def",
        [
            "vsnprintf",
            "strcpy",
            "strcmp",
            "strtol",
            "strtoull",
            "sysconf",
            "nanosleep",
            "strerror",
            "strerror_r",
            "snprintf",
            "__errno",
            "abort",
            "clock_gettime",
            "sched_yield",
        ],
        "libc.dll",
        output / "libc-varargs",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    clock_proxy = (
        str(sdk.prefix / "proxies/libc/libc.dso") if selected else libc_proxy
    )
    if selected:
        assert Path(clock_proxy).is_file()
    libm_proxy = build_import_proxy(
        root
        / "research/upstream/ossrv/genericopenlibs/openenvcore/libm/eabi"
        / "libmu.def",
        ["ceilf", "ldexp", "scalbnf"],
        "libm.dll",
        output / "libm-hash-table",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    if selected:
        libm_proxy = str(sdk.prefix / "proxies/libm/libm.dso")
        assert Path(libm_proxy).is_file()
    locale_libc_proxy = build_import_proxy(
        root
        / "research/upstream/ossrv/genericopenlibs/openenvcore/libc/eabi"
        / "libcu.def",
        [
            "__errno",
            "abort",
            "asprintf",
            "btowc",
            "free",
            "iswalpha",
            "iswblank",
            "iswcntrl",
            "iswdigit",
            "iswlower",
            "iswprint",
            "iswpunct",
            "iswspace",
            "iswupper",
            "iswxdigit",
            "localeconv",
            "malloc",
            "mbrlen",
            "mbrtowc",
            "mbsnrtowcs",
            "mbsrtowcs",
            "mbtowc",
            "memchr",
            "realloc",
            "snprintf",
            "sscanf",
            "strcmp",
            "strcoll",
            "strerror_r",
            "strftime",
            "strtod",
            "strtof",
            "strtold",
            "strxfrm",
            "tolower",
            "towlower",
            "towupper",
            "toupper",
            "wcscoll",
            "wcrtomb",
            "wcslen",
            "wcsnrtombs",
            "wcsxfrm",
            "wctob",
            "wmemchr",
        ],
        "libc.dll",
        output / "libc-locale",
        compiler=os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
        linker=os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
    )["artifact"]
    if selected:
        locale_libc_proxy = str(sdk.prefix / "proxies/libc/libc.dso")
    result = {}

    def build_artifact(architecture, mode):
        if (architecture, mode) not in result:
            project = output / (architecture + "-" + mode)
            shutil.copytree(root / "probes/runtime_probe", project)
            if selected:
                for source_file in project.glob("*.cc"):
                    source_file.write_text(
                        source_file.read_text().replace(
                            '#include "abi.h"',
                            '#include "symbian/runtime.h"',
                        )
                    )
                cmake = project / "CMakeLists.txt"
                source = cmake.read_text()
                adapted = source.replace(
                    'add_subdirectory("${root}/cpp/symbian/runtime" "runtime")',
                    f'include("{sdk.prefix}/cmake/SymbianApp.cmake")',
                ).replace(
                    "target_link_libraries(runtime_probe PRIVATE "
                    "symbian_guest_runtime)",
                    "target_link_libraries(runtime_probe PRIVATE "
                    "Symbian::Runtime)",
                )
                if mode in (
                    "std-thread",
                    "changed-std-thread",
                    "thread-error",
                    "tls",
                    "changed-tls",
                    "clock-thread",
                    "changed-clock-thread",
                    "a11-stackless",
                    "changed-a11-stackless",
                    "timer-future",
                    "changed-timer-future",
                    "fiber-locks",
                    "changed-fiber-locks",
                ):
                    adapted = adapted.replace(
                        "target_link_libraries(runtime_probe PRIVATE "
                        "Symbian::Runtime)",
                        "target_link_libraries(runtime_probe PRIVATE "
                        "Symbian::Threads)",
                    )
                if mode in (
                    "a11-stackless",
                    "changed-a11-stackless",
                    "fiber-locks",
                    "changed-fiber-locks",
                ):
                    adapted = (
                        adapted.replace(
                            '"${root}/cpp/symbian/concurrency/guest"',
                            '"${SYMBIAN_SDK_PREFIX}/include"',
                        )
                        .replace(
                            '"${root}/cpp/symbian/concurrency/common"',
                            '"${SYMBIAN_SDK_PREFIX}/include"',
                        )
                        .replace("Symbian::Threads)", "Symbian::Stackless)")
                    )
                if mode in ("native-timer", "changed-native-timer"):
                    adapted = adapted.replace(
                        '"${root}/cpp/symbian/concurrency/guest"',
                        '"${SYMBIAN_SDK_PREFIX}/include"',
                    ).replace(
                        '"${root}/cpp/symbian/concurrency/common"',
                        '"${SYMBIAN_SDK_PREFIX}/include"',
                    )
                if mode in ("timer-future", "changed-timer-future"):
                    adapted = (
                        adapted.replace(
                            '"${root}/cpp/symbian/concurrency/guest"',
                            '"${SYMBIAN_SDK_PREFIX}/include"',
                        )
                        .replace(
                            '"${root}/cpp/symbian/concurrency/common"',
                            '"${SYMBIAN_SDK_PREFIX}/include"',
                        )
                        .replace("Symbian::Threads)", "Symbian::Stackless)")
                    )
                if mode in ("locale-stream", "changed-locale-stream"):
                    adapted = adapted.replace(
                        "Symbian::Runtime)", "Symbian::Streams)"
                    )
                if mode in ("atomic64-native", "changed-atomic64-native"):
                    adapted = adapted.replace(
                        "Symbian::Runtime)", "Symbian::NativeAtomics64)"
                    )
                assert adapted != source and "add_subdirectory" not in adapted
                cmake.write_text(adapted)
            if mode in (
                "varargs",
                "changed-varargs",
                "system-error",
                "changed-system-error",
            ):
                cmake = project / "CMakeLists.txt"
                cmake.write_text(
                    cmake.read_text()
                    + "\ntarget_link_libraries(runtime_probe PRIVATE "
                    + f'"{libc_proxy}")\n'
                )
            if mode in ("hash-table", "changed-hash-table"):
                cmake = project / "CMakeLists.txt"
                math_library = f'"{libm_proxy}" ' if not selected else ""
                cmake.write_text(
                    cmake.read_text()
                    + "\ntarget_link_libraries(runtime_probe PRIVATE "
                    + math_library
                    + f'"{clock_proxy}")\n'
                )
            if mode in ("clock", "changed-clock") and not selected:
                cmake = project / "CMakeLists.txt"
                cmake.write_text(
                    cmake.read_text()
                    + "\ntarget_link_libraries(runtime_probe PRIVATE "
                    + f'"{clock_proxy}")\n'
                )
            if mode in ("system-error", "changed-system-error"):
                cmake = project / "CMakeLists.txt"
                cmake.write_text(
                    cmake.read_text()
                    + "\ntarget_link_libraries(runtime_probe PRIVATE "
                    + f'"{cxxabi_proxy}")\n'
                )
            if (
                mode in ("locale-stream", "changed-locale-stream")
                and not selected
            ):
                cmake = project / "CMakeLists.txt"
                cmake.write_text(
                    cmake.read_text()
                    + "\ntarget_link_libraries(runtime_probe PRIVATE "
                    + f'"{locale_libc_proxy}" "{libpthread_proxy}" '
                    + f'"{cxxabi_proxy}")\n'
                )
            if mode in ("thread-atomic", "changed-thread-atomic"):
                selected_euser_proxy = thread_proxy
            elif mode in (
                "atomic64",
                "changed-atomic64",
            ):
                selected_euser_proxy = atomic64_proxy
            elif mode in (
                "native-atomic64",
                "atomic64-native",
                "changed-atomic64-native",
            ):
                selected_euser_proxy = (
                    str(sdk.prefix / "proxies/euser-native64/euser.dso")
                    if selected
                    else native_atomic64_proxy
                )
            elif mode in ("chunk", "changed-chunk"):
                selected_euser_proxy = chunk_proxy
            elif mode in (
                "fast-lock",
                "atomic",
                "changed-atomic",
                "ownership",
                "changed-ownership",
                "std-thread",
                "changed-std-thread",
                "thread-error",
                "tls",
                "changed-tls",
                "clock-thread",
                "changed-clock-thread",
                "a11-stackless",
                "changed-a11-stackless",
                "native-timer",
                "changed-native-timer",
                "timer-future",
                "changed-timer-future",
                "fiber-context",
                "changed-fiber-context",
                "fiber-locks",
                "changed-fiber-locks",
                "varargs",
                "changed-varargs",
                "system-error",
                "changed-system-error",
                "clock",
                "changed-clock",
                "fast-counter",
                "changed-fast-counter",
                "locale-stream",
                "changed-locale-stream",
                "exception-metadata",
                "changed-exception-metadata",
                "exception-throw",
            ):
                selected_euser_proxy = native_proxy
            else:
                selected_euser_proxy = str(
                    root / ".symbian/runtime-sdk/euser/euser.dso"
                )
            manifest = project / "symbian.toml"
            manifest.write_text(
                manifest.read_text().replace(
                    "../../.symbian/runtime-sdk/euser/euser.dso",
                    selected_euser_proxy,
                )
            )
            if mode in (
                "std-thread",
                "changed-std-thread",
                "thread-error",
                "tls",
                "changed-tls",
                "clock-thread",
                "changed-clock-thread",
                "a11-stackless",
                "changed-a11-stackless",
                "timer-future",
                "changed-timer-future",
            ):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{selected_euser_proxy}"]',
                        f'"{selected_euser_proxy}", "{libpthread_proxy}", '
                        f'"{cxxabi_proxy}"]',
                    )
                )
            if selected and mode in (
                "a11-stackless",
                "changed-a11-stackless",
                "timer-future",
                "changed-timer-future",
            ):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{cxxabi_proxy}"]',
                        f'"{cxxabi_proxy}", "{libm_proxy}", '
                        f'"{locale_libc_proxy}"]',
                    )
                )
            if mode in ("system-error", "changed-system-error"):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{native_proxy}"]',
                        f'"{native_proxy}", "{libc_proxy}", '
                        f'"{cxxabi_proxy}"]',
                    )
                )
            if mode in ("locale-stream", "changed-locale-stream"):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{native_proxy}"]',
                        f'"{native_proxy}", "{locale_libc_proxy}", '
                        f'"{libpthread_proxy}", "{cxxabi_proxy}"]',
                    )
                )
            if mode in (
                "exception-metadata",
                "changed-exception-metadata",
                "exception-throw",
            ):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{native_proxy}"]',
                        f'"{native_proxy}", "{exception_proxy}"]',
                    )
                )
            if mode in ("varargs", "changed-varargs"):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{native_proxy}"]',
                        f'"{native_proxy}", "{libc_proxy}"]',
                    )
                )
            if mode in ("hash-table", "changed-hash-table"):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{selected_euser_proxy}"]',
                        f'"{selected_euser_proxy}", "{libm_proxy}", '
                        f'"{clock_proxy}"]',
                    )
                )
            if mode in ("clock", "changed-clock"):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{selected_euser_proxy}"]',
                        f'"{selected_euser_proxy}", "{clock_proxy}"]',
                    )
                )
            if mode in ("clock-thread", "changed-clock-thread"):
                manifest.write_text(
                    manifest.read_text().replace(
                        f'"{cxxabi_proxy}"]',
                        f'"{cxxabi_proxy}", "{clock_proxy}"]',
                    )
                )
            presets = json.loads((project / "CMakePresets.json").read_text())
            variables = presets["configurePresets"][0]["cacheVariables"]
            variables["SYMBIAN_PLATFORM_ROOT"] = str(root)
            variables["SYMBIAN_RUNTIME_FAIL_ALLOCATION"] = (
                "ON" if mode == "failure" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_GLOBAL_NOTHROW"] = (
                "ON" if mode in ("global-nothrow", "changed-got") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_GOT_VALUE"] = (
                "ON" if mode == "changed-got" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_DATA"] = (
                "ON" if mode in ("data", "changed-data") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_DATA"] = (
                "ON" if mode == "changed-data" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_GLOBAL_LIFETIME"] = (
                "ON" if mode == "global-lifetime" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_WIDE"] = (
                "ON" if mode == "changed-wide" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_FAST_LOCK"] = (
                "ON" if mode == "fast-lock" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHUNK"] = (
                "ON" if mode in ("chunk", "changed-chunk") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_TLS"] = (
                "ON" if mode in ("tls", "changed-tls") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_TLS"] = (
                "ON" if mode == "changed-tls" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_CHUNK"] = (
                "ON" if mode == "changed-chunk" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_ATOMIC"] = (
                "ON" if mode in ("atomic", "changed-atomic") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_ATOMIC"] = (
                "ON" if mode == "changed-atomic" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_ATOMIC64"] = (
                "ON"
                if mode
                in (
                    "atomic64",
                    "changed-atomic64",
                    "atomic64-native",
                    "changed-atomic64-native",
                )
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_COMPILER_RT"] = (
                "ON"
                if mode in ("compiler-rt", "changed-compiler-rt")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_COMPILER_RT"] = (
                "ON" if mode == "changed-compiler-rt" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_HASH_TABLE"] = (
                "ON" if mode in ("hash-table", "changed-hash-table") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_HASH_TABLE"] = (
                "ON" if mode == "changed-hash-table" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CLOCK"] = (
                "ON" if mode in ("clock", "changed-clock") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_CLOCK"] = (
                "ON" if mode == "changed-clock" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_FAST_COUNTER"] = (
                "ON"
                if mode in ("fast-counter", "changed-fast-counter")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_FAST_COUNTER"] = (
                "ON" if mode == "changed-fast-counter" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CLOCK_THREAD"] = (
                "ON"
                if mode in ("clock-thread", "changed-clock-thread")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_CLOCK_THREAD"] = (
                "ON" if mode == "changed-clock-thread" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_THREAD_ERROR"] = (
                "ON" if mode == "thread-error" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_ATOMIC64"] = (
                "ON"
                if mode in ("changed-atomic64", "changed-atomic64-native")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_DIRECT_ATOMIC64_DIAGNOSTIC"] = (
                "ON" if mode == "native-atomic64" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_NATIVE_ATOMIC64"] = (
                "ON"
                if mode in ("atomic64-native", "changed-atomic64-native")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_THREAD_ATOMIC"] = (
                "ON"
                if mode in ("thread-atomic", "changed-thread-atomic")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_THREAD_ATOMIC"] = (
                "ON" if mode == "changed-thread-atomic" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_OWNERSHIP"] = (
                "ON" if mode in ("ownership", "changed-ownership") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_OWNERSHIP"] = (
                "ON" if mode == "changed-ownership" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_STD_THREAD"] = (
                "ON" if mode in ("std-thread", "changed-std-thread") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_STD_THREAD"] = (
                "ON" if mode == "changed-std-thread" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_A11_STACKLESS"] = (
                "ON"
                if mode in ("a11-stackless", "changed-a11-stackless")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_A11_STACKLESS"] = (
                "ON" if mode == "changed-a11-stackless" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_NATIVE_TIMER"] = (
                "ON"
                if mode in ("native-timer", "changed-native-timer")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_NATIVE_TIMER"] = (
                "ON" if mode == "changed-native-timer" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_TIMER_FUTURE"] = (
                "ON"
                if mode in ("timer-future", "changed-timer-future")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_TIMER_FUTURE"] = (
                "ON" if mode == "changed-timer-future" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_FIBER_CONTEXT"] = (
                "ON"
                if mode in ("fiber-context", "changed-fiber-context")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_FIBER_CONTEXT"] = (
                "ON" if mode == "changed-fiber-context" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_FIBER_LOCKS"] = (
                "ON"
                if mode in ("fiber-locks", "changed-fiber-locks")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_FIBER_LOCKS"] = (
                "ON" if mode == "changed-fiber-locks" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_VARARGS"] = (
                "ON" if mode in ("varargs", "changed-varargs") else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_VARARGS"] = (
                "ON" if mode == "changed-varargs" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_SYSTEM_ERROR"] = (
                "ON"
                if mode in ("system-error", "changed-system-error")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_SYSTEM_ERROR"] = (
                "ON" if mode == "changed-system-error" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_LOCALE_STREAM"] = (
                "ON"
                if mode in ("locale-stream", "changed-locale-stream")
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_LOCALE_STREAM"] = (
                "ON" if mode == "changed-locale-stream" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_IMPORT_POINTER"] = (
                "ON" if mode == "import-pointer" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_LONG_THUNK"] = (
                "ON" if mode == "long-thunk" else "OFF"
            )
            variables["SYMBIAN_RUNTIME_EXCEPTIONS"] = (
                "ON"
                if mode
                in (
                    "exception-metadata",
                    "changed-exception-metadata",
                    "exception-throw",
                )
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_EXCEPTION_METADATA_ONLY"] = (
                "ON"
                if mode
                in (
                    "exception-metadata",
                    "changed-exception-metadata",
                )
                else "OFF"
            )
            variables["SYMBIAN_RUNTIME_CHANGED_EXCEPTION_METADATA"] = (
                "ON" if mode == "changed-exception-metadata" else "OFF"
            )
            if mode in (
                "atomic64",
                "changed-atomic64",
                "native-atomic64",
                "atomic64-native",
                "changed-atomic64-native",
            ):
                variables["SYMBIAN_IMPORT_PROXIES"] = selected_euser_proxy
            elif mode in ("hash-table", "changed-hash-table"):
                variables["SYMBIAN_IMPORT_PROXIES"] = (
                    f"{selected_euser_proxy};{libm_proxy};{clock_proxy}"
                )
            elif mode in ("clock", "changed-clock"):
                variables["SYMBIAN_IMPORT_PROXIES"] = (
                    f"{selected_euser_proxy};{clock_proxy}"
                )
            elif mode in ("clock-thread", "changed-clock-thread"):
                variables["SYMBIAN_IMPORT_PROXIES"] = (
                    f"{selected_euser_proxy};{libpthread_proxy};"
                    f"{cxxabi_proxy};{clock_proxy}"
                )
            elif mode in ("thread-atomic", "changed-thread-atomic"):
                variables["SYMBIAN_IMPORT_PROXIES"] = thread_proxy
            elif mode in ("chunk", "changed-chunk"):
                variables["SYMBIAN_IMPORT_PROXIES"] = chunk_proxy
            elif mode in (
                "fast-lock",
                "atomic",
                "changed-atomic",
                "ownership",
                "changed-ownership",
                "std-thread",
                "changed-std-thread",
                "thread-error",
                "tls",
                "changed-tls",
                "clock-thread",
                "changed-clock-thread",
                "a11-stackless",
                "changed-a11-stackless",
                "native-timer",
                "changed-native-timer",
                "timer-future",
                "changed-timer-future",
                "fiber-context",
                "changed-fiber-context",
                "fiber-locks",
                "changed-fiber-locks",
                "exception-metadata",
                "changed-exception-metadata",
                "exception-throw",
                "locale-stream",
                "changed-locale-stream",
            ):
                variables["SYMBIAN_IMPORT_PROXIES"] = native_proxy
            if mode in (
                "std-thread",
                "changed-std-thread",
                "tls",
                "changed-tls",
                "a11-stackless",
                "changed-a11-stackless",
                "timer-future",
                "changed-timer-future",
                "fiber-locks",
                "changed-fiber-locks",
            ):
                variables["SYMBIAN_IMPORT_PROXIES"] = (
                    f"{selected_euser_proxy};"
                    f"{libpthread_proxy};{cxxabi_proxy}"
                )
            if selected and mode in (
                "a11-stackless",
                "changed-a11-stackless",
                "timer-future",
                "changed-timer-future",
                "fiber-locks",
                "changed-fiber-locks",
            ):
                variables[
                    "SYMBIAN_IMPORT_PROXIES"
                ] += f";{libm_proxy};{locale_libc_proxy}"
            if mode in ("fiber-locks", "changed-fiber-locks"):
                selected_proxies = variables["SYMBIAN_IMPORT_PROXIES"].split(
                    ";"
                )
                manifest.write_text(
                    manifest.read_text().replace(
                        f'import_proxies = ["{selected_euser_proxy}"]',
                        "import_proxies = " + json.dumps(selected_proxies),
                    )
                )
            if mode in (
                "exception-metadata",
                "changed-exception-metadata",
                "exception-throw",
            ):
                variables["SYMBIAN_IMPORT_PROXIES"] = (
                    f"{native_proxy};{exception_proxy}"
                )
            if mode in ("locale-stream", "changed-locale-stream"):
                variables["SYMBIAN_IMPORT_PROXIES"] = (
                    f"{native_proxy};{locale_libc_proxy};"
                    f"{libpthread_proxy};{cxxabi_proxy}"
                )
            (project / "CMakePresets.json").write_text(json.dumps(presets))
            arguments = (
                project,
                output / (architecture + "-" + mode + "-artifacts"),
                os.environ.get("SYMBIAN_RUNTIME_COMPILER", "clang++"),
                os.environ.get("SYMBIAN_RUNTIME_LINKER", "ld.lld"),
            )
            result[architecture, mode] = toolchain.build(
                *arguments, architecture=architecture
            )
        return result[architecture, mode]

    class LazyArtifacts:
        def __getitem__(self, key):
            return build_artifact(*key)

    return LazyArtifacts()


def test_guest_typed_throw_requires_imported_typeinfo(artifacts):
    with pytest.raises(StatusException, match="R_ARM_GLOB_DAT imported object"):
        artifacts["armv6", "exception-throw"]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize("backend", ["dyncom", "dynarmic"])
@pytest.mark.parametrize(
    "mode,reason",
    [
        ("success", 0),
        ("failure", -4),
        ("global-nothrow", 0),
        ("changed-got", -113),
        ("data", 0),
        ("changed-data", -115),
        ("global-lifetime", 0),
        ("changed-wide", -126),
        ("fast-lock", 0),
        ("chunk", 0),
        ("changed-chunk", -205),
        ("tls", 0),
        ("changed-tls", -276),
        ("atomic", 0),
        ("changed-atomic", -136),
        ("atomic64", 0),
        ("changed-atomic64", -212),
        ("native-atomic64", 0),
        ("atomic64-native", 0),
        ("changed-atomic64-native", -212),
        ("compiler-rt", 0),
        ("changed-compiler-rt", -222),
        ("hash-table", 0),
        ("changed-hash-table", -231),
        ("clock", 0),
        ("changed-clock", -236),
        ("fast-counter", 0),
        ("changed-fast-counter", -239),
        ("clock-thread", 0),
        ("changed-clock-thread", -240),
        ("thread-error", -6),
        ("thread-atomic", 0),
        ("changed-thread-atomic", -140),
        ("ownership", 0),
        ("changed-ownership", -150),
        ("std-thread", 0),
        ("changed-std-thread", -154),
        ("a11-stackless", 0),
        ("changed-a11-stackless", -166),
        ("native-timer", 0),
        ("changed-native-timer", -256),
        ("timer-future", 0),
        ("changed-timer-future", -265),
        ("fiber-context", 0),
        ("changed-fiber-context", -302),
        ("fiber-locks", 0),
        ("changed-fiber-locks", -320),
        ("varargs", 0),
        ("changed-varargs", -183),
        ("system-error", 0),
        ("changed-system-error", -189),
        ("locale-stream", 0),
        ("changed-locale-stream", -194),
        ("import-pointer", 0),
        ("long-thunk", 0),
        ("exception-metadata", 0),
        ("changed-exception-metadata", -153),
    ],
)
def test_real_containers_cleanup_and_allocation_failure(
    tmp_path, artifacts, backend, mode, reason, architecture
):
    if not os.environ.get("SYMBIAN_APP_SDK") and mode in (
        "a11-stackless",
        "changed-a11-stackless",
        "timer-future",
        "changed-timer-future",
        "fiber-locks",
        "changed-fiber-locks",
    ):
        pytest.skip("StatusOr concurrency requires the installed Abseil SDK")
    root = Path(WORKSPACE).resolve()
    report = artifacts[architecture, mode]
    assert report["target"]["architecture"] == architecture
    if mode in ("exception-metadata", "changed-exception-metadata"):
        from symbian.e32 import inspect_image

        image = Path(report["artifact"])
        assert inspect_image(image)["exception_descriptor_offset"] > 0
    if mode == "data":
        objdump = (
            Path(os.environ["SYMBIAN_RUNTIME_COMPILER"]).parent / "llvm-objdump"
        )
        disassembly = subprocess.check_output(
            [
                str(objdump),
                "--disassemble-symbols=RuntimeReverseBytes",
                report["linked_elf"],
            ],
            text=True,
        )
        (tmp_path / "reverse-bytes.asm").write_text(disassembly)
        import re

        assert bool(re.search(r"\brev\b", disassembly)) == (
            architecture == "armv6"
        )
    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    target = instance / "data/drives/rm-807/c/sys/bin/runtime_probe.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(artifacts[architecture, mode]["artifact"], target)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(prefix="runtime-", dir="/tmp") as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\runtime_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                assert exits == [
                    {
                        "uid": 0xE0000813,
                        "name": "runtime_probe[e0000813]0001",
                        "type": 0,
                        "reason": reason,
                    }
                ]
            finally:
                _stop(process)
                assert {path: _digest(path) for path in pinned} == pinned


@pytest.mark.skipif(
    os.environ.get("SYMBIAN_RUNTIME_OTHER_FIRMWARE") != "1",
    reason="Set SYMBIAN_RUNTIME_OTHER_FIRMWARE=1 for imported EKA2 ROMs",
)
@pytest.mark.parametrize("reference", ["c7", "e6"])
@pytest.mark.parametrize("backend", ["dyncom", "dynarmic"])
def test_native_atomic64_on_other_eka2_roms(
    tmp_path, artifacts, reference, backend
):
    """Runs the same cross-thread atomic contract on imported EKA2 ROMs."""
    root = Path(WORKSPACE).resolve()
    store = Path.home() / ".local/share/symbian/firmware"
    source = locate(store, reference)
    manifest = validate_manifest(source)
    assert manifest.device.kernel == "eka2"
    instance = tmp_path / "instance"
    shutil.copytree(source / "instance", instance)
    executable = Path(artifacts["armv6", "atomic64-native"]["artifact"])
    target = instance / manifest.device.c_drive / "sys/bin/runtime_probe.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(executable, target)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    frontend = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="runtime-other-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(frontend, Path(private)),
                    "--device",
                    manifest.device.firmware_code,
                    "--run",
                    "C:\\sys\\bin\\runtime_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                assert len(exits) == 1
                assert exits[0]["reason"] == 0
            finally:
                _stop(process)
    validate_manifest(source)


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_ROM_ATOMIC_ORACLE"),
    reason="Set SYMBIAN_ROM_ATOMIC_ORACLE to the built EKA2L1 ROM parser",
)
def test_other_rom_atomic_exports_and_exclusive_instructions():
    """Distinguishes supported exclusive ROM paths from missing ordinals."""
    store = Path.home() / ".local/share/symbian/firmware"
    images = {}
    for reference in ("nokia808", "c7", "e6", "6120", "e71"):
        source = locate(store, reference)
        manifest = validate_manifest(source)
        images[reference] = (
            source / "instance" / manifest.device.z_drive / "sys/bin/euser.dll"
        )
    output = subprocess.run(
        [os.environ["SYMBIAN_ROM_ATOMIC_ORACLE"], *map(str, images.values())],
        text=True,
        capture_output=True,
        check=True,
    ).stdout
    entries = {}
    current = None
    for line in output.splitlines():
        if " exports=" in line:
            path, count = line.split(" exports=", 1)
            current = next(
                key for key, image in images.items() if str(image) == path
            )
            entries[current] = {"count": int(count.split()[0]), "bytes": []}
        elif " bytes=" in line:
            entries[current]["bytes"].append(
                bytes.fromhex(line.split("bytes=")[1])
            )
        elif " absent" in line:
            assert current in ("6120", "e71")
    assert {entries[name]["count"] for name in ("6120", "e71")} == {2228}
    expected = None
    for name in ("nokia808", "c7", "e6"):
        assert entries[name]["count"] >= 2373
        assert len(entries[name]["bytes"]) == 5
        digest = hashlib.sha256(b"".join(entries[name]["bytes"])).hexdigest()
        if expected is None:
            expected = digest
        assert digest == expected
    llvm_mc = Path(os.environ["SYMBIAN_RUNTIME_COMPILER"]).parent / "llvm-mc"
    for operation in entries["c7"]["bytes"][:2]:
        disassembly = subprocess.run(
            [
                str(llvm_mc),
                "--disassemble",
                "--hex",
                "--triple=armv6k-none-eabi",
            ],
            input=operation.hex(),
            text=True,
            capture_output=True,
            check=True,
        ).stdout
        assert "ldrexd" in disassembly and "strexd" in disassembly


def test_imported_function_pointer_rejects_data_object_relocation(artifacts):
    from symbian.e32 import convert_imported_executable

    report = artifacts["armv6", "import-pointer"]
    assert 0 in report["e32"]["data_relocations"]
    assert 0 not in report["e32"]["data_data_relocations"]
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    readelf = (
        Path(os.environ["SYMBIAN_RUNTIME_COMPILER"]).parent / "llvm-readelf"
    )
    sections = subprocess.check_output(
        [str(readelf), "-SW", report["linked_elf"]], text=True
    )
    match = re.search(r"\.rel\.dyn\s+REL\s+[0-9a-f]+\s+([0-9a-f]+)", sections)
    assert match is not None
    offset = int(match.group(1), 16)
    assert elf[offset + 4] == 2  # R_ARM_ABS32 in r_info's low byte.
    elf[offset + 4] = 21  # R_ARM_GLOB_DAT is an imported data object.
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith("/euser.dso")
    ]
    assert len(proxies) == 1
    with pytest.raises(StatusException, match="imported-function R_ARM_ABS32"):
        convert_imported_executable(bytes(elf), proxies, 0xE0000813)


def test_imported_function_pointer_rejects_wrong_plt_address(artifacts):
    from symbian.e32 import convert_imported_executable

    report = artifacts["armv6", "import-pointer"]
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    readelf = (
        Path(os.environ["SYMBIAN_RUNTIME_COMPILER"]).parent / "llvm-readelf"
    )
    sections = subprocess.check_output(
        [str(readelf), "-SW", report["linked_elf"]], text=True
    )
    table = re.search(r"\.dynsym\s+DYNSYM\s+[0-9a-f]+\s+([0-9a-f]+)", sections)
    assert table is not None
    symbols = subprocess.check_output(
        [str(readelf), "--dyn-syms", report["linked_elf"]], text=True
    )
    symbol = re.search(r"^\s*(\d+):.*\bmemmove@euser\.dll", symbols, re.M)
    assert symbol is not None
    value = int(table.group(1), 16) + int(symbol.group(1)) * 16 + 4
    elf[value : value + 4] = (0x1234).to_bytes(4, "little")
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith("/euser.dso")
    ]
    assert len(proxies) == 1
    with pytest.raises(StatusException, match="undefined global function"):
        convert_imported_executable(bytes(elf), proxies, 0xE0000813)


def test_generated_long_thunk_rejects_invalid_target(artifacts):
    from symbian.e32 import convert_imported_executable

    report = artifacts["armv6", "long-thunk"]
    elf = bytearray(Path(report["linked_elf"]).read_bytes())
    readelf = (
        Path(os.environ["SYMBIAN_RUNTIME_COMPILER"]).parent / "llvm-readelf"
    )
    symbols = subprocess.check_output(
        [str(readelf), "--symbols", report["linked_elf"]], text=True
    )
    thunk = re.search(
        r"^\s*\d+:\s+([0-9a-f]+)\s+8\s+FUNC\s+LOCAL\s+DEFAULT\s+"
        r"\d+\s+__ARMv5LongLdrPcThunk_RuntimeReverseBytes$",
        symbols,
        re.M,
    )
    assert thunk is not None
    sections = subprocess.check_output(
        [str(readelf), "-SW", report["linked_elf"]], text=True
    )
    text_section = re.search(
        r"\.text\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)", sections
    )
    assert text_section is not None
    assert (
        int(thunk.group(1), 16) + 4 - int(text_section.group(1), 16)
        in report["e32"]["code_relocations"]
    )
    location = (
        int(text_section.group(2), 16)
        + int(thunk.group(1), 16)
        - int(text_section.group(1), 16)
        + 4
    )
    elf[location : location + 4] = (0x1001).to_bytes(4, "little")
    proxies = [
        Path(path).read_bytes()
        for path in report["inputs"]
        if path.endswith("/euser.dso")
    ]
    assert len(proxies) == 1
    with pytest.raises(StatusException, match="interworking thunk target"):
        convert_imported_executable(bytes(elf), proxies, 0xE0000813)


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD for independent E32 validators",
)
@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize("mode", ["data", "changed-data"])
def test_original_validator_accepts_writable_storage(
    artifacts, tmp_path, mode, architecture
):
    from symbian._native import inspect_e32
    from symbian.toolchain.verification import run_oracles

    image = Path(artifacts[architecture, mode]["artifact"])
    info = inspect_e32(image.read_bytes())
    assert info.architecture == architecture
    assert info.data_size > 0
    # The probe's 64 words coexist with the runtime's destructor registry.
    assert info.bss_size >= 64 * 4
    assert info.code_data_relocations
    assert len(info.data_relocations) == 3
    assert len(info.data_data_relocations) == 2
    checks, _ = run_oracles(
        {"SYMBIAN_E32_TEST_IMAGE": image},
        (("symbian_checksum_oracle", 1), ("symbian_validator_oracle", 7)),
        Path(os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"]).resolve(),
        tmp_path / "checks",
    )
    assert all(check["passed"] for check in checks)
