# =============================================================================
# Quark-OS Build Automation Engine
# =============================================================================

CC      := gcc
AS      := nasm
LD      := ld
EMU     := qemu-system-i386
NET_ARGS := -netdev user,id=net0 -device e1000,netdev=net0
TARGET  := quark_kernel.elf
ISO_OUT := quark_os_5.iso

CFLAGS  := -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude -nostdlib \
           -fno-builtin -fno-stack-protector -fno-pie -fno-stack-check \
           -fno-omit-frame-pointer -fno-asynchronous-unwind-tables \
           -fno-unwind-tables

ifdef DEBUG
CFLAGS  += -g -DDEBUG
endif

ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -no-pie -z max-page-size=4096 --no-dynamic-linker

C_SRCS   := drivers/pic.c drivers/keyboard.c drivers/terminal.c drivers/timer.c \
            drivers/net.c drivers/mouse.c drivers/video.c \
            mm/heap.c mm/pmm.c \
            kernel/main.c kernel/gdt.c kernel/idt.c kernel/proc.c \
            kernel/shell.c kernel/shell_parser.c kernel/shell_commands.c \
            kernel/syscall.c kernel/ramfs.c kernel/forge_interp.c \
            kernel/desktop/desktop.c \
            kernel/desktop/de_theme.c \
            kernel/desktop/de_widgets.c \
            kernel/desktop/de_windows.c \
            kernel/desktop/de_wm.c \
            kernel/desktop/de_panel.c \
            kernel/desktop/de_spotlight.c \
            kernel/desktop/de_settings.c \
            kernel/desktop/de_notify.c \
            kernel/desktop/de_rtc.c \
            kernel/desktop/de_wallpaper.c \
            kernel/desktop/de_scroll.c \
            kernel/desktop/de_input.c \
            kernel/desktop/de_focus.c \
            kernel/desktop/de_damage.c \
            kernel/desktop/de_workspace.c \
            kernel/desktop/ws_terminal.c \
            kernel/desktop/de_anim.c \
            kernel/desktop/de_font.c \
            kernel/desktop/de_font_data_body.c \
            kernel/desktop/de_font_data_title.c \
            kernel/desktop/apps/app_registry.c \
            kernel/desktop/apps/editor.c \
            kernel/desktop/apps/calculator.c \
            kernel/desktop/apps/files.c \
            kernel/desktop/apps/about.c \
            kernel/desktop/apps/monitor.c \
            kernel/desktop/de_image.c \
            kernel/desktop/de_sprites.c \
            lib/string.c

ASM_SRCS := boot.asm kernel/gdt_flush.asm kernel/isr_stubs.asm
OBJS     := $(C_SRCS:.c=.o) $(ASM_SRCS:.asm=.o)

.PHONY: all clean run run-debug iso fonts sprites

all: $(TARGET) iso

$(TARGET): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[LINK] $(TARGET) built."
	@size $(TARGET) 2>/dev/null || true

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

iso: $(TARGET)
	@mkdir -p iso_root/boot/grub
	@cp $(TARGET) iso_root/boot/
	@printf 'set timeout=0\n'                          >  iso_root/boot/grub/grub.cfg
	@printf 'set default=0\n'                          >> iso_root/boot/grub/grub.cfg
	@printf 'menuentry "Quark-OS" {\n'                 >> iso_root/boot/grub/grub.cfg
	@printf '    multiboot /boot/quark_kernel.elf\n'   >> iso_root/boot/grub/grub.cfg
	@printf '    set gfxpayload=1024x768x32\n'         >> iso_root/boot/grub/grub.cfg
	@printf '    boot\n'                               >> iso_root/boot/grub/grub.cfg
	@printf '}\n'                                      >> iso_root/boot/grub/grub.cfg
	@grub2-mkrescue -o $(ISO_OUT) iso_root 2>/dev/null
	@echo "[ISO] $(ISO_OUT) built."

# Regenerate the baked font .c files from the .ttf source.
fonts:
	python3 tools/gen_font.py tools/fonts/static/Inter_18pt-Regular.ttf \
	    kernel/desktop/de_font_data_body.c de_font_body 9 32 126
	python3 tools/gen_font.py tools/fonts/static/Inter_18pt-Regular.ttf \
	    kernel/desktop/de_font_data_title.c de_font_title 13 32 126
	@echo "[FONTS] regenerated."

sprites:
	./tools/bake_sprites.sh
	@echo "[SPRITES] regenerated."

run: iso
	$(EMU) -cdrom $(ISO_OUT) -no-reboot -serial stdio -m 128M $(NET_ARGS)

run-debug: iso
	$(EMU) -cdrom $(ISO_OUT) -no-reboot -serial stdio -m 128M \
	       $(NET_ARGS) -d int,cpu_reset -no-shutdown

clean:
	@rm -f $(OBJS) $(TARGET) $(ISO_OUT)
	@rm -rf iso_root
	@echo "[CLEAN] done."