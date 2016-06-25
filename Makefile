# put your *.c source files here, make should handle the rest!
SRCS = $(wildcard Src/*.c)

# all the files will be generated with this name (main.elf, main.bin, main.hex, etc)
PROJ_NAME=main

# Location of the Libraries folder from the STM32F0xx Standard Peripheral Library
STD_PERIPH_LIB=Drivers

# Linker script path
LINKER_SCRIPT=STM32F303VC_FLASH.ld

DEVICE=STM32F303xC

# that's it, no need to change anything below this line!

###################################################

CC=arm-none-eabi-gcc
GDB=arm-none-eabi-gdb
OBJCOPY=arm-none-eabi-objcopy
OBJDUMP=arm-none-eabi-objdump
SIZE=arm-none-eabi-size

CFLAGS  = -Wall -g -std=c99 -Os -D$(DEVICE)
CFLAGS += -mlittle-endian -mcpu=cortex-m4  -march=armv7e-m -mthumb
CFLAGS += -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS += -ffunction-sections -fdata-sections
#CFLAGS += -ffreestanding -nostdlib

LDFLAGS += -Wl,--gc-sections -Wl,-Map=$(PROJ_NAME).map 
LDFLAGS += --specs=rdimon.specs
#LDFLAGS += --specs=nosys.specs

###################################################

vpath %.a $(STD_PERIPH_LIB)

ROOT=$(shell pwd)

CFLAGS += -I Inc 
CFLAGS += -I $(STD_PERIPH_LIB) 
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Device/ST/STM32F3xx/Include
CFLAGS += -I $(STD_PERIPH_LIB)/CMSIS/Include 
CFLAGS += -I $(STD_PERIPH_LIB)/STM32F3xx_HAL_Driver/Inc
CFLAGS += -I $(STD_PERIPH_LIB)/STM32_USB-FS-Device_Driver/inc
#CFLAGS += -include $(STD_PERIPH_LIB)/stm32f30x_conf.h
CFLAGS += -I Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS
CFLAGS += -I Middlewares/Third_Party/FreeRTOS/Source/include
CFLAGS += -I Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F
CFLAGS += -I Middlewares/Third_Party/FatFs/src
CFLAGS += -I Middlewares/ST/STM32_USB_Device_Library/Core/Inc
CFLAGS += -I Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc

#STARTUP = Device/startup_stm32f30x.s # add startup file to build
STARTUP = Drivers/CMSIS/Device/ST/STM32F3xx/Source/Templates/gcc/startup_stm32f303xc.s

# need if you want to build with -DUSE_CMSIS 
#SRCS += stm32f0_discovery.c
#SRCS += stm32f0_discovery.c stm32f0xx_it.c

SRCS += $(wildcard Drivers/STM32F3xx_HAL_Driver/Src/*.c)
SRCS += Drivers/CMSIS/Device/ST/STM32F3xx/Source/Templates/system_stm32f3xx.c

SRCS += $(wildcard Middlewares/Third_Party/FreeRTOS/Source/*.c) 
SRCS += Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c
SRCS += Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
SRCS += $(wildcard Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/*.c)

SRCS += $(wildcard Middlewares/ST/STM32_USB_Device_Library/Core/Src/*.c)
SRCS += $(wildcard Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/*.c)

SRCS += $(wildcard Middlewares/Third_Party/FatFs/src/*.c)
SRCS += $(wildcard Middlewares/Third_Party/FatFs/src/option/*.c)

OBJS = $(addprefix objs/,$(SRCS:.c=.o))
DEPS = $(addprefix deps/,$(SRCS:.c=.d))

###################################################

.PHONY: all lib proj program debug clean

all: lib proj
	$(SIZE) $(PROJ_NAME).elf

-include $(DEPS)

lib:
	@#$(MAKE) -C $(STD_PERIPH_LIB)

proj: 	$(PROJ_NAME).elf

objs:
	@mkdir -p deps objs

#objs/%.o : Src/%.c dirs
objs/%.o : %.c objs
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $< -MMD -MF deps/$(*F).d

$(PROJ_NAME).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(STARTUP) -T$(LINKER_SCRIPT)
	$(OBJCOPY) -O ihex $(PROJ_NAME).elf $(PROJ_NAME).hex
	$(OBJCOPY) -O binary $(PROJ_NAME).elf $(PROJ_NAME).bin
	$(OBJDUMP) -St $(PROJ_NAME).elf >$(PROJ_NAME).lst

program: all
	#openocd -f $(OPENOCD_BOARD_DIR)/stm32f3discovery.cfg -f $(OPENOCD_PROC_FILE) -c "stm_flash `pwd`/$(PROJ_NAME).bin" -c shutdown
	st-flash --reset write $(PROJ_NAME).bin 0x08000000

debug: program
	$(GDB) -x extra/gdb_cmds $(PROJ_NAME).elf

clean:
	find ./ -name '*~' | xargs rm -f	
	rm -f objs/*.o
	rm -f deps/*.d
	rm -rf objs
	rm -rf deps
	rm -f $(PROJ_NAME).elf
	rm -f $(PROJ_NAME).hex
	rm -f $(PROJ_NAME).bin
	rm -f $(PROJ_NAME).map
	rm -f $(PROJ_NAME).lst

