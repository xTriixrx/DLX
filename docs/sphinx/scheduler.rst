Rust Scheduler Modules
======================

.. raw:: html

   <link rel="stylesheet" href="_static/astro.css">
   <link rel="stylesheet" href="_static/code-block.css">
   <link rel="stylesheet" href="_static/dlx_pygments.css">
   <script defer src="_static/remove_sigs.js"></script>

The ``scheduler-rs`` workspace hosts the experimental scheduler encoder/decoder/server stack built
in Rust. These crates speak the same DLXB (cover) and DLXS (solution) packets used by the C++
solver, which means they can submit problems over the TCP server and consume solutions from any
streaming interface. This page documents the workspace topology plus the precise packet structures.

.. contents::
   :local:

Workspace Layout
----------------

The workspace groups every crate under a single manifest so shared dependencies and linting rules
are centralized:

.. code-block:: text

   scheduler-rs/
   ├── Cargo.toml          # workspace definition
   ├── scheduler_core/     # DLXB/DLXS packet models + shared domain objects
   ├── scheduler_encoder/  # CLI + library that emit scheduler problems as DLXB
   ├── scheduler_decoder/  # CLI + library that reads DLXS solution streams
   └── scheduler_server/   # Future Axum REST service that fronts the encoder/decoder

All crates depend on ``scheduler_core`` for protocol types, scheduler models, and future TCP
helpers. The encoder and decoder expose both library APIs (so other applications can embed them) and
thin ``main.rs`` entry points for CLI usage.

Crate Responsibilities
----------------------

``scheduler_core``
   - Owns the canonical definitions of ``DlxbHeader``, ``DlxbRowChunk``, ``DlxsHeader``, and
     ``DlxsSolutionRow``. Each struct includes helper methods to read/write network-ordered bytes
     plus round-trip tests that guard against format drift.
   - Provides ``scheduler_core::model`` with serializable scheduler domain objects and ``net`` with
     TCP endpoint helpers that future clients can extend.

``scheduler_encoder``
   - Will translate scheduler input files into DLXB packets and push them over stdin or sockets.
   - Uses ``scheduler_core::dlxb`` to ensure every packet conforms to the documented binary layout.

``scheduler_decoder``
   - Listens for DLXS frames, decodes each solution row, and writes verified schedules back to text
     formats or structured logs.

``scheduler_server``
   - Hosts an Axum-based REST façade that will eventually accept scheduling jobs from front-end
     clients and proxy those requests to ``dlx --server`` via the shared encoder/decoder libraries.

DLXB Packet Structure
---------------------

The ``scheduler_core::dlxb`` module provides a type-safe API for constructing DLXB packets. The Rust
definitions mirror the C++ structs used inside ``include/core/binary.h``:

.. code-block:: rust
   :class: astro-mui-prototypes

   pub struct DlxbHeader {
       pub magic: u32,        // 0x444C_5842 ("DLXB")
       pub version: u16,      // DLX_BINARY_VERSION (currently 1)
       pub flags: u16,        // reserved, zero today
       pub column_count: u32,
       pub row_count: u32,
   }

   pub struct DlxbRowChunk {
       pub row_id: u32,
       pub columns: Vec<u32>, // each entry is a column index set to 1
   }

``DlxbPacket`` simply combines one header with ``row_count`` row chunks. Calling
``DlxbPacket::write_to`` writes the fields in network byte order using the layout below:

.. list-table:: DLXB Header (scheduler_core::dlxb::DlxbHeader)
   :header-rows: 1
   :widths: 30 20 50

   * - Field
     - Size (bits)
     - Description
   * - ``magic``
     - 32
     - ASCII ``"DLXB"`` sentinel.
   * - ``version``
     - 16
     - Protocol version enforced by both the Rust and C++ implementations.
   * - ``flags``
     - 16
     - Reserved for future transports (writers set to ``0``).
   * - ``column_count``
     - 32
     - Number of columns in the cover matrix.
   * - ``row_count``
     - 32
     - Number of option rows serialized in this packet.

Each row chunk is encoded as ``row_id`` (32 bits) + ``entry_count`` (16 bits, derived from the
``columns`` vector length) followed by ``entry_count`` column indices (32 bits each). The
implementation enforces a 65,535-column maximum per row to match the underlying protocol limits.

DLXS Packet Structure
---------------------

On the solution side ``scheduler_core::dlxs`` exposes ``DlxsHeader``, ``DlxsSolutionRow``, and
``DlxsPacket``. These definitions ensure Rust decoders treat the streaming protocol exactly like the
existing C++ ``tcp_server`` implementation.

.. code-block:: rust
   :class: astro-mui-prototypes

   pub struct DlxsHeader {
       pub magic: u32,        // 0x444C_5853 ("DLXS")
       pub version: u16,      // DLX_BINARY_VERSION
       pub flags: u16,
       pub column_count: u32, // must match the column count from the originating DLXB
   }

   pub struct DlxsSolutionRow {
       pub solution_id: u32,
       pub indices: Vec<u32>, // row_ids chosen for this solution
   }

   pub struct DlxsPacket {
       pub header: DlxsHeader,
       pub rows: Vec<DlxsSolutionRow>,
   }

``DlxsPacket::write_to`` automatically appends the sentinel described below so callers never forget
to terminate a problem boundary. ``DlxsPacket::read_from`` stops at the sentinel and returns the
decoded rows, making it safe to iterate over many problems on a single TCP connection.

.. list-table:: DLXS Header (scheduler_core::dlxs::DlxsHeader)
   :header-rows: 1
   :widths: 30 20 50

   * - Field
     - Size (bits)
     - Description
   * - ``magic``
     - 32
     - ASCII ``"DLXS"`` sentinel.
   * - ``version``
     - 16
     - ``DLX_BINARY_VERSION`` (must match the cover packet).
   * - ``flags``
     - 16
     - Reserved for future metadata.
   * - ``column_count``
     - 32
     - Column count required to interpret the row indices.

Solution rows reuse the same ``row_id`` values emitted by the DLXB stream: a decoder applies the row
indices back to the original row chunks to recover column assignments (see
``DlxsPacket::apply_to_rows``). The TCP server streams problems sequentially, so the solver emits a
**sentinel packet** between problems encoded as ``solution_id = 0`` with ``entry_count = 0``. Rust
helpers expose ``DlxsSolutionRow::sentinel`` and ``DlxsSolutionRow::is_sentinel`` so network code
can recognize end-of-problem markers without manual bookkeeping.

Putting It Together
-------------------

Because both DLXB and DLXS packets are implemented inside ``scheduler_core``, the higher-level Rust
crates have a single source of truth for packet parsing. As you expand the encoder, decoder, or the
Axum REST server, reference these helpers instead of rebuilding serialization logic—the unit tests
cover round-trip encoding and keep interoperability with the C++ solver intact.

Rust API Reference
------------------

The directive below pulls the generated Rust documentation (exported as XML via ``cargo doc``) into
the Sphinx tree so the core packet structures stay in sync with compiler-verified docs.

.. rustdoc:: scheduler_core
   :module: dlxb

.. rustdoc:: scheduler_core
   :module: dlxs
