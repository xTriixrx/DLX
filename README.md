# DLX

<p align="center">
DLX is an efficient recursive, nondeterministic, depth-first search, backtracking algorithm that uses a technique to hide and unhide nodes from a circular doubly linked list in order to find all solutions to some exact cover problem. Many problems can be structured as a complete cover problem such as sudoku puzzles, tiling problems, and even simple scheduling problems. This algorithm was popularlized by Donald Knuth through his "The Art of Computer Programming" series, however gives credit to the algorithm to Hiroshi Hitotsumatsu and Kōhei Noshita with having invented the idea in 1979.

This specific implementation implements the sparse matrix approach and the latest revision of the algorithm, where only 1's are stored in the matrix and the need for linking non header nodes left to right are not necessary. This implementation deploys the use of "spacer nodes" which essentially is a way to determine the current row as well as changing between rows while the searching "dance" is taking place.

</p>

<p align="center"> <img src="https://github.com/xTriixrx/DLX/blob/master/imgs/matrix-structure.png" /> </p>
<p align="center"> <img src="https://github.com/xTriixrx/DLX/blob/master/imgs/algorithmx-description.png" /> </p>
<p align="center"> <img src="https://github.com/xTriixrx/DLX/blob/master/imgs/cover-psuedocode.png" /> </p>
<p align="center"> <img src="https://github.com/xTriixrx/DLX/blob/master/imgs/hide-psuedocode.png" /> </p>
<p align="center"> <img src="https://github.com/xTriixrx/DLX/blob/master/imgs/uncover-psuedocode.png" /> </p>
<p align="center"> <img src="https://github.com/xTriixrx/DLX/blob/master/imgs/unhide-psuedocode.png" /> </p>

## Build Requirements

- Conan 2.x
- Python 3.9+ (for Conan plugins, documentation tooling, and gcovr/sphinx helpers)
- C++17 & C11 compliant compiler (`g++`, `clang`, MSVC)
- CMake 3.15+
- GCC-style coverage tooling if you plan to run `conan coverage`:
  - `gcov` ships with GCC (install `sudo apt install build-essential` on Ubuntu or `xcode-select --install` on macOS for clang+gcov).
  - `gcovr` for HTML/XML reports (`pip install gcovr` or `brew install gcovr`).
