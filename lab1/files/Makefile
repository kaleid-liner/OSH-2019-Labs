bin = led.img
elf = led.elf
obj = led.o
src = led.s

AS = arm-linux-gnueabihf-as
LD = arm-linux-gnueabihf-ld
CP = arm-linux-gnueabihf-objcopy

$(bin) : $(elf)
	$(CP) $(elf) -O binary $(bin)

$(elf) : $(obj)
	$(LD) $(obj) -o $(elf)

$(obj) : $(src)
	$(AS) $(src) -o $(obj)

clean:
	rm $(obj) $(elf) $(bin)

