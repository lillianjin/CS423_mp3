# mp3 

## Implementation and Design Decisions
### 1) mp3_init
Load the module and create proc directory and status file.  
This function requires to initialize proc file directory, workqueue, spinlock, character device and profile buffer.  

### 2) mp3_exit  
Unload the module and remove proc directory and status file.  
This function requires to free all the memories used glabally or stop the process, including the shared memory and file directory.  

### 3) mp3_register  
Create and insert task struct into the process list.  
This function requires to initialize each variables of the task struct and insert queue delayed work into work queue. Make sure to add a new workqueue job if the current list is empty.

### 4) mp3_unregister  
Destroy the task struct from the process list.  
This function requires to remove the corresponding task from process task list. Make sure all works should be flushed if the list is empty.

### 5) mp3_write()  
Fetch task pid from userapp.  
This function requires to divide the input into two different cases(R, U).  

### 7) mp3_read()  
Read task pid, period and process time from /proc.  

### 8) work_handler():
Calculate minor page fault, major page fault, and utilization, and write them into shared profiler buffer. 
 
### 9) mp3_mmap():
Map the physical pages from the buffer to the virtual address space of a requested user process.


## Command to run the program
### 1) Compile the modules and user_app
```make all```

### 2) Insert compiled module into the kernel
```sudo insmod lujin2_mp3.ko```

### 3) Create node
Find the coresponding major number of your device and make node.
```cat /proc/devices```
```sudo mknod node c 246 0 ```

### 4) Run work and monitor functions

Work process 1: 1024MB Memory, Random Access, and 50,000 accesses per iteration   
Work process 2: 1024MB Memory, Random Access, and 10,000 accesses per iteration  

For example, to run the two work processes, we can input the commands like below.
```nice ./work 1024 R 50000 & nice ./work 1024 L 10000 &``` 

After running 20 iterations, run the next command.  
```sudo ./monitor profile1.data``` 

In the output file, the format of the data are as below: 
```[time in jiffies] [minor page fault] [major page fault] [utilization]```

### 5) Remove the module  
```sudo rmmod lujin2_mp3```


### Sample Output
### 1) Case Study 1: Thrasing and Locality. 
Given 4 work processes, we can compare the thrasing and locality by running process 1 & 2 and process 3 & 4.

Work process 1: 1024MB Memory, Random Access, and 50,000 accesses per iteration  
Work process 2: 1024MB Memory, Random Access, and 10,000 accesses per iteration
Work process 3: 1024MB Memory, Random Locality Access, and 50,000 accesses per iteration Work 
process 4: 1024MB Memory, Locality-based Access, and 10,000 accesses per iteration

The accumulated page fault count for process 1 & 2 and process 3 & 4 are shown below.

Process 1 & 2 | Process 3 & 4
------------ | -------------
![profile1](/profile1.png) | ![profile2](/profile2.png)

The total accumulated page faults for two random accesses is much larger than one random access and one locality access by about 100000 page faults. The reason of this situation is that random memory access takes no effort to preserve frequently accessed memory, which will increase the number of page faults. On the other hand, locality access takes advantages of spatial locality and the frequently used memories are less likely to be replaced. As a result, the process 1&2 will have larger page fault counts than process 3&4.

In addition, the completion time of the two work processes are 5133 and 5118 (unit is jiffies). The difference in computation time implies that 

### 2) Case Study 2. Multiprogramming
Work process 5: 200MB Memory, Random Locality Access, and 10,000 accesses per iteration
We run 1 time, 5 times and 11 times of process 5 to analyse cpu usage in this step, and the graph of results is in the below.  

![profile3](/profile3-1.png)

The average utilization in the plot is calculated by summing up the utlization and cpu time (in jiffies) for all processes and divides it by the total computing time (in jiffies).

With the increase of the degree of multiprogramming (1, 5, 11， 20, 22), the plot shows that the CPU utilization will increases exponentially first and than decrease because of thrashing. 

It is obvious that CPU utilization can be improved by using multiprogramming. This exponential increase in utilization can be proofed by equations. If there is one process in memory, the CPU utilization is (1-P), while if there are N processes in memory, the probability of N processes waiting for an I/O is $P^N$ and the CPU utilization is ($1 - P^N$ ). As N increases, the CPU utilization increases.

might result from the increasing number of page fault counts caused by more processes. 

As the memory for our VM is 4GB, after running 20 processes, thrashing will happen in the process. As the page fault rate increases due to more processes running, more transactions need processing from the paging device. Thrashing is harmful for CPU utilization and we need to control the number of multi-process to optimize utilization.

The completion time of the five work processes are 5006, 5022, 5028, 7016 and 18641 (unit is jiffies). This result shows that when memory is enough for multiprogramming, the computing time does not increase much. However, the completing time will increase dramatically once thrashing happens as the waiting queue at the paging device will increass. This is also because of the more page fault counts. 
