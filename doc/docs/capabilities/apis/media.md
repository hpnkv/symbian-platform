# Media

`Symbian::MidiOutput` and `Symbian::Vibration` are separate optional device services.
`MidiOutput` in `<symbian/api/media/midi_output.h>` queues individual notes
after the native MIDI server opens on its worker. `PlayNote(note, duration_ms,
velocity)` validates the MIDI values and reports queue saturation. `Start()`
accepts the startup work; `available()` and `status()` report the later service
result. The caller chooses the notes, sequence and cadence.

`Vibration` in `<symbian/api/media/vibration.h>` accepts a 1–5000 ms HWRM pulse.
`Start()` prepares the worker; `Pulse()` accepts one request and `status()`
reports its later native result or the bounded service deadline. The caller
chooses the event and rate limit. Destruction stops accepting work without
waiting indefinitely for a stalled firmware server. The service may be absent
or fail even when the executable loads. These APIs do not implement SDL audio,
PCM playback, SDL haptics or generic tactile effects.

Applications needing audio streams still use the original platform APIs with
their format, permission, asynchronous completion and buffer-lifetime
requirements. Camera discovery is available separately in
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
