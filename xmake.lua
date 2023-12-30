add_rules("mode.debug", "mode.release")

option("test")
    set_default(false)
    set_showmenu(true)
    set_description("Enable Test")
option_end()

includes("nekoav")
includes("tests")