- Documentation toolchain if you plan to run `conan doc` or the manual pipeline:
  - `doxygen` 1.15+ (`brew install doxygen`, `sudo apt install doxygen`, or grab prebuilt binaries from <https://www.doxygen.nl/download.html>).
  - Sphinx + Breathe (installed via `pip install -r docs/sphinx/requirements.txt` or the top-level `requirements.txt` which delegates to the same file).

Example setup commands:

```bash
# macOS (Homebrew)
brew install conan doxygen gcovr
python3 -m pip install -r requirements.txt

# Ubuntu (APT)
sudo apt update
sudo apt install build-essential cmake python3-pip doxygen gcovr
pip3 install --user -r requirements.txt
```

### Conan Workflow

The project ships with a `conanfile.py` plus custom Conan commands to streamline common developer tasks (build/test, coverage, documentation). Before installing dependencies, configure the provided profile and remotes (once per environment):

```bash
conan config install conan  # installs conan/remotes.json, profiles/default & plugins/commands
```

If you prefer to isolate Conan in a Python virtual environment, run:

```bash
python3 -m venv .venv
source .venv/bin/activate   # On Windows use .venv\Scripts\activate
pip install -r requirements.txt
```

Then configure/build/test:

```bash
# configure dependencies, then invoke the build (which runs ctest automatically in Debug)
conan install . && conan build . -s build_type=Debug

# optional: generate coverage report (requires gcov/gcovr)
conan coverage . --build-folder build-debug --cov-path coverage/ --theme github.dark-green

# optional: generate Doxygen XML + Sphinx HTML docs (requires doxygen+sphinx)
CONAN_HOME=$(pwd)/conan PYTHONPATH=$(pwd) conan doc --build-folder build-debug --sphinx-output docs/build/sphinx
```

`conan build` automatically invokes `ctest` when tests are enabled, so the pipeline stays in sync with the CMake setup. To clean everything, remove the `build/` directory (`rm -rf build`) and deactivate/delete the virtual environment when you’re done.

> **Note:** If you already have GoogleTest installed system-wide (e.g., via Homebrew), the build will use that installation. Otherwise, Conan will fetch and build `gtest` from ConanCenter (pass `--build gtest` if a binary isn’t available for your platform).

#### Conan Configuration

You must ensure you have Conan 2.x installed on your build machine. The repo includes the recommended remotes/profile/commands, so you can bootstrap everything with:

```bash
conan config install conan
```

This copies `conan/remotes.json`, `conan/profiles/default`, and the custom command plugins (`coverage`, `doc`) into your Conan home. If you have an existing profile you want to keep, back it up before running the install. Afterward, tweak `~/.conan2/profiles/default` to match your `os`, `arch`, and compiler if needed.

## Sudoku Solver

### Building

Within the cloned repository's main folder `/your_path/DLX`, you can execute the following commands to build the necessary binaries (Debug profile recommended for coverage flags):

```bash
conan install . && conan build . -s build_type=Debug
```

To collect code coverage, keep building in `Debug` mode and leverage the custom command:

```bash
conan coverage . --build-folder build --cov-path build/coverage-report --theme github.dark-green
```

This emits both `build/coverage/coverage.html` (detailed report) and `build/coverage/coverage.xml` (Cobertura format). Use `--cov-path <dir>` to copy the HTML outputs into an additional directory if needed.

### Execution

#### Sudoku Encoder

The `sudoku_encoder` application takes a sudoku problem file and converts the problem to a complete cover matrix representation in an output file. The required parameters to run `sudoku_encoder` is the following:

```bash
./sudoku_encoder [-t] <problem_file> <cover_output_path>
```

- Default mode writes a DLX binary cover stream.
- `-t`/`--text` writes a human-readable cover file.

An example that emits the default binary stream:
```bash
./sudoku_encoder sudoku_test.txt sudoku_cover.bin
```

All `sudoku_test*.txt` files under the `tests/` directory can be used as input into the sudoku encoder. To produce text for inspection, add `-t`:
```bash
./sudoku_encoder -t sudoku_test.txt sudoku_cover.txt
```
The `sudoku_cover.txt` file under the `tests/` directory shows an example of what one of these cover files look like.

To stream the cover matrix directly to another process, omit the destination path or pass `-` to write to stdout. This allows shell pipelines such as:
```bash
./sudoku_encoder -t tests/sudoku_test.txt - > tests/tmp_cover.txt
```
The same `-` shorthand works for reading puzzles from stdin (e.g., `cat puzzle.txt | ./sudoku_encoder - -`).

##### Sudoku Eecoder Binary Interface

Binary is now the default interface—no extra flags are necessary. The `sudoku_cover.bin` file path will be the binary outputed by the sudoku encoder.

##### Generating your own Sudoku Input File

If you would like to put in your own solvable sudoku problem into the pipeline, you can create a file of the below format. This example is `sudoku_test3.txt` in the `tests/` directory:

```
200301004
600900000
003060001
000600040
475192368
030004000
300070800
000003002
100806005
```

#### DLX

The dlx application takes a complete cover matrix as an input file and will find every possible solution permutation for the complete cover matrix into an output file. The required parameters to run dlx is the following:

```bash
./dlx [-t] <cover_output> <solution_output_path>
```

- Default mode expects a DLX binary cover file and writes binary solution rows.
- `-t` instructs `dlx` to read a text cover matrix and emit text solution rows for easier inspection.

An example of this command using one of the provided test files:
```bash
./dlx sudoku_cover.bin dlx_solution_output.bin
```

Use `-t` whenever you are working with `sudoku_cover.txt`/text artifacts:
```bash
./dlx -t sudoku_cover.txt dlx_solution_output.txt
```

##### DLX Binary Interface

Binary mode is now the default. The `sudoku_cover.bin` file path should reference the encoder's output, and the resulting `dlx_solution_output.bin` file will contain binary solution rows for the decoder.

#### Sudoku Decoder

The sudoku_decoder application takes the original sudoku problem file and the sudoku solution file as input and decodes the complete cover solution back into the original sudoku domain into an output file. The required parameters to run sudoku_decoder is the following:

```bash
./sudoku_decoder [-t] <problem_file> [solution_file] [answer_file]
```

- Default mode expects the DLX solution rows in binary form (matching the default `dlx` output); pass `-` (or omit trailing arguments) to read rows from stdin and/or write solved grids to stdout.
- `-t` tells the decoder to read the text DLX solution stream emitted by `dlx -t`.

An example of this command using the binary pipeline:
```bash
./sudoku_decoder sudoku_test.txt dlx_solution_output.bin sudoku_solution.txt
```

The `sudoku_test.txt` file under the `tests/` directory is an example of a problem file.

The `dlx_solution_output.txt` file under the `tests/` directory shows an example of what a complete cover solution file looks like.

The `sudoku_solution.txt` file under the `tests/` directory is an example of a sudoku final answer.

##### Sudoku Decoder Binary Interface

If the dlx application was used in text mode, mirror that choice via the decoder's `-t` flag:
```bash
./sudoku_decoder -t sudoku_test.txt dlx_solution_output.txt sudoku_solution.txt
```

#### End-to-End Pipeline Examples

With the new streaming support, the entire Sudoku workflow can be chained without temporary files. The binary-first path (default for every stage) looks like:

```bash
./build/sudoku_encoder tests/sudoku_test.txt - \
  | ./build/dlx - \
  | ./build/sudoku_decoder tests/sudoku_test.txt - \
  > answers
```

This command uses binary I/O everywhere (no flags required) and writes the final solved grids into `answers`. To run a fully text-based pipeline for debugging, add `-t` to each command:

```bash
./build/sudoku_encoder -t tests/sudoku_test.txt - \
  | ./build/dlx -t - \
  | ./build/sudoku_decoder -t tests/sudoku_test.txt - \
  > answers
```

Both snippets demonstrate the `-` convention: encoder writes the cover matrix to stdout, `dlx` consumes stdin and streams row identifiers, and the decoder reconstructs solutions directly into the final answers file.

### Documentation

This project uses Doxygen to extract C++ APIs into XML, then Sphinx+Breathe to transform that into HTML. The generated site is grouped into Core, Sudoku, and Tooling sections with a full API reference for quick navigation.

1. Install the toolchain once: `pip install -r docs/sphinx/requirements.txt`.
2. Generate docs automatically with the Conan helper:  
   `CONAN_HOME=$(pwd)/conan PYTHONPATH=$(pwd) conan doc --build-folder build --sphinx-output docs/build/sphinx`
3. Prefer a pure CMake/Sphinx flow? Run `cmake --build build --target doxygen-docs` then  
   `sphinx-build -b html docs/sphinx docs/build/sphinx`.

Open `docs/build/sphinx/index.html` in your browser to explore the Core modules, Sudoku encoder/decoder, and tooling sections. The default site now uses the Material theme (`sphinx-material` package). If you prefer a different look, place custom CSS/JS in `docs/sphinx/_static` or change `html_theme` inside `docs/sphinx/conf.py` (Groundwork is still available via the bundled dependency).
