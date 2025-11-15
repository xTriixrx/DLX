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

- C++ 17 & C 11 compliant compiler. `g++, clang, etc.`
- Cmake

## Sudoku Solver

### Building

Within the cloned repository's main folder `/your_path/dlx`, you can execute the following commands to build the necessary binaries.

```bash
cmake -S . -B build
cmake --build build
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