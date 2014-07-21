
#ifndef _Nes_h_
	#define _Nes_h_

#include <stdio.h>
#include "Cpu6502.h"

#ifndef false
	#define false 0
	#define true ! false
#endif

#define PRG_ROM_bank_size 0x4000 // PRG-ROM bank is 16kB
#define CHR_ROM_bank_size 0x2000 // CHR-ROM bank is  8kB
#define RAM_size 0x800 // Built-in actual unmirrored RAM is 2kB

typedef struct // Nes
{
	Cpu6502 *cpu;
	
	byte *chr_rom; // Chunk with all CHR-ROM banks
	int chr_rom_count; // How many 8kB CHR-ROM banks are present
	
	byte *prg_rom; // Chunk with all PRG-ROM banks
	int prg_rom_count; // How many 16kB PRG-ROM banks are present
	
	byte ram[RAM_size]; // Built-in 2kB of RAM
	
	struct
	{
		int cycles; // PPU cycles countdown to starting Vblank
		byte vblank_flag;
		byte nmi_enabled;
	} ppu;
	
} Nes;

Nes *Nes_Create();
void Nes_Free( Nes *this );
int Nes_LoadRom( Nes *this, FILE *rom_file );
void Nes_DoFrame( Nes *this );

#endif // #ifndef _Nes_h_