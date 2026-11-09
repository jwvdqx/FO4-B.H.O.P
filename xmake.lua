-- CommonLibF4 is downloaded into lib/commonlibf4 by the GitHub workflow
includes("lib/commonlibf4")

set_project("BHOP")
set_version("0.4.1")
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
    add_headerfiles(srcdir .. "/pch.h")
    add_includedirs(srcdir)
    set_pcxxheader(srcdir .. "/pch.h")
    -- MCM menu + default settings
    add_installfiles("res/mcm/config.json", { prefixdir = "MCM/Config/BHOP" })
    add_installfiles("res/mcm/settings.ini", { prefixdir = "MCM/Config/BHOP" })
