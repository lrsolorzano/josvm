#include <inc/lib.h>
#include <inc/vmx.h>
#include <inc/elf.h>
#include <inc/ept.h>
#include <inc/stdio.h>

#define GUEST_KERN "/vmm/kernel"
#define GUEST_BOOT "/vmm/boot"

#define JOS_ENTRY 0x7000

// Map a region of file fd into the guest at guest physical address gpa.
// The file region to map should start at fileoffset and be length filesz.
// The region to map in the guest should be memsz.  The region can span multiple pages.
//
// Return 0 on success, <0 on failure.
//
static int
map_in_guest( envid_t guest, uintptr_t gpa, size_t memsz, 
	      int fd, size_t filesz, off_t fileoffset ) {
	/* Your code here */

	char * theData = char[filesz];

	seek(fd, fileoffset);
	read(fd, theData, filesz);

	void * hva = NULL;

	struct env * guestEnv;

	envid2env(guest,&guestEnv,1)
	
	ept_gpa2hva(guestEnv->env_pml4,gpa, &hva);

	//Need to insert/alloc the page if it doesn't exist.
	if (hva == NULL) {
		int res = ept_page_insert(guestEnv->env_pml4,page_alloc(ALLOC_ZERO), gpa, __EPTE_FULL);
		ept_gpa2hva(guestEnv->env_pml4,gpa, &hva);
	}
	
	char * currHva = (char *) hva;
	
	for (int i = 0; i < filesz; i++) {

		//Check to see if we've crossed onto another physical page.
		//Gross.
		if ((uint64_t) currHva % PGSIZE == 0) {
			void * newHva = NULL;

			gpa = gpa + i * 8;
			
			ept_gpa2hva(guestEnv->env_pml4, gpa, &newHva);

			//Need to insert/alloc the page for gpa if it doesn't exist.
			if (newHva == NULL) {
				int res = ept_page_insert(guestEnv->env_pml4,page_alloc(ALLOC_ZERO), gpa, __EPTE_FULL);
				ept_gpa2hva(guestEnv->env_pml4,gpa, &newHva);
			}
			
			currHva = (char *) newHva;

		}

		*currHva = *theData;
		currHva++;
		theData++;
				
	}

	return 0;

	//return -E_NO_SYS;

	

} 

// Read the ELF headers of kernel file specified by fname,
// mapping all valid segments into guest physical memory as appropriate.
//
// Return 0 on success, <0 on error
//
// Hint: compare with ELF parsing in env.c, and use map_in_guest for each segment.
static int
copy_guest_kern_gpa( envid_t guest, char* fname ) {

	struct File * theFile = NULL;
	
	int fd = open(fname, O_RDONLY);

	file_open(fname, &theFile);
	
	uint8_t * theData = uint8_t[theFile->f_size];

	read(fd, (char *) theData,theFile->f_size );


	//Reusing some code from env.c here --
	
	struct ELF * theElf = (struct Elf *) theData;

	if (theElf->e_magic!= ELF_MAGIC)
		cprintf("\n\n\n Can't load Elf !!! \n\n\n)");
	
	struct Proghdr * ph = (struct Proghdr *)((uint8_t *) theElf + theElf->e_phoff);
	struct Proghdr * eph = ph +theElf->e_phnum;
	
	for (; ph < eph; ph++) {
		if ( ph->p_type == ELF_PROG_LOAD) {

			// address to load into
			uint8_t * cursor = (uint8_t *) ph->p_va;

			// address to load from
			uint8_t * index = theData + ph->p_offset;
			
			// the actual size is ph->p_filesz
			/*for (i = 0; i < ph->p_memsz; i++) {
				if (i < ph->p_filesz)
					*(cursor+i) = *(index+i);	
				else
					*(cursor+i) = 0;
					}*/

			map_in_guest(guest, (uint64_t) cursor, ph->p_memsz, fd, ph->p_memsz, ph->p_offset); 

			
		}
	}
	
	
	/* Your code here */
	return -E_NO_SYS;
}

void
umain(int argc, char **argv) {
	int ret;
	envid_t guest;
	char filename_buffer[50];	//buffer to save the path 
	int vmdisk_number;
	int r;
	if ((ret = sys_env_mkguest( GUEST_MEM_SZ, JOS_ENTRY )) < 0) {
		cprintf("Error creating a guest OS env: %e\n", ret );
		exit();
	}
	guest = ret;

	// Copy the guest kernel code into guest phys mem.
	if((ret = copy_guest_kern_gpa(guest, GUEST_KERN)) < 0) {
		cprintf("Error copying page into the guest - %d\n.", ret);
		exit();
	}

	// Now copy the bootloader.
	int fd;
	if ((fd = open( GUEST_BOOT, O_RDONLY)) < 0 ) {
		cprintf("open %s for read: %e\n", GUEST_BOOT, fd );
		exit();
	}

	// sizeof(bootloader) < 512.
	if ((ret = map_in_guest(guest, JOS_ENTRY, 512, fd, 512, 0)) < 0) {
		cprintf("Error mapping bootloader into the guest - %d\n.", ret);
		exit();
	}
#ifndef VMM_GUEST	
	sys_vmx_incr_vmdisk_number();	//increase the vmdisk number
	//create a new guest disk image
	
	vmdisk_number = sys_vmx_get_vmdisk_number();
	snprintf(filename_buffer, 50, "/vmm/fs%d.img", vmdisk_number);
	
	cprintf("Creating a new virtual HDD at /vmm/fs%d.img\n", vmdisk_number);
        r = copy("vmm/clean-fs.img", filename_buffer);
        
        if (r < 0) {
        	cprintf("Create new virtual HDD failed: %e\n", r);
        	exit();
        }
        
        cprintf("Create VHD finished\n");
#endif
	// Mark the guest as runnable.
	sys_env_set_status(guest, ENV_RUNNABLE);
	wait(guest);
}


