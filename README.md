# Quark-OS

Quark-OS is a 32-bit x86 hobby operating system written in C and NASM. It boots
through GRUB into a graphical desktop and includes a kernel, a small RAM-backed
filesystem, a shell, the Forge scripting language, and a window manager.

## Features

- Damage-tracked desktop compositor with a dock, clock, notifications, and
  Spotlight app launcher (`Ctrl+Space`)
- Desktop and fullscreen-terminal workspaces, switchable with
  `Ctrl+Alt+Left` and `Ctrl+Alt+Right`
- Dark and light themes, multiple wallpapers, and configurable window snapping
- Desktop icons are hidden by default; enable **Show desktop icons** in Settings
  to show them
- RamFS file manager with create, rename, copy, delete, and Forge launch actions
- Built-in shell with pipes, redirection, environment variables, aliases, and
  command history
- Forge interpreted scripting language; `.fg` files run directly from the
  shell, `source` routes `.fg` files to Forge while retaining shell-script
  execution for other files, and bare `forge` opens the interactive REPL
- Apps: Editor, Calculator, Files, About Quark, and System Monitor
- System Monitor reports uptime, kernel heap and physical memory, RamFS file
  storage, and open desktop apps. Its CPU activity percentage is an estimate
  based on PIT ticks observed during halted waits, not a hardware performance
  counter
- Wired Ethernet support for Intel e1000 and VIA Rhine PCI Fast Ethernet
  IDs `1106:3043`, `1106:3065`, `1106:3053`, and `1106:3106`, with a QEMU
  e1000 adapter enabled by `make run`. The kernel requests
  IPv4 configuration using DHCP, resolves the gateway with ARP, and supports
  DNS A lookups with `nslookup <hostname>`. It can also answer ARP and IPv4
  ICMP echo requests after receiving a lease. VIA physical hardware has not
  yet been tested. Wi-Fi, general outbound IP/UDP, TCP/HTTPS, downloads, and
  browser functionality are not implemented yet.

System Monitor is available through Spotlight and intentionally does not appear
in the dock.

## Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Space` | Open Spotlight |
| `Ctrl+Alt+Left` / `Ctrl+Alt+Right` | Switch workspaces |
| `Alt+Tab` | Cycle windows |
| `Ctrl+S` in Editor | Save to RamFS |
| `Ctrl+O` in Editor | Reload current file |
| `N` in Files | Create a file |
| `F` in Files | Create a folder |
| `R` in Files | Rename selected item |
| `D` in Files | Delete selected item |
| `E` in Files | Open selected file in Editor |
| `G` in Files | Run selected `.fg` file with Forge |
| `U` in Files | Go up one directory |
| `Esc` | Cancel a modal or clear selection |

## Build Requirements

The normal build requires GCC with 32-bit support, NASM, GNU binutils, GRUB
rescue tools, and xorriso. QEMU is needed to run the OS. Python 3 and Pillow are
needed only when regenerating sprite assets.

Debian or Ubuntu:

```sh
sudo apt install build-essential gcc-multilib nasm grub-pc-bin grub-common \
                 xorriso qemu-system-x86
```

Fedora:

```sh
sudo dnf install gcc nasm grub2-tools xorriso qemu-system-x86
```

Arch Linux:

```sh
sudo pacman -S base-devel nasm grub xorriso qemu-system-i386
```

To regenerate sprites, install Pillow for Python 3:

```sh
python3 -m pip install Pillow
```

## Build and Run

Build the kernel and bootable ISO:

```sh
make -j2
```

`make run` attaches an Intel e1000 adapter using QEMU user-mode networking.
This provides a test device for driver bring-up; it does not yet give Quark-OS
internet protocols or web browsing.

The build produces `quark_kernel.elf` and `quark_os_5.iso`. To build and start
the ISO in QEMU:

```sh
make run
```

Other useful targets:

```sh
make sprites   # regenerate baked image data
make fonts     # regenerate baked font data
make clean     # remove build outputs
```