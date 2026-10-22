# Neon Circuit demo

Generate the original 120-second, 128 BPM Electro House arrangement with the native Drums, Juno and DX7 engines. The Python script requires NumPy; the renderer requires CMake, a C++17 compiler and the AMY submodule.

From the repository root, using a POSIX shell:

```sh
git submodule update --init --recursive
cmake -S tools/demo -B build/demo -DCMAKE_BUILD_TYPE=Release
cmake --build build/demo --parallel 4
python3 tools/demo/compose_demo.py compose outputs/neon-circuit
for track in 0 1 2 3; do
  build/demo/CardenzaDemo outputs/neon-circuit/score.txt "$track" "outputs/neon-circuit/stem-$track.wav" 120
done
python3 tools/demo/compose_demo.py mix outputs/neon-circuit
```

The output includes the score, MIDI, composition metadata, four mono stems, a native mono mix, a processed stereo mix and audio measurements. Stereo effects, ducking, transitions and full-song playback belong to the offline renderer and mix stage.

Run `build/demo/CardenzaDemo outputs/neon-circuit/score.txt export` to save and read back four `.ccproj` loop projects. On macOS/Linux they are written under `sdcard/CardenzaSynth/Project/` in the current working directory; on Windows the storage directory is beside the executable. Each project contains eight patterns per track, with Drop, Intro, Break or Build selected initially. Copy the projects to the device's `CardenzaSynth/Project/` directory and load them through `SYS > PROJ`. Hardware playback loops the selected patterns; the firmware has no automatic Song/Chain arrangement.

Generated audio, captures, firmware and backups under `outputs/` are local artifacts and are ignored by Git.
