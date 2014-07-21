
#include "Nes.h"

// -------------------------------------------------------------------------------
// $0..$7FF unmirrored RAM
static byte read_ram( void *sys, word address )
{
	return ((Nes*)sys)->ram[address];
}

static void write_ram( void *sys, word address, byte value )
{
	((Nes*)sys)->ram[address] = value;
}

// -------------------------------------------------------------------------------
// $800..$1FFF mirrored RAM
static byte read_ram_mirror( void *sys, word address )
{
	address &= 0x7FF; // Convert mirrors to actual address
	return ((Nes*)sys)->ram[address];
}

static void write_ram_mirror( void *sys, word address, byte value )
{
	address &= 0x7FF; // Convert mirrors to actual address
	((Nes*)sys)->ram[address] = value;
}

// -------------------------------------------------------------------------------
// $8000..$FFF9 PRG-ROM
static byte read_prg_rom( void *sys, word address )
{
	assert( address >= 0x8000 && address <= 0xFFF9 );
	address -= 0x8000; // make address zero-based ( $0..$7FFF )
	if( ((Nes*)sys)->prg_rom_count == 1 ) // WIP bankswitching hell here in the future
	{
		address &= 0x3FFF; // Convert mirror in actual ROM address ( $0..$3FFF )		
	}
	return ((Nes*)sys)->prg_rom[address];
}

static void write_prg_rom( void *sys, word address, byte value )
{
	// Should trap this
}

// -------------------------------------------------------------------------------
// $2002 and mirrors up to $3FFF
static byte read_ppu_status( void *sys, word address )
{
	return ((Nes*)sys)->ppu.vblank_flag <<7; // return vblank flag in bit 7
}

























