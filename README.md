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

The `sudoku_encoder` application takes a sudoku problem file and converts the problem to a DLX binary cover stream. Usage:

```bash
./sudoku_encoder <problem_file> [cover_output_path]
```

If `cover_output_path` is omitted or set to `-`, the cover stream is written to stdout so it can be piped directly into `dlx`. All `sudoku_test*.txt` files under the `tests/` directory can be used as encoder input. Writing the binary stream to disk looks like:

```bash
./sudoku_encoder sudoku_test.txt sudoku_cover.bin
```

Use the same `-` shorthand to read puzzles from stdin (e.g., `cat puzzle.txt | ./sudoku_encoder - -`).

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

The `dlx` application takes a DLX binary cover matrix as input and emits every possible solution row in both text (stdout) and binary form:

```bash
./dlx <cover_file> [solution_output_path]
```

Passing `-` for either argument switches to stdin/stdout. When the binary solution output is written to stdout, console printing is automatically suppressed; otherwise, human-readable rows are streamed via the sink infrastructure while the DLXS file is written to the requested path.

#### Sudoku Decoder

The `sudoku_decoder` application takes the original sudoku problem file plus the binary solution rows and reconstructs solved grids:

```bash
./sudoku_decoder <problem_file> [solution_file] [answer_file]
```

Omit `solution_file` or `answer_file` (or pass `-`) to stream stdin/stdout respectively. A typical invocation looks like:

```bash
./sudoku_decoder sudoku_test.txt dlx_solution_output.bin sudoku_solution.txt
```

The `sudoku_test.txt` file under `tests/` is an example puzzle, and `sudoku_solution.txt` contains the expected solved grid.

#### End-to-End Pipeline Example

With the streaming sink in place, the full Sudoku workflow can be chained without temporary files:

```bash
./build/sudoku_encoder tests/sudoku_test.txt \
  | ./build/dlx \
  | ./build/sudoku_decoder tests/sudoku_test.txt \
  > answers
```

This command uses binary I/O for every hop and writes the solved grids into `answers`. When outputs are omitted, each stage automatically streams via stdin/stdout.

### DLX Binary Interchange (DLXB/DLXS)

The DLX toolchain ships with a compact binary format designed for fast streaming between the encoder, solver, and decoder. Two record types exist:

<table align="center">
<tr><th>Identifier</th><th>Magic</th><th>Purpose</th></tr>
<tr><td align="center"><code>DLXB</code></td><td align="center"><code>0x444C5842</code></td><td>Cover matrix serialization (produced by <code>sudoku_encoder</code>, consumed by <code>dlx</code>).</td></tr>
<tr><td align="center"><code>DLXS</code></td><td align="center"><code>0x444C5853</code></td><td>Solution stream (written by <code>dlx</code>, consumed by <code>sudoku_decoder</code>).</td></tr>
</table>

All integers are stored in network byte order (big-endian).

#### DLXB Binary Minor Frame

 An entire `DLXB` minor frame is composed of a single header followed by a sequence of row chunks:

<table align="center">
<tr><th>Field</th><th>Bits</th><th>Description</th></tr>
<tr><td align="center"><code>magic</code></td><td align="center">32</td><td>ASCII <code>\"DLXB\"</code> sentinel.</td></tr>
<tr><td align="center"><code>version</code></td><td align="center">16</td><td>Current value <code>1</code> (<code>DLX_BINARY_VERSION</code>).</td></tr>
<tr><td align="center"><code>flags</code></td><td align="center">16</td><td>Reserved; writers set to <code>0</code>.</td></tr>
<tr><td align="center"><code>column_count</code></td><td align="center">32</td><td>Number of constraint columns in the cover matrix.</td></tr>
<tr><td align="center"><code>row_count</code></td><td align="center">32</td><td>Number of option rows serialized (for statistics).</td></tr>
</table>

<p align="center">
  <img src="imgs/dlx_binary_frame_dlxb.svg" alt="DLXB frame grid" width="420"/>
</p>

##### DLXB Binary Row Chunk

Each `DLXB` row chunk immediately follows the header and uses the layout:

1. `row_id` (32 bits) — monotonically increasing identifier for the row.
2. `entry_count` (16 bits) — number of column indices present (always 4 for Sudoku).
3. `columns[i]` (`entry_count` × 32 bits) — zero-based column indices marking the `1` entries.

Readers call `dlx_read_row_chunk` until it returns `0`, which indicates EOF. Because the `entry_count` field is 16-bit, individual rows can reference up to 65,535 columns, which is well beyond the Sudoku requirement.

#### DLXS Binary Minor Frame

`DLXS` solution files mirror the cover header with a lighter structure:

<table align="center">
<tr><th>Field</th><th>Bits</th><th>Description</th></tr>
<tr><td align="center"><code>magic</code></td><td align="center">32</td><td>ASCII <code>\"DLXS\"</code>.</td></tr>
<tr><td align="center"><code>version</code></td><td align="center">16</td><td><code>DLX_BINARY_VERSION</code>.</td></tr>
<tr><td align="center"><code>flags</code></td><td align="center">16</td><td>Reserved for future metadata.</td></tr>
<tr><td align="center"><code>column_count</code></td><td align="center">32</td><td>Column count required to interpret row identifiers.</td></tr>
</table>

<p align="center">
  <img src="imgs/dlx_binary_frame_dlxs.svg" alt="DLXS frame grid" width="420"/>
</p>

##### DLXS Binary Solution Row

Each `DLXS` solution row immediately follows the header and uses the layout:

1. `solution_id` (32 bits) — sequential ID assigned by the solver.
2. `entry_count` (16 bits) — number of row identifiers composing the solution.
3. `row_index[i]` (`entry_count` × 32 bits) — the `row_id` values emitted in the `DLXB` stream.

If `entry_count` is zero the decoder has reached the end of the solution list. The decoder enforces Sudoku constraints by replaying the row indices against the original puzzle metadata.

The following diagram highlights the byte layout of the DLXB and DLXS sections (boxes are drawn left-to-right from the most significant bits down):

![DLX binary layout](imgs/dlx_binary_layout.svg)

For a higher fidelity look at how a specific DLXB/DLXS frame maps into contiguous 32-bit words (with per-field legends), review the diagrams above in their respective sections.

### Documentation

This project uses Doxygen to extract C++ APIs into XML, then Sphinx+Breathe to transform that into HTML. The generated site is grouped into Core, Sudoku, and Tooling sections with a full API reference for quick navigation.

1. Install the toolchain once: `pip install -r docs/sphinx/requirements.txt`.
2. Generate docs automatically with the Conan helper:  
   `CONAN_HOME=$(pwd)/conan PYTHONPATH=$(pwd) conan doc --build-folder build --sphinx-output docs/build/sphinx`
3. Prefer a pure CMake/Sphinx flow? Run `cmake --build build --target doxygen-docs` then  
   `sphinx-build -b html docs/sphinx docs/build/sphinx`.

Open `docs/build/sphinx/index.html` in your browser to explore the Core modules, Sudoku encoder/decoder, and tooling sections. The default site now uses the Material theme (`sphinx-material` package). If you prefer a different look, place custom CSS/JS in `docs/sphinx/_static` or change `html_theme` inside `docs/sphinx/conf.py` (Groundwork is still available via the bundled dependency).
