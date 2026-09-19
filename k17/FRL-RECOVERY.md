# FRL failure-path audit — September 18, 2026

## Finding

The reproduced failure path is inherited from the imported, unmerged Intel FRL
series. It is not introduced by the local VRR/clock/DFM fixes or by building Xe
and the display helper against the stock kernel. This is source-level attribution
of the relevant path, not a claim that an unmodified upstream kernel was tested
at the same FRL mode: the stock baseline does not support it on this machine.

The first timeout is expected when the sink is unavailable. The subsequent
inconsistent modeset state is a driver failure-handling defect to investigate,
not a reason to dismiss the log as harmless.

## Code provenance and mechanism

- Intel FRL series 171757 revision 1, patch 39, carried as commit
  `6c85a2c2d589a993e55b41c298a3a5cdf9d16beb`, adds the early return in
  `intel_ddi_enable_hdmi()` after a failed `intel_hdmi_start_frl()`.
- That branch disables FRL, schedules the link-status retry worker, and returns
  **before** `intel_ddi_enable_transcoder_and_vblank()`.
- The skipped helper enables the transcoder and calls `intel_crtc_vblank_on()`.
  The surrounding atomic commit nevertheless continues with a requested active
  CRTC. The observed `intel_crtc_arm_vblank_event()` warning, failed vblank ioctl,
  flip timeout and active-state mismatch are consistent with this exact path.
- The 250 ms sink-readiness timeout is also from the original series, carried
  as `529e60ba3a`; it is not our added timeout.
- Patch 38 (`1831b48ba`) supplies the retry worker which marks link-status BAD
  and sends a hotplug event. Sending that event is not proof of successful
  userspace recovery.


The original patch-39 mbox and the carried code have the same failure branch.
The later local changes to `intel_ddi.c` add early FRL clock readback in
`intel_ddi_get_config()`; they do not alter this enable/failure branch.

## Hardware reproduction

Environment: published module-pair image digest
`sha256:0f70ebca5af3e11740485060094b3c3ed90cf18f41e3845eabfa8e28b4b183d9`,
stock kernel `7.2.4-ogc3.1.fc44.x86_64`, K17 through Denon to LG G1.

1. Baseline was working at 4K120, 30 bpp, HDR/VRR. No training/mismatch/underrun
   errors had accumulated during about an hour of uptime.
2. Receiver standby alone plus a Gaming Mode restart did not reproduce the
   failure while the TV remained on. Standby passthrough matters here.
3. Turning the TV off and restarting Gaming Mode reproduced sink-not-ready and
   FRL failure at 17:04:06 PDT, followed by flip timeout and state mismatches at
   17:04:17. A hardware vblank query failed with EINVAL. SSH stayed available.
4. After waking the TV/receiver and selecting the correct inputs, the vblank
   query still failed; power-on alone did not recover this run.
5. Restarting Gaming Mode recovered without an OS reboot, after repeated
   flip timeouts during session teardown. Scanout first returned at 4K60 during
   startup, then FRL40 training passed at 17:06:16 and Gaming Mode returned to
   4K120, 30 bpp, HDR and variable scanout. No further training/underrun/state
   mismatch warnings were observed after recovery.

This establishes poor failed-enable handling and recovery in the current
driver/compositor combination. It does not isolate every part of the automatic
retry failure between Gamescope and the kernel. A suspected persistent rate-cap
limit was ruled out: this source writes `frl.rate_cap` but does not read it for
mode selection.

Full logs are retained locally under `linux-probe/option1-ci/recovery-audit`.
No driver changes were deployed during this diagnostic test.

## Fix direction

Audit failed-enable cleanup, atomic event completion, and the driver/userspace retry
path together. Do not merely suppress the state checker, extend the
readiness timeout indefinitely, or claim success without an active link.
Qualify the eventual fix with off-at-boot, TV off/on with receiver passthrough,
receiver input changes, and successful retraining without restarting the OS.
