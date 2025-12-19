#!/usr/bin/env python3
import os
import shutil
import subprocess


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
EXPECTED_TEXT_SOLUTION = os.path.join(REPO_ROOT, "tests", "sudoku_example", "sudoku_solution.txt")
PUZZLE_A = os.path.join("tests", "sudoku_tests", "sudoku_test.txt")
PUZZLE_B = os.path.join("tests", "sudoku_tests", "sudoku_test4.txt")
EXT_SRC_DIR = os.path.join(REPO_ROOT, "conan", "extensions", "commands")
LOCAL_CONAN_HOME = os.path.join(REPO_ROOT, "build", "conan_home_test")
EXT_DST_DIR = os.path.join(LOCAL_CONAN_HOME, "extensions", "commands")
EXT_FILES = ["__init__.py", "cmd_execute.py"]


def _sync_extensions():
    shutil.rmtree(LOCAL_CONAN_HOME, ignore_errors=True)
    os.makedirs(EXT_DST_DIR, exist_ok=True)
    for filename in EXT_FILES:
        shutil.copyfile(os.path.join(EXT_SRC_DIR, filename), os.path.join(EXT_DST_DIR, filename))


_sync_extensions()


def _run_command(cmd):
    env = os.environ.copy()
    env["CONAN_HOME"] = LOCAL_CONAN_HOME
    result = subprocess.run(cmd, cwd=REPO_ROOT, env=env, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"Command failed ({result.returncode}): {' '.join(cmd)}\n"
            f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}"
        )


def _read_file(path):
    with open(path, "r", encoding="utf-8") as handle:
        return handle.read()


def _remove_if_exists(path):
    try:
        os.remove(path)
    except FileNotFoundError:
        pass


def test_single_pipeline_generates_expected_answers():
    answers_path = os.path.join(REPO_ROOT, "sudoku_test-answers.txt")
    _remove_if_exists(answers_path)

    _run_command(
        [
            "conan",
            "execute",
            "sudoku",
            "--puzzle",
            PUZZLE_A,
            "--build-folder",
            "build",
        ]
    )

    assert os.path.isfile(answers_path), "Answers file was not generated"
    actual = _read_file(answers_path)
    expected = _read_file(EXPECTED_TEXT_SOLUTION)
    assert actual == expected, "Answers file does not match expected text solution"

    _remove_if_exists(answers_path)


def test_multiple_pipelines_run_in_order_without_collisions():
    answers_a = os.path.join(REPO_ROOT, "sudoku_test-answers.txt")
    answers_b = os.path.join(REPO_ROOT, "sudoku_test4-answers.txt")
    _remove_if_exists(answers_a)
    _remove_if_exists(answers_b)

    _run_command(
        [
            "conan",
            "execute",
            "sudoku",
            "--puzzle",
            PUZZLE_A,
            "--build-folder",
            "build",
            "sudoku",
            "--puzzle",
            PUZZLE_B,
            "--build-folder",
            "build",
        ]
    )

    assert os.path.isfile(answers_a), "First answers file missing"
    assert os.path.isfile(answers_b), "Second answers file missing"

    assert "Solution" in _read_file(answers_a)
    assert "Solution" in _read_file(answers_b)

    _remove_if_exists(answers_a)
    _remove_if_exists(answers_b)


if __name__ == "__main__":
    _sync_extensions()
    test_single_pipeline_generates_expected_answers()
    test_multiple_pipelines_run_in_order_without_collisions()
