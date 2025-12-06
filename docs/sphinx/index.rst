DLX Documentation
=================

.. raw:: html

   <link rel="stylesheet" href="_static/astro.css">
   <link rel="stylesheet" href="_static/code-block.css">
   <link rel="stylesheet" href="_static/dlx_pygments.css">
   <script defer src="_static/remove_sigs.js"></script>

The DLX project is split into focused modules so you can mix-and-match the encoder/solver/decoder
pieces. Use the sections below for a quick jump into each area.

.. contents:: On this page
   :local:

Core Library
------------

Algorithms, node structures, and the binary transport helpers that live under ``include/core`` and
``src/core``.

.. toctree::
   :maxdepth: 1
   :caption: Core Modules

   core

Sudoku Pipeline
---------------

Problem-specific encoder/decoder components that translate Sudoku puzzles into DLX cover matrices
and decode solution rows back into completed grids.

.. toctree::
   :maxdepth: 1
   :caption: Sudoku Modules

   sudoku

Tooling & Extensions
--------------------

Supporting pieces such as documentation helpers, Conan extensions, and binary-interface plans.

.. toctree::
   :maxdepth: 1
   :caption: Tooling

   tools

Scheduler Workspace
-------------------

The Rust-based scheduler crates expose higher-level encoder/decoder/server functionality on top of
the same DLXB/DLXS protocol. Use this section to understand the workspace layout and packet models.

.. toctree::
   :maxdepth: 1
   :caption: Scheduler

   scheduler

Header Reference
----------------

Need to browse the published headers without digging through the repository? Use the literal
reference view below.

.. toctree::
   :maxdepth: 1

   reference
