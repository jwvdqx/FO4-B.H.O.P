-- Contextual Ammo: kills with a gun refund ammo of the gun's CURRENT caliber.
-- CommonLibF4 is cloned into lib/commonlibf4 by .github/workflows/contextual-ammo.yml
includes("lib/commonlibf4")

set_project("ContextualAmmo")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

target("ContextualAmmo")
    add_rules("commonlibf4.plugin", {
        name = "ContextualAmmo",
        author = "jwvdqx",
        description = "Global contextual ammo: kills refund ammo for your gun's current caliber"
    })
    add_files("src/main.cpp")
    add_installfiles("res/ContextualAmmo.ini", { prefixdir = "F4SE/Plugins" })

target("ammo_tests")
    set_kind("binary")
    set_default(false)
    add_files("tests/test_formula.cpp")
