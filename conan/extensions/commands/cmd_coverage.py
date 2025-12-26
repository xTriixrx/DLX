import os
import subprocess
import shutil
import glob
from conan.errors import ConanException
from conan.api.output import ConanOutput
from conan.cli.command import conan_command

@conan_command(group="custom")
def coverage(conan_api, parser, *args):
    """
    Generate gcovr coverage reports (HTML/XML) from the specified build folder.
    Usage: conan coverage . --build-folder build
    """
    parser.add_argument(
        "path",
        nargs="?",
        default=".",
        help="Path to the project root (default: current directory).",
    )
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

    repo_root = os.path.abspath(ns.path)
    build_dir = ns.build_folder
    if not os.path.isabs(build_dir):
        build_dir = os.path.join(repo_root, build_dir)
    build_dir = os.path.abspath(build_dir)
    if not os.path.isdir(build_dir):
        raise ConanException(f"Build folder '{build_dir}' does not exist.")

    object_dir = os.path.join(build_dir, "CMakeFiles")
    gcda_files = glob.glob(os.path.join(object_dir, "**", "*.gcda"), recursive=True)
    gcno_files = glob.glob(os.path.join(object_dir, "**", "*.gcno"), recursive=True)
    if not gcda_files and not gcno_files:
        raise ConanException("No gcov symbols found. Please run tests in Debug mode before coverage.")

    coverage_dir = os.path.join(build_dir, "coverage")
    os.makedirs(coverage_dir, exist_ok=True)

    summary_html = os.path.join(coverage_dir, "index.html")
    coverage_target_dir = ns.coverage_out_dir
    coverage_xml = os.path.join(coverage_dir, "coverage.xml")
    coverage_lcov = os.path.join(coverage_dir, "coverage.lcov")
    cpp_lcov = os.path.join(coverage_dir, "coverage.cpp.lcov")
    source_root = repo_root

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
            "--lcov",
            "-o",
            cpp_lcov,
        ]
    )

    rust_lcov = _generate_rust_coverage(source_root, coverage_dir, output)
    _merge_lcov_files(cpp_lcov, rust_lcov, coverage_lcov, output)
    summary_html_path = _generate_combined_html(coverage_lcov, coverage_dir, source_root, output)
    if summary_html_path:
        summary_html = summary_html_path

    if coverage_target_dir:
        destination = os.path.abspath(coverage_target_dir)
        os.makedirs(destination, exist_ok=True)
        for filename in os.listdir(coverage_dir):
            src_path = os.path.join(coverage_dir, filename)
            dst_path = os.path.join(destination, filename)
            if os.path.isdir(src_path):
                if os.path.exists(dst_path):
                    shutil.rmtree(dst_path)
                shutil.copytree(src_path, dst_path)
            elif filename.endswith((".html", ".css", ".xml", ".lcov")):
                shutil.copyfile(src_path, dst_path)

    output.info(
        f"Coverage reports generated: {summary_html} (HTML), "
        f"{coverage_xml} (XML), {coverage_lcov} (LCOV).\n"
        + (f"Copied all coverage report files to {coverage_target_dir}." if coverage_target_dir else "")
    )

def _generate_rust_coverage(source_root, coverage_dir, output):
    workspace_dir = os.path.join(source_root, "scheduler-rs")
    if not os.path.isdir(workspace_dir):
        output.info("scheduler-rs workspace not found; skipping Rust coverage.")
        return None

    cargo = shutil.which("cargo")
    cargo_llvm_cov = shutil.which("cargo-llvm-cov")
    if not cargo or not cargo_llvm_cov:
        output.warning("cargo llvm-cov not found; skipping Rust coverage generation.")
        return None

    rust_cov_root = os.path.join(coverage_dir, "scheduler-rs")
    os.makedirs(rust_cov_root, exist_ok=True)
    lcov_path = os.path.join(rust_cov_root, "coverage.lcov")

    output.info("Generating Rust coverage via cargo llvm-cov...")
    _run_command(
        [
            cargo,
            "llvm-cov",
            "--workspace",
            "--lcov",
            "--output-path",
            lcov_path,
        ],
        cwd=workspace_dir,
    )
    return lcov_path


def _run_command(cmd, cwd=None):
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd)
    if result.returncode != 0:
        raise ConanException(
            f"Command '{' '.join(cmd)}' failed with code {result.returncode}:\n{result.stderr}"
        )


def _merge_lcov_files(cpp_lcov, rust_lcov, combined_path, output):
    try:
        shutil.copyfile(cpp_lcov, combined_path)
    except OSError as exc:
        raise ConanException(f"Failed to write combined LCOV file: {exc}") from exc

    if rust_lcov and os.path.isfile(rust_lcov):
        output.info("Merging Rust LCOV data into combined report...")
        with open(combined_path, "a", encoding="utf-8") as combined, open(rust_lcov, "r", encoding="utf-8") as rust_file:
            combined.write("\n")
            combined.write(rust_file.read())


def _generate_combined_html(lcov_path, output_dir, source_root, output):
    genhtml = shutil.which("genhtml")
    if not genhtml:
        output.warning("genhtml not found; skipping HTML generation.")
        return None

    genhtml_cmd = [
        genhtml,
        lcov_path,
        "--output-directory",
        output_dir,
        "--title",
        "DLX Coverage Report",
        "--prefix",
        source_root,
        "--legend",
    ]

    ignore_error_categories = ["format", "unsupported", "category", "inconsistent", "corrupt"]
    for category in ignore_error_categories:
        genhtml_cmd.extend(["--ignore-errors", category])

    _run_command(genhtml_cmd)
    return os.path.join(output_dir, "index.html")
