"""Point *-eeprom-net environments at src/eeprom-net-writer only."""

import os

Import("env")

if env["PIOENV"].endswith("-eeprom-net"):
    writer_src = env.subst("$PROJECT_DIR/src/eeprom-net-writer")
    env.Replace(SRC_DIR=writer_src)
    env.Replace(PROJECT_SRC_DIR=writer_src)

    hw_version = os.environ.get("EEPROM_NET_HW_VERSION")
    if hw_version:
        env.Append(CPPDEFINES=[("HW_VERSION", int(hw_version))])

    if os.environ.get("EEPROM_NET_FORCE_HW_ID"):
        env.Append(CPPDEFINES=[("EEPROM_NET_FORCE_HW_ID",)])
