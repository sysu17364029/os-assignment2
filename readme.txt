【虚存管理模拟程序】

vm.c包含FIFO和LRU的TLB及Page Replacement

运行vmm.sh可直接打印FIFO和LRU分别运行vm（TLB和页置换统一策略）的Page-fault rate和TLB hit rate

[附加题]

locality.c用来生成addresses_locality.txt

运行vmmp.sh可直接用locality.c生成addresses_locality.txt，并打印用vm.c测试的结果


【Linux内存管理实验】

mtest.c为自己创建的例子，打印代码段、数据段、BSS，栈、堆等的相关地址

[附加题]

myalloc.c包括一对简单的函数myalloc/myfree，实现堆上的动态内存分配和释放，并含有测试函数