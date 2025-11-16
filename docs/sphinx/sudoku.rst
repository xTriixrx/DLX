Sudoku Modules
==============

.. raw:: html

   <link rel="stylesheet" href="_static/astro.css">
   <link rel="stylesheet" href="_static/code-block.css">
   <link rel="stylesheet" href="_static/dlx_pygments.css">
   <script defer src="_static/remove_sigs.js"></script>

Sudoku-specific components live under ``include/sudoku`` and ``src/sudoku``. They reuse the DLX
core to build cover matrices and decode row selections back into solved boards.

Defines
-------
.. code-block:: c
   :class: astro-mui-prototypes

   #define GRID_SIZE 9

.. doxygendefine:: GRID_SIZE
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define BOX_SIZE 3

.. doxygendefine:: BOX_SIZE
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define DIGIT_COUNT 9

.. doxygendefine:: DIGIT_COUNT
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define COLUMN_COUNT 324

.. doxygendefine:: COLUMN_COUNT
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define CELL_CONSTRAINTS (GRID_SIZE * GRID_SIZE)

.. doxygendefine:: CELL_CONSTRAINTS
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define ROW_DIGIT_OFFSET CELL_CONSTRAINTS

.. doxygendefine:: ROW_DIGIT_OFFSET
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define ROW_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)

.. doxygendefine:: ROW_DIGIT_CONSTRAINTS
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define COL_DIGIT_OFFSET (ROW_DIGIT_OFFSET + ROW_DIGIT_CONSTRAINTS)

.. doxygendefine:: COL_DIGIT_OFFSET
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define COL_DIGIT_CONSTRAINTS (GRID_SIZE * DIGIT_COUNT)

.. doxygendefine:: COL_DIGIT_CONSTRAINTS
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define BOX_DIGIT_OFFSET (COL_DIGIT_OFFSET + COL_DIGIT_CONSTRAINTS)

.. doxygendefine:: BOX_DIGIT_OFFSET
   :project: dlx

Typedefs
--------
.. code-block:: c
   :class: astro-mui-prototypes

   typedef int (*sudoku_candidate_handler)(int row, int col, int digit, void* ctx);

.. doxygentypedef:: sudoku_candidate_handler
   :project: dlx

Structs
-------
.. code-block:: cpp
   :class: astro-mui-prototypes

   struct SudokuCandidate {
       int row;
       int col;
       int digit;
   };

.. doxygenstruct:: SudokuCandidate
   :project: dlx
   :members:

Encoder
-------

.. code-block:: c
   :class: astro-mui-prototypes

   int convert_sudoku_to_cover(const char* puzzle_path, const char* cover_path, bool binary_format);

.. doxygenfunction:: convert_sudoku_to_cover
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   int load_sudoku_state(const char* puzzle_path,
                         int grid[GRID_SIZE][GRID_SIZE],
                         bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                         bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                         bool box_used[GRID_SIZE][DIGIT_COUNT + 1]);

.. doxygenfunction:: load_sudoku_state
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   int iterate_sudoku_candidates(const int grid[GRID_SIZE][GRID_SIZE],
                                 const bool row_used[GRID_SIZE][DIGIT_COUNT + 1],
                                 const bool col_used[GRID_SIZE][DIGIT_COUNT + 1],
                                 const bool box_used[GRID_SIZE][DIGIT_COUNT + 1],
                                 sudoku_candidate_handler handler,
                                 void* ctx);

.. doxygenfunction:: iterate_sudoku_candidates
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   static inline int sudoku_box_index(int row, int col);

.. doxygenfunction:: sudoku_box_index
   :project: dlx

Decoder
-------

.. code-block:: c
   :class: astro-mui-prototypes

   int decode_sudoku_solution(const char* puzzle_path,
                              const char* solution_rows_path,
                              const char* output_path,
                              bool binary_input);

.. doxygenfunction:: decode_sudoku_solution
   :project: dlx
