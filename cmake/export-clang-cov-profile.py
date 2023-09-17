from argparse import ArgumentParser
from glob import glob
from pathlib import Path
from subprocess import run


def init_argparse() -> ArgumentParser:
    parser = ArgumentParser(
        description="export coverage data from the profdata format to the lcov format",
    )
    parser.add_argument(
        "--llvm-cov",
        help="llvm-cov binary.",
        required=True,
        type=str,
    )
    parser.add_argument(
        "--llvm-profdata",
        help="llvm-profdata binary.",
        required=True,
        type=str,
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

    profiles = glob("**/*.profraw", root_dir=args.profiles_dir, recursive=True)
    profiles = list(map(lambda v: Path(args.profiles_dir)/v ,profiles))

    for profile in profiles:
        exe = profile.stem[8:]
        profdata = f"{profile.parent/exe}.profdata"

        cmd = f"{args.llvm_profdata} merge {profile} -o {profdata}"
        run(args=cmd,shell=True)

        cmd = f"{args.llvm_cov} export --format=lcov --instr-profile={profdata} {args.binary_dir}/bin/test_{exe} > {profile.parent/exe}.info"
        run(args=cmd,shell=True)


if __name__ == "__main__":
    main()
