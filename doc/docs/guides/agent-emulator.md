# Run the development agent in an emulator

This guide starts the SDK's read-only development-agent example in a disposable
EKA2L1 instance. EKA2L1 emulates Symbian software on a desktop host. The
example uses a **public test certificate and key**, binds only to the
emulator's loopback address and is for the pinned RM-807 emulator profile.
It is not a device-pairing procedure.

## Before starting

Prepare and activate a current SDK using the [SDK export guide](../reference/sdk.md).
Import and select the preserved RM-807 fixture using the
[firmware guide](firmware.md). Check what the emulator will use:

```sh
symbian emu resolve --project examples/agent_service
```

The selected device should be `808 PureView`, with the configured EKA2L1 build
available. That model label describes the **emulated firmware profile**; it
does not establish compatibility with a physical Nokia 808.

## Start the service

From the repository root, leave this command running in one terminal:

```sh
symbian app run --project examples/agent_service
```

The command builds the ARMv6 application against the active SDK, copies the
selected firmware into a private disposable instance and launches
`agent_service.exe`. It prints the session directory when the emulator starts.
The service listens at `127.0.0.1:39101` through an active-object accept
request. Its TLS and control work runs on the SDK's bounded worker.

## Read the negotiated profile

In a second terminal at the repository root, use the example's public fixture
identity. These paths are part of the source tree:

```sh
symbian agent hello 127.0.0.1 39101 \
  --server-name sdk-test \
  --ca-bundle third_party/mbedtls-symbian/tests/fixtures/server-cert.pem \
  --client-certificate third_party/mbedtls-symbian/tests/fixtures/server-cert.pem \
  --client-key third_party/mbedtls-symbian/tests/fixtures/server-key.pem
```

The result should show version 1, a 4 KiB control limit, 16 requests per
connection and `status`/`logs`. The host checks the server certificate name
and chain, presents the client certificate, then performs hello before any
read request. The fixture key provides **test authentication mechanics only**;
anyone with the repository can possess it.

Run the same command with `hello` changed to `status` for a native tick and
display snapshot, or to `logs` for a page of service-local events. Use
`symbian agent logs --help` for the sequence cursor and page limit. The
[protocol reference](../reference/agent-protocol.md) describes each field and
its limit.

## Prepare a SIS in the desktop console

Open `symbian console` and choose **Development Agents**. With an active SDK
selected, **Build agent package** compiles and packages this example. A
connected USB phone appears both beneath **Devices** in the sidebar and as a
card in **Development Agents**. If the phone exposes a writable storage volume,
**Stage SIS on device** copies the verified package to its `Installs` folder.
Safely eject the volume and complete the installer prompts on the phone.

The card continues to say that installation is **unknown** or **unverified**.
USB detection and SIS staging cannot establish that the service installed or
started. This agent package has only passed emulator tests and is not a
validated Nokia 808 release.

## Stop and inspect

Press **Ctrl-C** in the first terminal. The launcher reaps only the emulator
process it started and retains `launch.json` and `frontend.log` in the printed
session directory. Check `inputs_unchanged` and guest failures there if a run
does not behave as expected. The agent package has no boot-start script or
application registration, and the example has no on-device status/disable UI.

An emulator run proves only this selected guest/host path. The Nokia 808 still
needs a separate physical-device gate, identity provisioning, local pairing,
permission controls and idle-power measurements.
