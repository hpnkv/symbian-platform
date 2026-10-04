# SD card filesystem throughput on Symbian

## Finding

Software can plausibly improve **application-observed** SD read and write
throughput by reducing File Server calls, using a suitable buffer size and
alignment, avoiding needless metadata work, and choosing buffering deliberately.
It cannot raise the card/host bus ceiling or make a slow card controller fast.
No throughput gain is claimed for the Nokia 808 yet: this repo has no controlled
on-device SD benchmark, and the emulator uses host storage rather than the
phone's removable-media path.

The [SD Association's speed classes](https://www.sdcard.org/developers/sd-standard-overview/speed-class/)
describe minimum sustained write performance under specified conditions;
its [Application Performance Classes](https://www.sdcard.org/developers/sd-standard-overview/application-performance-class/)
separately address small random reads and writes. Neither rating predicts a
particular Symbian File Server workload or proves the 808 supports a newer
card's performance features.

## Available software levers

| Lever | Expected effect | Constraint or measurement needed |
| --- | --- | --- |
| Keep `RFs`/`RFile` open and transfer larger sequential chunks | Fewer IPC, allocation and metadata operations per byte | Sweep chunk sizes on the actual card; avoid large phone RAM buffers and long uncancellable steps. |
| Query `RFs::VolumeIOParam` for block, cluster and recommended read/write sizes | Gives a drive-specific starting point for buffer size and alignment | Any field may be unsupported; recommendations are hints, not measured 808 results. |
| Use aligned, whole-block writes where practical | Can avoid media read-modify-write for partial blocks | Logical cluster size and physical block size differ; verify returned values. |
| Compare buffered/read-ahead modes with defaults | May reduce small-read overhead or improve sequential reads | Cache modes depend on drive policy; direct I/O can lose useful caching. Do not enable it by default. |
| Pre-size a known output with `RFile::SetSize` | May reduce repeated file growth and allocation work | Could add up-front latency or unnecessary writes; benchmark it and handle low-space failure. |
| Flush at a meaningful durability boundary | Avoids a potentially expensive flush after every chunk | A successful File Server flush is not proof of power-loss persistence through the card controller. |
| Separate producer/consumer work with a small bounded queue | Can overlap computation or USB input with storage I/O | On a single core and one storage request path, extra threads may add context switches with no throughput benefit. |

These levers are grounded in the preserved Symbian
[`f32file.h`](../research/upstream/kernelhwsrv/userlibandfileserver/fileserver/inc/f32file.h):
`TVolumeIOParamInfo` documents block/cluster size and recommended buffer sizes;
`EFileReadBuffered`, `EFileWriteBuffered`, `EFileReadAheadOn` and the direct-I/O
flags describe optional cache behavior; `RFile` provides positional reads,
writes, `SetSize` and `Flush`. The current modern `Symbian::Storage` API already
reuses open handles and caller buffers. Its 32 KiB `FileCopy::Step` size is a
**responsiveness and memory bound**, not an SD throughput optimum. A later
copy option can select a bounded drive-informed chunk size after measurement.

## Measurement before changing defaults

On a disposable test directory on the phone's removable drive, record the
model/firmware, card identity/capacity, filesystem, free space and returned
volume I/O parameters. Preserve any existing data; create and delete only
files owned by the benchmark. Compare at least 4, 16, 32, 64 and 128 KiB
buffers, default versus explicit buffering/read-ahead, and pre-sized versus
growing outputs. Measure cold and warm sequential reads, sustained writes,
small random I/O, per-step latency and time spent in `Flush`, reporting
median and tail latency as well as throughput. Use distinct files and verify
every output digest; repeated warm reads alone mostly measure cache. Record
power and temperature where available because throttling and card garbage
collection can distort short runs. Stop when the card is removed or errors
occur, and keep cancellation responsive between chunks.

The first implementation change should be **measurement plumbing and a
bounded configurable copy chunk**, with the current 32 KiB default retained
until the device results justify another value. Avoid raw block access,
filesystem remounting or card formatting in application APIs.
