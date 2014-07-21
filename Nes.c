
#include <stdlib.h>
#include <stdio.h>
#include "Nes.h"
#include "MemoryAccess.c"

// How many PPU cycles until starting VBlank. 262 scanlines * 341 ppu cycles (one per pixel)
#define VBlank_ppu_cycles 262 * 341

static byte read_memory_disasm( void *parent_system, word address ){ return 0; };

// -------------------------------------------------------------------------------
static void initialize( Nes *this )
{
	this->ppu.vblank_flag = 0;
	this->ppu.nmi_enabled = 1;
	this->ppu.cycles = VBlank_ppu_cycles;
}
// -------------------------------------------------------------------------------
Nes *Nes_Create()
{	
	Nes *this = (Nes*) malloc( sizeof( Nes ) );
	if( this == NULL ){
		return NULL;
	}
		
	this->cpu = Cpu6502_Create( this );
	if( this->cpu == NULL ) {
		return NULL;
	}
	
	#ifdef _Cpu6502_Disassembler
		this->cpu->stack = &this->ram[0x100];
		this->cpu->read_memory_disasm = read_memory_disasm;
	#endif
	
	this->prg_rom_count = 0;
	this->prg_rom = NULL;
	this->chr_rom_count = 0;
	this->chr_rom = NULL;
	
	initialize( this );
	
	return this;
}

// -------------------------------------------------------------------------------
void Nes_Free( Nes *this )
{
	if( this->prg_rom != NULL ) {
		free( this->prg_rom );
	}
	if( this->chr_rom != NULL ) {
		free( this->chr_rom );
	}
	free( this );
}

// -------------------------------------------------------------------------------
void Nes_DoFrame( Nes *this )
{
	int cpu_cycles;
	while( this->ppu.cycles > 0 )
	{
		cpu_cycles = Cpu6502_CpuStep( this->cpu );
		this->ppu.cycles -= 3 * cpu_cycles;
	}
	this->ppu.vblank_flag = 1;
	if( this->ppu.nmi_enabled )
	{
		cpu_cycles = Cpu6502_NMI( this->cpu );
		this->ppu.cycles -= 3 * cpu_cycles;
	}
	this->ppu.cycles += VBlank_ppu_cycles;	
}

// -------------------------------------------------------------------------------
int Nes_LoadRom( Nes *this, FILE *rom_file )
{
	if( this->prg_rom != NULL ) {
		free( this->prg_rom );
		this->prg_rom = NULL;
	}
	if( this->chr_rom != NULL ) {
		free( this->chr_rom );
		this->chr_rom = NULL;
	}
	
	rewind( rom_file );
	byte header[10];
	fread( header, 10, 1, rom_file );
			
	int trainer = ( header[6] & (1<<2) ) > 0;	
	int offset = 16 + ( trainer ? 512 : 0 ); // skip 16 bytes header + 512B trainer
	
	// Read PRG-ROM banks
	this->prg_rom_count = (int) header[4];
	this->prg_rom = (byte*) malloc( this->prg_rom_count * PRG_ROM_bank_size );
	if( this->prg_rom == NULL ) {
		goto Exception;
	}
	fseek( rom_file, offset, SEEK_SET );
	size_t read_count = fread( this->prg_rom, PRG_ROM_bank_size, this->prg_rom_count, rom_file );
	if( read_count != this->prg_rom_count ) {
		goto Exception;
	}
	
	// The CHR-ROM banks immediately follow the PRG-ROM banks, no fseek() needed
	this->chr_rom_count = (int) header[5];
	if( this->chr_rom_count == 0 ) {
		this->chr_rom_count = 1; // WIP: it's like this this or it has only CHR-RAM?
	}
	this->chr_rom = (byte*) malloc( this->chr_rom_count * CHR_ROM_bank_size );
	if( this->chr_rom == NULL ) {
		goto Exception;
	}
	read_count = fread( this->chr_rom, CHR_ROM_bank_size, this->chr_rom_count, rom_file );
	if( read_count != this->chr_rom_count ) {
		goto Exception;
	}
	
	// Extra check to trap any unseen error in reading the rom file
	byte dummy;
	size_t remaining = fread( &dummy, 1, 1, rom_file );	
	if( ! feof( rom_file ) || ( remaining != 0 ) ) {
		fprintf( stderr, "The rom file didn't end after CHR-ROM banks as expected.\n" );
	}
	
	return true;
	
Exception:
	
	if( this->prg_rom != NULL ) {
		free( this->prg_rom );
		this->prg_rom = NULL;
	}
	if( this->chr_rom != NULL ) {
		free( this->chr_rom );
		this->chr_rom = NULL;
	}	
	return false;
}



















