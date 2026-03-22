import os
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parent
BUILD_DIR = ROOT / "Build"
TARGET_NAME = "LauncherQiu-Core.exe" if os.name == "nt" else "LauncherQiu-Core"
DEFAULT_SOURCES = ["test", "account/account"]


def source_to_path(name: str) -> Path:
    path = ROOT / f"{name}.cpp"
    if not path.exists():
        raise FileNotFoundError(f"source file not found: {path}")
    return path


def object_path_for(source: Path) -> Path:
    relative = source.relative_to(ROOT).with_suffix("")
    safe_name = "__".join(relative.parts)
    suffix = ".obj" if os.name == "nt" else ".o"
    return BUILD_DIR / f"{safe_name}{suffix}"


def run(command, env=None):
    print(">", " ".join(str(part) for part in command))
    subprocess.run(command, cwd=ROOT, env=env, check=True)


def run_shell(command: str, env=None):
    print(">", command)
    subprocess.run(command, cwd=ROOT, env=env, check=True, shell=True)


def find_vs_vcvars() -> Path | None:
    if os.name != "nt":
        return None

    env_path = os.environ.get("VSINSTALLDIR")
    if env_path:
        candidate = Path(env_path) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
        if candidate.exists():
            return candidate

    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if not vswhere.exists():
        return None

    result = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    install_path = result.stdout.strip()
    if not install_path:
        return None

    candidate = Path(install_path) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    return candidate if candidate.exists() else None


def find_toolchain() -> str:
    if shutil.which("g++"):
        return "g++"
    if os.name == "nt" and find_vs_vcvars() is not None:
        return "msvc"
    raise RuntimeError("no supported compiler found: expected g++ or Visual Studio C++ tools")


def gpp_link_libs() -> list[str]:
    return ["-lminizip", "-lssl", "-lcrypto", "-lz"]


def vcpkg_root() -> Path:
    configured = os.environ.get("VCPKG_ROOT")
    if configured:
        return Path(configured)
    return Path(r"C:\vcpkg")


def vcpkg_triplet_dir() -> Path:
    path = vcpkg_root() / "installed" / "x64-windows"
    if not path.exists():
        raise RuntimeError(f"vcpkg triplet not found: {path}")
    return path


def build_with_gpp(sources: list[Path]):
    BUILD_DIR.mkdir(exist_ok=True)
    objects = []
    for source in sources:
        obj = object_path_for(source)
        run(
            [
                "g++",
                "-c",
                str(source.relative_to(ROOT)),
                "-std=c++20",
                "-I.",
                "-o",
                str(obj.relative_to(ROOT)),
            ]
        )
        objects.append(obj)

    output = BUILD_DIR / TARGET_NAME
    run(
        [
            "g++",
            *[str(obj.relative_to(ROOT)) for obj in objects],
            "./libs/json_reader.o",
            "./libs/json_value.o",
            "./libs/json_writer.o",
            *gpp_link_libs(),
            "-o",
            str(output.relative_to(ROOT)),
        ]
    )
    run([str(output)])


def build_with_msvc(sources: list[Path]):
    BUILD_DIR.mkdir(exist_ok=True)
    vcvars = find_vs_vcvars()
    if vcvars is None:
        raise RuntimeError("Visual Studio C++ environment not found")

    triplet = vcpkg_triplet_dir()
    include_dir = triplet / "include"
    lib_dir = triplet / "lib"
    bin_dir = triplet / "bin"

    env = os.environ.copy()
    env["PATH"] = str(bin_dir) + os.pathsep + env.get("PATH", "")

    objects = []
    for source in sources:
        obj = object_path_for(source)
        command = (
            f'call "{vcvars}" && '
            f'cl /nologo /utf-8 /std:c++20 /EHsc /I. /I "{include_dir}" '
            f'/c "{source}" /Fo"{obj}"'
        )
        run_shell(command, env=env)
        objects.append(obj)

    output = BUILD_DIR / TARGET_NAME
    link_libs = ["jsoncpp.lib", "minizip.lib", "zlib.lib", "ws2_32.lib"]
    object_args = " ".join(f'"{obj}"' for obj in objects)
    command = (
        f'call "{vcvars}" && '
        f'link /nologo '
        f"{object_args} "
        f'/LIBPATH:"{lib_dir}" {" ".join(link_libs)} '
        f'/OUT:"{output}"'
    )
    run_shell(command, env=env)
    run([str(output)], env=env)


def selected_sources(argv: list[str]) -> list[Path]:
    if len(argv) < 2 or argv[1] == "all":
        names = DEFAULT_SOURCES
    else:
        names = argv[1:]
    return [source_to_path(name) for name in names]


def main():
    sources = selected_sources(sys.argv)
    toolchain = find_toolchain()
    print(f"Using toolchain: {toolchain}")
    if toolchain == "g++":
        build_with_gpp(sources)
    else:
        build_with_msvc(sources)


if __name__ == "__main__":
    main()
