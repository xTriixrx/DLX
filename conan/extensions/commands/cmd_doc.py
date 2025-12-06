import glob
import json
import os
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from conan.errors import ConanException
from conan.api.output import ConanOutput
from conan.cli.command import conan_command


@conan_command(group="custom")
def doc(conan_api, parser, *args):
    """
    Generate documentation (Doxygen XML + Sphinx HTML).
    Usage: conan doc . [--build-folder build] [--sphinx-output docs/build/sphinx]
    """
    parser.add_argument(
        "--build-folder",
        default="build",
        help="Path to the CMake build directory (used by sphinx-build for relative includes).",
    )
    parser.add_argument(
        "--sphinx-output",
        default="docs/build/sphinx",
        help="Directory where the Sphinx HTML output will be generated.",
    )
    parser.add_argument(
        "--sphinx-builder",
        default="html",
        help="Sphinx builder (default: html).",
    )
    ns = parser.parse_args(*args)

    repo_root = os.path.abspath(".")
    docs_dir = os.path.join(repo_root, "docs")
    doxyfile_path = os.path.join(docs_dir, "Doxyfile")
    sphinx_source = os.path.join(docs_dir, "sphinx")
    sphinx_output = os.path.abspath(ns.sphinx_output)

    if not os.path.isfile(doxyfile_path):
        raise ConanException(f"Doxyfile not found at {doxyfile_path}")

    if not os.path.isdir(sphinx_source):
        raise ConanException(f"Sphinx source directory not found at {sphinx_source}")

    output = ConanOutput()

    doxygen_output_dir = os.path.join(docs_dir, "build", "doxygen")
    os.makedirs(doxygen_output_dir, exist_ok=True)

    doxygen_env = os.environ.copy()
    doxygen_env["CMAKE_BUILD_DIR"] = os.path.abspath(ns.build_folder)
    output.info("Running Doxygen to generate XML...")
    _run(["doxygen", doxyfile_path], env=doxygen_env)

    output.info("Generating Rust XML documentation from cargo doc...")
    _generate_rust_docs(repo_root, output)

    os.makedirs(sphinx_output, exist_ok=True)
    output.info("Running Sphinx to generate HTML...")
    sphinx_exe = shutil.which("sphinx-build")
    if sphinx_exe:
        sphinx_cmd = [
            sphinx_exe,
            "-b",
            ns.sphinx_builder,
            "-j",
            "1",
            sphinx_source,
            sphinx_output,
        ]
    else:
        python3_exe = shutil.which("python3")
        if not python3_exe:
            raise ConanException("Unable to find 'sphinx-build' or 'python3' to run Sphinx.")
        sphinx_cmd = [
            python3_exe,
            "-m",
            "sphinx",
            "-b",
            ns.sphinx_builder,
            "-j",
            "1",
            sphinx_source,
            sphinx_output,
        ]
    _run(sphinx_cmd)

    output.success(f"Documentation generated in {sphinx_output}")


def _run(cmd, env=None, cwd=None):
    result = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=cwd)
    if result.returncode != 0:
        raise ConanException(
            f"Command '{' '.join(cmd)}' failed with code {result.returncode}:\n{result.stderr}"
        )


def _generate_rust_docs(repo_root, output):
    workspace = os.path.join(repo_root, "scheduler-rs")
    if not os.path.isdir(workspace):
        output.info("Rust workspace not found, skipping cargo doc export.")
        return

    rustdoc_root = os.path.join(repo_root, "docs", "build", "rustdoc")
    xml_dir = os.path.join(rustdoc_root, "xml")
    os.makedirs(xml_dir, exist_ok=True)
    for stale in glob.glob(os.path.join(xml_dir, "*.xml")):
        os.remove(stale)

    env = os.environ.copy()
    env["RUSTDOCFLAGS"] = "-Z unstable-options --output-format json"
    output.info("Running 'cargo +nightly doc --workspace --no-deps'...")
    _run(
        ["cargo", "+nightly", "doc", "--workspace", "--no-deps"],
        env=env,
        cwd=workspace,
    )

    rustdoc_json_dir = os.path.join(workspace, "target", "doc")
    json_files = glob.glob(os.path.join(rustdoc_json_dir, "*.json"))
    if not json_files:
        output.warning("No rustdoc JSON artifacts were produced; skipping XML conversion.")
        return

    for json_path in json_files:
        _convert_rustdoc_json_to_xml(json_path, xml_dir)


