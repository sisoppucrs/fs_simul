#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "libdisksimul.h"
#include "filesystem.h"

/**
 * @brief Format disk.
 * 
 */

/*
int print_sectors() {
	struct root_table_directory root_dir;
	ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);
	struct sector_data sector;
	int sector_pointer = root_dir.free_sectors_list;
	printf("0");
	while(sector_pointer) {
		printf(" -> %d", sector_pointer);
		ds_read_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);
		sector_pointer = sector.next_sector;
		//if (sector_pointer > 10)
			//break;
	}
	printf("\n");
	return 0;

}

void debug_sector(int pos) {
	if (!pos) {
		struct root_table_directory root_dir;
		ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);
		printf("free_sectors_list: %d\n", root_dir.free_sectors_list);
		printf("entries:\n");
			printf("┌───┬──────────────────────┬─────────┬───────┐\n");
			printf("│ T │ Nome                 │ Tamanho │ Setor │\n");
			printf("├───┼──────────────────────┼─────────┼───────┤\n");
		for (int i = 0; i < 15; i++) {
			struct file_dir_entry *entrada = &root_dir.entries[i];
			printf("│ %c │ %.20s%*s│ %4d kB │  %.4x │\n", (entrada->dir? 'd' : 'f'), entrada->name, (int) (21 - strlen(entrada->name)), " ", entrada->size_bytes/1024, entrada->sector_start);

		}
			printf("└───┴──────────────────────┴─────────┴───────┘\n");
		printf("\n");

	} else {
		unsigned char buffer[16][SECTOR_SIZE/16];
		ds_read_sector(pos, (void*)&buffer, SECTOR_SIZE);
		for (int j = 0; j < 16; j++) {
			for (int i = 0; i < SECTOR_SIZE/16; i++)
	   			printf("%.2x ", buffer[j][i]);
			printf("\n");
		}
	}

}
*/

struct file_dir_entry* get_file_dir_entry(struct root_table_directory* root_dir, char* path, int new, struct table_directory* td, int* sector_pointer) {
	*sector_pointer = 0;
	if (!strlen(path) || !strcmp(path, "/"))
		return 0;
	struct file_dir_entry *entrada;
	struct file_dir_entry *entries = root_dir->entries;
	int entries_length = 15;
	char *pch = strtok(path, "/");
	char *next_pch = strtok(NULL, "/");

look:for (int i = 0; i < entries_length; i++) {
		entrada = &entries[i];
		if (entrada->sector_start) {
			entrada->name[20] = '\0';
			if (!strcmp(entrada->name, pch)) {
				if (next_pch == NULL) {
					if (new) {
						printf("Arquivo '%s' já existe!\n", pch);
						return 0;
					} else
						return entrada;
				} else
					if (entrada->dir) {
						*sector_pointer = entrada->sector_start;
						pch = next_pch;
						next_pch = strtok(NULL, "/");
						ds_read_sector(*sector_pointer, (void*)td, SECTOR_SIZE);
						entries = td->entries;
						entries_length = 16;
						goto look;
					} else {
						printf("'%s' não é um diretório!\n", pch);
						return 0;
					}
			}
		} else
			if (new) {
				if (next_pch == NULL) {
					strcpy(entrada->name, pch);
					return entrada;
				} else {
					printf("Pasta '%s' não encontrada!\n", pch);
					return 0;
				}
			}
}
	if (new)
		printf("Não há mais entradas disponíveis!\n");
	else
		printf("Arquivo não encontrado!\n");

	return 0;
}

int fs_format(){
	int ret, i;
	struct root_table_directory root_dir;
	struct sector_data sector;
	
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 1)) != 0 ){
		return ret;
	}
	
	memset(&root_dir, 0, sizeof(root_dir));
	
	root_dir.free_sectors_list = 1; /* first free sector. */
	
	ds_write_sector(0, (void*)&root_dir, SECTOR_SIZE);
	
	/* Create a list of free sectors. */
	memset(&sector, 0, sizeof(sector));
	
	for(i=1;i<NUMBER_OF_SECTORS;i++){
		if(i<NUMBER_OF_SECTORS-1){
			sector.next_sector = i+1;
		}else{
			sector.next_sector = 0;
		}
		ds_write_sector(i, (void*)&sector, SECTOR_SIZE);
	}
	
	ds_stop();
	
	printf("Disk size %d kbytes, %d sectors.\n", (SECTOR_SIZE*NUMBER_OF_SECTORS)/1024, NUMBER_OF_SECTORS);
	
	return 0;
}

