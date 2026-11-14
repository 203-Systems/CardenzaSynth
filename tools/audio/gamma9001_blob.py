"""AMY's Gamma9001 drum sample banks as one file of int16 samples, for a test build that links them in
(make DEVICE=Cardenza stress AMY_BENCH=1; Library/Synth/Audio/Amy/AmyBench.h).

usage: python3 tools/audio/gamma9001_blob.py <output, e.g. build/amy-bench/drums.bin>

Runs AMY's own generator (amy.headers, `gamma9001`) on a copy of the submodule's sounds, outside the
submodule: the generator writes its headers beside its sources, and the submodule is not ours to write
in. The maps it writes there must be the ones the submodule has (src/pcm_gamma9001.h, by which AMY
reads the blob, and src/pcm_gamma808.h): otherwise it stops.
"""

import filecmp
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AMY = os.path.join(ROOT, "Library", "third_party", "amy")
MAPS = ("pcm_gamma9001.h", "pcm_gamma808.h")


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    output = os.path.abspath(sys.argv[1])
    if not os.path.isdir(os.path.join(AMY, "sounds", "gamma9001")):
        sys.exit("gamma9001_blob: the AMY submodule is not checked out (%s)" % AMY)
    with tempfile.TemporaryDirectory() as work:
        shutil.copytree(os.path.join(AMY, "amy"), os.path.join(work, "amy"))
        for sounds in ("gamma9001", "wavetables"):
            shutil.copytree(os.path.join(AMY, "sounds", sounds), os.path.join(work, "sounds", sounds))
        os.makedirs(os.path.join(work, "src"))
        subprocess.run([sys.executable, "-m", "amy.headers", "gamma9001"], cwd=work, check=True,
                       stdout=subprocess.DEVNULL)
        for name in MAPS:
            if not filecmp.cmp(os.path.join(work, "src", name), os.path.join(AMY, "src", name), shallow=False):
                sys.exit("gamma9001_blob: the generated %s differs from the submodule's" % name)
        blob = os.path.join(work, "build", "drums.bin")
        # Left alone when it is the same, so a build does not link the app again for nothing.
        if not (os.path.exists(output) and filecmp.cmp(blob, output, shallow=False)):
            os.makedirs(os.path.dirname(output), exist_ok=True)
            shutil.copyfile(blob, output)
        print("gamma9001_blob: %s, %d bytes" % (output, os.path.getsize(output)))


if __name__ == "__main__":
    main()
