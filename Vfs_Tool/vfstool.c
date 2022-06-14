#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef unsigned long int u_long;
const char magic[4] = { 'V', 'F', 'S', 0x00 };
const int addr_size = 2048; //maybe use 4th byte of header to change this?
const int buffer_max = 262144;
const char padder[2048] = { 0 };

typedef struct {
	char	name[16];
	unsigned long int	size;
	unsigned long int	addr;
	//unsigned long int	sector_size;
	//unsigned long int	byte_addr;
} VFSFILE;



int splitvfs(int c, char* v[]);
int makevfs(int c, char* v[]);
int main(int argc, char* argv[]);


int splitvfs(int c, char* v[]) {
	int i;
	char header[4];
	long int filenum = 0;
	long int startsect = 0;
	VFSFILE* filelist;
	VFSFILE* readtmp;
	int buffer_left = 0;
	char* buffer;
	FILE* outvfs;
	FILE* invfs = fopen(v[2], "rb+");
	if (!invfs) {
		perror("Can't open VFS");
		return 1;
	}

	fread(header, 1, 4, invfs);
	if (memcmp(magic, header, 4)) {
		printf("Wrong VFS Header!\n");
	}
	fread(&filenum, 4, 1, invfs);
	fread(&startsect, 4, 1, invfs);
	printf("Files in VFS: %i\n", filenum);
	filelist = calloc(filenum, sizeof(VFSFILE));
	if (!filelist) {
		printf("Malloc error!\n");
	}

	readtmp = filelist;
	for (i = 0; i < filenum; i++) {
		fread(readtmp, sizeof(VFSFILE), 1, invfs);
		readtmp++;
	}
	readtmp = filelist;

	for (i = 0; i < filenum; i++) {
		buffer_left = readtmp->size;
		fseek(invfs, readtmp->addr * addr_size, SEEK_SET);
		//printf("Saving file #%i %s (%i bytes)...\n", i + 1, readtmp->name, readtmp->size);
		outvfs = fopen(readtmp->name, "wb+");
		if (!outvfs) {
			perror("Can't open output file");
			return 1;
		}
		while (buffer_left > 0) {
			if (buffer_left > buffer_max) {
				buffer = malloc(buffer_max);
				fread(buffer, 1, buffer_max, invfs);
				fwrite(buffer, 1, buffer_max, outvfs);
				buffer_left -= buffer_max;

			}
			else {
				buffer = malloc(buffer_left);
				fread(buffer, 1, buffer_left, invfs);
				fwrite(buffer, 1, buffer_left, outvfs);
				buffer_left = 0;
			}
			free(buffer);
		}
		fclose(outvfs);
		readtmp++;
	}
	free(filelist);
	printf("VFS save completed!\n");
	return 0;
}

int makevfs(int c, char* v[]) {
	int oldsize, padbytes, fnlen, i, j, buffer_left, hdrpad;
	char* buffer;
	FILE* outvfs;
	long int filenum = c - 2;
	VFSFILE* filelist = calloc(filenum, sizeof(VFSFILE));
	VFSFILE* writetmp = filelist;
	long int lastaddr = ((filenum * sizeof(VFSFILE)) + 12); //Header must be the same size as system pointer (4)!
	//printf("header size: %i remainder bytes: %i", lastaddr, lastaddr % addr_size);
	hdrpad = addr_size - (lastaddr % addr_size);
	//printf("padding bytes of header: %i header size: %i remainder bytes: %i", hdrpad, lastaddr, lastaddr % addr_size);
	if (hdrpad == addr_size) {
		hdrpad = 0;
	}
	lastaddr = (lastaddr + hdrpad) / addr_size;
	for (i = 2; i < c; i++) {
		FILE* invfs = fopen(v[i], "rb+");
		if (!invfs) {
			perror("Couldn't open input file");
			printf("Failed file: %s\n", v[i]);
			return 1;
		}
		fseek(invfs, 0, SEEK_END);
		writetmp->size = ftell(invfs);
		padbytes = addr_size - (writetmp->size % addr_size);
		if (padbytes == addr_size) {
			padbytes = 0;
		}
		if (i == 2) {
			writetmp->addr = lastaddr;
			oldsize = writetmp->size + padbytes;
		}
		else {

			writetmp->addr = (oldsize / addr_size) + lastaddr;
			oldsize = writetmp->size + padbytes;
		}

		lastaddr = writetmp->addr;
		fnlen = strlen(v[i]);
		if (fnlen > 15) {
			for (j = 0; j < 15; j++) {
				writetmp->name[j] = v[i][j + (fnlen - 15)];
			}
		}
		else {
			for (j = 0; j < fnlen; j++) {
				writetmp->name[j] = v[i][j];
			}
		}
		//printf("file found: %s size:%i addr:%i\n", writetmp->name, writetmp->size, writetmp->addr);
		fclose(invfs);
		writetmp++;
	}
	outvfs = fopen(v[1], "wb+");
	if (!outvfs) {
		perror("Couldn't open output VFS file");
		return 1;
	}
	fwrite(magic, sizeof(magic), 1, outvfs);
	fwrite(&filenum, 1, 4, outvfs);
	fwrite(&filelist[0].addr, 1, 4, outvfs);
	fwrite(filelist, filenum, sizeof(VFSFILE), outvfs);
	fwrite(padder, 1, hdrpad, outvfs);
	writetmp = filelist;
	buffer_left = 0;
	for (i = 2; i < c; i++) {
		FILE* invfs = fopen(v[i], "rb+");
		if (!invfs) {
			perror("Couldn't open input file");
			printf("Failed file: %s\n", v[i]);
			return 1;
		}
		buffer_left = writetmp->size;
		while (buffer_left > 0) {
			//printf("%s has %i bytes left!\n", v[i], buffer_left);
			if (buffer_left > buffer_max) {
				buffer = malloc(buffer_max);
				fread(buffer, 1, buffer_max, invfs);
				fwrite(buffer, 1, buffer_max, outvfs);
				buffer_left -= buffer_max;

			}
			else {
				buffer = malloc(buffer_left);
				fread(buffer, 1, buffer_left, invfs);
				fwrite(buffer, 1, buffer_left, outvfs);
				buffer_left = 0;
			}
			free(buffer);
		}
		padbytes = addr_size - (writetmp->size % addr_size);
		if (padbytes == addr_size) {
			padbytes = 0;
		}
		fwrite(padder, 1, padbytes, outvfs);
		writetmp++;

	}
	printf("Packing complete!\n");
	return 0;
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		printf("Usage: %s [options] outfile infile [infile2] [infile3]...\n", argv[0]);
		printf("Option -x extracts all resources from the first file passed as an argument.\n");
		printf("Otherwise, VFSTool will pack all other specified files into the first file.\n");
		return 0;
	}
	else if (!strcmp(argv[1], "-x")) {
		return splitvfs(argc, argv);
	}
	else {
		return makevfs(argc, argv);
	}

}