/**
 * @brief Create a new file on the simulated filesystem.
 * @param input_file Source file path.
 * @param simul_file Destination file path on the simulated file system.
 * @return 0 on success.
 */
int fs_create(char* input_file, char* simul_file){
	int ret;
	
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}

	//print_sectors();

	FILE *fp = fopen(input_file, "r");

	fseek(fp, 0L, SEEK_END);
	int size = ftell(fp);
	if (size > NUMBER_OF_SECTORS * (SECTOR_SIZE - sizeof(int))) {
		printf("Arquivo grande demais para o sistema de arquivos!\n");
		return 1;
	}

	struct root_table_directory root_dir;
	ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);

	int sector_pointer = root_dir.free_sectors_list;
	struct sector_data sector;
	int free_space = 0;

	while(sector_pointer) {
		ds_read_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);
		sector_pointer = sector.next_sector;
		free_space += SECTOR_SIZE;
	}

	if (size > free_space) {
		printf("Não há espaço suficiente disponível para o arquivo!\n");
		return 1;
	}

	struct table_directory td;
	int td_sector;
	struct file_dir_entry *entrada = get_file_dir_entry(&root_dir, simul_file, 1, &td, &td_sector);
	if (!entrada)
		return 1;

	entrada->dir = 0;

	entrada->size_bytes = size;
	sector_pointer = root_dir.free_sectors_list;
	entrada->sector_start = sector_pointer;

	int position_inside_file = 0;

	while (size > 0) {

		int sector_size = (size > SECTOR_SIZE - sizeof(int) ? SECTOR_SIZE - sizeof(int) : size);
		char buffer[sector_size];

		fseek(fp, SEEK_SET, position_inside_file);
		fread(buffer, sector_size, 1, fp);

		ds_read_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);

		memcpy(&sector, buffer, sector_size);

		ds_write_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);

		sector_pointer = sector.next_sector;

		size -= sector_size;
		position_inside_file += sector_size;
	}

	root_dir.free_sectors_list = sector_pointer;
	
	ds_write_sector(0, (void*)&root_dir, SECTOR_SIZE);
	
	if (td_sector)
		ds_write_sector(td_sector, (void*)&td, SECTOR_SIZE);

	//print_sectors();

	ds_stop();
	
	return 0;
}

/**
 * @brief Read file from the simulated filesystem.
 * @param output_file Output file path.
 * @param simul_file Source file path from the simulated file system.
 * @return 0 on success.
 */
int fs_read(char* output_file, char* simul_file){
	int ret;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	/* Write the code to read a file from the simulated filesystem. */
	
	ds_stop();
	
	return 0;
}

/**
 * @brief Delete file from file system.
 * @param simul_file Source file path.
 * @return 0 on success.
 */
int fs_del(char* simul_file){
	int ret;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}

	//print_sectors();
	
	struct root_table_directory root_dir;
	ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);

	struct table_directory td;
	int td_sector;
	struct file_dir_entry *entrada = get_file_dir_entry(&root_dir, simul_file, 0, &td, &td_sector);
	if (!entrada)
		return 1;

	int sector_pointer = entrada->sector_start;
	int first_pointer = sector_pointer;
	struct sector_data sector;
	int size = entrada->size_bytes;
	while(1) {
		int sector_size = (size > SECTOR_SIZE - sizeof(int) ? SECTOR_SIZE - sizeof(int) : size);
		char buffer[sector_size];
		ds_read_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);
		memcpy(&sector, buffer, sector_size);
		ds_write_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);

		size -= sector_size;
		if (!size)
			break;

		sector_pointer = sector.next_sector;
	}
	sector.next_sector = root_dir.free_sectors_list;
	ds_write_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);
	root_dir.free_sectors_list = first_pointer;

	entrada->sector_start = 0;
	strcpy(entrada->name, "");

	ds_write_sector(0, (void*)&root_dir, SECTOR_SIZE);
	
	if (td_sector)
		ds_write_sector(td_sector, (void*)&td, SECTOR_SIZE);

	ds_stop();

	return 0;
}

/**
 * @brief List files from a directory.
 * @param simul_file Source file path.
 * @return 0 on success.
 */
