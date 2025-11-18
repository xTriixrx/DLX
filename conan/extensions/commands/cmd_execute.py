import argparse
import os
import subprocess
from typing import List, Tuple, Callable, Dict

from conan.api.output import ConanOutput
from conan.cli.command import conan_command
from conan.errors import ConanException


def _abs(path: str) -> str:
    return os.path.abspath(path)


def _ensure_file(path: str, label: str) -> None:
    if not os.path.isfile(path):
        raise ConanException(f"{label} '{path}' does not exist.")


def _resolve_binary(build_folder: str, binary_name: str) -> str:
    base = os.path.join(build_folder, binary_name)
    candidates = [base]
    if os.name == "nt":
        candidates.insert(0, base + ".exe")

    for candidate in candidates:
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    raise ConanException(f"Executable '{binary_name}' not found in '{build_folder}'.")


class _PipelineArgumentParser(argparse.ArgumentParser):
    def __init__(self, pipeline_name: str):
        super().__init__(prog=f"conan execute {pipeline_name}")
        self.pipeline_name = pipeline_name

    def error(self, message):
        raise ConanException(f"{self.pipeline_name}: {message}")


def _sudoku_parser() -> argparse.ArgumentParser:
    parser = _PipelineArgumentParser("sudoku")
    parser.add_argument("--puzzle", required=True, help="Path to the Sudoku puzzle text file.")
    parser.add_argument(
        "--answers",
        default=None,
        help="Destination file for decoded Sudoku solutions "
        "(default: <puzzle-name>-answers.txt in the current directory).",
    )
    parser.add_argument(
        "--build-folder",
        default="build",
        help="CMake build folder containing the sudoku binaries (default: build).",
    )
    return parser


def _run_sudoku_pipeline(opts: argparse.Namespace, output: ConanOutput) -> None:
    build_folder = _abs(opts.build_folder)
    puzzle_path = _abs(opts.puzzle)
    default_answers = opts.answers
    if default_answers:
        answers_path = _abs(default_answers)
    else:
        answers_path = _abs(_default_answers_filename(puzzle_path))

    _ensure_file(puzzle_path, "Puzzle file")
    os.makedirs(os.path.dirname(answers_path) or ".", exist_ok=True)

    encoder_bin = _resolve_binary(build_folder, "sudoku_encoder")
    dlx_bin = _resolve_binary(build_folder, "dlx")
    decoder_bin = _resolve_binary(build_folder, "sudoku_decoder")

    encoder_cmd = [encoder_bin, puzzle_path]
    dlx_cmd = [dlx_bin]
    decoder_cmd = [decoder_bin, puzzle_path]

    output.info(
        f"[execute] sudoku pipeline: puzzle={puzzle_path} answers={answers_path} (binary mode)"
    )

    answers_file = open(answers_path, "w")
    try:
        encoder_proc = subprocess.Popen(
            encoder_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE
        )
        dlx_proc = subprocess.Popen(
            dlx_cmd,
            stdin=encoder_proc.stdout,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        encoder_proc.stdout.close()
        decoder_proc = subprocess.Popen(
            decoder_cmd,
            stdin=dlx_proc.stdout,
            stdout=answers_file,
            stderr=subprocess.PIPE,
        )
        dlx_proc.stdout.close()

        decoder_stderr = decoder_proc.communicate()[1]
        dlx_stderr = dlx_proc.communicate()[1]
        encoder_stderr = encoder_proc.communicate()[1]
    except FileNotFoundError as exc:
        raise ConanException(f"Failed to launch subprocess: {exc}")
    finally:
        answers_file.close()

    _raise_on_failure("sudoku_encoder", encoder_proc, encoder_cmd, encoder_stderr)
    _raise_on_failure("dlx", dlx_proc, dlx_cmd, dlx_stderr)
    _raise_on_failure("sudoku_decoder", decoder_proc, decoder_cmd, decoder_stderr)

    output.success(f"[execute] sudoku pipeline complete -> {answers_path}")


def _default_answers_filename(puzzle_path: str) -> str:
    base_name = os.path.splitext(os.path.basename(puzzle_path))[0]
    filename = f"{base_name}-answers.txt"
    return os.path.join(os.getcwd(), filename)


def _raise_on_failure(name: str, proc: subprocess.Popen, cmd: List[str], stderr: bytes) -> None:
    if proc.returncode == 0:
        return
    stderr_text = stderr.decode("utf-8", errors="ignore").strip()
    message = f"{name} failed with exit code {proc.returncode}. Command: {' '.join(cmd)}"
    if stderr_text:
        message += f"\n{stderr_text}"
    raise ConanException(message)


PIPELINES: Dict[str, Tuple[Callable[[], argparse.ArgumentParser], Callable]] = {
    "sudoku": (_sudoku_parser, _run_sudoku_pipeline),
}


def _split_pipeline_invocations(tokens: List[str]) -> List[Tuple[str, List[str]]]:
    invocations: List[Tuple[str, List[str]]] = []
    index = 0
    while index < len(tokens):
        name = tokens[index]
        if name not in PIPELINES:
            available = ", ".join(PIPELINES.keys())
            raise ConanException(f"Unknown pipeline '{name}'. Available: {available}")
        index += 1
        start = index
        while index < len(tokens) and tokens[index] not in PIPELINES:
            index += 1
        invocations.append((name, tokens[start:index]))
    return invocations


@conan_command(group="custom")
def execute(conan_api, parser, *args):
    """
    Execute one or more pre-defined pipelines. Example:
      conan execute sudoku --puzzle tests/sudoku_test.txt --answers solved.txt
    Multiple pipelines can be chained:
      conan execute sudoku --puzzle tests/a.txt sudoku --puzzle tests/b.txt --answers b.txt
    """
    parser.add_argument(
        "pipeline_args",
        nargs=argparse.REMAINDER,
        help="Pipeline invocation pairs, e.g. 'sudoku --puzzle tests/sudoku_test.txt --answers out.txt'",
    )
    ns = parser.parse_args(*args)

    remaining = list(ns.pipeline_args)
    if not remaining:
        options = ", ".join(PIPELINES.keys())
        raise ConanException(
            f"No pipelines specified. Usage: conan execute <pipeline> [args] ... "
            f"(available: {options})"
        )

    output = ConanOutput()

    invocations = _split_pipeline_invocations(remaining)

    for pipeline_name, pipeline_args in invocations:
        parser_factory, runner = PIPELINES[pipeline_name]
        pipeline_parser = parser_factory()
        opts = pipeline_parser.parse_args(pipeline_args)
        runner(opts, output)
