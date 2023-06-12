# 基于RSIC-V的实时操作系统

项目描述：基于RSIC-V架构开发一款简易的实时操作系统。功能包括UART读写，中断管理，内存管理，多任务调度。

项目地址：https://github.com/SheldonLoveCoding/my_RVOS

该项目是在中科院软件所的汪辰老师的RVOS基础上进行改进（[RVOS](https://gitee.com/unicornx/riscv-operating-system-mooc)）

主要职责：
1. 阅读RVOS源码在原始项目的汇编和文件上添加必要注释；
2. 利用首次适配法和内存控制块实现字节级的内存管理；
3. 基于优先级数组和双向链表实现协作式多任务调度；
4. 基于定时器中断和时间片轮转方法实现抢占式多任务调度；
5. 调整项目结构，按功能对各文件进行整理