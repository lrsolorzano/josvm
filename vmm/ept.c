
#include <vmm/ept.h>
#include <inc/x86.h>
#include <inc/error.h>
#include <inc/memlayout.h>
#include <kern/pmap.h>
#include <inc/string.h>

// Return the physical address of an ept entry
static inline uintptr_t epte_addr(epte_t epte)
{
	return (epte & EPTE_ADDR);
}

// Return the host kernel virtual address of an ept entry
static inline uintptr_t epte_page_vaddr(epte_t epte)
{
	return (uintptr_t) KADDR(epte_addr(epte));
}

// Return the flags from an ept entry
static inline epte_t epte_flags(epte_t epte)
{
	return (epte & EPTE_FLAGS);
}

// Return true if an ept entry's mapping is present
static inline int epte_present(epte_t epte)
{
	return (epte & __EPTE_FULL) > 0;
}

// Find the final ept entry for a given guest physical address,
// creating any missing intermediate extended page tables if create is non-zero.
//
// If epte_out is non-NULL, store the found epte_t* at this address.
//
// Return 0 on success.  
// 
// Error values:
//    -E_INVAL if eptrt is NULL
//    -E_NO_ENT if create == 0 and the intermediate page table entries are missing.
//    -E_NO_MEM if allocation of intermediate page table entries fails
//
// Hint: Set the permissions of intermediate ept entries to __EPTE_FULL.
//       The hardware ANDs the permissions at each level, so removing a permission
//       bit at the last level entry is sufficient (and the bookkeeping is much simpler).
static int ept_lookup_gpa(epte_t* eptrt, void *gpa, 
			  int create, epte_t **epte_out) {
/* Your code here */
	
	if (eptrt == NULL)
		return -E_INVAL;


        //////////////
	//Level 1/////
	//////////////

//Start by finding the PML4 Entry using the ept pointer and gpa

// Per intel documentation, bits 63-52 are 0
	// bits 51 -12 of the pml4e are from 51-12 of the eptrt
	// bits 11-3 come from 47-39 of the gpa
	// bits 0, 1, and 2 are read, write, and execute permissions respectively.

	epte_t * ept_pml4e = 0;
	//Set bits 51-12.  Bits 63-52 should be zero'd in eptrt but we can double check that later if necessary.
	ept_pml4e = (epte_t *) (epte_addr( (epte_t)eptrt));
	//Set bits 11-3 with 47-39 of the gpa
	ept_pml4e = (epte_t *)  ((uint64_t) ept_pml4e |( (ADDR_TO_IDX(gpa,3)) >>3 ));
	
	//Check  address to see if pointer to next level exists 

	if (!epte_present(*ept_pml4e)) {

		//If create == 0, return -E_NO_ENT.
		//Otherwise, go ahead and create the next level of the ept page table.
		if (create == 0) {
			return -E_NO_ENT;
		}
		else {
			//Allocate a new page zero'd out.
			struct PageInfo * newPage = page_alloc(ALLOC_ZERO);
			//Insert the page with a mapping for the kernel.  Should just need present and write permissions.
			int res = page_insert(boot_pml4e, newPage,KADDR(page2pa(newPage)) , (PTE_P & PTE_W));
			if (res != 0)
				return -E_NO_MEM;
			//Set the pointer to the next level (PDPTEs) to be the physical address alloc'd, and set __EPTE_FULL perms.
			*ept_pml4e = page2pa(newPage) & __EPTE_FULL;
		}
		
		
	}

	//By this point, *ept_pml4e should be populated with a valid pointer to the PDPT entries.

	///////////////////////
	//Level 2//////////////
	///////////////////////
	
	//Find the correct entry in the PDPT table

	//Set bits 51-12 from 51-12 of the pml4 entry
	epte_t * ept_pdpte = (epte_t *) epte_addr((epte_t) *ept_pml4e);
	//set bits 11-3 from 38-30 of the gpa
	ept_pdpte = (epte_t *)  ((uint64_t) ept_pdpte | (ADDR_TO_IDX(gpa,2) >>3));

	//Check address to see if pointer to next level exists.

	if (!epte_present(*ept_pdpte)) {

		//If create == 0, return -E_NO_ENT.
		//Otherwise, go ahead and create the next level of the ept page table.
		if ( create ==0 ) {
			return -E_NO_ENT;
		}
		else {

			//Allocate a new page zero'd out.
			struct PageInfo * newPage = page_alloc(ALLOC_ZERO);
			//Insert the page with a mapping for the kernel.  Should just need present and write permissions.
			int res = page_insert(boot_pml4e, newPage,KADDR(page2pa(newPage)) , (PTE_P & PTE_W));
			if (res != 0)
				return -E_NO_MEM;
			*ept_pdpte = (page2pa(newPage)) & __EPTE_FULL;
			
		}
			
	}

	//By this point, *ept_pdpte should be populated with a valid physical addr ptr to the PDE table

        //////////////
	//Level 3/////
	//////////////
	
	//Find the correct entry in the PDE table

	//Set bits 51-12 from 51-12 of the pdpte
	epte_t * ept_pde = (epte_t *) epte_addr( (epte_t)*ept_pdpte);
	//set bits 11-3 from 29-21 of the gpa
	ept_pde = (epte_t *)  ((uint64_t)ept_pde | (ADDR_TO_IDX(gpa,1) >>3));

	if (!epte_present(*ept_pde)) {

		//If create == 0, return -E_NO_ENT
		//Otherwise, go ahead and create the next level of the ept page table.
		if ( create ==0 ) {
			return -E_NO_ENT;
		}
		else {

			//Allocate a new page zero'd out.
			struct PageInfo * newPage = page_alloc(ALLOC_ZERO);
			//Insert the page with a mapping for the kernel.  Should just need present and write permissions.
			int res = page_insert(boot_pml4e, newPage,KADDR(page2pa(newPage)) , (PTE_P & PTE_W));
			if (res != 0)
				return -E_NO_MEM;
			*ept_pde = (page2pa(newPage)) & __EPTE_FULL;
		}
		
		
	}

	//By this point, *ept_pde should now be populated with a valid physical addr ptr to a page table.

	/////////////////////
	//Level 4////////////
	/////////////////////

	
	//Find the correct entry in the Page Table

	//Set bits 51-12 from 51-12 of the pde
	epte_t * ept_pt = (epte_t *) epte_addr((epte_t)*ept_pde);
	//Set bits 11-3 from 20-12 of the gpa
	ept_pt = (epte_t *)( (uint64_t)ept_pt | (ADDR_TO_IDX(gpa,0) >>3));

	//Should not need to create the page if ept_pt isn't present.
	/*
	if (!epte_present(*ept_pt)) {

		//If create == 0, return -E_NO_ENT
		//Otherwise, go ahead and create the next level of the ept page table.
		if ( create ==0 ) {
			return -E_NO_ENT;
		}
		else {

			//Allocate a new page zero'd out.
			struct PageInfo * newPage = page_alloc(ALLOC_ZERO);
			//Insert the page with a mapping for the kernel.  Should just need present and write permissions.
			int res = page_insert(boot_pml4e, newPage,KADDR(page2pa(newPage)) , (PTE_P & PTE_W));
			if (res != 0)
				return -E_NO_MEM;
			*ept_pt = (page2pa(newPage)) & __EPTE_FULL;
		}
		
		}*/

	*epte_out = ept_pt;

	
	

//panic("ept_lookup_gpa not implemented\n");
	return 0;
	
}

