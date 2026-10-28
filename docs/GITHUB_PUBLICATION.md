# Publishing Instaurix on GitHub

## Repository presentation

Short GitHub description:

> Experimental C++20 emulator targeting Nintendo Switch 2. ARM64 CPU, ELF/NRO loading and HLE services under development. No games currently validated.

The project's own code uses **GPL-3.0-only**. LICENSE contains the full GNU GPL v3 text. Third-party files retain their terms, documented in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

## Files to publish

Include sources, tests, reference programs, build scripts, documentation, CMakeLists.txt, program.bin, .gitignore, .gitattributes, LICENSE and THIRD_PARTY_NOTICES.md. Test ELF/NRO/bin files are intentional; excluding all of them would break tests on a fresh checkout.

Three references exceed 100 MiB and use **Git LFS**, as defined in .gitattributes: cpu_fp/reference.bin (about 192 MiB), cpu_simd_arithmetic/reference.bin (122 MiB), and cpu_subnormal/reference.bin (129 MiB). Install Git LFS and run `git lfs install` before first staging files. Then check `git lfs ls-files`: all three references must appear. Do not upload them as ordinary Git files through the web interface. Their storage and downloads use the GitHub account's LFS quotas.

Exclude build/, out/, caches, downloaded tools, local Python installations and dependency archives. .gitignore does not remove files already tracked: review staged files before the first commit. Do not upload the entire working directory without reviewing it.

## Update the existing repository

The current repository is https://github.com/PHENGOX/Instaurix. Clone that
repository with Git LFS, create an update branch, and copy the prepared project
files into the clone while preserving its .git directory. Review changes,
commit, push the branch and open a pull request into main. Merge after the
Windows/Linux Debug/Release checks pass. This retains history and contributors.
Do not create a replacement repository or force-push a new history.

Only copy repository content from a publication bundle, not the outer bundle's
personal instructions, manifests or build outputs. Existing LFS references must
remain managed by .gitattributes. Preserve the author note and contributor credits.

## Create a repository for the first time only

1. Create an empty repository on GitHub, for example Instaurix. Do not generate a second README, license or .gitignore.
2. In GitHub Desktop, add the local directory as a repository; if it is not already a Git repository, use the offered creation option.
3. Enable Git LFS as described above, review the file list, and create the first commit.
4. Select Publish repository, choose public visibility, and inspect the published repository.

A prepared publication archive can instead be extracted into a new directory and added to GitHub Desktop. This keeps build directories and caches separate from the upload.

After cloning, `git lfs pull` retrieves the references. Prefer a Git clone with LFS for testing: a GitHub ZIP download may contain pointers instead of data.

## Status to communicate

This is an experimental emulator; homebrews and test programs are validation targets. The CPU is partial. A bounded historical Maxwell copy interpreter and software framebuffer presentation are available; a general GPU rendering pipeline and native Switch 2 compatibility remain unvalidated. Reference programs reach project main through default libnx startup and original cleanup. No commercial game compatibility is claimed.

See the [current README](../README.md) and [TLS milestone](LIBNX_TLS.md). File and signature checks alone do not certify all copyrights or the absence of secrets. Review future additions, especially binaries.
