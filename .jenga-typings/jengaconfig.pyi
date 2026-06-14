# =============================================================================
# jengaconfig.pyi — STUB GENERE par Jenga (au build / `jenga ide`). NE PAS EDITER.
# Declare les symboles des fichiers de config charges via useconfig(), pour que
# l'editeur (Pyright/Pylance) les COLORE et donne go-to-def / autocomplete.
# =============================================================================
from typing import Any

GLSLANG_LIBS_UNIX: Any
GLSLANG_LIBS_WINDOWS: Any
def HasLibdecor(*args: Any, **kwargs: Any) -> Any: ...
def NormPath(*args: Any, **kwargs: Any) -> Any: ...
def PkgExists(*args: Any, **kwargs: Any) -> Any: ...
TC_WINDOWS: Any
USE_CANVAS_NKUI: Any
USE_NKGLAD: Any
USE_NKGLSLANG: Any
USE_NKSPIRVCROSS: Any
VULKAN_INCLUDE: Any
VULKAN_LIB: Any
VULKAN_SDK: Any
WANT_VULKAN: Any
WAYLAND_DEFINES: Any
WAYLAND_LINKS: Any
WAYLAND_TEST_LINKS: Any
def dep(*args: Any, **kwargs: Any) -> Any: ...
def nkentseudependson(*args: Any, **kwargs: Any) -> Any: ...
def nkentseutoolchain(*args: Any, **kwargs: Any) -> Any: ...
def useappdeps(*args: Any, **kwargs: Any) -> Any: ...
def useglobalkind(*args: Any, **kwargs: Any) -> Any: ...
