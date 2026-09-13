# Quark-OS

> A 32-bit x86 hobby operating system written from scratch — with its own
> kernel, filesystem, shell, scripting language, and KDE Plasma-inspired
> desktop environment.

![status](https://img.shields.io/badge/status-active-brightgreen)
![arch](https://img.shields.io/badge/arch-i386-blue)
![license](https://img.shields.io/badge/license-MIT-lightgrey)

Quark-OS boots via GRUB into protected mode and hands control to a
graphical desktop. It is written in C and NASM with no external libraries
and no host OS underneath — every subsystem is implemented in-tree.

## Highlights

- **Graphical desktop** — windowed environment with a damage-tracked
  compositor, panel, app launcher, floating clock, and notifications
- **Quark-OS-DE wallpaper** — procedural wave field, cached and
  blitted per-region
- **Two workspaces** — graphical desktop and fullscreen terminal,
  switchable with `Ctrl+Alt+Arrow`
- **Shell** — ~10 builtin commands with pipes, redirection, env vars,
  aliases, and history
- **Forge** — a small interpreted scripting language built into the kernel
- **RamFS** — flat filesystem with directories and file ops
- **Apps** — text editor, calculator, file manager, settings, about

## Keyboard Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Alt+Left` / `Ctrl+Alt+Right` | Switch workspace |
| `Alt+Tab` | Cycle windows |
| `Ctrl+S` in Editor | Save to RamFS |
| `Ctrl+O` in Editor | Reload current file |
| `N` in Files | New file |
| `F` in Files | New folder |
| `R` in Files | Rename selected |
| `D` in Files | Delete selected |
| `E` in Files | Open in Editor |
| `G` in Files | Run `.fg` with Forge |
| `U` in Files | Go up one directory |
| `Esc` | Cancel modal / clear selection |

## Building

Requires `gcc` (with 32-bit support), `nasm`, `ld`, `grub-mkrescue`,
`xorriso`, and `qemu-system-i386`.

**Debian/Ubuntu:**
```sh
sudo apt install build-essential gcc-multilib nasm grub-pc-bin \
                 grub-common xorriso qemu-system-x86

**Fedora:**
sudo dnf install gcc nasm grub2-tools xorriso qemu-system-x86

**Arch:**
sudo pacman -S base-devel nasm grub xorriso qemu-system-i386
