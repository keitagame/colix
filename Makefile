# Makefile - x86_64 マイクロカーネル

TARGET  = microkernel.elf
ISO     = microkernel.iso

CC      = gcc
AS      = nasm
LD      = ld

# コンパイルフラグ
CFLAGS  = -m64 \
           -ffreestanding \
           -fno-stack-protector \
           -fno-pic \
           -mno-red-zone \
           -mno-mmx -mno-sse -mno-sse2 \
           -nostdlib \
           -nostdinc \
           -Wall -Wextra \
           -O2 \
           -I$(PWD)/include

ASFLAGS = -f elf64

LDFLAGS = -T linker.ld \
           -melf_x86_64 \
           --no-dynamic-linker

# ソースファイル
C_SRCS = kernel/main.c \
		  kernel/initrd.c \
		  kernel/elf_loader.c \
		  kernel/lib/mem.c \
          kernel/lib/serial.c \
          kernel/arch/x86_64/vga.c \
          kernel/arch/x86_64/gdt.c \
          kernel/arch/x86_64/idt.c \
          kernel/arch/x86_64/pic_pit.c \
          kernel/mm/pmm.c \
          kernel/mm/vmm.c \
          kernel/proc/proc.c \
          kernel/proc/syscall.c \
          kernel/ipc/ipc.c

ASM_SRCS = boot/boot.asm \
            kernel/arch/x86_64/isr_stub.asm \
            kernel/arch/x86_64/context.asm \
            kernel/proc/proc_entry.asm

C_OBJS   = $(C_SRCS:.c=.o)
ASM_OBJS = $(ASM_SRCS:.asm=.o)
OBJS     = $(ASM_OBJS) $(C_OBJS)

.PHONY: all clean run iso

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Built: $@"
	@size $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.asm
	$(AS) $(ASFLAGS) -o $@ $<

# ISO作成 (GRUBが必要)
iso: $(TARGET)
	@mkdir -p isodir/boot/grub
	@cp initrd.cpio isodir/boot/
	@cp $(TARGET) isodir/boot/
	@echo 'set timeout=0'                          > isodir/boot/grub/grub.cfg
	@echo 'set default=0'                         >> isodir/boot/grub/grub.cfg
	@echo 'menuentry "MicroKernel" {'             >> isodir/boot/grub/grub.cfg
	@echo '  multiboot2 /boot/$(TARGET)'          >> isodir/boot/grub/grub.cfg
	@echo '  module2   /boot/initrd.cpio'         >> isodir/boot/grub/grub.cfg
	@echo '  boot'                                >> isodir/boot/grub/grub.cfg
	@echo '}'                                     >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO) isodir
	@echo "ISO: $(ISO)"

# QEMUで起動 (GRUBなし - multiboot2直接)
run: $(TARGET)
	qemu-system-x86_64 \
		-kernel $(TARGET) \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-no-shutdown

# QEMUでISOから起動
run-iso: $(ISO)
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-m 128M \
		-no-reboot -nographic

# デバッグ (GDB接続待ち)
debug: $(TARGET)
	qemu-system-x86_64 \
		-kernel $(TARGET) \
		-serial stdio \
		-m 128M \
		-no-reboot \
		-S -gdb tcp::1234 &
	@echo "Connect GDB: target remote localhost:1234"

clean:
	rm -f $(OBJS) $(TARGET) $(ISO)
	rm -rf isodir
