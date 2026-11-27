# World generation checks

The test is headless and does not require a window or a raylib library; it only
needs the raylib headers (for example, from the CMake-fetched raylib source):

```sh
g++ -std=c++17 -O2 -ffunction-sections -fdata-sections \
    -I src -I path/to/raylib/src tests/world_generation.cpp src/world.cpp \
    -Wl,--gc-sections -o /tmp/world_generation_test
/tmp/world_generation_test
```

It checks deterministic tiles and props, tree spacing, collision and terrain
compatibility, village and landmark clearings, and the quest road for several
seeds. The linker discards unrelated game functions, so no graphics context is
needed.
