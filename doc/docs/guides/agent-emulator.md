# Run the development agent in an emulator

This guide starts the SDK's read-only development agent in a disposable
EKA2L1 instance. EKA2L1 emulates Symbian software on a desktop host. The
example uses a **public test key**, binds only to the
emulator's loopback address and is for the pinned RM-807 emulator profile.
It is not a device-pairing procedure.

## Before starting

Prepare and activate a current SDK using the [SDK export guide](../reference/sdk.md).
Import and select the preserved RM-807 fixture using the
[firmware guide](firmware.md). Check what the emulator will use:

```sh
symbian emu resolve --project agent_service
```

The selected device should be `808 PureView`, with the configured EKA2L1 build
available. That model label describes the **emulated firmware profile**; it
does not establish compatibility with a physical Nokia 808.

## Start the service

From the repository root, leave this command running in one terminal:

```sh
symbian app run --project agent_service
```

The command builds the ARMv6 application against the active SDK, copies the
selected firmware into a private disposable instance and launches
`agent_service.exe`. It prints the session directory when the emulator starts.
The service listens at `127.0.0.1:39101` through an active-object accept
request. Authentication and control work run on the SDK's bounded worker. The
application menu registers it as **Development Agent**. The local panel shows
**RUNNING** while the service accepts connections.

![Development Agent running in the emulator, with BACK and STOP controls](../assets/screenshots/agent-status.png)

Choose **BACK** to leave the local panel while the service keeps running.
Return to the agent from the application menu. Choose **STOP** to close the
listener and exit the agent process. These controls were exercised in a
disposable emulator; no physical-device background or idle-power behavior has
been measured.

## Read the negotiated profile

In a second terminal at the repository root, use the example's public test
key. This key is part of the source tree and has no device-security value:

```sh
symbian agent hello 127.0.0.1 39101 \
  --key-file agent_service/test-agent.key
```

The result should show version 1, a 4 KiB control limit, 16 requests per
connection and `status`/`logs`/`workspace-list`. The host and guest prove possession of the
same key using fresh challenges, then perform hello before any read request.
The fixture key provides **test authentication mechanics only**;
anyone with the repository can possess it.

Run the same command with `hello` changed to `status` for a native tick and
display snapshot, or to `logs` for a page of service-local events. Use
`symbian agent logs --help` for the sequence cursor and page limit. The
[protocol reference](../reference/agent-protocol.md) describes each field and
its limit.

## Inspect the agent workspace

To list files directly inside the agent's private workspace, run:

```sh
symbian agent files 127.0.0.1 39101 \
  --key-file agent_service/test-agent.key
```

An empty result is expected until the agent creates files there. The request
cannot name another directory or read file contents. Use `--after` and
`--limit` to page through at most eight entries at a time. The
[protocol reference](../reference/agent-protocol.md#agent-workspace) explains
the 256-entry bound and what happens if the directory changes mid-listing.

## Prepare a SIS in the desktop console

Open `symbian console` and choose **Development Agents**. With an active SDK
selected, **Build for this phone** compiles and packages a phone-specific agent
with a private protocol key and a separate self-signing key stored outside the
repository. A
connected USB phone appears both beneath **Devices** in the sidebar and as a
card in **Development Agents**. **Stage agent package** transfers the checked
SIS to `Installs` through a writable mounted volume or the phone's MTP
interface in PC Suite mode. The SDK reads the staged bytes back to verify the
hash. Safely eject a mounted volume, then complete the installer prompts on
the phone. Self-signing checks the package's origin and integrity, but the
handset can still reject it under its own installation policy.

USB detection and SIS staging cannot establish that the service installed or
started. The phone-specific panel shows an eight-character pairing code;
compare it with the console card before checking status. On the same local
Wi-Fi, choose **Check live status**. The agent discovers the console, connects
to it and proves possession of its phone-specific key. Only an authenticated
response earns the **Verified live** label. This protocol
authenticates the peer but does not encrypt traffic; use a trusted local
network. A failed check does not prove the agent is absent: discovery, network,
installer or guest entropy may be at fault. Nokia 808 compatibility remains a
separate physical-device gate.

## Stop and inspect

Press **Ctrl-C** in the first terminal. The launcher reaps only the emulator
process it started and retains `launch.json` and `frontend.log` in the printed
session directory. Check `inputs_unchanged` and guest failures there if a run
does not behave as expected. The package includes application-menu
registration and local BACK/STOP controls; it has no boot-start script.

An emulator run proves only this selected guest/host path. The Nokia 808 still
needs a separate physical-device gate, installation and network checks, local
pairing confirmation, permission controls and idle-power measurements.
