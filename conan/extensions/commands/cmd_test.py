import os

from conan.api.model import RecipeReference
from conan.api.output import ConanOutput
from conan.cli.args import (
    add_common_install_arguments,
    add_lockfile_args,
    add_reference_args,
)
from conan.cli.command import OnceArgument, conan_command
from conan.cli.commands.test import run_test as builtin_run_test
from conan.cli.printers import print_profiles
from conan.internal.errors import conanfile_exception_formatter


@conan_command(group="Creator")
def test(conan_api, parser, *args):
    """
    Run either the local conanfile.py test() implementation (no reference argument)
    or the standard Conan test_package flow (when a reference is provided).
    """
    parser.add_argument(
        "path",
        action=OnceArgument,
        help="Path to the local recipe (for workspace tests) or to a test_package folder.",
        default="test_package",
        nargs="?",
    )
    parser.add_argument(
        "reference",
        action=OnceArgument,
        nargs="?",
        help="Package reference (name/version[@user/channel]) to test via test_package. "
        "Omit this argument to run the workspace conanfile.py test() using the current build tree.",
    )
    add_reference_args(parser)
    add_common_install_arguments(parser)
    add_lockfile_args(parser)
    args = parser.parse_args(*args)

    cwd = os.getcwd()
    path = conan_api.local.get_conanfile_path(args.path, cwd, py=True)
    overrides = eval(args.lockfile_overrides) if args.lockfile_overrides else None
    lockfile = conan_api.lockfile.get_lockfile(
        lockfile=args.lockfile,
        conanfile_path=path,
        cwd=cwd,
        partial=args.lockfile_partial,
        overrides=overrides,
    )
    remotes = conan_api.remotes.list(args.remote) if not args.no_remote else []
    profile_host, profile_build = conan_api.profiles.get_profiles_from_args(args)
    print_profiles(profile_host, profile_build)

    if args.reference:
        _run_builtin_test(
            conan_api,
            path,
            args.reference,
            profile_host,
            profile_build,
            remotes,
            lockfile,
            args,
            cwd,
        )
        return

    _run_workspace_tests(
        conan_api,
        path,
        profile_host,
        profile_build,
        lockfile,
        remotes,
        args,
    )


def _run_builtin_test(
    conan_api,
    path,
    reference,
    profile_host,
    profile_build,
    remotes,
    lockfile,
    args,
    cwd,
):
    ref = RecipeReference.loads(reference)
    deps_graph = builtin_run_test(
        conan_api,
        path,
        ref,
        profile_host,
        profile_build,
        remotes,
        lockfile,
        args.update,
        build_modes=args.build,
    )
    lockfile = conan_api.lockfile.update_lockfile(
        lockfile,
        deps_graph,
        args.lockfile_packages,
        clean=args.lockfile_clean,
    )
    conan_api.lockfile.save_lockfile(lockfile, args.lockfile_out, cwd)
    return deps_graph


def _run_workspace_tests(
    conan_api,
    path,
    profile_host,
    profile_build,
    lockfile,
    remotes,
    args,
):
    deps_graph = conan_api.graph.load_graph_consumer(
        path,
        args.name,
        args.version,
        args.user,
        args.channel,
        profile_host,
        profile_build,
        lockfile,
        remotes,
        args.update,
    )
    conanfile = deps_graph.root.conanfile
    out = ConanOutput()
    out.title("Running workspace tests via conanfile.test()")
    if hasattr(conanfile, "layout"):
        with conanfile_exception_formatter(conanfile, "layout"):
            conanfile.layout()
    source_folder = (
        conanfile.recipe_folder
        if conanfile.folders.root is None
        else os.path.normpath(
            os.path.join(conanfile.recipe_folder, conanfile.folders.root)
        )
    )
    conanfile.folders.set_base_source(source_folder)
    conanfile.folders.set_base_build(source_folder)
    conanfile.in_local_cache = False
    conan_api.local.test(conanfile)
