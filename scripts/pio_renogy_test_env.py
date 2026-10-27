"""Point renogy_test at src/renogy-test only (avoid compiling the station sketch)."""

Import("env")

if env["PIOENV"] == "renogy_test":
    test_src = env.subst("$PROJECT_DIR/src/renogy-test")
    env.Replace(SRC_DIR=test_src)
    env.Replace(PROJECT_SRC_DIR=test_src)
