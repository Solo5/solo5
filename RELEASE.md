# Make a release of Solo5

The usual way to make a release of Solo5 is:
- Make a new branch: `git checkout -b prepare-vX.Y.Z`
- Update CHANGES.md file to include changes
- Do a commit with this file: "Update CHANGES for release vX.Y.Z"
- Push the current branch and make a new pull-request on GitHub
- Waiting review from someone (if the CHANGES.md is clear, etc.)

If you have some feedbacks:
- Redo the commit with feedbacks
- Redo the tag and `make distrib`
- Push-force on the same branch `prepare-vX.Y.Z`

Only one commit should contains the diff on `CHANGES.md`

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
