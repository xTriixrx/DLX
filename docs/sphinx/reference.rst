Reference
=========

.. raw:: html

   <link rel="stylesheet" href="_static/astro.css">
   <link rel="stylesheet" href="_static/code-block.css">
   <link rel="stylesheet" href="_static/dlx_pygments.css">
   <script defer src="_static/remove_sigs.js"></script>

This page exposes the complete public headers that make up the DLX toolkit. Each block mirrors the
source exactly, allowing you to view macros, structs, typedefs, and functions without leaving the
reference view.

Core Library
------------

``include/core/dlx.h``
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "core/dlx.h"

.. literalinclude:: ../../include/core/dlx.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes

Defines


``include/core/dlx_binary.h``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "core/dlx_binary.h"

.. literalinclude:: ../../include/core/dlx_binary.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes

``include/core/dlx_server.h``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "core/dlx_server.h"

.. literalinclude:: ../../include/core/dlx_server.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes
^^^^^^^

.. doxygendefine:: DLX_COVER_MAGIC
   :project: dlx

.. doxygendefine:: DLX_SOLUTION_MAGIC
   :project: dlx

.. doxygendefine:: DLX_BINARY_VERSION
   :project: dlx

Typedefs
^^^^^^^^

(None)

Structs
^^^^^^^

.. doxygenstruct:: node
   :project: dlx
   :members:

Functions
^^^^^^^^^

.. doxygenfunction:: fileExists
   :project: dlx
.. doxygenfunction:: getNodeCount
   :project: dlx
.. doxygenfunction:: getItemCount
   :project: dlx
.. doxygenfunction:: hide
   :project: dlx
.. doxygenfunction:: cover
   :project: dlx
.. doxygenfunction:: unhide
   :project: dlx
.. doxygenfunction:: uncover
   :project: dlx
.. doxygenfunction:: getOptionsCount
   :project: dlx
.. doxygenfunction:: repeatStr
   :project: dlx
.. doxygenfunction:: printItems
   :project: dlx
.. doxygenfunction:: getOptionNodesCount
   :project: dlx
.. doxygenfunction:: printSolutions
   :project: dlx
.. doxygenfunction:: printOptionRow
   :project: dlx
.. doxygenfunction:: generateHeadNode
   :project: dlx
.. doxygenfunction:: printItemColumn
   :project: dlx
.. doxygenfunction:: pickItem
   :project: dlx
.. doxygenfunction:: insertIntoSet
   :project: dlx
.. doxygenfunction:: search
   :project: dlx
.. doxygenfunction:: printMatrix
   :project: dlx
.. doxygenfunction:: generateMatrix
   :project: dlx
.. doxygenfunction:: generateTitles
   :project: dlx
.. doxygenfunction:: freeMemory
   :project: dlx
.. doxygenfunction:: handleSpacerNodes
   :project: dlx
.. doxygenfunction:: insertItem
   :project: dlx
.. doxygenfunction:: dlx_enable_binary_solution_output
   :project: dlx
.. doxygenfunction:: dlx_disable_binary_solution_output
   :project: dlx

Sudoku Pipeline
---------------

``include/sudoku/encoder/sudoku_encoder.h``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "sudoku/encoder/sudoku_encoder.h"

.. literalinclude:: ../../include/sudoku/encoder/sudoku_encoder.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes

``include/sudoku/decoder/sudoku_decoder.h``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "sudoku/decoder/sudoku_decoder.h"

.. literalinclude:: ../../include/sudoku/decoder/sudoku_decoder.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes

Defines (Sudoku)
^^^^^^^^^^^^^^^^

.. doxygendefine:: GRID_SIZE
   :project: dlx
.. doxygendefine:: BOX_SIZE
   :project: dlx
.. doxygendefine:: DIGIT_COUNT
   :project: dlx
.. doxygendefine:: COLUMN_COUNT
   :project: dlx
.. doxygendefine:: CELL_CONSTRAINTS
   :project: dlx
.. doxygendefine:: ROW_DIGIT_OFFSET
   :project: dlx
.. doxygendefine:: ROW_DIGIT_CONSTRAINTS
   :project: dlx
.. doxygendefine:: COL_DIGIT_OFFSET
   :project: dlx
.. doxygendefine:: COL_DIGIT_CONSTRAINTS
   :project: dlx
.. doxygendefine:: BOX_DIGIT_OFFSET
   :project: dlx

Typedefs (Sudoku)
^^^^^^^^^^^^^^^^^

.. doxygentypedef:: sudoku_candidate_handler
   :project: dlx

Structs (Sudoku)
^^^^^^^^^^^^^^^^

.. doxygenstruct:: SudokuCandidate
   :project: dlx
   :members:

Functions (Sudoku)
^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: convert_sudoku_to_cover
   :project: dlx
.. doxygenfunction:: load_sudoku_state
   :project: dlx
.. doxygenfunction:: iterate_sudoku_candidates
   :project: dlx
.. doxygenfunction:: sudoku_box_index
   :project: dlx
.. doxygenfunction:: decode_sudoku_solution
   :project: dlx

``include/sudoku/encoder/sudoku_encoder.h``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "sudoku/encoder/sudoku_encoder.h"

.. literalinclude:: ../../include/sudoku/encoder/sudoku_encoder.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes

``include/sudoku/decoder/sudoku_decoder.h``
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: cpp
   :class: astro-mui-prototypes

   #include "sudoku/decoder/sudoku_decoder.h"

.. literalinclude:: ../../include/sudoku/decoder/sudoku_decoder.h
   :language: cpp
   :linenos:
   :class: astro-mui-prototypes

Defines (Sudoku)
^^^^^^^^^^^^^^^^

.. doxygendefine:: GRID_SIZE
   :project: dlx
.. doxygendefine:: BOX_SIZE
   :project: dlx
.. doxygendefine:: DIGIT_COUNT
   :project: dlx
.. doxygendefine:: COLUMN_COUNT
   :project: dlx
.. doxygendefine:: CELL_CONSTRAINTS
   :project: dlx
.. doxygendefine:: ROW_DIGIT_OFFSET
   :project: dlx
.. doxygendefine:: ROW_DIGIT_CONSTRAINTS
   :project: dlx
.. doxygendefine:: COL_DIGIT_OFFSET
   :project: dlx
.. doxygendefine:: COL_DIGIT_CONSTRAINTS
   :project: dlx
.. doxygendefine:: BOX_DIGIT_OFFSET
   :project: dlx

Typedefs (Sudoku)
^^^^^^^^^^^^^^^^^

.. doxygentypedef:: sudoku_candidate_handler
   :project: dlx

Structs (Sudoku)
^^^^^^^^^^^^^^^^

.. doxygenstruct:: SudokuCandidate
   :project: dlx
   :members:

Functions (Sudoku)
^^^^^^^^^^^^^^^^^^

.. doxygenfunction:: convert_sudoku_to_cover
   :project: dlx
.. doxygenfunction:: load_sudoku_state
   :project: dlx
.. doxygenfunction:: iterate_sudoku_candidates
   :project: dlx
.. doxygenfunction:: sudoku_box_index
   :project: dlx
.. doxygenfunction:: decode_sudoku_solution
   :project: dlx
Full API Reference
------------------

.. doxygenindex:: dlx
