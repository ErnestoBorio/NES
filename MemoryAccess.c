
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
// $8000..$FFFF PRG-ROM
static byte read_prg_rom( void *sys, word address )
{
	address -= 0x8000; // make address zero-based ( $0..$7FFF )
	if( ((Nes*)sys)->prg_rom_count == 1 ) // WIP bankswitching hell here in the future
	{
		address &= 0x3FFF; // Convert mirror in actual ROM address ( $0..$3FFF )		
	}
	return ((Nes*)sys)->prg_rom[address];
}

// -------------------------------------------------------------------------------
// $2002 and mirrors up to $3FFF
static byte read_ppu_status( void *sys, word address )
{
	byte value = ((Nes*)sys)->ppu.vblank_flag <<7;
	((Nes*)sys)->ppu.vblank_flag = 0;
	printf( "Read from $%04X : $%02X\n", address, value );
	return  value;
}

static void write_ppu_control1( void *sys, word address, byte value )
{
	((Nes*)sys)->ppu.nmi_enabled = bit_value( value, 7 );
}

static void write_ppu_control2( void *sys, word address, byte value )
{
}

// -------------------------------------------------------------------------------
static byte read_unimplemented( void *sys, word address )
{
	printf( "Unimplemented read at $%4X.\n", address );
	//assert(0);
	return 0;
}

static void write_unimplemented( void *sys, word address, byte value )
{
	printf( "Unimplemented write of $%2X at $%4X.\n", value, address );
	//assert(0);
}























