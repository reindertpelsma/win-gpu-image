"""Use already-installed host C tools for resource-free native policy tests."""

import os
from pathlib import Path
import shutil
import subprocess


def compile_c(sources, output, shared=False, defines=()):
    env = dict(os.environ)
    compiler = next(
        (shutil.which(x) for x in ("cl", "cc", "gcc", "clang") if shutil.which(x)), None
    )
    if compiler is None and os.name == "nt":
        vswhere = Path(env["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        install = subprocess.check_output(
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
            text=True,
            timeout=30,
        ).strip()
        devcmd = Path(install) / "Common7/Tools/VsDevCmd.bat"
        ready = subprocess.check_output(
            ["cmd.exe", "/d", "/s", "/c", f'call "{devcmd}" -arch=x64 >nul && set'],
            text=True,
            timeout=60,
        )
        env = {k.upper(): v for k, v in env.items()}
        for line in ready.splitlines():
            if "=" in line and not line.startswith("="):
                key, value = line.split("=", 1)
                env[key.upper()] = value
        compiler = shutil.which("cl", path=env.get("PATH"))
    if not compiler:
        raise RuntimeError(
            "CPU tests require an installed C compiler; no installation is attempted"
        )
    if Path(compiler).stem.lower() == "cl":
        args = [compiler, "/nologo", "/W4", "/WX", "/std:c11", "/D_CRT_SECURE_NO_WARNINGS"]
        args += [f"/D{x}" for x in defines]
        if shared:
            args += ["/LD"]
        args += [str(s) for s in sources] + [f"/Fe:{output}"]
    else:
        args = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2"]
        args += [f"-D{x}" for x in defines]
        if shared:
            args += ["-shared", "-fPIC"]
        args += [str(s) for s in sources] + ["-o", str(output)]
    subprocess.run(args, cwd=output.parent, env=env, check=True, capture_output=True, timeout=60)
