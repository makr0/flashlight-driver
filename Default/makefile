################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
LSS += \
lux-drv.lss \

FLASH_IMAGE += \
lux-drv.hex \

EEPROM_IMAGE += \
lux-drv.eep \

SIZEDUMMY += \
sizedummy \


# All Target
all: lux-drv.elf secondary-outputs

# Tool invocations
lux-drv.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: AVR C Linker'
	avr-gcc -Wl,-Map,lux-drv.map -mmcu=attiny13 -o "lux-drv.elf" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

lux-drv.lss: lux-drv.elf
	@echo 'Invoking: AVR Create Extended Listing'
	-avr-objdump -h -S lux-drv.elf  >"lux-drv.lss"
	@echo 'Finished building: $@'
	@echo ' '

lux-drv.hex: lux-drv.elf
	@echo 'Create Flash image (ihex format)'
	-avr-objcopy -R .eeprom -O ihex lux-drv.elf  "lux-drv.hex"
	@echo 'Finished building: $@'
	@echo ' '

lux-drv.eep: lux-drv.elf
	@echo 'Create eeprom image (ihex format)'
	-avr-objcopy -j .eeprom --no-change-warnings --change-section-lma .eeprom=0 -O ihex lux-drv.elf  "lux-drv.eep"
	@echo 'Finished building: $@'
	@echo ' '

sizedummy: lux-drv.elf
	@echo 'Invoking: Print Size'
	-avr-size --format=avr --mcu=attiny13 lux-drv.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(ASM_DEPS)$(EEPROM_IMAGE)$(FLASH_IMAGE)$(ELFS)$(LSS)$(S_DEPS)$(SIZEDUMMY)$(S_UPPER_DEPS) lux-drv.elf
	-@echo ' '

secondary-outputs: $(LSS) $(FLASH_IMAGE) $(EEPROM_IMAGE) $(SIZEDUMMY)

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets
