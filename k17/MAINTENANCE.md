# Maintaining HDMI FRL/VRR until upstream support replaces this fork

Status: proposed plan, September 17, 2026. Only the source archive, test baseline and prototype full-kernel build exist today. No automatic update service, production image, DKMS package or akmod package is installed by this plan.

## Recommendation

Deliver a **custom Bazzite Deck image containing a matched, prebuilt and tested graphics stack**, with candidate and stable channels. First evaluate a stock-kernel module replacement to minimize maintenance; retain a correctly packaged full OGC kernel as the fallback if that evaluation fails. Build before deployment rather than compiling the machine's display driver during a client update or boot.

Universal Blue already builds/caches matching kernel and kmod RPMs in [akmods](https://github.com/ublue-os/akmods), and provides an [image template](https://github.com/ublue-os/image-template) for custom bootc images. Bazzite's own [kernel/akmods installation code](https://github.com/ublue-os/bazzite/blob/main/build_files/install-kernel-akmods) is the integration reference. Follow the release's supported updater and signing mechanism; don't introduce an independent second updater on the client.

## Can DKMS do it?

In principle, yes: DKMS runs a module build/install recipe for each kernel. It does not adapt patches to new APIs, establish ABI compatibility, fix an in-tree driver's build assumptions, or validate display behavior. The kernel explicitly does not promise a [stable internal driver API](https://docs.kernel.org/process/stable-api-nonsense.html).

This patch stack changes 27 files, including the shared Intel display code under `i915/display` that is compiled into Xe, plus `drm_scdc_helper.c` compiled into `drm_display_helper.ko`. It adds exported SCDC helpers used by Xe. **A patched xe.ko against the unchanged stock helper module is insufficient for this stack.** Today's live-module tests used an already patched full kernel, so they do not establish stock-kernel module-only compatibility.

| Approach | Benefit | Remaining cost / decision |
|---|---|---|
| Client-side DKMS | Can trigger rebuilds on kernel changes | Must package an in-tree driver backport, helper dependencies, immutable deployment and initramfs integration; failure can affect graphics at the next boot. Not the first choice here. |
| Prebuilt kmod/akmods pair on stock OGC kernel | Retains stock kernel and its existing third-party kmods | Must prove matching Xe + display-helper replacements work; build every supported exact kernel version, stage both together and test on hardware. Preferred experiment. |
| Patched full OGC kernel in a custom image | Closest to the demonstrated implementation; builds dependent in-tree modules consistently | Rebuild external kmods too, retain full Bazzite/OGC configuration, automate source rebases and packaging. Reliable fallback, not maintenance-free. |

Akmods and DKMS are build automation, not automatic compatibility. An akmod source RPM may be useful, but consume its **prebuilt output in the image pipeline**. Don't assume generic Fedora DKMS instructions apply unchanged to an immutable Bazzite installation.

## Phase 1 — prove the smallest replaceable module set

1. Take the exact current stock Bazzite kernel source/package revision, config, compiler constraints, Module.symvers and BTF inputs. Record their digests; do not use just an upstream kernel version with the same number. Kernel [external-module documentation](https://docs.kernel.org/kbuild/modules.html) describes the prepared build tree and symbol-version requirements; modules_prepare alone is insufficient for CONFIG_MODVERSIONS.
2. Apply the carried Intel FRL series and local patches to that source. Try building the complete Xe module and shared DRM display-helper module using the source tree's own build rules. Package as a matched pair; evaluate whether any additional provider modules are required through symbol checks. Preserve any configuration/ABI expectations for other consumers of the shared helper.
3. Check undefined/exported symbols, vermagic, symbol versions when enabled, module signatures, BTF compatibility and kernel config. Never force-load or falsify a module version to bypass these checks.
4. Stage replacements into a *new test image*, with explicit module lookup priority, depmod output, and regenerated initramfs. Verify the boot image contains the replacements and that loading resolves to those paths. Merely writing into the running root or relying on filename ordering is not sufficient. Both modules must be installed/rolled back together.
5. Boot that candidate with the stock kernel and validate the hardware matrix below. Confirm xone/xpadneo and other required stock modules still work. Initially limit the image to this Xe/Lunar Lake machine; other shared-helper consumers need their own validation.
6. If successful, use this route for production candidates. If the scope expands into broad DRM core backports or requires fragile source surgery, stop the module-only effort and use the full-kernel route.

Success criterion: the same display behavior on the *unmodified stock kernel*, with the exact matched replacements, normal boot and rollback, no new warnings, and retained controller support. This experiment is not yet done.

## Phase 2 — automate complete candidate images

- Base on `bazzite-deck` and pin each build to a specific upstream image digest. Record base digest, source/patch commits, kernel release/config, compiler and package checksums in a manifest.
- Keep Intel's upstream series separate from local fixes and the opt-in VRR prototype. Detect already-merged patches and remove them deliberately. A patch conflict or unexpected base change must fail the candidate build, not silently skip a fix.
- For module-only builds, use exact matching stock kernel artifacts. For full-kernel builds, use OGC's normal packaging and Bazzite's full configuration, retaining BTF and the required third-party akmods. Replace the whole kernel package set coherently. Don't perpetuate the minimal test config or mass removal of kmods.
- Build on a controlled runner/container, cache compiler output, run DFM/source/build checks, and produce versioned artifacts plus checksums. Add a full image boot smoke test where practical; virtual machines cannot certify physical FRL or VRR.
- Publish signed candidates to a separate image channel. Image signing and Secure Boot/module signing are separate concerns; test/enroll the latter before advertising Secure Boot support. Current hardware testing had Secure Boot disabled.
- After local hardware qualification, promote the **same tested digest** to stable. Until then stable stays on the previous qualified image. A scheduled build may create candidates; it must not silently promote an untested graphics stack.

## Phase 3 — normal updates and recovery

Once the image build is proven, perform a single supported switch/rebase to the custom image after clearing conflicting prototype local overrides in the new deployment. The system then updates the image as a unit, through the normal updater, instead of repeatedly applying manual kernel RPM overrides.

Retain stock and last-known-good boot deployments. Verify boot-menu recovery and the return-to-stock path before enabling unattended stable updates. A failed build never reaches the client; a failed boot/display test rolls back. Document an SSH-accessible recovery path because the framebuffer can fail while the machine itself boots.

Track upstream Bazzite security updates promptly. Holding the entire image at a known-good digest is a recovery policy, not a long-term excuse to stop updates. If a kernel rebase blocks promotion, evaluate an explicitly built userspace update with the last supported kernel, document the security tradeoff, and prioritize fixing the rebase. Don't promise zero maintenance until the patches are upstream.

## Hardware qualification before stable promotion

- Cold boot and reboot to Gaming Mode without manual module loading.
- 4K120 RGB10 HDR and real variable refresh; TV overlay plus vblank evidence.
- Fixed 120.00 Hz behavior, a steady below-ceiling rate, 70–100 FPS sweep, and low-refresh/LFC boundary behavior.
- Fullscreen → Steam/KDE → fullscreen transitions without persistent flicker, stale VRR packets or blackouts.
- HDMI audio, TV/AVR off/on, input switching, cable hotplug/retraining, and suspend/resume.
- Moonlight end-to-end HDR/4:4:4 decode plus VRR with the intended host; do not equate output format with decoder support.
- Xbox dongle/xone, Bluetooth/xpadneo, other intended controllers and USB forwarding if used.
- Kernel logs free of new underruns/state mismatches; visual confirmation still required because logs alone miss some failures.
- Successful rollback to a known-good deployment.

Record each result separately; do not inherit “tested” status from another kernel just because the patches applied.

## Phase 4 — reduce and retire the fork

Rebase to current Intel FRL work, submit narrowly scoped arithmetic/clock fixes with reproduced evidence, and discuss the HDMI VRR design with Intel display maintainers. The VTEM-over-GMP and blanking-accounting changes require substantive review rather than assuming the experiment is upstream-ready. Include Codex authorship disclosure and follow the target project's contribution/AI policies at submission time.

When the normal Bazzite/OGC kernel passes the same matrix without our patches, switch back to the stock image, remove experimental parameters/config where no longer needed, and retain this fork as an archived reproducer. “HDMI 2.1 merged” is not by itself the exit criterion: this hardware's FRL, HDR, VRR and lifecycle behavior must all be verified.
