from argparse import ArgumentParser
from glob import glob
from pathlib import Path
from subprocess import run


def init_argparse() -> ArgumentParser:
    parser = ArgumentParser(
        description="export coverage data from the profdata format to the lcov format",
    )
    parser.add_argument(
        "--genhtml",
        help="genhtml binary.",
        required=True,
        type=str,
    )
    parser.add_argument(
        "--source-dir",
        help="cmake source dir",
        required=True,
        type=Path,
    )
    parser.add_argument(
        "--binary-dir",
        help="cmake binary dir",
        required=True,
        type=Path,
    )
    parser.add_argument(
        "--profiles-dir",
        help="profiles dir",
        required=True,
        type=Path,
    )
    return parser


def main() -> None:
    parser = init_argparse()
    args = parser.parse_args()

    profiles = glob("**/*.info", root_dir=args.profiles_dir, recursive=True)
    profiles = list(map(lambda v: str(Path(args.profiles_dir)/v) ,profiles))
    profiles = " ".join(profiles)

    cmd = f"{args.genhtml} --dark-mode --branch-coverage --demangle-cpp -o coverage --prefix {args.source_dir} {profiles}"
    print(cmd)
    run(args=cmd,shell=True)

if __name__ == "__main__":
    main()
