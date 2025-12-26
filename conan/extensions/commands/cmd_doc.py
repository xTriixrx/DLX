import os
import shutil
import subprocess
from conan.errors import ConanException
from conan.api.output import ConanOutput
from conan.cli.command import conan_command


@conan_command(group="custom")
def doc(conan_api, parser, *args):
    """
    Generate documentation (Doxygen XML + Sphinx HTML).
    Usage: conan doc . [--build-folder build] [--sphinx-output docs/build/sphinx]
    """
    parser.add_argument(
        "--build-folder",
        default="build",
        help="Path to the CMake build directory (used by sphinx-build for relative includes).",
    )
    parser.add_argument(
        "--sphinx-output",
        default="docs/build/sphinx",
        help="Directory where the Sphinx HTML output will be generated.",
    )
    parser.add_argument(
        "--sphinx-builder",
        default="html",
        help="Sphinx builder (default: html).",
    )
    ns = parser.parse_args(*args)

    repo_root = os.path.abspath(".")
    docs_dir = os.path.join(repo_root, "docs")
    doxyfile_path = os.path.join(docs_dir, "Doxyfile")
    sphinx_source = os.path.join(docs_dir, "sphinx")
    sphinx_output = os.path.abspath(ns.sphinx_output)

    if not os.path.isfile(doxyfile_path):
        raise ConanException(f"Doxyfile not found at {doxyfile_path}")

    if not os.path.isdir(sphinx_source):
        raise ConanException(f"Sphinx source directory not found at {sphinx_source}")

    output = ConanOutput()

    doxygen_output_dir = os.path.join(docs_dir, "build", "doxygen")
    os.makedirs(doxygen_output_dir, exist_ok=True)

    doxygen_env = os.environ.copy()
    doxygen_env["CMAKE_BUILD_DIR"] = os.path.abspath(ns.build_folder)
    output.info("Running Doxygen to generate XML...")
    _run(["doxygen", doxyfile_path], env=doxygen_env)

    os.makedirs(sphinx_output, exist_ok=True)
    output.info("Running Sphinx to generate HTML...")
    sphinx_exe = shutil.which("sphinx-build")
    if sphinx_exe:
        sphinx_cmd = [
            sphinx_exe,
            "-b",
            ns.sphinx_builder,
            "-j",
            "1",
            sphinx_source,
            sphinx_output,
        ]
    else:
        python3_exe = shutil.which("python3")
        if not python3_exe:
            raise ConanException("Unable to find 'sphinx-build' or 'python3' to run Sphinx.")
        sphinx_cmd = [
            python3_exe,
            "-m",
            "sphinx",
            "-b",
            ns.sphinx_builder,
            "-j",
            "1",
            sphinx_source,
            sphinx_output,
        ]
    _run(sphinx_cmd)

    output.success(f"Documentation generated in {sphinx_output}")


def _run(cmd, env=None, cwd=None):
    result = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=cwd)
    if result.returncode != 0:
        raise ConanException(
            f"Command '{' '.join(cmd)}' failed with code {result.returncode}:\n{result.stderr}"
        )
