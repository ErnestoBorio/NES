#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "Nes.h"

// How many PPU cycles until starting VBlank. 262 scanlines * 341 ppu cycles (one per pixel)
#define VBlank_ppu_cycles 262 * 341
#define VBlank_end_ppu_cycles 20 * 341

#ifdef _Cpu6502_Disassembler
	static byte read_memory_disasm( void *parent_system, word address );
#endif
static void builtin_memory_handlers_init( Nes *this );

// -------------------------------------------------------------------------------
static void initialize( Nes *this )
{
	this->ppu.cycles = 0;
   this->ppu.nmi_enabled        = 1;   
   this->ppu.sprite_height      = 8;
   this->ppu.back_pattern       = 0x1000;
   this->ppu.sprite_pattern     = 0;
   this->ppu.increment_vram     = 1;
   this->ppu.scroll_high_bits   = 0;
   this->ppu.color_emphasis     = 0;
   this->ppu.sprites_visible    = 1;
   this->ppu.background_visible = 1;
   this->ppu.sprite_clip        = 0;
   this->ppu.background_clip    = 0;
   this->ppu.monochrome         = 0;
   this->ppu.vblank_flag        = 0;
   this->ppu.sprite0_hit        = 0;
   this->ppu.sprites_lost       = 0;    
	this->ppu.write_1st_2nd      = 0;
   this->ppu.horz_scroll        = 0;
   this->ppu.vert_scroll        = 0;
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
	Cpu6502_Initialize( this->cpu );
	
	#ifdef _Cpu6502_Disassembler
		this->cpu->read_memory_disasm = read_memory_disasm;
	#endif
	
	this->prg_rom_count = 0;
	this->prg_rom = NULL;
	this->chr_rom_count = 0;
	this->chr_rom = NULL;
	
	initialize( this );
	builtin_memory_handlers_init( this );
	
	return this;
}

// -------------------------------------------------------------------------------
void Nes_Reset( Nes *this )
{
   Cpu6502_Reset( this->cpu );
   this->ppu.vblank_flag  = 0;
   this->ppu.sprite0_hit  = 0;
   this->ppu.sprites_lost = 0;
   this->ppu.vram_write   = 0;
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
	while( this->ppu.cycles < VBlank_ppu_cycles )
	{
      // int old_cycles = this->ppu.cycles;
      this->ppu.cycles += 3 * Cpu6502_CpuStep( this->cpu );
      
		#ifdef _Cpu6502_Disassembler
			Cpu6502_Disassemble( this->cpu, 0 ); // old_cycles );
		#endif
      
      if( this->ppu.cycles > VBlank_end_ppu_cycles ) {
         this->ppu.vblank_flag = 0;
      }
	}
	
   this->ppu.vblank_flag = 1;
   
	if( this->ppu.nmi_enabled )
	{
		this->ppu.cycles += 3 * Cpu6502_NMI( this->cpu );
	}

	this->ppu.cycles -= VBlank_ppu_cycles;	
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
		this->chr_rom_count = 1; // WIP: CHR-ROM count of 0 means 1 as most docs say or does it mean it has only CHR-RAM?
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

// -------------------------------------------------------------------------------
#ifdef _Cpu6502_Disassembler
   static byte read_memory_disasm( void *sys, word address )
   {
      // For side effect reads, this should be avoided and direct access should be done instead.
      // return ((Nes*)sys)->cpu->read_memory[address]( sys, address );
      if( address < 0x2000 || address >= 0x8000 ) {
         return ((Nes*)sys)->cpu->read_memory[address]( sys, address );
      }
      else {
         return 0;
      }
   }
#endif

// -------------------------------------------------------------------------------
byte read_ram( void *sys, word address );
void write_ram( void *sys, word address, byte value );
byte read_ram_mirror( void *sys, word address );
void write_ram_mirror( void *sys, word address, byte value );
byte read_prg_rom( void *sys, word address );
void write_ppu_control1( void *sys, word address, byte value );
void write_ppu_control2( void *sys, word address, byte value );
byte read_ppu_status( void *sys, word address );
void write_spr_ram_address( void *sys, word address, byte value  );
byte read_spr_ram_io( void *sys, word address );
void write_spr_ram_io( void *sys, word address, byte value  );
void write_scroll( void *sys, word address, byte value  );
void write_vram_address( void *sys, word address, byte value  );
byte read_vram_io( void *sys, word address );
void write_vram_io( void *sys, word address, byte value  );
void write_sprite_dma( void *sys, word address, byte value );
byte read_unimplemented( void *sys, word address );
void write_unimplemented( void *sys, word address, byte value );

// -------------------------------------------------------------------------------
static void builtin_memory_handlers_init( Nes *this )
{
	int i;
// RAM
	for( i=0; i<=0x7FF; ++i ) {
		this->cpu->read_memory[i] = read_ram;
		this->cpu->write_memory[i] = write_ram;		
	}
	for( i=0x800; i<=0x1FFF; ++i ) {
		this->cpu->read_memory[i] = read_ram_mirror;
		this->cpu->write_memory[i] = write_ram_mirror;
	}
   // Default all registers as unimplemented and then overwrite each one as they are implemented
   for( i=0x2000; i<=0x7FFF; ++i ) {
      this->cpu->read_memory[i] = read_unimplemented;
      this->cpu->write_memory[i] = write_unimplemented;
   }
// PPU
	for( i=0x2000; i<=0x3FFF; i += 8 ) {
		this->cpu->write_memory[i] = write_ppu_control1;
		this->cpu->read_memory[i] = read_unimplemented;
	}
	for( i=0x2001; i<=0x3FFF; i += 8 ) {
		this->cpu->write_memory[i] = write_ppu_control2;
		this->cpu->read_memory[i] = read_unimplemented;
	}
	for( i=0x2002; i<=0x3FFF; i += 8 ) {
		this->cpu->read_memory[i] = read_ppu_status;
		this->cpu->write_memory[i] = write_unimplemented;
	}
   for( i=0x2003; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i] = read_unimplemented;
      this->cpu->write_memory[i] = write_spr_ram_address;
   }
   for( i=0x2004; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i] = read_spr_ram_io;
      this->cpu->write_memory[i] = write_spr_ram_io;
   }
   for( i=0x2005; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i] = read_unimplemented;
      this->cpu->write_memory[i] = write_scroll;
   }
   for( i=0x2006; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i] = read_unimplemented;
      this->cpu->write_memory[i] = write_vram_address;
   }
   for( i=0x2007; i<=0x3FFF; i += 8 ) {
      this->cpu->read_memory[i] = read_vram_io;
      this->cpu->write_memory[i] = write_vram_io;
   }
// APU
// Sprite DMA
   for( i=0x4014; i<=0x4014; i += 8 ) {
      this->cpu->read_memory[i] = read_unimplemented;
      this->cpu->write_memory[i] = write_sprite_dma;
   }
// PRG ROM
	for( i=0x8000; i<=0xFFFF; ++i ) {
		this->cpu->read_memory[i] = read_prg_rom;
		this->cpu->write_memory[i] = write_unimplemented;
	}
}