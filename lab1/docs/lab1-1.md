# lab1-1 Report

- **姓名**：魏剑宇

- **学号**：PB17111586

## RPi Boot Process

树莓派的启动和一般的计算机有所不同，主要是存储设备的原因。主要是为了省钱。

其启动可以分为三个阶段：

- **Stage 1**：主管整个系统系统的是VideoCore IV GPU，它从板上的一个ROM中加载了一个体积很小，功能简单的Bootloader。其唯一的目的就是加载第二阶段的bootloader。
- **Stage 2**：第二阶段的bootloader，也就是存于SD卡上的`bootcode.bin`。SD卡的文件系统是FAT16/32，`bootcode.bin`存在于其第一个分区中，一般是`/dev/mmcblk0p1`。在加载完成后，由GPU负责执行此程序，来加载第三阶段的bootloader——`start.elf`。
- **Stage 3**：`start.elf`负责主要的启动任务。它：
  - 首先读入`config.txt`，来配置GPU和linux kernel的加载过程。
  - 加载并分析`cmdline.txt`，它为系统内核提供了一些参数。
  - 加载`kernel.img`。这是存有系统内核的镜像文件。
  - `start.elf`从`cmdline.txt`中读入了一些配置，其中有一个类似于`root=/dev/mmcblk0p2`。这个`root` 是文件系统的根目录。
  - linux kernel从将`/dev/mmcblk0p2`挂载到`/`上，之后便一步一步加载内核的其它模块。

## Common Boot Process

对一般的计算机而言，上电后，由BIOS读取加载程序，这个加载程序存储在第一个扇区内，也就是MBR(Master Boot Record)。每个文件系统上安装操作系统时，其bootloader会加载到MBR上，并覆盖掉原来的。

从MBR的bootloader，对linux而言，一般是GRUB。之后内核将一步步加载，内核的加载过程并无差别。

## File Systems

从[BootProcess](#RPI-Boot-Process)可以看出，至少要用到的文件系统是FAT16/32和一般的linux fs（如ext4）。

由于stage1的bootloader的功能较简单，其只能识别FAT格式的文件系统，故`bootcode.bin`一般存在于FAT文件系统中。而linux kernel则在SD卡的第二个分区中，此分区会采用一般的linux文件系统。

## References

- [Understanding RaspberryPi Boot Process - BeyondLogic](https://wiki.beyondlogic.org/index.php?title=Understanding_RaspberryPi_Boot_Process)

- [Standalone-partitioning-explained](https://github.com/raspberrypi/noobs/wiki/Standalone-partitioning-explained)
