"""Point *-eeprom-test environments at src/eeprom-test only."""

Import("env")

if env["PIOENV"].endswith("-eeprom-test"):
    test_src = env.subst("$PROJECT_DIR/src/eeprom-test")
    env.Replace(SRC_DIR=test_src)
    env.Replace(PROJECT_SRC_DIR=test_src)
    # Fast pass: Athena bootloader block only. Full chip: append -D EEPROM_TEST_LAST_ADDR=4095
    env.Append(CPPDEFINES=[("EEPROM_TEST_LAST_ADDR", 69)])
