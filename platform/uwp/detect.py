import methods
import os
import sys
import platform
from platform_methods import detect_arch
from SCons.Script import Tool

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from SCons import Environment


def is_active():
    return True


def get_name():
    return "UWP"


def can_build():
    return platform.system() == "Windows"


def get_opts():
    return []


def get_flags():
    return [
        ("arch", detect_arch()),
        ("xaudio2", True),
        ("builtin_pcre2_with_jit", False),
        ("d3d12", True),
        ("vulkan", False),
        ("opengl3", False),
        ("module_glslang_enabled", False),
        ("module_raycast_enabled", False),  # For Embree
    ]


def configure(env: "Environment"):
    # Validate arch.
    supported_arches = ["x86_32", "x86_64", "arm32"]
    if env["arch"] not in supported_arches:
        print(
            'Unsupported CPU architecture "%s" for UWP. Supported architectures are: %s.'
            % (env["arch"], ", ".join(supported_arches))
        )
        sys.exit()

    env.msvc = True

    # env["MSVC_SCRIPT_ARGS"] = "store"
    # env["TARGET_ARCH"] = env["arch"]
    # Tool("msvc")(env)

    # env = env.Clone(MSVC_SCRIPT_ARGS=['store'])

    ## Build type

    if env["target"] == "template_release":
        env.Append(CCFLAGS=["/MD"])
        env.Append(LINKFLAGS=["/SUBSYSTEM:WINDOWS"])
        if env["optimize"] != "none":
            env.Append(CCFLAGS=["/O2", "/GL"])
            env.Append(LINKFLAGS=["/LTCG"])

    elif env["target"] == "template_debug":
        env.Append(CCFLAGS=["/MDd"])
        # env.Append(LINKFLAGS=["/SUBSYSTEM:WINDOWS"])
        env.Append(LINKFLAGS=["/SUBSYSTEM:CONSOLE"])
        # env.AppendUnique(CPPDEFINES=["WINDOWS_SUBSYSTEM_CONSOLE"])
        if env["optimize"] != "none":
            env.Append(CCFLAGS=["/O2", "/Zi"])
        else:
            env.Append(LINKFLAGS=["/DEBUG"])

    elif env["target"] == "editor":
        env.Append(CCFLAGS=["/Zi", "/FS"])
        env.Append(CCFLAGS=["/MDd"])
        # env.Append(LINKFLAGS=["/SUBSYSTEM:WINDOWS"])
        env.Append(LINKFLAGS=["/SUBSYSTEM:CONSOLE"])
        # env.AppendUnique(CPPDEFINES=["WINDOWS_SUBSYSTEM_CONSOLE"])
        env.Append(LINKFLAGS=["/DEBUG"])

    # Force to use Unicode encoding
    env.AppendUnique(CCFLAGS=["/utf-8"])

    ## Architecture

    arch = env["arch"]
    if arch == "auto":
        if str(os.getenv("Platform")).lower() == "arm":
            print("Compiled program architecture will be an ARM executable (forcing arch=arm32).")

            arch = "arm"
            env["arch"] = "arm32"

        else:
            compiler_version_str = methods.detect_visual_c_compiler_version(env["ENV"])

            if compiler_version_str == "amd64" or compiler_version_str == "x86_amd64":
                env["arch"] = "x86_64"
                print("Compiled program architecture will be a x64 executable (forcing arch=x86_64).")
            elif compiler_version_str == "x86" or compiler_version_str == "amd64_x86":
                env["arch"] = "x86_32"
                print("Compiled program architecture will be a x86 executable (forcing arch=x86_32).")
            else:
                print(
                    "Failed to detect MSVC compiler architecture version... Defaulting to x86 32-bit executable settings"
                    " (forcing arch=x86_32). Compilation attempt will continue, but SCons can not detect for what architecture"
                    " this build is compiled for. You should check your settings/compilation setup."
                )
                env["arch"] = "x86_32"

    if arch == "x86_32":
        env.Append(LINKFLAGS=["/MACHINE:X86"])
    elif arch == "x86_64":
        env.Append(LINKFLAGS=["/MACHINE:X64"])
    elif arch == "arm32":
        env.Append(LINKFLAGS=["/MACHINE:ARM"])

    ## Compile flags

    if env["builtin_icu4c"]:
        env.Append(CPPDEFINES=["U_PLATFORM_HAS_WINUWP_API"])

    env.Prepend(CPPPATH=["#platform/uwp", "#drivers/windows"])
    env.Append(CPPDEFINES=["UWP_ENABLED", "TYPED_METHOD_BIND"])
    env.Append(CPPDEFINES=[("PNG_ABORT", "abort")])
    winver = "0x0A00"  # Windows 10 is the minimum target for UWP build
    env.Append(CPPDEFINES=[("WINVER", winver), ("_WIN32_WINNT", winver), "WIN32"])
    env.Append(
        CPPDEFINES=[
            # "_UNICODE",
            # "UNICODE",
            ("WINAPI_FAMILY", "WINAPI_FAMILY_APP"),
            # 'WIN32_LEAN_AND_MEAN',
            "WINRT_LEAN_AND_MEAN",
            "__WRL_NO_DEFAULT_LIB__",
        ]
    )
    env.Append(
        CXXFLAGS=[
            "/ZW:nostdlib",
            "/FUplatform.winmd",
        ]
    )
    env.Append(
        CCFLAGS=[
            "/FIplatform/uwp/shim.h",
        ]
    )

    ## Link flags

    env.Append(
        LINKFLAGS=[
            "/MANIFEST:NO",
            "/NXCOMPAT",
            "/DYNAMICBASE",
            "/WINMD",
            "/APPCONTAINER",
            "/NOLOGO",
            '/NODEFAULTLIB:"kernel32.lib"',
            '/NODEFAULTLIB:"ole32.lib"',
        ]
    )

    LIBS = [
        "WindowsApp",
        # "mincore",
        # "ws2_32",
        # "bcrypt",
    ]
    env.Prepend(LINKFLAGS=[p + ".lib" for p in LIBS])

    # Incremental linking fix
    env["BUILDERS"]["ProgramOriginal"] = env["BUILDERS"]["Program"]
    env["BUILDERS"]["Program"] = methods.precious_program

    # Export("env")