int fs_ls(char *dir_path){
	int ret;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	int td_sector = 0;
	struct root_table_directory root_dir;
	struct table_directory td;
	ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);

	struct file_dir_entry *entries, *entrada;
	int size;

	if (dir_path == NULL || !strlen(dir_path) || !strcmp(dir_path, "/")) {
		entries = root_dir.entries;
		size = 15;
	} else {
		entrada = get_file_dir_entry(&root_dir, dir_path, 0, &td, &td_sector);
		if (!entrada)
			return 1;

		ds_read_sector(entrada->sector_start, (void*)&td, SECTOR_SIZE);
		entries = td.entries;
		size = 16;
	}

	int file_count = 0;
	int total_size = 0;

	printf("┌───┬──────────────────────┬─────────┐\n");
	printf("│ T │ Nome                 │ Tamanho │\n");
	printf("├───┼──────────────────────┼─────────┤\n");
	for (int i = 0; i < size; i++)
		if (entries[i].sector_start) {
			entrada = &entries[i];
			file_count++;
			total_size += entrada->size_bytes;
			printf("│ %c │ %.20s%*s│ %4d kB │\n", (entrada->dir? 'd' : 'f'), entrada->name, (int) (21 - strlen(entrada->name)), " ", entrada->size_bytes/1024);

		}
	printf("└───┴──────────────────────┴─────────┘\n");
	printf("\nQuantidade: %d Tamanho total: %d kB\n", file_count, total_size/1024);
	
	ds_stop();
	
	return 0;
}

/**
 * @brief Create a new directory on the simulated filesystem.
 * @param directory_path directory path.
 * @return 0 on success.
 */
int fs_mkdir(char* directory_path){
	int ret;
	
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}

	struct root_table_directory root_dir;
	ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);

	int sector_pointer = root_dir.free_sectors_list;

	struct table_directory td;
	int td_sector;
	struct file_dir_entry *entrada = get_file_dir_entry(&root_dir, directory_path, 1, &td, &td_sector);
	if (!entrada)
		return 1;

	entrada->dir = 1;

	entrada->size_bytes = 0;
	entrada->sector_start = root_dir.free_sectors_list;

	sector_pointer = root_dir.free_sectors_list;
	
	struct sector_data sector;

	ds_read_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);

	struct table_directory *buffer = malloc(sizeof (struct table_directory));
	
	root_dir.free_sectors_list = sector.next_sector;
	
	memcpy(&sector, buffer, SECTOR_SIZE);

	ds_write_sector(sector_pointer, (void*)&sector, SECTOR_SIZE);
	
	ds_write_sector(0, (void*)&root_dir, SECTOR_SIZE);

	ds_stop();
	
	return 0;
}	

/**
 * @brief Remove directory from the simulated filesystem.
 * @param directory_path directory path.
 * @return 0 on success.
 */
int fs_rmdir(char *directory_path){
	int ret;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	/* Write the code to delete a directory. */
	
	ds_stop();
	
	return 0;
}

/**
 * @brief Generate a map of used/available sectors. 
 * @param log_f Log file with the sector map.
 * @return 0 on success.
 */
int fs_free_map(char *log_f){
	int ret, i, next;
	struct root_table_directory root_dir;
	struct sector_data sector;
	char *sector_array;
	FILE* log;
	int pid, status;
	int free_space = 0;
	char* exec_params[] = {"gnuplot", "sector_map.gnuplot" , NULL};

	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	/* each byte represents a sector. */
	sector_array = (char*)malloc(NUMBER_OF_SECTORS);

	/* set 0 to all sectors. Zero means that the sector is used. */
	memset(sector_array, 0, NUMBER_OF_SECTORS);
	
	/* Read the root dir to get the free blocks list. */
	ds_read_sector(0, (void*)&root_dir, SECTOR_SIZE);
	
	next = root_dir.free_sectors_list;

	while(next){
		/* The sector is in the free list, mark with 1. */
		sector_array[next] = 1;
		
		/* move to the next free sector. */
		ds_read_sector(next, (void*)&sector, SECTOR_SIZE);
		
		next = sector.next_sector;
		
		free_space += SECTOR_SIZE;
	}

	/* Create a log file. */
	if( (log = fopen(log_f, "w")) == NULL){
		perror("fopen()");
		free(sector_array);
		ds_stop();
		return 1;
	}
	
	/* Write the the sector map to the log file. */
	for(i=0;i<NUMBER_OF_SECTORS;i++){
		if(i%32==0) fprintf(log, "%s", "\n");
		fprintf(log, " %d", sector_array[i]);
	}
	
	fclose(log);
	
	/* Execute gnuplot to generate the sector's free map. */
	pid = fork();
	if(pid==0){
		execvp("gnuplot", exec_params);
		return 0;
	}
	
	wait(&status);
	
	free(sector_array);
	
	ds_stop();
	
	printf("Free space %d kbytes.\n", free_space/1024);
	
	return 0;
}
