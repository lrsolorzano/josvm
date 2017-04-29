#include <inc/lib.h>
#include <inc/vmx.h>
#include <inc/elf.h>
#include <inc/ept.h>
#include <inc/stdio.h>

//#include <vmm/ept.h>

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
map_in_guest(envid_t guest, uintptr_t gpa, size_t memsz, 
	      int fd, size_t filesz, off_t fileoffset) {

	uint8_t *f_arr = PFTEMP;
	int i, r, datasz;
	
	for (i = 0, datasz = filesz; i < memsz; i += PGSIZE) {
		sys_page_alloc(0, f_arr, PTE_P|PTE_U|PTE_W);
		seek(fd, fileoffset + i);

		if (datasz >=  PGSIZE) {
			read(fd, f_arr, PGSIZE);
			datasz -= PGSIZE;	
		}
		else {
			if (datasz == 0)
				memset(f_arr, 0, PGSIZE);
			else {
				read(fd, f_arr, datasz);
				memset(f_arr + datasz, 0, PGSIZE - datasz);
				datasz = 0;
			}
		}

		r = sys_ept_map(sys_getenvid(), f_arr, 
			guest, (void *)gpa, __EPTE_FULL);
		sys_page_unmap(0, f_arr);
	}

	return 0;
} 

// Read the ELF headers of kernel file specified by fname,
// mapping all valid segments into guest physical memory as appropriate.
//
// Return 0 on success, <0 on error
//
// Hint: compare with ELF parsing in env.c, and use map_in_guest for each segment.
static int
copy_guest_kern_gpa(envid_t guest, char* fname) {
	//struct File * theFile = NULL;
	int fd = open(fname, O_RDONLY);
	struct Elf elfh;
	struct Proghdr ph;
	int i, offset;

	read(fd, &elfh, sizeof(struct Elf));

	if (elfh.e_magic != ELF_MAGIC)
		cprintf("\n\n\n Can't load Elf !!! \n\n\n)");
	
	for (i = 0, offset = elfh.e_phoff; i < elfh.e_phnum; i++) {
		seek(fd, offset);
		read(fd, &ph, sizeof(struct Proghdr));
		map_in_guest(guest, ph.p_pa, ph.p_memsz, fd, 
					ph.p_filesz, ph.p_offset); 
		offset += sizeof(struct Proghdr);
	}
	
	return 0;
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


