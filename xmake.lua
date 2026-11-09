-- CommonLibF4 is downloaded into lib/commonlibf4 by the GitHub workflow
includes("lib/commonlibf4")

set_project("BHOP")
set_version("1.4.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

target("BHOP")
    add_rules("commonlibf4.plugin", {
        name = "BHOP",
        author = "jwvdqx",
        description = "Boston Hopping Overhaul Project - Half-Life 1 and 2 player movement for Fallout 4"
    })
    -- works whether the files are in src/ and res/ or uploaded straight to the repo root
    local root = os.scriptdir()
    local srcdir = os.isfile(path.join(root, "src", "main.cpp")) and "src" or "."
    add_files(srcdir .. "/main.cpp")
    add_headerfiles(srcdir .. "/pch.h", srcdir .. "/core/*.h", srcdir .. "/fo4/*.h")
    add_includedirs(srcdir)
    set_pcxxheader(srcdir .. "/pch.h")
    -- MCM menu + default settings
    add_installfiles("res/mcm/config.json", { prefixdir = "MCM/Config/BHOP" })
    add_installfiles("res/mcm/settings.ini", { prefixdir = "MCM/Config/BHOP" })

-- Unit tests for the Half-Life math (src/HLMath.h). Run by the workflow: xmake build bhop_tests && xmake run bhop_tests
target("bhop_tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_hlmath.cpp")

-- Replay tool: runs the movement core on a recording from the game (BHOP_record.csv).
--   xmake build bhop_replay && xmake run bhop_replay path/to/BHOP_record.csv
target("bhop_replay")
    set_kind("binary")
    set_default(false)
    add_files("tests/replay.cpp")
