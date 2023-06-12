include ./common.mk

SRCS_ASM = \
	start.S \
	./mem/mem.S \
	entry.S \

SRCS_C = \
	kernel.c \
	./uart/uart.c \
	./uart/printf.c \
	./mem/page.c \
	./sched/sched.c \
	./user/user.c \
	./trap/trap.c \
	./trap/plic.c \
	./trap/timer.c \
	./lock/lock.c \

OBJS = $(SRCS_ASM:.S=.o)
OBJS += $(SRCS_C:.c=.o)

.DEFAULT_GOAL := all
all: os.elf

# start.o must be the first in dependency!
os.elf: ${OBJS}
	${CC} ${CFLAGS} -T os.ld -o os.elf $^
	${OBJCOPY} -O binary os.elf os.bin

%.o : %.c
	${CC} ${CFLAGS} -c -o $@ $<

%.o : %.S
	${CC} ${CFLAGS} -c -o $@ $<

run: all
	@${QEMU} -M ? | grep virt >/dev/null || exit
	@echo "Press Ctrl-A and then X to exit QEMU"
	@echo "------------------------------------"
	@${QEMU} ${QFLAGS} -kernel os.elf

.PHONY : debug
debug: all
	@echo "Press Ctrl-C and then input 'quit' to exit GDB and QEMU"
	@echo "-------------------------------------------------------"
	@${QEMU} ${QFLAGS} -kernel os.elf -s -S &
	@${GDB} os.elf -q -x ./gdbinit

.PHONY : code
code: all
	@${OBJDUMP} -S os.elf | less

.PHONY : clean
clean:
	find . -type f -name "*.o" -delete
	find . -type f -name "*.bin" -delete
	find . -type f -name "*.elf" -delete

