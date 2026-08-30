# Security

## Supported version

Only the latest GitHub release is supported.

## Reporting

For a security issue, open a private GitHub security advisory rather than a public issue. For ordinary crashes or compatibility problems, use the issue tracker and attach `PeaceWalkerUltraWideFix.log` after checking it for information you do not want to share.

## Installer behavior

The setup modifies files only inside the selected Peace Walker installation and, on Linux, the current user's Steam launch options for AppID 2492660. Existing files are backed up. Uninstall restores a managed backup only when the active installed file still matches the recorded package hash; externally changed files are preserved.
