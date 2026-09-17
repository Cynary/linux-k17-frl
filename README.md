# K17 Intel HDMI FRL + VRR experiment

Experimental Linux kernel fork for the **GMKtec K17 / Lunar Lake / Arc 130V**.
On the tested K17 → Denon AVR-S760H → LG G1 chain, this enables **3840×2160 at
120 Hz, RGB 10-bit (no chroma subsampling), HDR output and real variable refresh**.
It is a hardware-tested prototype, not a general HDMI 2.1 implementation or a
claim of HDMI compliance.

- [Results, provenance and build notes](k17/README.md)
- [Sustainable update plan and DKMS assessment](k17/MAINTENANCE.md)
- [Exact tested kernel configuration](k17/configs/k17-vrr4.config)
- [Original kernel README](README)

The immutable test reference is tag **k17-vrr4-tested**, pointing to source
`f37b49ee5`. Development and documentation are on `k17-frl-vrr-test`.
The installed kernel reports `7.2.4-k17vrr4+`.

Built on OpenGamingCollective's kernel and Intel's public FRL patch series,
with additional local fixes and an opt-in HDMI VRR experiment developed with
OpenAI Codex and tested on physical hardware by the owner. Original patch
authorship is retained. No upstream acceptance or endorsement is implied.

The maintenance plan is a proposal: automated image builds and DKMS/akmods
packages have **not** been implemented in this fork yet.
