
#include <stdlib.h>
#include <stdio.h>
#include "Nes.h"

byte read_memory( void *parent_system, word address ) { return 0; }
void write_memory( void *parent_system, word address, byte value ){}

// -------------------------------------------------------------------------------
Nes *Nes_Create()
{	
	Nes *this = (Nes*) malloc( sizeof( Nes ) );
	if( this == NULL ){
		return NULL;
	}
		
	this->cpu = Cpu6502_Create( this, &this->ram[0x100],
							read_memory, write_memory, read_memory );
	if( this->cpu == NULL ) {
		return NULL;
	}
	
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
void Nes_Initialize( Nes *this )
{
	this->prg_rom_count = 0;
	this->prg_rom = NULL;
	this->chr_rom_count = 0;	
	this->chr_rom = NULL;
}

// -------------------------------------------------------------------------------
int Nes_LoadRom( Nes *this, FILE *rom_file )
{
	this->prg_rom = NULL;
	this->chr_rom = NULL;
	
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



















