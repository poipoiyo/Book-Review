# Chapter 5: Debugging

## Kernel Debugging
- user mode: attaching to a process, setting breakpoints
- kernel mode: if a breakpoint is set and then hit, the entire machine is frozen. Two machines are included.

## Local Kernel Debugging
- Allows viewing system memory and other system information on local machine. 
- LKD is no way to set up breakpoints, which means you’re always looking at the current state of system. 
- Also means that things change, even while commands are being executed, so some information may not be reliable.
- With full kernel debugging, commands can only be entered while
the target system is in a breakpoint, so system state is unchanged.
- Enter command prompt and restart system: `$bcdedit /debug on`
