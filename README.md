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
- C++ 17 & C 11 compliant compiler. `g++, clang, etc.`
- CMake

### Conan Workflow

The project ships with a `conanfile.py` so you can use Conan to configure, build, and run the tests in a single workflow. Before installing dependencies, configure the provided profile and remotes (once per environment):

```bash
conan config install conan  # installs conan/remotes.json and profiles/default
```

If you prefer to isolate Conan in a Python virtual environment, run:

```bash
python3 -m venv .venv
source .venv/bin/activate   # On Windows use .venv\Scripts\activate
pip install -r requirements.txt
```

Then configure/build/test:

```bash
# configure dependencies, generate toolchain info, configure + build + run tests (ctest)
conan install . && conan build .
```

`conan build` automatically invokes `ctest` when tests are enabled, so the pipeline stays in sync with the CMake setup. To clean everything, remove the `build/` directory (`rm -rf build`) and deactivate/delete the virtual environment when you’re done.

> **Note:** If you already have GoogleTest installed system-wide (e.g., via Homebrew), the build will use that installation. Otherwise, Conan will fetch and build `gtest` from ConanCenter (pass `--build gtest` if a binary isn’t available for your platform).

#### Conan Configuration

You must ensure you have Conan 2.x installed on your build machine. Once you have conan installed you can create the `default` profile with the following:

```bash
# Creates a default profile
conan profile detect --name default

# Will overwrite the detected environment with the appropriate settings
cp conan/profiles/default ~/.conan2/profiles
```

You will have to update your `os`, `arch`, and `compiler` settings in the default profile provided as per your own build machine settings.

## Sudoku Solver

### Building

Within the cloned repository's main folder `/your_path/dlx`, you can execute the following commands to build the necessary binaries.

```bash
conan install . && conan build .
```

### Execution

#### Sudoku Matrix

The sudoku_matrix application takes a sudoku problem file and converts the problem to a complete cover matrix representation in an output file. The required parameters to run sudoku_matrix is the following:

```bash
./sudoku_matrix <problem_file> <cover_output_path>
```

An example of this command using one of the provided test files:
```bash
./sudoku_matrix sudoku_test.txt sudoku_cover.txt
```
All `sudoku_test*.txt` files under the `tests/` directory can be used as input into the sudoku_matrix.

The `sudoku_cover.txt` file under the `tests/` directory shows an example of what one of these cover files look like.

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
./dlx <cover_output> <solution_output_path>
```

An example of this command using one of the provided test files:
```bash
./dlx sudoku_cover.txt dlx_solution_output.txt
```
The `sudoku_cover.txt` file under the `tests/` directory shows an example of what one of these input cover files look like.

The `dlx_solution_output.txt` file under the `tests/` directory shows an example of what a complete cover solution file looks like.

#### Sudoku Decoder

The sudoku_decoder application takes the original sudoku problem file and the sudoku solution file as input and decodes the complete cover solution back into the original sudoku domain into an output file. The required parameters to run sudoku_decoder is the following:

```bash
./sudoku_decoder <problem_file> <solution_file> <answer_file>
```

An example of this command using one of the provided test files:
```bash
./sudoku_decoder sudoku_test.txt dlx_solution_output.txt sudoku_solution.txt
```

The `sudoku_test.txt` file under the `tests/` directory is an example of a problem file.

The `dlx_solution_output.txt` file under the `tests/` directory shows an example of what a complete cover solution file looks like.

The `sudoku_solution.txt` file under the `tests/` directory is an example of a sudoku final answer.
