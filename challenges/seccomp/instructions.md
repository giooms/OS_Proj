# Challenge 2: Seccomp

You downloaded an executable named “malicious” from the internet. You suspect that it might be malicious, but you’re not sure. You have heard very bad things about ransomware and you want to make sure that the executable can’t encrypt your files. Therefore, you decide to develop an eBPF program that prevents the “malicious” executable from opening any file!

## Description
The executable “malicious” indeed tries to write to a file in the current directory and replace its content with the string “I’m a malicious program!”.

Your objective is to develop an eBPF program that prevents any process whose name is “malicious” from opening any file. You will use the eunomia-bpf framework.

This does not seem very hard at first, but preventing a process from opening a file is actually not something that can be done with eBPF by default. To achieve this, you will have to use the eBPF LSM (Linux Security Modules) mechanism.

This challenge requires you to research by yourself how to use the eBPF LSM mechanism to prevent a process from opening a file.

### Tip

Check out the “Important Resources” section of tutorial 3 for some useful links.

## Setup
Download the files for this challenge using:
```
$ wget --no-check-certificate https://people.montefiore.uliege.be/~gain/courses/info0940/asset/seccomp.tar.gz
$ tar -xzvf seccomp.tar.gz
```
The malicious program is located in `seccomp/malicious`.

The file that is going to be overwritten is the “very_important_file” file.

You can compile the malicious program using the Makefile provided (simply run `make` within the `malicious` directory). Then you can run it:
```
$ ./malicious
```
The file “very_important_file” will be overwritten with the string “I’m a malicious program!”.

Inside `seccomp/src`, you will find the same template as in tutorial 3. Use it to implement the eBPF program that prevents the “malicious” program from opening any file.

## What you need to do
You are expected to implement an eBPF program that detects when a process named “malicious” attempts to open a file. If detected, the eBPF program should block the operation, preventing the file from being opened by the process.

Be careful, you should not **kill** the process, but simply deny the call to open the file (i.e., when executing `FILE *file = fopen("malicious.txt", "w");`, the call to fopen should return `NULL`).

This is what it will look like when you run the “malicious” program with your eBPF program loaded:
```
$ ./malicious
Error opening file: Operation not permitted
```
The solution to this challenge should be quite small as well, but it will be a little bit more complex than the first one.

### Important

The VM on which your code will be tested will have LSM enabled.