"""Build native visual identities from the verified offline item catalog."""
import argparse
from pathlib import Path
from native_loot_catalog_data import build

if __name__ == "__main__":
    p=argparse.ArgumentParser()
    p.add_argument("catalog",type=Path)
    p.add_argument("--audit-dir",type=Path,help="Audit output directory (default: build/native-loot-audit)")
    args=p.parse_args()
    build(args.catalog,args.audit_dir)
