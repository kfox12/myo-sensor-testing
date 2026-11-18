# PlatformIO pre-build script for [env:simple]: build the day-1 sketch
# (simple.ino in the repo root) on its own.
#
# PlatformIO would merge every .ino in a source folder into one program, so
# copy just simple.ino into a private folder under .pio/ and build from there.
import os
import shutil

Import("env")  # noqa: F821 - provided by PlatformIO

project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
src_dir = os.path.join(project_dir, ".pio", "simple_src")

os.makedirs(src_dir, exist_ok=True)
shutil.copy(os.path.join(project_dir, "simple.ino"), src_dir)

env.Replace(PROJECT_SRC_DIR=src_dir)  # noqa: F821
