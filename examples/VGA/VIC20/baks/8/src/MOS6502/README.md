## MOS6502 Emulator in C++ ##

This is my C++ implementation of the MOS Technology 6502 CPU. The code is written to be more readable than fast, however some minor tricks have been introduced to greatly reduce the overall execution time.

main features:

 * 100% coverage of legal opcodes
 * decimal mode implemented
 * read/write bus callback
 * jump table opcode selection

still to implement

 * 100% cycle accuracy
 * illegal opcodes
 * hardware glitches, the known ones of course :-)

The emulator was extensively tested against this test suite:

https://github.com/Klaus2m5/6502_65C02_functional_tests

and in parallel emulation with Fake6502 http://rubbermallet.org/fake6502.c

so expect nearly 100% compliance with the real deal... at least on the normal behavior: as I said stuff like illegal opcodes or hardware glitches are currently not implemented. 
 

## Why yet another 6502 emulator? ##

Just for fun :). This CPU (and its many derivatives) powered machines such as:

 * Apple II
 * Nintendo Entertainment system (NES)
 * Atari 2600
 * Commodore 64
 * BBC Micro

and many other embedded devices still used today. 
You can use this emulator in your machine emulator project. However cycle accuracy is not yet implemented so mid-frame register update tricks cannot be reliably emulated.  


## Some things emulators: emulator types ##

You can group all the CPU emulators out there in 4 main categories:

 * switch-case based
 * jump-table based
 * PLA or microcode emulation based
 * graph based

The latter are the most accurate as they emulate the connections between transistors inside the die of the CPU. They emulate even the unwanted glitches, known and still unknown. However, the complexity of such emulators is non-linear with the number of transistors: in other word, *you don't want to emulate a modern Intel quad core using this approach!!!*

for an example check this out: http://visual6502.org/JSSim/index.html 

The PLA/microcode based are the best as they offer both speed and limited complexity.
The switch-case based are the simpler ones but also the slowest: the opcode value is thrown inside a huge switch case which selects the code snippet to execute; compilers can optimize switch case to reach near O(log(n)) complexity but they hardly do it when dealing with sparse integers (like most of the CPU opcode tables). 


## Emulator features ##

My project is a simple jump-table based emulator: the actual value of the opcode (let's say 0x80) is used to address a function pointer table, each entry of such table is a C++ function which emulates the behavior of the corresponding real instruction. 

All the 13 addressing modes are emulated:

```
// addressing modes
uint16_t Addr_ACC(); // ACCUMULATOR
uint16_t Addr_IMM(); // IMMEDIATE
uint16_t Addr_ABS(); // ABSOLUTE
uint16_t Addr_ZER(); // ZERO PAGE
uint16_t Addr_ZEX(); // INDEXED-X ZERO PAGE
uint16_t Addr_ZEY(); // INDEXED-Y ZERO PAGE
uint16_t Addr_ABX(); // INDEXED-X ABSOLUTE
uint16_t Addr_ABY(); // INDEXED-Y ABSOLUTE
uint16_t Addr_IMP(); // IMPLIED
uint16_t Addr_REL(); // RELATIVE
uint16_t Addr_INX(); // INDEXED-X INDIRECT
uint16_t Addr_INY(); // INDEXED-Y INDIRECT
uint16_t Addr_ABI(); // ABSOLUTE INDIRECT
```

All the 151 opcodes are emulated. Since the 6502 CPU uses 8 bit to encode the opcode value it also has a lot of "illegal opcodes" (i.e. opcode values other than the designed 151). Such opcodes perform weird operations, write multiple registers at the same time, sometimes are the combination of two or more "valid" opcodes. Such illegals were used to enforce software copy protection or to discover the exact CPU type. 

The illegals are not supported yet, so instead a simple NOP is executed.


## Inner main loop ##

It's a classic fetch-decode-execute loop:

```
while(start + n > cycles && !illegalOpcode)
{
	// fetch
	opcode = Read(pc++);
	
	// decode
	instr = InstrTable[opcode];
		
	// execute
	Exec(instr);
}
```

The next instruction (the opcode value) is retrieved from memory. Then it's decoded (i.e. the opcode is used to address the instruction table) and the resulting code block is executed.   


## Public methods ##
 
The emulator comes as a single C++ class with five public methods:

```
mos6502(BusRead r, BusWrite w);
void NMI();
void IRQ();
void Reset();
void Run(uint32_t n);
```


```mos6502(BusRead r, BusWrite w);```

it's the class constructor. It requires you to pass two external functions:

```
uint8_t MemoryRead(uint16_t address);
void MemoryWrite(uint16_t address, uint8_t value);
```

respectively to read/write from/to a memory location (16 bit address, 8 bit value). In such functions you can define your address decoding logic (if any) to address memory mapped I/O, external virtual devices and such.

```
void NMI();
```

triggers a Non-Mascherable Interrupt request, as done by the external pin of the real chip

```
void IRQ();
```

triggers an Interrupt ReQuest, as done by the external pin of the real chip

```
void Reset();
```

performs an hardware reset, as done by the external pin of the real chip

```
void Run(uint32_t n);
```

It runs the CPU for the next 'n' machine instructions.

## Links ##

Some useful stuff I used...

http://en.wikipedia.org/wiki/MOS_Technology_6502

http://www.6502.org/documents/datasheets/mos/

http://www.mdawson.net/vic20chrome/cpu/mos_6500_mpu_preliminary_may_1976.pdf

http://rubbermallet.org/fake6502.c



