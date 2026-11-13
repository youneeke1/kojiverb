# kojiverb — VST3 multi-effect

JUCE-based Windows x64 VST3 effect for FL Studio.

## Effects
- LOSSY: blendable bit-depth reduction (4–16 bit) and sample-rate reduction.
- SPACE: algorithmic reverb with room size, damping, wet mix and Freeze.
- DELAY: Mono, Stereo and Ping-Pong routing; feedback and wet mix.
- BPM sync: host tempo with straight, triplet (T) and dotted (D) divisions; free-time fallback.
- Interface: dark burgundy/red with hand-drawn childlike doodles illustrating crunchy audio, a space planet and echo repeats.

## Build
GitHub Actions builds the Windows x64 VST3 on every push to main. Open the repository's **Actions** tab and download the artifact named **kojiverb-windows-x64-vst3** after a successful run.

Extract the ZIP and copy the complete `kojiverb.vst3` folder to:
`C:\\Program Files\\Common Files\\VST3\\`

Then open FL Studio → Options → Manage plugins → Find installed plugins.

## Technical notes
- Source is C++17 and uses JUCE 9.0.5 via CMake FetchContent.
- This is an initial functional implementation; verify behavior in your DAW before using it in critical sessions.
- JUCE is dual-licensed. Review JUCE's current licensing terms before distributing the binary.
