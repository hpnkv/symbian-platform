# SDL2 and SDL3

The SDK carries pinned SDL2 2.30.11 and SDL3 3.2.22 source builds. Applications
can use the original C headers and functions, or the SDK's C++ `Session`,
`Window` and `Renderer` owners. The latter return `absl::StatusOr` on creation
and destroy their SDL resources automatically. Use `Symbian::PortableSdl2` or
`Symbian::PortableSdl3` for the software renderer; use
`Symbian::PortableSdl2Gpu` or `Symbian::PortableSdl3Gpu` for OpenGL ES 2 with
software fallback if EGL context creation fails. The GPU archives import
`libegl.dll` and `libglesv2.dll` at load time, so select a software archive
on firmware without those libraries. The guest graphics link check can
validate selected firmware.

| Area | Current implementation |
| --- | --- |
| Video | One Window Server window, RGB565 software framebuffer, OpenGL ES 2 renderer in GPU builds, display size and reported refresh rate. |
| Input | Pointer press, release and continuous motion, arrows, select/enter, escape/back, and focus notifications. |
| Time | SDL ticks and delay; SDK monotonic `FramePacer` for a refresh cap. |
| Audio, haptics | SDK `GameFeedback` offers MIDI playback and light system tactile feedback, with HWRM fallback and a bounded worker for applications. SDL's audio and haptic device backends are currently dummy. |
| Other SDL services | Upstream core APIs are compiled where they do not require a missing platform backend. Joystick, filesystem, sensor, power and dynamic-library backends currently use upstream dummy implementations. |

The SDL2 and SDL3 Bounce-style Arkanoid targets share game and assets, and
select the corresponding C/C++ adapter at build time. The game requests GPU
rendering by default, shows `GPU` only when SDL actually selected the GLES2
renderer, and falls back to software after context failure. Its `FPS` overlay
uses measured presentation count. The SDK display layer owns the native window,
pointer event stream, AppArc task name, refresh query and EGL context; the game
owns its fixed-step simulation and application policy.

The pause menu shows the first GPU initialization error if SDL used the software
fallback. It also shows the failed tactile or HWRM stage after a hit pulse.
The tactile resolver is loaded dynamically when its server is already running;
the game asks for the system's `Sensitive` cue with vibration only and uses a
short HWRM pulse otherwise. Both calls are synchronous server transactions,
so the SDK keeps them on a worker. Starting the optional tactile server on
demand caused an RM-807 emulator haptics-stack stall; the guard avoids that
startup during gameplay. The normal HWRM vibration profile switch can differ
from the system's tactile feedback switch. The HWRM server's ordinary IPC range is marked
`EAlwaysPass`; FM transmitter operations have separate capability checks.

The current RM-807, C7/RM-675 and E6/RM-609 emulator fixtures rendered both
SDL2 and SDL3 GPU targets. The Nokia 808 ran the earlier GPU build at about
54 FPS and reported an HWRM vibration timeout. The new tactile path requires
a physical check. The source-built emulator control frontend injected held
pointer motion and system-menu keys. The separately installed emulator 0.1.0
lacks the task-close control used by the executable matrix. The API table
describes implemented adapters and
should not be read as an exhaustive claim of upstream SDL feature parity.

E71/RM-346 and 6120c/RM-243 lack the Open C and EGL/GLES2 DLLs required
by the default GPU profile. ARMv5T software builds using the older-EKA2
runtime have no `libc.dll`, `libpthread.dll`, `libm.dll`, `libegl.dll` or
`libglesv2.dll` imports. Source-built SDL2 and SDL3 apps loaded, rendered,
accepted input and exited with guest reason zero on both named EKA2L1 ROM
fixtures. For an installed SDK, configure `SYMBIAN_TARGET_ARCH=armv5t`,
`SYMBIAN_RUNTIME_LEGACY_EKA2=ON` and `SYMBIAN_ARKANOID_GPU=OFF` before
linking `Symbian::PortableSdl2` or `Symbian::PortableSdl3`. Each additional
firmware needs its own ordinal and guest-execution check. No physical older
device run has been recorded.
GL Cube also needs a renderer suitable for the graphics API available there.
