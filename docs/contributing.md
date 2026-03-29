# Contributing to Solo5

Before starting this document, we recommend that you read about the Solo5
[architecture][architecture] to understand the project. Regarding
contributions, Solo5 uses GitHub CI to compile the project on several
platforms:
- Linux (Ubuntu, x86 and ARM)
- FreeBSD
- OpenBSD

Tests are only run on Linux/x86. We advise contributors to test on their own
machines. These tests require superuser access to allocate virtual interfaces.
We recommend reading [tests/README.md][tests].

Solo5 uses [`clang-format`][clang-format] to format the code. If a commit
consists solely of code formatting and needs to be kept, it must also be added
to the `.git-blame-ignore-revs` file to avoid cluttering the history.

If someone wishes to format their code, this command applies `clang-format` to
the entire codebase:
```shell
$ git ls-files '*.c' '*.h' | xargs clang-format -i
```

## How to release Solo5?

The usual way to make a release of Solo5 is:
- Make a new branch: `git checkout -b prepare-vX.Y.Z`
- Update [CHANGES.md][changes] file to include changes
- Do a commit with this file: "Update CHANGES for release vX.Y.Z"
- Push the current branch and make a new pull-request on GitHub
- Waiting review from someone (if the CHANGES.md is clear, etc.)

If you have some feedbacks:
- Redo the commit with feedbacks
- Redo the tag and `make distrib`
- Push-force on the same branch `prepare-vX.Y.Z`

Only one commit should contains the diff on CHANGES.md.

If everything is ok:
- `git tag vX.Y.Z`
- Execute `./configure.sh` to generate `Makeconf`
- Execute `make distrib`
- Execute `./scripts/opam-release.sh`
- Push the tag: `git push --tags`
- On GitHub, you must create a new release which includes the
  `solo5-vX.Y.Z.tar.gz` artifact
- Locally, you can clone `ocaml/opam-repository` and copy OPAM files into
  `ocaml/opam-repository/packages`
- Make a pull-request on `ocaml/opam-repository`
- Waiting review and that's all!