void ept_gpa2hva(epte_t* eptrt, void *gpa, void **hva) {
    epte_t* pte;
    int ret = ept_lookup_gpa(eptrt, gpa, 0, &pte);
    if(ret < 0) {
        *hva = NULL;
    } else {
        if(!epte_present(*pte)) {
           *hva = NULL;
        } else {
           *hva = KADDR(epte_addr(*pte));
        }
    }
}

static void free_ept_level(epte_t* eptrt, int level) {
    epte_t* dir = eptrt;
    int i;

    for(i=0; i<NPTENTRIES; ++i) {
        if(level != 0) {
            if(epte_present(dir[i])) {
                physaddr_t pa = epte_addr(dir[i]);
                free_ept_level((epte_t*) KADDR(pa), level-1);
                // free the table.
                page_decref(pa2page(pa));
            }
        } else {
            // Last level, free the guest physical page.
            if(epte_present(dir[i])) {
                physaddr_t pa = epte_addr(dir[i]);                
                page_decref(pa2page(pa));
            }
        }
    }
    return;
}

// Free the EPT table entries and the EPT tables.
// NOTE: Does not deallocate EPT PML4 page.
void free_guest_mem(epte_t* eptrt) {
    free_ept_level(eptrt, EPT_LEVELS - 1);
    tlbflush();
}

// Add Page pp to a guest's EPT at guest physical address gpa
//  with permission perm.  eptrt is the EPT root.
// 
// Return 0 on success, <0 on failure.
//
int ept_page_insert(epte_t* eptrt, struct PageInfo* pp, void* gpa, int perm) {

	/* Your code here */
	//panic("ept_page_insert not implemented\n");
	
	epte_t* pt = NULL;
	epte_t** pt_ptr = &pt;
	
	int res = ept_lookup_gpa (eptrt,gpa,1,pt_ptr);

	pt = (epte_t *) page2pa(pp);

	pt =(epte_t *) ((uint64_t)pt | perm);

	return 0;
}

	

