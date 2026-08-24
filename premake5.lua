local to = ".build/"..(_ACTION or "nullaction")

if _ACTION == "gmake2" then
    error("Use `premake5 gmake` instead; gmake2 neglects to link resources.")
end

if _PREMAKE_VERSION:find("^4") then
    error("Requires premake 5.0.0-beta8 or newer.")
elseif (tonumber(_PREMAKE_VERSION:match("^5%.0%.0.beta(.*)") or "0") or 0) < 8 then
    error("Requires premake 5.0.0-beta8 or newer.")
end


--------------------------------------------------------------------------------
local function init_configuration(cfg)
    filter {cfg}
        defines("BUILD_"..cfg:upper())
        targetdir(to.."/bin/%{cfg.buildcfg}/%{cfg.platform}")
        objdir(to.."/obj/")
end

--------------------------------------------------------------------------------
workspace("tib")
    configurations({"debug", "release"})
    platforms({"x64"})
    location(to)

    characterset("Unicode")
    manifest("off")
    staticruntime("on")
    symbols("on")
    exceptionhandling("off")

    init_configuration("release")
    init_configuration("debug")

    filter "debug"
        rtti("on")
        optimize("off")
        defines("DEBUG")
        defines("_DEBUG")

    filter "release"
        rtti("off")
        optimize("full")
        omitframepointer("on")
        defines("NDEBUG")

    filter {"release", "action:vs*"}
        linktimeoptimization("on")

    -- filter "action:vs*"
    --     defines("_HAS_EXCEPTIONS=0")

--------------------------------------------------------------------------------
project("tib")
    fatalwarnings("all")
    language("c++")
    kind("staticlib")

    includedirs("include")
    files("tib/*.cpp")

--------------------------------------------------------------------------------
project("tib_host")
    fatalwarnings("all")
    language("c++")
    kind("staticlib")

    includedirs("include")
    files("host/*.cpp")

--------------------------------------------------------------------------------
project("tib_wcwidth")
    fatalwarnings("all")
    language("c++")
    kind("staticlib")

    includedirs("include")
    files("wcwidth/*.cpp")

--------------------------------------------------------------------------------
project("test")
    fatalwarnings("all")
    language("c++")
    kind("consoleapp")

    exceptionhandling("on")

    targetname("test")
    links("tib")
    links("tib_host")
    links("tib_wcwidth")

    includedirs("include")
    files("test/*.cpp")
    files("test/test.rc")

--------------------------------------------------------------------------------
project("example")
    fatalwarnings("all")
    language("c++")
    kind("consoleapp")

    targetname("example")
    links("tib")
    links("tib_host")
    links("tib_wcwidth")

    includedirs("include")
    files("example/*.cpp")
    files("example/example.rc")

