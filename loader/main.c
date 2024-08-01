#include "uefi.h"
#include "uefi_loaded_image_protocol.h"
#include "uefi_simple_file_system_protocol.h"
#include "uefi_file_protocol.h"
#include "uefi_graphics_output_protocol.h"
#include "elf.h"
#include "memory_map.h"

EFI_BOOT_SERVICES *gBS;
EFI_SYSTEM_TABLE *gST;

void hlt() {
	while(1) asm("hlt");
}

VOID Print(CHAR16 *str) {
	gST->ConOut->OutputString(gST->ConOut, str);
	return;
}

UINTN pow(UINTN a, UINTN b) {
	UINTN ret = 1;
	for(UINTN i = 0; i < b; i++) {
		ret *= a;
	}
	return ret;
}

VOID Print_int(UINTN a, unsigned int radix) {
	CHAR16 str[25];
	CHAR16 *p = str;
	unsigned int v = a;
	int n = 1;
	while(v >= radix) {
		v/=radix;
		n++;
	}
	p = str + n;
	v = a;
	*p = 0;
	do {
		p--;
		*p = v % radix + (CHAR16)'0';
		if(*p > (CHAR16)'9') {
			*p = v % radix - 10 + 'A';
		}
		v /= radix;
	} while(p != str);
	Print(str);
}

