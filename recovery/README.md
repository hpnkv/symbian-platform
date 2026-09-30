# Preserve the reference Nokia 808

Keep the single phone as the reference device. Charge it normally and record
its condition before development. Read the product label and the software
version screen (`*#0000#` where available). Record observations, including
unknown values, in a private inventory based on device.example.json. Keep the
IMEI and serial number private. Verify RM/product code from the actual device;
RM-807 in the project plan is not a recorded observation.

Record firmware version/date, language/region, installed software, storage
volumes, free space, lock state, modifications, USB behavior and existing backup
tools. Record whether a backup tool changes device state before using it. Do
not install a ROM dumper or change system protection just to fill in this form.

Preserve any existing firmware files, original installers, backups, ROM and
matched Z-drive dump with source/provenance and acquisition date. Copy them
into a new archive with `symbian preserve create SOURCE ARCHIVE`. Verify with
`symbian preserve verify ARCHIVE`. Keep the returned manifest SHA-256 separately,
and verify a second offline copy with `--manifest-sha256 DIGEST`. An archive
does not establish recovery compatibility or protect against deletion by its
owner. Use an offline or independently enforced immutable copy for the reference.

A human must establish recovery independently: correct RM/product-code firmware,
flashing tool/version, licensed OS and drivers, host architecture, USB access,
power requirements, and a documented validation method. A sealed recovery VM
must not contain ordinary development services or agent endpoints. Test VM/USB
setup without writing firmware first. Recovery instructions and an appliance
remain incomplete until supported by evidence for this unit.

Flashing, erasure, bootloader, OTP, calibration and partition operations are
human-operated only. This repository deliberately contains no executor for them.
