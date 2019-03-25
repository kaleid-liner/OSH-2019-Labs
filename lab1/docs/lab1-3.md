# lab1-3 Report

- **姓名**：魏剑宇
- **学号**：PB17111586

---

## Error prone works

在这个实验中，卡我最长时间的bug就是gpio地址的问题。读芯片手册发现gpio的基址是`0x7E200000`，和教程中不一样，结果经过尝试并不对。原因在于我对[physical address](https://en.wikipedia.org/wiki/Physical_address)和[bus address](https://stackoverflow.com/questions/24903841/difference-between-the-physical-address-device-address-and-virtiual-address)之间的区别没有概念，搞清楚这个，能正确访问RPi的外围设备，汇编就比较好写了。

## Assembly

```assembly
.section .init
.global _start
_start:

    ldr r0, =0x3F200000
    ldr r1, =0x3F003000

    mov r2, #1
    lsl r2, #27
    str r2, [r0, #8]

    lsl r2, #2
    mov r5, #0  @old
    ldr r6, =1000000

    led_on:

        ldr r3, [r1, #4] @now
        add r8, r5, r6
        cmp r3, r8
        blo led_on

        str r2, [r0, #28]
        mov r5, r3

        noblink:

        ldr r3, [r1, #4] @now
        add r8, r5, r6
        cmp r3, r8
        blo noblink

        str r2, [r0, #40]
        mov r5, r3

        skip:

    b led_on
```

在上述代码中，访问硬件的部分和教程一致。在时间的控制上，我通过轮询system timer，获取当前计数器的值，每过1000000次计数（即1000000 * 1 / 1MHz = 1s）将灯置为开/关，从而达到每2s blink一次的效果。

## Q & A

> **Q**：在这一部分实验中 `/dev/sdc1` 中除了 `kernel7.img` （即 `led.img`）以外的文件哪些是重要的？他们的作用是什么？

和[lab1-2](./lab1-2.md#questions)中的基本相同，只不过此时`cmdline.txt`和`*.dtb`不再重要了，因为不再有操作系统内核了，同样我写的程序也不会读取`cmdline.txt`内的参数了。

> **Q**：在这一部分实验中`/dev/sdc2`是否被用到？为什么？

没有用到。在lab1-2中会被用到的原因是linux内核会挂载`/dev/mmcblk0p2`到`/`上，并执行`/init`程序。但在这个实验中，我们的`kernel7.img`只是一个使led blink的程序而已，并不会读取`/dev/mmcblk0p2`的内容。

> **Q**：生成`led.img`的过程中用到了`as`, `ld`, `objcopy` 这三个工具，他们分别有什么作用，我们平时编译程序会用到其中的哪些？

- `as`：将汇编代码汇编为目标文件
- `ld`：对各个目标文件进行符号链接和数据重定位（还包括填充文件头等过程），形成可执行文件。这里形成的是linux下的ELF格式文件。
- `objcopy`：将一种格式的文件转化为另一种格式的文件，这里是将ELF转化为纯二进制文件。

平时我们会用到`as`和`ld`。编译器会先将源代码转化为汇编，再汇编为二进制，这一步需要用到`as`（部分编译器可能跳过这一步直接生成机器码）。同时需要进行连接，于是要用到`ld`。
