"""

"""
from typing import List
import re
import os
import json
import subprocess
import logging
from convert_project_common import TranslationUnitInfo, compdb_from_json
from expand_macros import expandMacros, ExpandMacrosOptions

SLASH = os.sep
# file in which the total commands will be stored.
TOTAL_COMMANDS_FILE = os.path.realpath("convert_all.sh")

# to separate multiple commands in a line
DEFAULT_ARGS = ["-dump-stats"]
# XXX: Do we actually intend for convert_project to support Windows? We haven't
# tested that in a long time.
if os.name == "nt":
    DEFAULT_ARGS.append("-extra-arg-before=--driver-mode=cl")


# We no longer take the checkedc_include_dir here because we assume the working
# tree is set up so that the Checked C headers get used automatically by 3c.
def run3C(_3c_bin,
          extra_3c_args,
          compilation_base_dir,
          compile_commands_json,
          skip_paths,
          expand_macros_opts: ExpandMacrosOptions,
          skip_running=False):
    filters = [re.compile(i) for i in skip_paths]
    with open(compile_commands_json) as cdb_f:
        compdb = compdb_from_json(json.load(cdb_f))

    translation_units: List[TranslationUnitInfo] = []
    all_files = []
    for tu in compdb:
        if tu.input_filename.endswith(".cpp"):
            continue  # Checked C extension doesn't support cpp files yet
        file_realpath = tu.input_realpath()
        file_relative_to_basedir = os.path.relpath(file_realpath,
                                                   compilation_base_dir)
        # Only let filters match path components under the base dir. For
        # example, we might have a filter `test` that is intended to exclude a
        # subdirectory of the base dir, but if the user put the base dir at
        # something like `~/test/foo-benchmark`, we don't want to exclude
        # everything in the base dir.
        if any(j.match(file_relative_to_basedir) for j in filters):
            continue
        all_files.append(file_realpath)
        translation_units.append(tu)

    expandMacros(expand_macros_opts, compilation_base_dir, translation_units)

    args = []
    args.append(_3c_bin)
    args.extend(DEFAULT_ARGS)
    args.extend(extra_3c_args)
    args.append('-p')
    args.append(compile_commands_json)
    # Disable all compiler warnings during the 3C input loading phase.
    # Generally, we don't want to do anything about them and they are just
    # distracting.
    args.append('-extra-arg=-w')
    args.append('-base-dir="' + compilation_base_dir + '"')
    # Try to choose a name unlikely to collide with anything in any real
    # project.
    args.append('-output-dir="' + compilation_base_dir + '/out.checked"')
    args.extend(list(set(all_files)))

    with open(TOTAL_COMMANDS_FILE, 'w') as f:
        # Using an array literal rather than backslashes makes it easy for the
        # user to comment out individual lines.
        arg_lines = '\n'.join(args)
        f.write(f'''\
#!/bin/bash
args=(
{arg_lines}
)
"${{args[@]}}"
''')
    os.system("chmod +x " + TOTAL_COMMANDS_FILE)
    # run whole command
    if not skip_running:
        logging.info("Running:" + str(' '.join(args)))
        subprocess.check_call(' '.join(args), shell=True)
    logging.debug("Saved the total command into the file:" +
                  TOTAL_COMMANDS_FILE)
    os.system("cp " + TOTAL_COMMANDS_FILE + " " + os.path.join(
        compilation_base_dir, os.path.basename(TOTAL_COMMANDS_FILE)))
    logging.debug("Saved to:" + os.path.join(
        compilation_base_dir, os.path.basename(TOTAL_COMMANDS_FILE)))