EFI_STATUS EFIAPI efi_main(void *image_handle __attribute((unused)),
	EFI_SYSTEM_TABLE *system_table)
{
	EFI_STATUS status;
	gST = system_table;
	gBS = gST->BootServices;

	gST->ConOut->ClearScreen(gST->ConOut);
	gST->ConOut->OutputString(gST->ConOut,
			L"Hello, UEFI!\r\n");

	// open loaded image protocol
	EFI_GUID loaded_image_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
	status = gBS->OpenProtocol(
			image_handle,
			&loaded_image_guid,
			(VOID**)&loaded_image,
			image_handle,
			NULL,
			EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
	if(status != EFI_SUCCESS) {
		Print(L"failed to open EFI_LOADED_IMAGE_PROTOCOL\r\n");
		hlt();
	}
	Print(L"open EFI_LOADED_IMAGE_PROTOCOL\r\n");

	// open simple file system protocol
	EFI_GUID simple_file_system_protocol_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	status = gBS->OpenProtocol(
			loaded_image->DeviceHandle,
			&simple_file_system_protocol_guid,
			(VOID**)&fs,
			image_handle,
			NULL,
			EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
	if(status != EFI_SUCCESS) {
		Print(L"failed to open EFI_SIMPLE_FILE_SYSTEM_PROTOCOL\r\n");
		hlt();
	}
	Print(L"open EFI_SIMPLE_FILE_SYSTEM_PROTOCOL\r\n");

	// open root
	EFI_FILE_PROTOCOL *root_dir;
	status = fs->OpenVolume(fs, &root_dir);
	if(status != EFI_SUCCESS) {
		Print(L"failed to open root.\r\n");
		hlt();
	}
	Print(L"open root\r\n");

	// open kernel.elf
	EFI_FILE_PROTOCOL *kernel_file;
	status = root_dir->Open(
		root_dir, &kernel_file, L"\\kernel.elf",
		EFI_FILE_MODE_READ, 0);
	if( status != EFI_SUCCESS) {
		Print(L"failed to open kernel.elf\r\n");
		hlt();
	}
	Print(L"open kernel.elf\r\n");

	// get kernel info
	CHAR8 kernel_info_buf[sizeof(EFI_FILE_INFO)+100];
	UINTN kernel_info_size = (UINTN)sizeof(kernel_info_buf);
	EFI_FILE_INFO *kernel_info = (EFI_FILE_INFO*)kernel_info_buf;
	EFI_GUID file_info_guid = EFI_FILE_INFO_ID;
	status = kernel_file->GetInfo(
			kernel_file,
			&file_info_guid,
			&kernel_info_size,
			kernel_info);
	if(status != EFI_SUCCESS) {
		Print(L"failed to get kernel info\r\n");
		hlt();
	}

	// allocate tmp buffer for kernel
	UINTN kernel_file_size = (UINTN)kernel_info->FileSize;
	CHAR8 *kernel_tmp_buf;
	status = gBS->AllocatePool(
			EfiLoaderData,
			kernel_file_size,
			(VOID**)&kernel_tmp_buf
			);
	if(status != EFI_SUCCESS) {
		Print(L"failed to allocate pool for kernel_tmp_buf\r\n");
		hlt();
	}
	Print(L"allocate pool for kernel_tmp_buf\r\n");

	// read kernel
	status = kernel_file->Read(
			kernel_file,
			&kernel_file_size,
			kernel_tmp_buf
			);
	if(status != EFI_SUCCESS) {
		Print(L"failed to read kernel.elf\r\n");
		hlt();
	}
	Print(L"read kernel.elf\r\n");
	if(kernel_tmp_buf[0] != 0x7f ||
			kernel_tmp_buf[1] != 'E' || 
			kernel_tmp_buf[2] != 'L' ||
			kernel_tmp_buf[3] != 'F') {
		Print(L"read kernel is not elf format.\r\n");
		hlt();
	}
	Print(L"this is elf format\r\n");

	/*
	// allocate kernel buffer
	status = gBS->AllocatePages(
			AllocateAddress,
			EfiLoaderData,
			*/

	// read kernel elf header
	Elf64_Ehdr *ehdr = (Elf64_Ehdr*)kernel_tmp_buf;
	Elf64_Half ph_ent_size = ehdr->e_phentsize;
	Elf64_Half ph_num = ehdr->e_phnum;
	UINTN kernel_start_addr = 0xffffffffffff;
	UINTN kernel_end_addr = 0x0;

	// calc kernel mem size
	for(int i = 0; i < ph_num; i++) {
		Elf64_Phdr *phdr = (Elf64_Phdr*)((UINTN)kernel_tmp_buf + ehdr->e_phoff + i*ph_ent_size);
		if(phdr->p_type != PT_LOAD)
			continue;
	
		if(phdr->p_vaddr < kernel_start_addr) 
			kernel_start_addr = (UINTN)phdr->p_vaddr;
		if(kernel_end_addr < (phdr->p_vaddr + phdr->p_memsz))
			kernel_end_addr = phdr->p_vaddr + phdr->p_memsz;
	}
	UINTN page_num = (kernel_end_addr - kernel_start_addr) / 0x1000 + 1;
	status = gBS->AllocatePages(
			AllocateAddress,
			EfiLoaderData,
			page_num,
			&kernel_start_addr
			);
	if(status != EFI_SUCCESS) {
		Print(L"failed to allocate kernel buffer\r\n");
		Print(L"kernel_start_addr : 0x");
		Print_int(kernel_start_addr, 16);
		Print(L"\r\n");
		hlt();
	}
	Print(L"allocate kernel buffer successfully\r\n");
	Print(L"kernel_start_addr : 0x");
	Print_int(kernel_start_addr, 16);
	Print(L"\r\nkernel_end_addr : 0x");
	Print_int(kernel_end_addr, 16);
	Print(L"\r\n");

	// copy kernel
	gBS->SetMem(
			(VOID*)kernel_start_addr,
			kernel_end_addr - kernel_start_addr + 1,
			0
			);
	for(int i = 0; i < ph_num; i++) {
		Elf64_Phdr *phdr = (Elf64_Phdr*)((UINTN)kernel_tmp_buf + ehdr->e_phoff+i*ph_ent_size);
		if(phdr->p_type != PT_LOAD)
			continue;
		gBS->CopyMem(
				(VOID*)phdr->p_vaddr,
				(VOID*)((UINTN)kernel_tmp_buf + phdr->p_offset),
				phdr->p_filesz
				);
		Print(L"\r\ncopy\r\ndst : 0x");
		Print_int((UINTN)phdr->p_vaddr, 16);
		Print(L"\r\nsrc : 0x");
		Print_int((UINTN)kernel_tmp_buf + phdr->p_offset, 16);
		Print(L"\r\nlen : 0x");
		Print_int((UINTN)phdr->p_filesz, 16);
		Print(L"\r\n");
	}

	// open graphic_output_protocol
	/*
	EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
	UINTN num_gop_handles = 0;
	EFI_HANDLE *gop_handles = NULL;
	status = gBS->LocateHandleBuffer(
			ByProtocol,
			&gop_guid,
			NULL,
			&num_gop_handles,
			&gop_handles);
	if(status != EFI_SUCCESS) {
		Print(L"LocateHandleBuffer() failed.\r\n");
		hlt();
	}
	for(UINTN i = 0; i < num_gop_handles; i++) {
		status = gBS->OpenProtocol(
				gop_handles[i],
				&gop_guid,
				(VOID**)gop,
				image_handle,
				NULL,
				EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if(status != EFI_SUCCESS) {
			Print(L"failed to open gop\r\nindex: ");
			Print_int(i, 10);
			Print(L"\r\nstatus: 0x");
			Print_int(status, 16);
			Print(L"\r\n");
		} else {
			Print(L"open gop successfully\r\n");
			break;
		}
	}
	if(status != EFI_SUCCESS) hlt();
	*/
	EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
	status = gBS->LocateProtocol(
			&gop_guid,
			NULL,
			(VOID**)&gop	
			);
	if(status != EFI_SUCCESS) {
		Print(L"Locate gop failed.\r\n");
		hlt();
	}

	// exit bootservice
	MemoryMap mmap = {NULL,0,0,4096*4,0};
	status = gBS->AllocatePool(
			EfiLoaderData,
			mmap.map_size,
			(VOID**)&mmap.desc
			);
	if(status != EFI_SUCCESS) {
		Print(L"failed to allocate pool for memory map\r\n");
		hlt();
	}

	status = gBS->GetMemoryMap(
			&mmap.map_size,
			mmap.desc,
			&mmap.map_key,
			&mmap.desc_size,
			&mmap.desc_ver
			);
	if(status != EFI_SUCCESS) {
		Print(L"failed to get memory map\r\nstatus = 0x");
		Print_int((UINTN)status,16);
		hlt();
	}

	status = gBS->ExitBootServices(image_handle, mmap.map_key);
	if(status != EFI_SUCCESS) {
		Print(L"faield to exit bootservices\r\n");
		hlt();
	}

	typedef void (*kernel_main_t) (void);
	kernel_main_t kernel_main = (kernel_main_t)ehdr->e_entry;
	kernel_main();

	hlt();
	return EFI_SUCCESS;
}
