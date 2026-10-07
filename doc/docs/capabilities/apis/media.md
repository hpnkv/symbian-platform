# Media

Modern audio playback and recording APIs are planned, but no public header or
`Symbian::Media` archive is exported. Applications needing audio must use the
original platform APIs with their format, permission, asynchronous completion
and buffer-lifetime requirements. Camera discovery is available separately in
[the camera component](camera.md).

The original MDA stream interface is available through
`<mdaaudiooutputstream.h>` and `Symbian::AudioStream`. Its 0.2.0 classic
example built and displayed its GUI with the published SDK on the preserved
RM-807/Dynarmic and Dyncom emulator fixture. Invoking the stream panicked
with `E32USER-CBase` reason 46 (a stray active-scheduler event) on both
backends. The cause remains unisolated; this is not evidence that streaming
works on Belle or physical hardware. Image conversion is separately available
through `<imageconversion.h>` and `Symbian::ImageConversion`; the bounded
embedded-PNG example decoded and displayed its pixel on RM-807/Dynarmic.
The source example now calls `KeepOpenAtEnd()` before `RequestStop()`, as the
public MDA contract requires, but the Dynarmic panic persists after that fix.
