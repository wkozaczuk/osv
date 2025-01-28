#include "efi.h"
#include "clib.h"
#include "log.h"
#include "io.h"
#include "compiler.h"

struct efi_guid uefi_acpi20_table = {
        0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}
};

typedef void *efi_handle_t;

static efi_status_t get_loader_image(
	efi_handle_t loader,
	struct efi_system_table *system,
	struct efi_loaded_image_protocol **image)
{
	struct efi_guid guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

	/*err(
		system,
		"Przed open_protocol\n");*/
	return system->boot->open_protocol(
		loader,
		&guid,
		(void **)image,
		loader,
		NULL,
		EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
}

static efi_status_t get_rootfs(
	efi_handle_t loader,
	struct efi_system_table *system,
	efi_handle_t device,
	struct efi_simple_file_system_protocol **rootfs)
{
	struct efi_guid guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

	return system->boot->open_protocol(
		device,
		&guid,
		(void **)rootfs,
		loader,
		NULL,
		EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
}

static efi_status_t get_rootdir(
	struct efi_simple_file_system_protocol *rootfs,
	struct efi_file_protocol **rootdir)
{
	return rootfs->open_volume(rootfs, rootdir);
}

static efi_status_t setup_loader(
	efi_handle_t handle,
	struct efi_system_table *system,
	struct efi_file_protocol **rootdir)
{
	efi_status_t status = EFI_SUCCESS;

	struct efi_loaded_image_protocol *image;
	status = get_loader_image(handle, system, &image);
	if (status != EFI_SUCCESS) {
		err(
			system,
			"failed to get loader image protocol\r\n");
		return status;
	}
	/*err(
		system,
		"Po get_loader_image\n");*/

	struct efi_simple_file_system_protocol *rootfs;
	status = get_rootfs(
		handle, system, image->device, &rootfs);
	if (status != EFI_SUCCESS) {
		err(
			system,
			"failed to get root volume\r\n");
		return status;
	}

	status = get_rootdir(rootfs, rootdir);
	if (status != EFI_SUCCESS) {
		err(
			system,
			"failed to get root filesystem directory\r\n");
		return status;
	}

