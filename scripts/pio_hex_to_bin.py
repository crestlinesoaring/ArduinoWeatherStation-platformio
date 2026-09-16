"""Post-build: convert firmware.hex to firmware.hex.bin via avr-objcopy."""

import subprocess

Import("env")


def hex_to_bin(target, source, env):
    hex_path = str(target[0])
    bin_path = hex_path + ".bin"
    objcopy = env.subst("$OBJCOPY")
    print("Converting %s to binary" % hex_path)
    subprocess.run(
        [objcopy, "-I", "ihex", "-O", "binary", hex_path, bin_path],
        check=True,
    )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.hex", hex_to_bin)
