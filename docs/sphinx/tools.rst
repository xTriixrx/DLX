Tooling & Extensions
====================

.. raw:: html

   <link rel="stylesheet" href="_static/astro.css">
   <link rel="stylesheet" href="_static/code-block.css">
   <link rel="stylesheet" href="_static/dlx_pygments.css">
   <script defer src="_static/remove_sigs.js"></script>

The repository ships a few helper utilities to streamline development:

- ``docs/Doxyfile`` and the ``docs/sphinx`` tree drive the Doxygen → Sphinx documentation flow.
- ``conan/extensions/commands/cmd_doc.py`` adds a ``conan doc`` command that runs both Doxygen
  and Sphinx in one step (see the README for usage).
- ``conan/extensions/commands/cmd_coverage.py`` exposes ``conan coverage`` to emit gcovr reports.

Refer to the README to install the required Python dependencies and to learn how to customize the
Sphinx theme via ``docs/sphinx/_static``.

Astro UI Prototypes
-------------------

The Astro UX specification describes a set of Material-esque UI primitives. The snippet below
captures their canonical C++-style interfaces so you can reference them while building tooling
that mirrors the Astro look and feel.

.. code-block:: cpp
   :class: astro-mui-prototypes

   // Navigation & layout
   void AstroNavigationRail(const AstroNavigationConfig& config);
   void AstroWorkbenchLayout(const AstroWorkbenchConfig& config);

   // Buttons & toggles
   void AstroPrimaryButton(const AstroButtonProps& props);
   void AstroIconButton(const AstroIconButtonProps& props);
   void AstroToggleSwitch(const AstroToggleProps& props);

   // Inputs & forms
   void AstroTextInput(AstroTextField& field);
   void AstroDropdown(AstroDropdownModel& model);
   bool AstroDateTimePicker(AstroCalendarContext& ctx);

   // Feedback & indicators
   void AstroToastBanner(const AstroToastProps& props);
   void AstroStatusIndicator(AstroStatus status);
   void AstroModalDialog(const AstroModalProps& props);

   // Data visualization hooks
   void AstroTelemetryTile(const AstroTelemetryProps& props);
   void AstroTimelinePlot(const AstroTimelineProps& props);
