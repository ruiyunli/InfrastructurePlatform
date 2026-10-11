#!/usr/bin/env python3
"""milvus-tool.py —— 查看 Milvus Lite 本地库内容的小工具"""

import argparse
import glob
import os
import sys

from pymilvus import DataType, MilvusClient

# 默认只打印向量的前几维，完整向量用 --full
VECTOR_PREVIEW_DIMS = 3


def find_db_file(directory="."):
    """找目录下第一个 .db 文件（按名字排序），没有则返回 None"""
    matches = sorted(glob.glob(os.path.join(directory, "*.db")))
    return matches[0] if matches else None


def format_field(field):
    """把 describe_collection 的一个字段格式化成 `名字: 类型 附加信息`"""
    text = f"{field['name']}: {DataType(field['type']).name}"

    params = field.get("params") or {}
    if "dim" in params:
        text += f" dim={params['dim']}"
    if "max_length" in params:
        text += f" max_length={params['max_length']}"
    if field.get("is_primary"):
        text += " (主键)"

    return text


def _format_number(value):
    # 向量以 float32 存储，0.1 落盘会变成 0.10000000149011612，四舍五入抹掉噪声
    return repr(round(float(value), 6))


def format_value(value, full=False):
    """格式化一个字段值；向量默认只显示前几维"""
    if isinstance(value, (list, tuple)):
        shown = list(value) if full else list(value[:VECTOR_PREVIEW_DIMS])
        text = ", ".join(_format_number(v) for v in shown)
        if not full and len(value) > VECTOR_PREVIEW_DIMS:
            text += ", ..."
        return f"[{text}]"

    return str(value)


def format_row(row, full=False):
    """把一行数据格式化成 `字段=值` 拼起来的一行；字符串加引号以便看出空串"""
    parts = []
    for key, value in row.items():
        if isinstance(value, str):
            parts.append(f"{key}={value!r}")
        else:
            parts.append(f"{key}={format_value(value, full)}")
    return "  ".join(parts)


SHOW_DESCRIPTION = """查看库内容。

不带 -c：只列出库里所有集合的名字、条数和字段结构（概览），看不到条目详情。
带 -c ：打印指定集合的条目详情；-n、-f、--full 这三个选项只有此时才生效。
"""


def build_parser():
    parser = argparse.ArgumentParser(
        prog="milvus-tool.py",
        description="查看 Milvus Lite 本地库（.db 文件）里有什么",
        add_help=False,
    )
    parser.add_argument("-h", "-help", "--help", action="help", help="显示帮助并退出")

    subparsers = parser.add_subparsers(dest="command", title="可用命令", required=True)

    show = subparsers.add_parser(
        "show",
        help="查看库内容：不带 -c 给概览，带 -c 打印某个集合的数据",
        description=SHOW_DESCRIPTION,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        add_help=False,
    )
    show.add_argument("-h", "-help", "--help", action="help", help="显示帮助并退出")
    show.add_argument("-p", "--path", help="指定 .db 库文件；默认取当前目录下第一个")
    show.add_argument("-c", "--collection", help="指定集合名；不带则只列概览，看不到条目详情")
    show.add_argument("-n", "--limit", type=int, default=10, help="打印几条数据（默认 10，需配合 -c）")
    show.add_argument("-f", "--filter", default="", help='过滤表达式，如 "id in [1, 2]"（需配合 -c）')
    show.add_argument("--full", action="store_true", help="向量完整打印，默认只打前 3 维（需配合 -c）")
    show.set_defaults(func=cmd_show)

    help_parser = subparsers.add_parser(
        "help",
        help="显示帮助：help 看总览，help 加命令名看某个命令的帮助",
        add_help=False,
    )
    help_parser.add_argument("-h", "-help", "--help", action="help", help="显示帮助并退出")
    help_parser.add_argument("topic", nargs="?", help="要查看帮助的命令名，如 show")
    help_parser.set_defaults(func=cmd_help)

    # cmd_help 需要拿到根 parser 和子命令表才能转发打印
    parser.set_defaults(_parser=parser, _subparsers=subparsers)

    return parser


def _print_overview(client, name):
    """打印一个集合的概览：条数 + 字段"""
    row_count = client.get_collection_stats(name).get("row_count", 0)
    print(f"  {name}（{row_count} 条记录）")
    for field in client.describe_collection(name)["fields"]:
        print(f"    {format_field(field)}")


def _show_all(client):
    collections = client.list_collections()
    if not collections:
        print("库里还没有任何集合。")
        return 0

    print(f"共 {len(collections)} 个集合：")
    print()
    for name in collections:
        _print_overview(client, name)
    return 0


def _show_one(client, args):
    name = args.collection
    collections = client.list_collections()
    if name not in collections:
        print(f"集合不存在: {name}", file=sys.stderr)
        print(f"可用的集合: {', '.join(collections) or '（无）'}", file=sys.stderr)
        return 1

    row_count = client.get_collection_stats(name).get("row_count", 0)

    # 查询前集合需处于 loaded 状态；Lite 下通常已自动加载，重复加载会报错，故忽略
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
    # filter 恰好是 "主键 in [...]" 时，Milvus 走主键检索路径、忽略 limit 返回全部命中，
    # 所以这里再按 -n 截一刀，保证 -n 说到做到
    rows = rows[: args.limit]

    print(f"集合 {name}（共 {row_count} 条记录，显示 {len(rows)} 条）")
    print()
    for row in rows:
        print(format_row(row, full=args.full))
    return 0


def cmd_show(args):
    if args.path:
        # Milvus Lite 的 .db 是目录（新版本）也可能是文件（旧版本），所以只判存在
        if not os.path.exists(args.path):
            print(f"库不存在: {args.path}", file=sys.stderr)
            return 1
        path = args.path
    else:
        path = find_db_file()
        if path is None:
            print(f"没有找到 .db 库（当前目录: {os.getcwd()}），可用 -p 指定", file=sys.stderr)
            return 1

    print(f"库: {path}")
    if not args.path:
        candidates = sorted(glob.glob("*.db"))
        if len(candidates) > 1:
            print(f"（当前目录有 {len(candidates)} 个 .db，已选第一个，可用 -p 指定）")
    print()

    client = MilvusClient(path)
    try:
        return _show_all(client) if not args.collection else _show_one(client, args)
    finally:
        client.close()


def cmd_help(args):
    """不带参数打印总览帮助，带命令名打印该命令的帮助（等价于 `show -help`）"""
    if args.topic is None:
        args._parser.print_help()
        return 0

    target = args._subparsers.choices.get(args.topic)
    if target is None:
        print(f"没有这个命令: {args.topic}", file=sys.stderr)
        print(f"可用命令: {', '.join(args._subparsers.choices)}", file=sys.stderr)
        return 1

    target.print_help()
    return 0


def main(argv=None):
    # Windows 控制台默认 GBK，中文输出会乱码，这里显式改为 UTF-8
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")

    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
