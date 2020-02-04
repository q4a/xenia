include("tools/build")
require("third_party/premake-export-compile-commands/export-compile-commands")

location(build_root)
targetdir(build_bin)
objdir(build_obj)

-- Define an ARCH variable
-- Only use this to enable architecture-specific functionality.
if os.istarget("linux") then
  ARCH = os.outputof("uname -p")
else
  ARCH = "unknown"
end

includedirs({
  ".",
  "src",
  "third_party",
})

defines({
  "_UNICODE",
  "UNICODE",
})

-- TODO(DrChat): Find a way to disable this on other architectures.
if ARCH ~= "ppc64" then
  filter("architecture:x86_64")
    vectorextensions("AVX")
  filter({})
end

characterset("Unicode")
flags({
  --"ExtraWarnings",        -- Sets the compiler's maximum warning level.
  "FatalWarnings",        -- Treat warnings as errors.
})

filter("kind:StaticLib")
  defines({
    "_LIB",
  })

filter("configurations:Checked")
  runtime("Debug")
  defines({
    "DEBUG",
  })
  runtime("Debug")
filter({"configurations:Checked", "platforms:Windows"})
  buildoptions({
    "/RTCsu",   -- Full Run-Time Checks.
  })

filter("configurations:Debug")
  runtime("Debug")
  defines({
    "DEBUG",
    "_NO_DEBUG_HEAP=1",
  })
  runtime("Release")
filter({"configurations:Debug", "platforms:Windows"})
  linkoptions({
    "/NODEFAULTLIB:MSVCRTD",
  })

filter({"configurations:Debug", "platforms:Linux"})
  buildoptions({
    "-g",
  })

filter("configurations:Release")
  runtime("Release")
  defines({
    "NDEBUG",
    "_NO_DEBUG_HEAP=1",
  })
  optimize("speed")
  inlining("Auto")
  floatingpoint("Fast")
  flags({
    "LinkTimeOptimization",
  })
  runtime("Release")
filter({"configurations:Release", "platforms:Windows"})
  linkoptions({
    "/NODEFAULTLIB:MSVCRTD",
  })

filter("platforms:Linux")
  system("linux")
  toolset("clang")
  buildoptions({
    -- "-mlzcnt",  -- (don't) Assume lzcnt is supported.
    "`pkg-config --cflags gtk+-x11-3.0`",
    "-fno-lto", -- Premake doesn't support LTO on clang
  })
  links({
    "pthread",
    "dl",
    "lz4",
    "rt",
  })
  linkoptions({
    "`pkg-config --libs gtk+-3.0`",
  })

filter({"platforms:Linux", "kind:*App"})
  linkgroups("On")

filter({"platforms:Linux", "language:C++", "toolset:gcc"})
  buildoptions({
    "-std=c++14",
  })
  links({
  })
  disablewarnings({
    "unused-result"
  })

filter({"platforms:Linux", "toolset:gcc"})
  if ARCH == "ppc64" then
    buildoptions({
      "-m32",
      "-mpowerpc64"
    })
    linkoptions({
      "-m32",
      "-mpowerpc64"
    })
  end

filter({"platforms:Linux", "language:C++", "toolset:clang"})
  links({
    "c++",
    "c++abi"
  })
  disablewarnings({
    "deprecated-register"
  })
filter({"platforms:Linux", "language:C++", "toolset:clang", "files:*.cc or *.cpp"})
  buildoptions({
    "-std=c++14",
    "-stdlib=libstdc++",
  })

filter("platforms:Windows")
  system("windows")
  toolset("msc")
  buildoptions({
    "/MP",      -- Multiprocessor compilation.
    "/wd4100",  -- Unreferenced parameters are ok.
    "/wd4201",  -- Nameless struct/unions are ok.
    "/wd4512",  -- 'assignment operator was implicitly defined as deleted'.
    "/wd4127",  -- 'conditional expression is constant'.
    "/wd4324",  -- 'structure was padded due to alignment specifier'.
    "/wd4189",  -- 'local variable is initialized but not referenced'.
    "/utf-8",   -- 'build correctly on systems with non-Latin codepages'.
  })
  flags({
    "NoMinimalRebuild", -- Required for /MP above.
  })

  symbols("On")
  defines({
    "_CRT_NONSTDC_NO_DEPRECATE",
    "_CRT_SECURE_NO_WARNINGS",
    "WIN32",
    "_WIN64=1",
    "_AMD64=1",
  })
  linkoptions({
    "/ignore:4006",  -- Ignores complaints about empty obj files.
    "/ignore:4221",
  })
  links({
    "ntdll",
    "wsock32",
    "ws2_32",
    "xinput",
    "comctl32",
    "shcore",
    "shlwapi",
    "dxguid",
  })

-- Create scratch/ path
if not os.isdir("scratch") then
  os.mkdir("scratch")
end

solution("xenia")
  uuid("931ef4b0-6170-4f7a-aaf2-0fece7632747")
  startproject("xenia-app")
  architecture("x86_64")
  if os.istarget("linux") then
    platforms({"Linux"})
  elseif os.istarget("windows") then
    platforms({"Windows"})
    -- Minimum version to support ID3D12GraphicsCommandList1 (for
    -- SetSamplePositions).
    filter("action:vs2017")
      systemversion("10.0.15063.0")
    filter("action:vs2019")
      systemversion("10.0")
    filter({})
  end
  configurations({"Checked", "Debug", "Release"})

  include("third_party/aes_128.lua")
  include("third_party/capstone.lua")
  include("third_party/dxbc.lua")
  include("third_party/discord-rpc.lua")
  include("third_party/cxxopts.lua")
  include("third_party/cpptoml.lua")
  include("third_party/glslang-spirv.lua")
  include("third_party/imgui.lua")
  include("third_party/libav.lua")
  include("third_party/mspack.lua")
  include("third_party/snappy.lua")
  include("third_party/spirv-tools.lua")
  include("third_party/volk.lua")
  include("third_party/xxhash.lua")
  include("third_party/yaml-cpp.lua")

  include("src/xenia")
  include("src/xenia/app")
  include("src/xenia/app/discord")
  include("src/xenia/apu")
  include("src/xenia/apu/nop")
  include("src/xenia/base")
  include("src/xenia/cpu")
  include("src/xenia/cpu/backend/x64")
  include("src/xenia/debug/ui")
  include("src/xenia/gpu")
  include("src/xenia/gpu/null")
  include("src/xenia/gpu/vk")
  include("src/xenia/gpu/vulkan")
  include("src/xenia/hid")
  include("src/xenia/hid/nop")
  include("src/xenia/hid/sdl")
  include("src/xenia/kernel")
  include("src/xenia/ui")
  include("src/xenia/ui/spirv")
  include("src/xenia/ui/vk")
  include("src/xenia/ui/vulkan")
  include("src/xenia/vfs")

  if os.istarget("windows") then
    include("src/xenia/apu/xaudio2")
    include("src/xenia/gpu/d3d12")
    include("src/xenia/hid/winkey")
    include("src/xenia/hid/xinput")
    include("src/xenia/ui/d3d12")
  end
