"""
PlatformIO pre-build script for PageMind (pagemind_*) environments.

Replaces CROSSPOINT_VERSION with the fork version from platformio.fork.ini
([pagemind] version) and adds CROSSPOINT_UPSTREAM_VERSION from
platformio.ini ([crosspoint] version). Development envs get "-dev-<sha>".

OTA compares CROSSPOINT_VERSION with release tags as major.minor.patch,
so the fork version must stay numeric (e.g. "0.1.0").
"""

import configparser
import os
import subprocess


def read_option(path, section, option):
    config = configparser.ConfigParser()
    config.read(path, encoding='utf-8')
    return config.get(section, option)


def short_sha(project_dir):
    try:
        return subprocess.check_output(
            ['git', 'rev-parse', '--short', 'HEAD'], text=True, cwd=project_dir
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return 'unknown'


def is_version_define(define):
    name = define[0] if isinstance(define, (tuple, list)) else define
    return str(name).split('=', 1)[0] == 'CROSSPOINT_VERSION'


def setup(env):
    project_dir = env['PROJECT_DIR']
    fork_version = read_option(os.path.join(project_dir, 'platformio.fork.ini'), 'pagemind', 'version')
    upstream_version = read_option(os.path.join(project_dir, 'platformio.ini'), 'crosspoint', 'version')
    if env.GetProjectOption('custom_fork_release', 'no') != 'yes':
        fork_version = f'{fork_version}-dev-{short_sha(project_dir)}'

    def add_version(build_env, node):
        # Only translation units that use the version macros need the clone.
        contents = node.srcnode().get_contents()
        if b'CROSSPOINT_VERSION' not in contents and b'ForkIdentity.h' not in contents:
            return node
        version_env = build_env.Clone()
        defines = [d for d in version_env.get('CPPDEFINES', []) if not is_version_define(d)]
        defines.append(('CROSSPOINT_VERSION', f'\\"{fork_version}\\"'))
        defines.append(('CROSSPOINT_UPSTREAM_VERSION', f'\\"{upstream_version}\\"'))
        version_env.Replace(CPPDEFINES=defines)
        return version_env.Object(node)

    env.AddBuildMiddleware(add_version)
    print(f'PageMind build version: {fork_version} (CrossPoint {upstream_version})')


Import('env')  # noqa: F821  # type: ignore[name-defined]
setup(env)  # noqa: F821  # type: ignore[name-defined]
