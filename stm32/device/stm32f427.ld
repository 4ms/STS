/*
Linker script for STM32F4xx_1024K_192K
Modified and commented by: Dan Green <danngreen1@gmail.com>
*/

ENTRY(Reset_Handler)

/*
If we were to have the stack live in normal RAM,
we would define _estack like this:
_estack = 0x20020000;

To have the stack live in CCMRAM, we do this:
_estack = 0x10010000;

This allows DMA transfers to/from regular RAM at the same time
the main code uses the stack for local variables
This creates a memory map like this:

CCMRAM:
0x10010000  [---Initial Stack Pointer--]
..........  [   ~~~  stack space ~~~   ]
..........  [ |                        ]
..........  [ |   stack ptr moves      ]
..........  [ v   downwards            ]
..........  [--------------------------]
..........  [   ~~~ heap space  ~~~    ]
..........  [   ~~~  (min 256B) ~~~    ]
..........  [--------------------------]
..........  [   ~~~ space for      ~~~ ]
..........  [   ~~~ CCMRAM defined ~~~ ]
..........  [   ~~~ globals        ~~~ ]
0x10000000  [--------------------------]

Note: STM32F4xx chips cannot do DMA transfers to/from CCMRAM,
so all DMA buffers must be allocated in normal RAM.
If a buffer is not declared global or static, it will be allocated in the stack,
and if the stack is in CCMRAM, the DMA transfer will hang and never complete
*/


_estack = 0x20020000;

/*
The heap is used for dynamically allocated variables, such as with f_printf() 
By declaring a min heap and stack size, we are making sure we have enough memory after allocating space for variables.
The linker will throw an error when we compile if end-of-allocated-variables + Min_Heap_Size + Min_Stack_Size > memory size
*/

_Min_Heap_Size = 0x100;

/*
The stack is used for local variables, when a function is called
We also can assign some globals to CCMRAM by defining them with __attribute__ ((section (".ccmdata")))
*/

_Min_Stack_Size = 0x1000;



MEMORY
{
  RAM (xrw)        : ORIGIN = 0x20000000, LENGTH = 128K     /* 0x20000000 - 0x2001FFFF */
  CCMRAM (xrw)     : ORIGIN = 0x10000000, LENGTH = 64K      /* 0x10000000 - 0x1000FFFF */
  FLASH (rx)       : ORIGIN = 0x08008000, LENGTH = 1M-32K   /* 0x08008000 - 0x080FFFFF */
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(16);
    KEEP(*(.isr_vector))            /* Startup code */
    . = ALIGN(16);
  } >FLASH

  .flashtext :
  {
    . = ALIGN(16);
    *(.flashtext)            /* Startup code */
    . = ALIGN(16);
  } >FLASH

  .text :
  {
    . = ALIGN(16);
    *(.text)                   /* remaining code */
    *(.text.*)                 /* remaining code */
    *(.rodata)                 /* read-only data (constants) */
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(16);
     _etext = .;
     _sidata = _etext;
  } >FLASH
  
  .data : AT ( _sidata )
  {
    . = ALIGN(16);
    _sdata = . ;
    *(.ramtext*)
    . = ALIGN(16);
    *(.data*)
    *(.data.*)
    . = ALIGN(16);
     _edata = . ;
  } >RAM

  .bss :
  {
    . = ALIGN(16);
    _sbss = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(16);
   _ebss = . ;
  } >RAM
  
  PROVIDE ( end = _ebss );
  PROVIDE ( _end = _ebss );
 
   ._user_heap_stack :
  {
    . = ALIGN(16);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(16);
  } >RAM
  
  /* CCM section, vars must be located here explicitly */
  /* Example: int foo __attribute__ ((section (".ccmdata"))); */
  .ccmdata (NOLOAD) :
  {
    . = ALIGN(16);
    *(.ccmdata)
    . = ALIGN(16);
  } >CCMRAM


 /*
  DISCARD :
  {
    fpu/libc.a ( * )
    fpu/libm.a ( * )
    fpu/libgcc.a ( * )
  }
  */
}
