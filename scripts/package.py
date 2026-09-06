"""Route packaging to the DX12 experiment; never package the stable renderer here."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1] / "experimental/package.py"), run_name="__main__")