// Map host virtual address hva to guest physical address gpa,
// with permissions perm.  eptrt is a pointer to the extended
// page table root.
//
// Return 0 on success.
// 
// If the mapping already exists and overwrite is set to 0,
//  return -E_INVAL.
// 
// Hint: use ept_lookup_gpa to create the intermediate 
//       ept levels, and return the final epte_t pointer.
//       You should set the type to EPTE_TYPE_WB and set __EPTE_IPAT flag.
int ept_map_hva2gpa(epte_t* eptrt, void* hva, void* gpa, int perm, 
        int overwrite) {

	epte_t * pt = NULL;
	epte_t** pt_ptr = &pt;
	
	int res = ept_lookup_gpa(eptrt, gpa,overwrite,pt_ptr );

	if (res ==0)
		pt = (epte_t *)  PADDR(hva);
	pt = (uint64_t *) ((uint64_t) pt | perm | __EPTE_IPAT | __EPTE_TYPE(EPTE_TYPE_WB));

	
    /* Your code here */
    //panic("ept_map_hva2gpa not implemented\n");

	return res;
}

int ept_alloc_static(epte_t *eptrt, struct VmxGuestInfo *ginfo) {
    physaddr_t i;
    
    for(i=0x0; i < 0xA0000; i+=PGSIZE) {
        struct PageInfo *p = page_alloc(0);
        p->pp_ref += 1;
        int r = ept_map_hva2gpa(eptrt, page2kva(p), (void *)i, __EPTE_FULL, 0);
    }

    for(i=0x100000; i < ginfo->phys_sz; i+=PGSIZE) {
        struct PageInfo *p = page_alloc(0);
        p->pp_ref += 1;
        int r = ept_map_hva2gpa(eptrt, page2kva(p), (void *)i, __EPTE_FULL, 0);
    }
    return 0;
}

#ifdef TEST_EPT_MAP
#include <kern/env.h>
#include <kern/syscall.h>
int _export_sys_ept_map(envid_t srcenvid, void *srcva,
	    envid_t guest, void* guest_pa, int perm);