def _convert_rustdoc_json_to_xml(json_path, xml_dir):
    crate_name = os.path.splitext(os.path.basename(json_path))[0]
    with open(json_path, "r", encoding="utf-8") as handle:
        data = json.load(handle)

    root = ET.Element("crate", name=crate_name, version=data.get("crate_version", ""))
    index = {str(k): v for k, v in data.get("index", {}).items()}
    paths = data.get("paths", {})

    for item_id, meta in paths.items():
        if meta.get("kind") != "struct":
            continue
        path_parts = meta.get("path", [])
        if not path_parts or path_parts[0] != crate_name:
            continue

        struct = index.get(str(item_id))
        if not struct:
            continue

        module_path = "::".join(path_parts[1:-1])
        struct_elem = ET.SubElement(root, "struct", name=struct.get("name", ""))
        if module_path:
            struct_elem.set("module", module_path)

        doc_text = (struct.get("docs") or "").strip()
        doc_elem = ET.SubElement(struct_elem, "doc")
        if doc_text:
            doc_elem.text = doc_text

        struct_inner = struct.get("inner", {}).get("struct", {})
        kind = struct_inner.get("kind", {})
        field_ids = []
        if "plain" in kind:
            field_ids = kind["plain"].get("fields", [])
        elif "tuple" in kind:
            field_ids = kind["tuple"].get("fields", [])

        for field_id in field_ids:
            field = index.get(str(field_id))
            if not field:
                continue
            field_elem = ET.SubElement(
                struct_elem,
                "field",
                name=field.get("name") or "",
                type=_render_rust_type(field.get("inner", {}).get("struct_field", {})),
            )
            field_doc = (field.get("docs") or "").strip()
            doc_node = ET.SubElement(field_elem, "doc")
            if field_doc:
                doc_node.text = field_doc

    tree = ET.ElementTree(root)
    xml_path = os.path.join(xml_dir, f"{crate_name}.xml")
    tree.write(xml_path, encoding="utf-8", xml_declaration=True)


def _render_rust_type(type_info):
    if not type_info:
        return "unknown"
    if "struct_field" in type_info:
        return _render_rust_type(type_info["struct_field"])
    if "primitive" in type_info:
        return type_info["primitive"]
    if "generic" in type_info:
        return type_info["generic"]
    if "tuple" in type_info:
        return "(" + ", ".join(_render_rust_type(t) for t in type_info["tuple"]) + ")"
    if "borrowed_ref" in type_info:
        ref = type_info["borrowed_ref"]
        lifetime = ref.get("lifetime")
        lifetime_part = f"{lifetime} " if lifetime else ""
        mut = "mut " if ref.get("mutability") == "mutable" else ""
        return f"&{lifetime_part}{mut}{_render_rust_type(ref.get('type'))}".strip()
    if "slice" in type_info:
        return f"[{_render_rust_type(type_info['slice'])}]"
    if "array" in type_info:
        arr = type_info["array"]
        return f"[{_render_rust_type(arr.get('type'))}; {arr.get('len', 0)}]"
    if "resolved_path" in type_info:
        path = type_info["resolved_path"]
        args = _render_generic_args(path.get("args"))
        return f"{path.get('path')}<{args}>" if args else path.get("path")
    if "qualified_path" in type_info:
        qp = type_info["qualified_path"]
        trait = qp.get("trait", {}).get("path", "")
        name = qp.get("name", "")
        self_type = _render_rust_type(qp.get("self_type"))
        return f"<{self_type} as {trait}>::{name}"
    return "unknown"


def _render_generic_args(arg_info):
    if not arg_info or "angle_bracketed" not in arg_info:
        return ""
    rendered = []
    for arg in arg_info["angle_bracketed"].get("args", []):
        if "type" in arg:
            rendered.append(_render_rust_type(arg["type"]))
        elif "lifetime" in arg:
            rendered.append(arg["lifetime"])
        elif "const" in arg:
            const = arg["const"]
            rendered.append(str(const.get("expr", "")))
    return ", ".join(filter(None, rendered))
