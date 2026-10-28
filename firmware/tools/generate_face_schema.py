#!/usr/bin/env python3
"""Generate the shared C++ face schema header from pet_faces/face_schema.json."""
import argparse
import sys
from pathlib import Path

from face_schema import SCHEMA_PATH, SchemaError, load_schema, render_header

OUTPUT_PATH = Path(__file__).resolve().parents[1] / "main" / "pet" / "face_schema.generated.h"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", type=Path, default=SCHEMA_PATH)
    parser.add_argument("--output", type=Path, default=OUTPUT_PATH)
    args = parser.parse_args()
    try:
        schema = load_schema(args.schema)
    except SchemaError as error:
        print(f"face schema: {error}", file=sys.stderr)
        return 2
    args.output.write_text(render_header(schema))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