int test_ept_map(void)
{
	struct Env *srcenv, *dstenv;
	struct PageInfo *pp;
	epte_t *epte;
	int r;
	int pp_ref;
	int i;
	epte_t* dir;
	/* Initialize source env */
	if ((r = env_alloc(&srcenv, 0)) < 0)
		panic("Failed to allocate env (%d)\n", r);
	if (!(pp = page_alloc(ALLOC_ZERO)))
		panic("Failed to allocate page (%d)\n", r);
	if ((r = page_insert(srcenv->env_pml4e, pp, UTEMP, 0)) < 0)
		panic("Failed to insert page (%d)\n", r);
	curenv = srcenv;

	/* Check if sys_ept_map correctly verify the target env */
	if ((r = env_alloc(&dstenv, srcenv->env_id)) < 0)
		panic("Failed to allocate env (%d)\n", r);
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP, __EPTE_READ)) < 0)
		cprintf("EPT map to non-guest env failed as expected (%d).\n", r);
	else
		panic("sys_ept_map success on non-guest env.\n");

	/*env_destroy(dstenv);*/

	if ((r = env_guest_alloc(&dstenv, srcenv->env_id)) < 0)
		panic("Failed to allocate guest env (%d)\n", r);
	dstenv->env_vmxinfo.phys_sz = (uint64_t)UTEMP + PGSIZE;
	
	/* Check if sys_ept_map can verify srcva correctly */
	if ((r = _export_sys_ept_map(srcenv->env_id, (void *)UTOP, dstenv->env_id, UTEMP, __EPTE_READ)) < 0)
		cprintf("EPT map from above UTOP area failed as expected (%d).\n", r);
	else
		panic("sys_ept_map from above UTOP area success\n");
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP+1, dstenv->env_id, UTEMP, __EPTE_READ)) < 0)
		cprintf("EPT map from unaligned srcva failed as expected (%d).\n", r);
	else
		panic("sys_ept_map from unaligned srcva success\n");

	/* Check if sys_ept_map can verify guest_pa correctly */
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP + PGSIZE, __EPTE_READ)) < 0)
		cprintf("EPT map to out-of-boundary area failed as expected (%d).\n", r);
	else
		panic("sys_ept_map success on out-of-boundary area\n");
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP-1, __EPTE_READ)) < 0)
		cprintf("EPT map to unaligned guest_pa failed as expected (%d).\n", r);
	else
		panic("sys_ept_map success on unaligned guest_pa\n");

	/* Check if the sys_ept_map can verify the permission correctly */
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP, 0)) < 0)
		cprintf("EPT map with empty perm parameter failed as expected (%d).\n", r);
	else
		panic("sys_ept_map success on empty perm\n");
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP, __EPTE_WRITE)) < 0)
		cprintf("EPT map with write perm parameter failed as expected (%d).\n", r);
	else
		panic("sys_ept_map success on write perm\n");
	
	pp_ref = pp->pp_ref;	
	/* Check if the sys_ept_map can succeed on correct setup */
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP, __EPTE_READ)) < 0)
		panic("Failed to do sys_ept_map (%d)\n", r);
	else
		cprintf("sys_ept_map finished normally.\n");
		
	if (pp->pp_ref != pp_ref + 1) 
		panic("Failed on checking pp_ref\n");
	else
		cprintf("pp_ref incremented correctly\n");		
	
	/* Check if the sys_ept_map can handle remapping correctly */
	pp_ref = pp->pp_ref;
	if ((r = _export_sys_ept_map(srcenv->env_id, UTEMP, dstenv->env_id, UTEMP, __EPTE_READ)) < 0)
		cprintf("sys_ept_map finished normally.\n");
	else
		panic("sys_ept_map success on remapping the same page\n");
	/* Check if the sys_ept_map reset the pp_ref after failed on remapping the same page */
	if (pp->pp_ref == pp_ref)
		cprintf("sys_ept_map handled pp_ref correctly.\n");
	else
		panic("sys_ept_map failed to handle pp_ref.\n");
	
	/* Check if ept_lookup_gpa can handle empty eptrt correctly */
	if ((r = ept_lookup_gpa(NULL, UTEMP, 0, &epte)) < 0)
		cprintf("EPT lookup with a null eptrt failed as expected\n");
	else
		panic ("ept_lookup_gpa success on null eptrt\n");
	
		
	/* Check if the mapping is valid */
	if ((r = ept_lookup_gpa(dstenv->env_pml4e, UTEMP, 0, &epte)) < 0)
		panic("Failed on ept_lookup_gpa (%d)\n", r);
	if (page2pa(pp) != (epte_addr(*epte)))
		panic("EPT mapping address mismatching (%x vs %x).\n",
				page2pa(pp), epte_addr(*epte));
	else
		cprintf("EPT mapping address looks good: %x vs %x.\n",
				page2pa(pp), epte_addr(*epte));
	
	/* Check if the map_hva2gpa handle the overwrite correctly */
	if ((r = ept_map_hva2gpa(dstenv->env_pml4e, page2kva(pp), UTEMP, __EPTE_READ, 0)) < 0)
		cprintf("map_hva2gpa handle not overwriting correctly\n");
	else
		panic("map_hva2gpa success on overwriting with non-overwrite parameter\n");
		
	/* Check if the map_hva2gpa can map a page */
	if ((r = ept_map_hva2gpa(dstenv->env_pml4e, page2kva(pp), UTEMP, __EPTE_READ, 1)) < 0)
		panic ("Failed on mapping a page from kva to gpa\n");
	else
		cprintf("map_hva2gpa success on mapping a page\n");
		
	/* Check if the map_hva2gpa set permission correctly */
	if ((r = ept_lookup_gpa(dstenv->env_pml4e, UTEMP, 0, &epte)) < 0)
		panic("Failed on ept_lookup_gpa (%d)\n", r);
	if (((uint64_t)*epte & (~EPTE_ADDR)) == (__EPTE_READ | __EPTE_TYPE( EPTE_TYPE_WB ) | __EPTE_IPAT))
		cprintf("map_hva2gpa success on perm check\n");
	else
		panic("map_hva2gpa didn't set permission correctly\n");	
	/* Go through the extended page table to check if the immediate mappings are correct */
	dir = dstenv->env_pml4e;
	for ( i = EPT_LEVELS - 1; i > 0; --i ) {
        	int idx = ADDR_TO_IDX(UTEMP, i);
        	if (!epte_present(dir[idx])) {
        		panic("Failed to find page table item at the immediate level %d.", i);
        	}	
		if (!(dir[idx] & __EPTE_FULL)) {
			panic("Permission check failed at immediate level %d.", i);
		}        	
		dir = (epte_t *) epte_page_vaddr(dir[idx]);
        }
	cprintf("EPT immediate mapping check passed\n");
		
	
	/* stop running after test, as this is just a test run. */
	panic("Cheers! sys_ept_map seems to work correctly.\n");

	return 0;
}
#endif

