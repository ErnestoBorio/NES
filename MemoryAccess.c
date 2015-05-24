#include <assert.h>
#include "Nes.h"

#define NES ((Nes*)sys) // some syntax de-clutter to compensate for the unfortunate void *sys

// -------------------------------------------------------------------------------
// $0..$7FF unmirrored RAM
byte read_ram( void *sys, word address )
{
	return NES->ram[address];
}

void write_ram( void *sys, word address, byte value )
{
	NES->ram[address] = value;
}

// -------------------------------------------------------------------------------
// $800..$1FFF mirrored RAM
byte read_ram_mirror( void *sys, word address )
{
	address &= 0x7FF; // Convert mirrors to actual address
	return NES->ram[address];
}

void write_ram_mirror( void *sys, word address, byte value )
{
	address &= 0x7FF; // Convert mirrors to actual address
	NES->ram[address] = value;  
}

// -------------------------------------------------------------------------------
// $8000..$FFFF PRG-ROM
byte read_prg_rom( void *sys, word address )
{
	address -= 0x8000; // make address zero-based ( $0..$7FFF )
	if( NES->prg_rom_count == 1 ) // WIP bankswitching hell here in the future
	{
		address &= 0x3FFF; // Convert mirror in actual ROM address ( $0..$3FFF )		
	}
	return NES->prg_rom[address];
}

// -------------------------------------------------------------------------------
// $2000
void write_ppu_control1( void *sys, word address, byte value )
{
   NES->ppu.nmi_enabled    = ( value & (1<<7) ) ? 1 : 0;
   NES->ppu.sprite_height  = ( value & (1<<5) ) ? 16 : 8;
   NES->ppu.back_pattern   = ( value & (1<<4) ) ? 0x1000 : 0;
   NES->ppu.sprite_pattern = ( value & (1<<3) ) ? 0x1000 : 0;
   NES->ppu.increment_vram = ( value & (1<<2) ) ? 32 : 1;
   NES->ppu.scroll_high_bits = value & 3; // & %11
}
// $2001
void write_ppu_control2( void *sys, word address, byte value )
{
   NES->ppu.color_emphasis     = ( value & 0xE0 ) >>5; // & %11100000
   NES->ppu.sprites_visible    = ( value & (1<<4) ) ? 1 : 0;
   NES->ppu.background_visible = ( value & (1<<3) ) ? 1 : 0;
   NES->ppu.sprite_clip        = ( value & (1<<2) ) ? 0 : 1;
   NES->ppu.background_clip    = ( value & (1<<1) ) ? 0 : 1;
   NES->ppu.monochrome         = ( value & (1<<1) ) ? 1 : 0;
}
// $2002
byte read_ppu_status( void *sys, word address )
{
   // Unused bits should actually return the open bus
   byte value = 
      ( NES->ppu.vblank_flag  <<7 ) |
      ( NES->ppu.sprite0_hit  <<6 ) |
      ( NES->ppu.sprites_lost <<5 );
   
   NES->ppu.vblank_flag   = 0; // reset flag once read
   NES->ppu.write_1st_2nd = 0; // writes count is reset
   
   #ifdef _Cpu6502_Disassembler
      NES->cpu->disasm.value = value;
   #endif
   
   return value;
}
// $2003
void write_spr_ram_address( void *sys, word address, byte value  )
{
}
// $2004
byte read_spr_ram_io( void *sys, word address )
{
   return 0;
}
void write_spr_ram_io( void *sys, word address, byte value  )
{
}
// $2005
void write_scroll( void *sys, word address, byte value  )
{
   if( NES->ppu.write_1st_2nd == 0 ) {
      NES->ppu.horz_scroll = value;
      NES->ppu.write_1st_2nd = 1;
   }
   else {
      NES->ppu.vert_scroll = value;
      NES->ppu.write_1st_2nd = 0;
   }
}
// $2006
void write_vram_address( void *sys, word address, byte value  )
{
}
// $2007
byte read_vram_io( void *sys, word address )
{
   return 0;
}
void write_vram_io( void *sys, word address, byte value  )
{
}
// $4014
void write_sprite_dma( void *sys, word address, byte value )
{
}

// -------------------------------------------------------------------------------
byte read_unimplemented( void *sys, word address )
{
	// printf( "Unimplemented read at $%4X.\n", address );
	//assert(0);
	return 0;
}

void write_unimplemented( void *sys, word address, byte value )
{
	// printf( "Unimplemented write of $%2X at $%4X.\n", value, address );
	//assert(0);
}