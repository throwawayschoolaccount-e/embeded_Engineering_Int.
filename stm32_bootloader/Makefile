TARGET  = ota_bootloader
CC      = arm-none-eabi-gcc
CXX     = arm-none-eabi-g++
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

ED25519_DIR = third_party/ed25519/src

INCLUDES = -I. -I$(ED25519_DIR)

COMMON_FLAGS = -mcpu=cortex-m3 -mthumb -O2 -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -Wall -Wextra $(INCLUDES)
CFLAGS   = $(COMMON_FLAGS) -std=c11
CXXFLAGS = $(COMMON_FLAGS) -std=c++17 -fno-exceptions -fno-rtti -fno-unwind-tables

LDFLAGS = -T linker.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(TARGET).map

CXX_SOURCES = main.cpp system_init.cpp uart2.cpp flash_driver.cpp \
              rollback.cpp uart_receiver.cpp signature_verify.cpp libc_shim.cpp led.cpp
C_SOURCES   = $(ED25519_DIR)/verify.c $(ED25519_DIR)/sha512.c \
              $(ED25519_DIR)/ge.c $(ED25519_DIR)/fe.c $(ED25519_DIR)/sc.c
ASM_SOURCES = startup.s

OBJECTS = $(ASM_SOURCES:.s=.o) $(CXX_SOURCES:.cpp=.o) $(C_SOURCES:.c=.o)

all: $(TARGET).bin $(TARGET).hex
	$(SIZE) $(TARGET).elf

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJECTS) linker.ld
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJECTS) -o $(TARGET).elf

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $(TARGET).elf $(TARGET).hex

clean:
	rm -f $(OBJECTS) *.elf *.bin *.hex *.map

.PHONY: all clean