	return EFI_SUCCESS;
}

static efi_status_t load_kernel(struct efi_system_table *system,
		struct efi_file_protocol *rootdir, 
		uint16_t *kernel_path, 
		uint64_t offset,
        	size_t image_size,
		size_t file_size,
		uint64_t *kernel_phys_start)
{
	efi_status_t status = EFI_SUCCESS;
	uint64_t page_size = 4096;
	uint64_t image_addr;

	struct efi_file_protocol *kernel_image;
	status = rootdir->open(
		rootdir,
		&kernel_image,
		kernel_path,
		EFI_FILE_MODE_READ,
		EFI_FILE_READ_ONLY);
	//err( system, "After rootdir->open()\r\n");
	if (status != EFI_SUCCESS) {
		err(
			system,
			"failed to open kernel file\r\n");
		return status;
	}

        uint64_t pages_to_alloc = (image_size + 0x200000 + 0x90000) / page_size; //Add 2MB
	status = system->boot->allocate_pages(
		EFI_ALLOCATE_ANY_PAGES,
		EFI_LOADER_DATA,
		pages_to_alloc,
		&image_addr);
	if (status != EFI_SUCCESS) {
		err(
			system,
			"failed to allocate buffer for the kernel\r\n");
		return status;
	}
	err( system, "Allocated %u pages at address:%p\r\n", pages_to_alloc, image_addr);
	//memset((void *)image_addr, 0, image_size); //Some BSS zero-ing?
	//
        //Load kernel file into memory
	image_addr = ((image_addr + 0x200000 -1) & (-0x200000)) + 0x90000; //Align it to conform to OSv expecting it like with regular boot
	err( system, "Loading kernel at: %p\r\n", image_addr);
	status = efi_read_fixed(
		system,
		kernel_image,
		offset,//phdr->p_offset,
		file_size,//phdr->p_filesz,
		(void *)image_addr);
	//err( system, "After efi_read_fixed()\r\n");
	if (status != EFI_SUCCESS) {
		err(
			system,
			"failed to read the kernel segment in memory\r\n");
		return status;
	}
	*kernel_phys_start = image_addr;
/*
	loader->kernel_image_entry =
		image_addr + loader->kernel_header.e_entry - image_begin;
*/
	return EFI_SUCCESS;
}

static efi_status_t exit_efi_boot_services(struct efi_boot_table *boot, efi_handle_t handle, struct efi_simple_text_output_protocol* out)
{
	struct efi_memory_descriptor *mmap = 0;
	efi_uint_t mmap_size = 4096;
	efi_uint_t mmap_key = 0;
	efi_uint_t desc_size = 0;
	uint32_t desc_version = 0;
	efi_status_t status;

	efi_uint_t loop = 0;
	while (1) {
		loop++;
		status = boot->allocate_pool(
			EFI_LOADER_DATA,
			mmap_size,
			(void **)&mmap);

		if (status != EFI_SUCCESS || mmap == 0)
			return status;

		status = boot->get_memory_map(
			&mmap_size,
			mmap,
			&mmap_key,
			&desc_size,
			&desc_version);
		if (status == EFI_SUCCESS)
			break;

		boot->free_pool(mmap);

		// If the buffer size turned out too small then get_memory_map
		// should have updated mmap_size to contain the buffer size
		// needed for the memory map. However subsequent free_pool and
		// allocate_pool might change the memory map and therefore I
		// additionally multiply it by 2.
		if (status == EFI_BUFFER_TOO_SMALL) {
			mmap_size *= 2;
			continue;
		}

		return status;
	}

	uint16_t buffer[512];

	u16snprintf(buffer, 512, "Max memory type:%lu, loop:%u, handle:%p\n", EFI_MAX_MEMORY_TYPE, loop, handle);
	out->output_string(out, buffer);

	efi_uint_t i = 0;
	uint64_t desc_num = ((uint64_t)mmap_size) / desc_size;
        for (; i < desc_num; i++) {
	    struct efi_memory_descriptor* desc = (struct efi_memory_descriptor*)((void*)mmap + i * desc_size);
	    uint32_t type = desc->type;
	    if (type != EFI_LOADER_CODE && type != EFI_LOADER_DATA && type != EFI_BOOT_SERVICES_CODE && type != EFI_BOOT_SERVICES_DATA && type != EFI_CONVENTIAL_MEMORY) continue;
	    u16snprintf(buffer, 512, "Memory descriptor: type:%u, start:%p, pages:%u\n", type, desc->physical_start, desc->pages);
	    out->output_string(out, buffer);
	}
	uint16_t msg[] = u"-------------------------\n";
	out->output_string(out, msg);

	status = boot->exit_boot_services(
		handle,
		mmap_key);
	if (status != EFI_SUCCESS)
		boot->free_pool(mmap);

	return status;
}

//From readelf -Wl build/release/loader-stripped.elf 
//  Type           Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg Align
//  LOAD           0x000000 0x0000000fc0090000 0x0000000fc0090000 0x6220a4 0x6b0e78 RWE 0x10000
//  LOAD           0x000000 0x0000000fc0090000 0x0000000fc0090000 0x6720a4 0x701578 RWE 0x10000 (Newer with ACPI)
#define KERNEL_MEMORY_SIZE 0x710000 //0x701578 Rounded up to the align 0x10000
#define KERNEL_FILE_SIZE   0x6720a4

static int memcmp( const void *ptr1, const void *ptr2, size_t num)
{
	for (int i = 0; i < num; i++)
	     if (*((const char*)ptr1++) != *((const char*)ptr2++))
		  return 1;
        return 0;
}

efi_status_t efi_main(
	efi_handle_t handle, struct efi_system_table *system)
{
	efi_handle_t my_handle = handle;
	uint16_t msg[] = u"Hello World, Smoku!\n";
	efi_status_t status;

	status = system->out->output_string(system->out, msg);
	if (status != 0)
		return status;

	err(system, "Conf table entries num:%u\n", system->number_of_table_entries);
	uint64_t acpi_rsdp = 0;
	for (int i = 0; i < system->number_of_table_entries; i++) {
            struct efi_configuration_table *table = &system->configuration_table[i];
            if (!memcmp(&table->guid, &uefi_acpi20_table, sizeof(struct efi_guid))) {
                acpi_rsdp = ((uint64_t)(table->table));
	        err(system, "ACPI rdsp:%p\n", acpi_rsdp);
	    }
	}

	struct efi_file_protocol *rootdir;
	status = setup_loader(handle, system, &rootdir);
	if (status != 0) {
	        err(system, "setup_loader wysypal sie\n");
		return status;
	}

        uint64_t offset = 0;
        size_t memory_size = KERNEL_MEMORY_SIZE;
	size_t file_size = KERNEL_FILE_SIZE;
	uint16_t kernel_path[] = u"efi\\boot\\loader.elf";
	//err(system, "Przed load_kernel\n");
	uint64_t kernel_phys_start;
	status = load_kernel(
		system,
                rootdir,
                kernel_path,
                offset,
                memory_size,
		file_size,
		&kernel_phys_start);
	//err(system, "Po load_kernel, kernel_phys_start:%p\n", kernel_phys_start);
	if (status != 0)
		return status;
        uint64_t entry_point = kernel_phys_start + 0x30000; //TODO do not hardcode 0x30000
	err(system, "Entry point:%p\n", entry_point);

	exit_efi_boot_services(system->boot, my_handle, system->out);

	void (ELFABI *entry)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) ;
	entry = (void (ELFABI *)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)) entry_point;
	const char* cmdline = "--nomount --bootchart --rootfs=rofs /hello";

	/* disable MMU */
        uint64_t sctlr = 0;
        asm volatile ("msr SCTLR_EL1, %0;"
                      "isb":: "r" (sctlr));
	(*entry)(0, 0, acpi_rsdp, kernel_phys_start, (uint64_t)cmdline, 0x40000000);

	//while (1) {}

	return 0;
}
