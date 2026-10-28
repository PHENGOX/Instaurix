# GitHub preparation — October 25, 2026

This is a historical preparation report. See [README.md](../README.md) for current development status.

## Scope

Review of files selected for publication using .gitignore, documented dependencies, existing licenses, and reference programs used by CMake. This review is not a legal certification of every line of code or an exhaustive guarantee that no secrets exist.

## Results

- Project code uses GPL-3.0-only, with the full text in LICENSE. The unmodified GNU GPL v3 text came from COPYING3 in the locally verified newlib archive.
- libnx ISC notices and newlib's multiple licenses are retained in reference directories. THIRD_PARTY_NOTICES.md and source/header notice collections under docs/licenses/ were added.
- No XCI/NSP/NCA game files, firmware identified by its location, console keys, host executables or downloaded tools were among selected files. Test ELF/NRO programs and binary vectors were retained with available sources, generators and manifests.
- No matches for the searched private-key, GitHub-token, AWS-credential or Slack-token signatures were found in selected files. This check does not detect every form of secret.
- Files and reference directories explicitly used by CMake are included.
- Three CPU files exceeding 100 MiB require Git LFS. .gitattributes defines them; pointer generation was verified locally, including SHA-256 correspondence and recognition by `git lfs ls-files`.
- Git attributes preserve file bytes so that verified source and reference hashes are not changed.
- A copy extracted from the archive was configured and built in Release without working-copy caches (`BUILD_TESTING=OFF`). The base program produced X0=5, X1=7, X2=12. The existing application test, run against the extracted NRO, reproduced 7,428 instructions, 35 IPC exchanges and zero failures. Preparation did not change emulator code; the preceding milestone's 60 Debug/Release suites remained the latest complete validation at that time.

The main working directory was not initialized as a Git repository. A temporary repository under build/ checked attributes and LFS pointers. No commit, remote repository or GitHub upload was created.

## Local deliverables

`build/Switch2Emu-github.zip` contained the selected files under a Switch2Emu directory, without caches or builds. It contained actual LFS data; after LFS is enabled, Git must convert these files to pointers during staging.

`build/github-publication-audit.json` recorded the inventory, sizes, SHA-256 hashes and automated check results. Both preparation artifacts remain excluded from the repository. They are historical snapshots and do not automatically include subsequent development changes.

## User step

Extract the archive and follow [GITHUB_PUBLICATION.md](GITHUB_PUBLICATION.md). Enable Git LFS before first staging and check all three references. Creating the remote repository and its first upload remain user actions.
