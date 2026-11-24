"""blosc2_kolmo: locates the Kolmo codec plugin for c-blosc2 and python-blosc2 (DRAFT, not published).

c-blosc2 loads a registered external codec on first use: it tries dlopen("libblosc2_kolmo.so"), then runs
``python -c "import blosc2_kolmo; blosc2_kolmo.print_libpath()"`` and opens the path printed. A wheel of this package
ships ``libblosc2_kolmo.so`` (built by ``make -C integration/blosc2``) next to this file. See
../../REGISTRY-SUBMISSION.md: the package is published only after the codec has a global ID and Kolmo is released.
"""
import os

__all__ = ["ID", "NAME", "libpath", "print_libpath"]

ID = 244  # PLACEHOLDER from Blosc2's user range; the global ID once c-blosc2 registers it
NAME = "kolmo"


def libpath() -> str:
    return os.path.join(os.path.dirname(os.path.abspath(__file__)), "libblosc2_kolmo.so")


def print_libpath() -> None:
    # c-blosc2 reads one line and opens it as is: no trailing newline
    print(libpath(), end="")
