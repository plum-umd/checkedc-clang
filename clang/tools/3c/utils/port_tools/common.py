# Data structures that need to be imported by both generate_ccommands and
# expand_macros.

from typing import List, NamedTuple

class TranslationUnitInfo(NamedTuple):
    compiler_path: str
    compiler_args: List[str]
    target_directory: str
    file: str
