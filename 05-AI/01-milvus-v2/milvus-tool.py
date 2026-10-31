#!/usr/bin/env python3
"""milvus-tool.py -- a small utility to inspect a local Milvus Lite 2.x database"""

import argparse
import glob
import os
import sys
import warnings

from pymilvus import DataType, MilvusClient

# milvus-lite 2.5.1 still imports the deprecated pkg_resources API; silence that
# specific warning so the tool output stays readable
warnings.filterwarnings("ignore", message="pkg_resources is deprecated as an API")

# Only print the first few vector dimensions by default; use --full for the whole vector
VECTOR_PREVIEW_DIMS = 3


def choose_db_file(candidates):
    """Ask the user to pick one of several .db databases; return the chosen path or None"""
    print("Multiple .db databases found:")
    for index, path in enumerate(candidates, start=1):
        print(f"  {index}. {path}")

    while True:
        try:
            answer = input(f"Select one [1-{len(candidates)}]: ").strip()
        except EOFError:
            print("\nNo interactive input available; use -p to specify a database.", file=sys.stderr)
            return None

        if answer.isdigit() and 1 <= int(answer) <= len(candidates):
            return candidates[int(answer) - 1]

        print(f"Invalid selection, enter a number between 1 and {len(candidates)}.")


def format_field(field):
    """Format one field from describe_collection as `name: type extra-info`"""
    text = f"{field['name']}: {DataType(field['type']).name}"

    params = field.get("params") or {}
    if "dim" in params:
        text += f" dim={params['dim']}"
    if "max_length" in params:
        text += f" max_length={params['max_length']}"
    if field.get("is_primary"):
        text += " (primary key)"

    return text


def _format_number(value):
    # Vectors are stored as float32, so 0.1 becomes 0.10000000149011612 on disk;
    # round to remove that noise
    return repr(round(float(value), 6))


def format_value(value, full=False):
    """Format one field value; vectors show only the first few dimensions by default"""
    if isinstance(value, (list, tuple)):
        shown = list(value) if full else list(value[:VECTOR_PREVIEW_DIMS])
        text = ", ".join(_format_number(v) for v in shown)
        if not full and len(value) > VECTOR_PREVIEW_DIMS:
            text += ", ..."
        return f"[{text}]"

    return str(value)


def format_row(row, full=False):
    """Format a row as `field=value` pairs; quote strings so empty strings are visible"""
    parts = []
    for key, value in row.items():
        if isinstance(value, str):
            parts.append(f"{key}={value!r}")
        else:
            parts.append(f"{key}={format_value(value, full)}")
    return "  ".join(parts)


SHOW_DESCRIPTION = """Show database content.

Without -c: list only the names, row counts and field schemas of all collections
(overview); row details are not shown.
With -c    : print the row details of the given collection; the -n, -f and --full
options only take effect in this mode.
"""


def build_parser():
    parser = argparse.ArgumentParser(
        prog="milvus-tool.py",
        description="Inspect what is inside a local Milvus Lite 2.x database (.db file)",
        add_help=False,
    )
    parser.add_argument("-h", "-help", "--help", action="help", help="show help and exit")

    subparsers = parser.add_subparsers(dest="command", title="available commands", required=True)

    show = subparsers.add_parser(
        "show",
        help="show database content: overview without -c, rows of one collection with -c",
        description=SHOW_DESCRIPTION,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        add_help=False,
    )
    show.add_argument("-h", "-help", "--help", action="help", help="show help and exit")
    show.add_argument("-p", "--path", help="path to the .db file; defaults to the only one in the current directory, or asks you to choose from several")
    show.add_argument("-c", "--collection", help="collection name; without it only an overview is listed")
    show.add_argument("-n", "--limit", type=int, default=10, help="number of rows to print (default 10, requires -c)")
    show.add_argument("-f", "--filter", default="", help='filter expression, e.g. "id in [1, 2]" (requires -c)')
    show.add_argument("--full", action="store_true", help="print full vectors instead of the first 3 dimensions (requires -c)")
    show.set_defaults(func=cmd_show)

    help_parser = subparsers.add_parser(
        "help",
        help="show help: `help` for an overview, `help <command>` for a specific command",
        add_help=False,
    )
    help_parser.add_argument("-h", "-help", "--help", action="help", help="show help and exit")
    help_parser.add_argument("topic", nargs="?", help="command name to show help for, e.g. show")
    help_parser.set_defaults(func=cmd_help)

    # cmd_help needs the root parser and the subparser table to forward the print_help call
    parser.set_defaults(_parser=parser, _subparsers=subparsers)

    return parser


def _print_overview(client, name):
    """Print an overview of one collection: row count + fields"""
    row_count = client.get_collection_stats(name).get("row_count", 0)
    print(f"  {name} ({row_count} rows)")
    for field in client.describe_collection(name)["fields"]:
        print(f"    {format_field(field)}")


def _show_all(client):
    collections = client.list_collections()
    if not collections:
        print("The database has no collections yet.")
        return 0

    print(f"{len(collections)} collection(s):")
    print()
    for name in collections:
        _print_overview(client, name)
    return 0


def _show_one(client, args):
    name = args.collection
    collections = client.list_collections()
    if name not in collections:
        print(f"Collection does not exist: {name}", file=sys.stderr)
        print(f"Available collections: {', '.join(collections) or '(none)'}", file=sys.stderr)
        return 1

    row_count = client.get_collection_stats(name).get("row_count", 0)

    # The collection must be loaded before querying; under Lite it is usually loaded
    # automatically, and loading again raises an error, so ignore it
    try:
        client.load_collection(name)
    except Exception:
        pass

    rows = client.query(
        collection_name=name,
        filter=args.filter,
        output_fields=["*"],
        limit=args.limit,
    )
    # When the filter is exactly "primary_key in [...]", Milvus uses the primary-key
    # lookup path and ignores limit, returning all matches. Truncate here so -n is
    # honored.
    rows = rows[: args.limit]

    print(f"Collection {name} ({row_count} rows in total, showing {len(rows)})")
    print()
    for row in rows:
        print(format_row(row, full=args.full))
    return 0


def cmd_show(args):
    if args.path:
        # Milvus Lite 2.x stores the database as a single .db file
        if not os.path.exists(args.path):
            print(f"Database does not exist: {args.path}", file=sys.stderr)
            return 1
        path = args.path
    else:
        candidates = sorted(glob.glob("*.db"))
        if not candidates:
            print(f"No .db database found (current directory: {os.getcwd()}); use -p to specify one", file=sys.stderr)
            return 1
        if len(candidates) == 1:
            path = candidates[0]
        else:
            path = choose_db_file(candidates)
            if path is None:
                return 1

    print(f"Database: {path}")
    print()

    client = MilvusClient(path)
    try:
        return _show_all(client) if not args.collection else _show_one(client, args)
    finally:
        client.close()


def cmd_help(args):
    """Print the top-level help without arguments, or one command's help (same as `show -help`)"""
    if args.topic is None:
        args._parser.print_help()
        return 0

    target = args._subparsers.choices.get(args.topic)
    if target is None:
        print(f"No such command: {args.topic}", file=sys.stderr)
        print(f"Available commands: {', '.join(args._subparsers.choices)}", file=sys.stderr)
        return 1

    target.print_help()
    return 0


def main(argv=None):
    # Force UTF-8 output so non-ASCII text is not garbled in some Linux terminals
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
