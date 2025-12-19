Core Modules
============

.. raw:: html

   <link rel="stylesheet" href="_static/astro.css">
   <link rel="stylesheet" href="_static/code-block.css">
   <link rel="stylesheet" href="_static/dlx_pygments.css">
   <script defer src="_static/remove_sigs.js"></script>

The ``src/core`` and ``include/core`` directories contain the reusable DLX data structures,
binary protocol helpers, and orchestration logic. Each API is documented via Doxygen and paired
with an Astro-styled prototype as a quick reference.

Defines
-------
.. code-block:: c
   :class: astro-mui-prototypes

   #define DLX_COVER_MAGIC 0x444C5842u /* 'DLXB' */

.. doxygendefine:: DLX_COVER_MAGIC
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define DLX_SOLUTION_MAGIC 0x444C5853u /* 'DLXS' */

.. doxygendefine:: DLX_SOLUTION_MAGIC
   :project: dlx

.. code-block:: c
   :class: astro-mui-prototypes

   #define DLX_BINARY_VERSION 1

.. doxygendefine:: DLX_BINARY_VERSION
   :project: dlx

Typedefs
--------
The core headers do not currently expose any typedef aliases beyond the direct struct names.

Structs
-------
.. code-block:: cpp
   :class: astro-mui-prototypes

   struct node {
       int len;
       int data;
       char* name;
       struct node* top;
       struct node* up;
       struct node* down;
       struct node* left;
       struct node* right;
   };

.. doxygenstruct:: node
   :project: dlx
   :members:

.. code-block:: cpp
   :class: astro-mui-prototypes

   struct DlxCoverHeader {
       uint32_t magic;
       uint16_t version;
       uint16_t flags;
       uint32_t column_count;
       uint32_t row_count;
   };

.. doxygenstruct:: DlxCoverHeader
   :project: dlx
   :members:

.. code-block:: cpp
   :class: astro-mui-prototypes

   struct DlxRowChunk {
       uint32_t row_id;
       uint16_t entry_count;
       uint16_t capacity;
       uint32_t* columns;
   };

.. doxygenstruct:: DlxRowChunk
   :project: dlx
   :members:

.. code-block:: cpp
   :class: astro-mui-prototypes

   struct DlxSolutionHeader {
       uint32_t magic;
       uint16_t version;
       uint16_t flags;
       uint32_t column_count;
   };

.. doxygenstruct:: DlxSolutionHeader
   :project: dlx
   :members:

.. code-block:: cpp
   :class: astro-mui-prototypes

   struct DlxSolutionRow {
       uint32_t solution_id;
       uint16_t entry_count;
       uint16_t capacity;
       uint32_t* row_indices;
   };

.. doxygenstruct:: DlxSolutionRow
   :project: dlx
   :members:

Binary Interface Helpers
------------------------
.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_write_cover_header(std::ostream& output, const DlxCoverHeader* header);

.. doxygenfunction:: dlx::binary::dlx_write_cover_header
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_read_cover_header(std::istream& input, DlxCoverHeader* header);

.. doxygenfunction:: dlx::binary::dlx_read_cover_header
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_write_row_chunk(std::ostream& output, uint32_t row_id, const uint32_t* columns, uint16_t column_count);

.. doxygenfunction:: dlx::binary::dlx_write_row_chunk
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_read_row_chunk(std::istream& input, DlxRowChunk* chunk);

.. doxygenfunction:: dlx::binary::dlx_read_row_chunk
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void dlx::binary::dlx_free_row_chunk(DlxRowChunk* chunk);

.. doxygenfunction:: dlx::binary::dlx_free_row_chunk
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_write_solution_header(std::ostream& output, const DlxSolutionHeader* header);

.. doxygenfunction:: dlx::binary::dlx_write_solution_header
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_read_solution_header(std::istream& input, DlxSolutionHeader* header);

Streaming Interfaces
--------------------
.. doxygenstruct:: dlx::SolutionView
   :project: dlx
   :members:

.. doxygenclass:: dlx::SolutionSink
   :project: dlx
   :members:

.. doxygenclass:: dlx::OstreamSolutionSink
   :project: dlx
   :members:

.. doxygenclass:: dlx::CompositeSolutionSink
   :project: dlx
   :members:

.. doxygenfunction:: dlx::binary::dlx_read_solution_header
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_write_solution_row(std::ostream& output, uint32_t solution_id, const uint32_t* row_indices, uint16_t row_count);

.. doxygenfunction:: dlx::binary::dlx_write_solution_row
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx::binary::dlx_read_solution_row(std::istream& input, DlxSolutionRow* row);

.. doxygenfunction:: dlx::binary::dlx_read_solution_row
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void dlx::binary::dlx_free_solution_row(DlxSolutionRow* row);

.. doxygenfunction:: dlx::binary::dlx_free_solution_row
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   struct node* dlx::binary::dlx_read_binary(std::istream& input, char*** solutions_out, int* item_count_out, int* option_count_out);

.. doxygenfunction:: dlx::binary::dlx_read_binary
   :project: dlx



Solver Interfaces
-----------------
.. code-block:: cpp
   :class: astro-mui-prototypes

   int getNodeCount(FILE*);

.. doxygenfunction:: getNodeCount
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int getItemCount(FILE*);

.. doxygenfunction:: getItemCount
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void hide(struct node*);

.. doxygenfunction:: hide
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void cover(struct node*);

.. doxygenfunction:: cover
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void unhide(struct node*);

.. doxygenfunction:: unhide
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void uncover(struct node*);

.. doxygenfunction:: uncover
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int getOptionsCount(FILE*);

.. doxygenfunction:: getOptionsCount
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int getOptionNodesCount(FILE*);

.. doxygenfunction:: getOptionNodesCount
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   struct node* generateHeadNode(int);

.. doxygenfunction:: generateHeadNode
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   struct node* pickItem(struct node*);

.. doxygenfunction:: pickItem
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void printSolutions(char**, const uint32_t*, int, dlx::SolutionSink*);

.. doxygenfunction:: printSolutions
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void search(struct node*, int, char**, uint32_t*, dlx::SolutionSink*);

.. doxygenfunction:: search
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   struct node* generateMatrix(FILE*, int);

.. doxygenfunction:: generateMatrix
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int generateTitles(struct node*, char*);

.. doxygenfunction:: generateTitles
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void freeMemory(struct node*, char**);

.. doxygenfunction:: freeMemory
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void handleSpacerNodes(struct node*, int*, int, int);

.. doxygenfunction:: handleSpacerNodes
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   int dlx_enable_binary_solution_output(std::ostream& output, uint32_t column_count);

.. doxygenfunction:: dlx_enable_binary_solution_output
   :project: dlx


.. code-block:: cpp
   :class: astro-mui-prototypes

   void dlx_disable_binary_solution_output(void);

.. doxygenfunction:: dlx_disable_binary_solution_output
   :project: dlx
Streaming Interfaces
--------------------
.. doxygenstruct:: dlx::SolutionView
   :project: dlx
   :members:

.. doxygenclass:: dlx::SolutionSink
   :project: dlx
   :members:

.. doxygenclass:: dlx::OstreamSolutionSink
   :project: dlx
   :members:

.. doxygenclass:: dlx::CompositeSolutionSink
   :project: dlx
   :members:

*** End Patch
