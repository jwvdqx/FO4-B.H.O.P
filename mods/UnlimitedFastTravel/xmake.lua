-- Unlimited Fast Travel - Immersive Mode (companion plugin for Unlimited Fast Travel)
-- CommonLibF4 is cloned into lib/commonlibf4 by .github/workflows/uft-immersive.yml
includes("lib/commonlibf4")

add_requires("minhook")

set_project("UnlimitedFastTravelImmersive")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

target("UnlimitedFastTravelImmersive")
    add_rules("commonlibf4.plugin", {
        name = "UnlimitedFastTravelImmersive",
        author = "jwvdqx",
        description = "Immersive Mode for Unlimited Fast Travel: no fast travel at night"
    })
    add_packages("minhook")
    add_files("src/main.cpp")
    add_headerfiles("src/NightRule.h")
    -- Unlimited Fast Travel's MCM page with the Immersive Mode section added
    add_installfiles("res/mcm/config.json", { prefixdir = "MCM/Config/UnlimitedFastTravel" })
    add_installfiles("res/mcm/settings.ini", { prefixdir = "MCM/Config/UnlimitedFastTravel" })

target("uft_tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_nightrule.cpp")
