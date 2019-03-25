# lab1-2 Report

- **姓名**：魏剑宇
- **学号**：PB17111586

---

所有的操作在[官方文档](https://www.raspberrypi.org/documentation/linux/kernel/building.md)和lab1的[教程](https://github.com/OSH-2019/OSH-2019.github.io/tree/master/1/kernel)中都可以找到。

## Error prone works

- 在RPi 2/3中默认寻找的内核名称是`kernel7.img`，若要使用别的名称，如`kernel.img`，须在`config.txt`中设定`kernel=kernel.img`

## Tailor The Kernel

我裁减了以下内容，由于选项很多并没有一个一个去了解每个模块的意思故只裁剪了我确定的内容。

- Support for large (2TB+) block devices and files，唯一的存储是128GB的存储卡。
- Sound Card support，只用让led发亮不需要发出声音。
- networking support，不需要网络

## Questions

> **Q**:  `/dev/sdc1` 中除了 `kernel7.img` 以外的文件哪些是重要的？他们的作用是什么？

以下这些重要的文件主要用于[树莓派的启动过程](./lab1-1.md#rpi-boot-process)

- `bootcode.bin`：可执行程序，用于启动中，加载第三阶段的bootloader
- `start.elf`：可执行程序，用于第二阶段的启动过程，读入配置文件，加载内核
- `config.txt`：包含VideoCore的配置和linux内核加载地址等，取代了一般计算机的BIOS设置，详见[文档](https://www.raspberrypi.org/documentation/configuration/config-txt/README.md)
- `cmdline.txt`：包含了kernel运行时传入的参数
- `*.dtb`：硬件数据库文件，将硬件信息传给内核。

> **Q**：`/dev/sdc1` 中用到了什么文件系统，为什么？可以换成其他的吗？

用到了FAT文件系统。FAT是非常简单、古老、轻量级的一种文件系统，被大多数的底层程序支持。不能换成其他的，因为Raspberry pi用于加载`bootcode.bin`的bootloader只能识别FAT文件系统。

> **Q**：`/dev/sdc1` 中的 kernel 启动之后为什么会加载 `/dev/sdc2` 中的 init 程序？

linux内核启动后，加载的第一个应用程序就是`init`，其`PID=1`。init程序一般会用于用户空间的构建，包括挂载文件系统等。linux会加载init程序，而init程序的位置一般默认为`/init`，若使用`initrd`，其默认位置为`/sbin/init`。init程序的位置可以通过`cmdline.txt`中的`init=/path/to/init/program`来设置。

至于为什么会加载`/init`，可以见[linux源码](https://github.com/torvalds/linux/blob/master/init/main.c)。如下所示，

```c
static int __init init_setup(char *str)
{
	unsigned int i;

	execute_command = str;
	/*
	 * In case LILO is going to boot us with default command line,
	 * it prepends "auto" before the whole cmdline which makes
	 * the shell think it should execute a script with such name.
	 * So we ignore all arguments entered _before_ init=... [MJ]
	 */
	for (i = 1; i < MAX_INIT_ARGS; i++)
		argv_init[i] = NULL;
	return 1;
}
__setup("init=", init_setup);

static int __init rdinit_setup(char *str)
{
	unsigned int i;

	ramdisk_execute_command = str;
	/* See "auto" comment in init_setup */
	for (i = 1; i < MAX_INIT_ARGS; i++)
		argv_init[i] = NULL;
	return 1;
}
__setup("rdinit=", rdinit_setup);
```

这一段配置了command line，分为init和rdinit。init会设置`execute_command`的内容。rdinit会设置`ramdisk_execute_command`的内容。

然后是，

```c
static int __ref kernel_init(void *unused)
{
    kernel_init_freeable();

	/*
		--snippet--
	*/

	if (ramdisk_execute_command) {
		ret = run_init_process(ramdisk_execute_command);
		if (!ret)
			return 0;
		pr_err("Failed to execute %s (error %d)\n",
		       ramdisk_execute_command, ret);
	}

	/*
	 * We try each of these until one succeeds.
	 *
	 * The Bourne shell can be used instead of init if we are
	 * trying to recover a really broken machine.
	 */
	if (execute_command) {
		ret = run_init_process(execute_command);
		if (!ret)
			return 0;
		panic("Requested init %s failed (error %d).",
		      execute_command, ret);
	}
	if (!try_to_run_init_process("/sbin/init") ||
	    !try_to_run_init_process("/etc/init") ||
	    !try_to_run_init_process("/bin/init") ||
	    !try_to_run_init_process("/bin/sh"))
		return 0;

	panic("No working init found.  Try passing init= option to kernel. "
	      "See Linux Documentation/admin-guide/init.rst for guidance.");
}
```

 从上可以看出，如果既没有给`ramdisk_execute_command`，也没有给`execute_command`，`kernel_init`就会依次尝试`/sbin/init`、`/etc/init`、`/bin/init`、`/bin/sh`。那`/init`又是哪里来的呢？

观察到`kernel_init`开始还调用了`kernel_init_freeable`，看这个函数，可以发现如下内容，

```c
static noinline void __init kernel_init_freeable(void)
{
	// --snippet-- //
    
	if (!ramdisk_execute_command)
		ramdisk_execute_command = "/init";

	if (ksys_access((const char __user *)
			ramdisk_execute_command, 0) != 0) {
		ramdisk_execute_command = NULL;
		prepare_namespace();
	}
    
	// --snippet-- //
}
```

发现如果没有通过`Initial ramdisk`启动，则会先查找`/init`。

当然，在这个例子中，助教准备的`lab1.img`中的`cmdline.txt`里有`init=/init`，所以会从`/init`路径加载init程序。

> **Q**：开机两分钟后，`init` 程序退出， Linux Kernel 为什么会 panic？

这里可以从linux内核中的[exit.c](https://github.com/torvalds/linux/blob/master/kernel/exit.c)代码中看出，

```c
static struct task_struct *find_child_reaper(struct task_struct *father,
						struct list_head *dead)
	__releases(&tasklist_lock)
	__acquires(&tasklist_lock)
{
	// --snippet-- //
	if (unlikely(pid_ns == &init_pid_ns)) {
		panic("Attempted to kill init! exitcode=0x%08x\n",
			father->signal->group_exit_code ?: father->exit_code);
	}


	// --snippet-- //
}
```

当一个程序通过`exit`退出，通过C的库函数进行了sys call结束这个进程。这里`find_child_reaper`会找到父进程的子进程并“收割”掉。但它还检查了这个进程是否是`init`程序的进程（即比较其PID是否为1）。如果是，它会`panic`，告知试图kill掉init程序。（虽然我们并没有通过`kill`命令来结束init进程。）

虽然在做这个实验的时候我还没有屏幕，但我猜测此时屏幕上应该会输出`Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000000`。

## References

- [Kernel building - Raspberry Pi Documentation](https://www.raspberrypi.org/documentation/linux/kernel/building.md)