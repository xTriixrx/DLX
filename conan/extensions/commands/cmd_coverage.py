import os
import subprocess
import shutil
from conan.errors import ConanException
from conan.api.output import ConanOutput
from conan.cli.command import conan_command
import glob

@conan_command(group="custom")
def coverage(conan_api, parser, *args):
    """
    Generate gcovr coverage reports (HTML/XML) from the specified build folder.
    Usage: conan coverage . --build-folder build
    """
    parser.add_argument(
        "--build-folder",
        default="build",
        help="Path to the CMake build directory that contains compiled objects/tests.",
    )
    parser.add_argument(
        "--cov-path",
        dest="coverage_out_dir",
        default=None,
        help="Destination directory to copy coverage HTML files (e.g., build/coverage-report).",
    )
    parser.add_argument(
        "--theme",
        dest="coverage_theme",
        default="github.dark-green",
        help="gcovr HTML theme (default: github.dark-green).",
    )
    ns = parser.parse_args(*args)

    build_dir = os.path.abspath(ns.build_folder)
    if not os.path.isdir(build_dir):
        raise ConanException(f"Build folder '{build_dir}' does not exist.")

    object_dir = os.path.join(build_dir, "CMakeFiles")
    gcda_files = glob.glob(os.path.join(object_dir, "**", "*.gcda"), recursive=True)
    gcno_files = glob.glob(os.path.join(object_dir, "**", "*.gcno"), recursive=True)
    if not gcda_files and not gcno_files:
        raise ConanException("No gcov symbols found. Please run tests in Debug mode before coverage.")

    coverage_dir = os.path.join(build_dir, "coverage")
    os.makedirs(coverage_dir, exist_ok=True)

    default_html = os.path.join(coverage_dir, "coverage.html")
    coverage_target_dir = ns.coverage_out_dir
    coverage_xml = os.path.join(coverage_dir, "coverage.xml")
    source_root = os.path.abspath(os.path.join(build_dir, os.pardir))

    output = ConanOutput()
    _run_command(
        [
            "gcovr",
            "--root",
            source_root,
            "--object-directory",
            build_dir,
            "--print-summary",
            "--xml",
            "-o",
            coverage_xml,
        ]
    )
    _run_command(
        [
            "gcovr",
            "--root",
            source_root,
            "--object-directory",
            build_dir,
            "--html-details",
            "--html-theme",
            ns.coverage_theme,
            "-o",
            default_html,
        ]
    )

    if coverage_target_dir:
        destination = os.path.abspath(coverage_target_dir)
        os.makedirs(destination, exist_ok=True)
        for filename in os.listdir(coverage_dir):
            if filename.endswith(".html") or filename.endswith(".css") or filename.endswith(".xml"):
                shutil.copyfile(
                    os.path.join(coverage_dir, filename),
                    os.path.join(destination, filename),
                )

    output.info(
        f"Coverage reports generated: {default_html} (HTML), {coverage_xml} (XML).\n"
        + (f"Copied all coverage report files to {coverage_target_dir}." if coverage_target_dir else "")
    )

def _run_command(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise ConanException(
            f"Command '{' '.join(cmd)}' failed with code {result.returncode}:\n{result.stderr}"
        )
