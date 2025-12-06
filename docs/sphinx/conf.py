import os
import sys

# Sphinx-material uses multiprocessing.Manager() which is blocked in this
# environment. Provide a small stub to avoid spawning extra processes.
import sphinx_material


class _DummyManager:
    def list(self):
        return []

    def shutdown(self):
        pass


sphinx_material.Manager = lambda: _DummyManager()

project = "DLX"
author = "Vincent Nigro"
release = "0.1"

sys.path.insert(0, os.path.abspath("../../src"))
ext_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "_ext"))
if os.path.isdir(ext_dir):
    sys.path.insert(0, ext_dir)

extensions = [
    "breathe",
    "rustdoc",
]

templates_path = ["_templates"]
exclude_patterns = ["_build"]

html_theme = "sphinx_material"
html_static_path = ["_static"]
html_search = False
html_css_files = ["astro.css", "code-block.css", "dlx_pygments.css"]
html_js_files = ["remove_sigs.js"]

root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
doxygen_xml = os.path.join(root_dir, "docs", "build", "doxygen", "xml")

breathe_projects = {"dlx": doxygen_xml}
breathe_default_project = "dlx"
rustdoc_xml_dir = os.path.join(root_dir, "docs", "build", "rustdoc", "xml")
