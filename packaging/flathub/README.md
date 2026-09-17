# Flathub submission kit (human-filed — see note at the bottom)

## What to submit

1. Fork https://github.com/flathub/flathub, create branch
   `new/io.github.hultwl.rufux`.
2. Copy `packaging/flathub/io.github.hultwl.rufux.yml` from this repo
   into the fork root (same filename).
3. Open a PR against `flathub/flathub:master` with the body below
   (fill in the video link first).

## PR body to paste

```markdown
Please confirm your submission meets all the criteria

Please describe the application briefly.
Rufux is a Linux-native port of Rufus (C/GTK4, GPLv3): format and
create bootable USB drives from ISO and disk images, with GPT/MBR
partitioning, FAT32/NTFS/exFAT/UDF/ext4 filesystems, SHA-256
checksums, persistence partitions, bad-blocks checks, and Secure
Boot status reporting, via CLI and GUI.

Please attach a video showcasing the application on Linux using the Flatpak.
<PASTE user-attachments video URL here>

The Flatpak ID follows all the rules listed in the Application ID requirements.
I have read and followed all the Submission requirements and the Submission guide and I agree to them.
The application has a meaningful development history, evidence of real-world use, and a clear commitment to ongoing maintenance, as required by the development history requirements.
I have disclosed any AI-generated material included in the application or its Flathub packaging, as required by the Generative AI policy. Affected parts and approximate extent: the Linux port layer (src/linux/*, src/gui/*), test suites, and most docs/packaging were drafted with AI coding assistance, then reviewed, built, hardware-tested (real USB burns), and released by the human author. Upstream Rufus sources kept in-tree are human-written (Pete Batard, GPLv3). CI workflows and Flatpak manifest were AI-drafted, human-verified via green builds.
I have not used AI tools or agents to generate or automate this submission pull request or its review interactions.
I am an author to the project.
```

## IMPORTANT: why you file it, not the AI

The template requires you to affirm *you* did not use AI for the PR
or its review thread. So: you paste the body, you click Create, and
you reply to reviewers yourself. This kit only prepares the files.

## Still needed from you

- **Video**: record the Flatpak build running (install it with
  `flatpak-builder --install`, capture with your phone or a screen
  recorder), upload to the PR as a user-attachments link. The policy
  explicitly requires the video to show the Flatpak, not a native build.
- Expect reviewer threads on `--device=all` (justified: raw block
  access is the app's purpose) and on testing block writes under
  `flatpak run` (currently refused up front with directions; see
  packaging/README.md).
