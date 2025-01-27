## Dead Code Elimination

Your first assignment is to design a pass that eliminates redundant instructions.
We define dead instructions as instructions that write to a register not used by subsequent instructions and have no
observable side effects (e.g., write to memory, return, perform a jump). this implementation focuses on removing trivially dead Instructions, irrelevant basic blocks and useless load/stores.

## Memory Safety

Implementation of a basic version of AddressSanitizer.
A pass that runs over the program is implemented, and inserts calls to a runtime library that checks if a memory
access is valid.

The idea is that the runtime library keeps track of allocated memory and checks if a memory access is within the bounds
of the allocated memory.

Heap out of Bounds, Heap use after free and Stack out of bounds are checked in this implementation.The program detects only over- and underflows up to 16 bytes.
