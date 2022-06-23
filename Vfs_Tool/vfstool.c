#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

typedef unsigned long int u_long;
const char magic[4] = { 'V', 'F', 'S', 0x00 };
const int addr_size = 2048; //maybe use 4th byte of header to change this?
const int buffer_max = 262144;
const char padder[2048] = { 0 };
const char extn[7][5] = {
	".hit",
	".pxm",
	".psq",
	".psp",
	".txt",
	".mnu",
	".exe"
};

const u_long extstk[7] = {
	0xFFFFFF01,
	0xFFFFFF02,
	0xFFFFFF03,
	0xFFFFFF04,
	0xFFFFFFFF,
	0xFFFFFFFD,
	0x801FFFF0
};

typedef struct {
	char	name[64];
	u_long	size;
	u_long	addr;
	u_long	sector_size;
	u_long	byte_addr;
	u_long	stack;
} VFSFILE;



int splitvfs(int c, char* v[]);
int makevfs(int c, char* v[]);
int main(int argc, char* argv[]);
int txttovfs(int c, char* v[]);


int txttovfs(int c, char* v[]) {

	typedef struct {
		char fn[256];
		long fs;
		long pad;
		long sectors;
		int source;
	} FILENAME;

	char name[64];
	char filename[256];
	FILENAME* fnlist;
	u_long stack = 0;
	int trackof = 0;
	int filenum = 0;
	int f = 0;
	long cur_sector = 0;
	long start_pad = 0;
	long start_byte = 0;
	long start_sect = 0;
	//long cur_byte = 0;
	int buffer_left = 0;
	char* buffer;
	VFSFILE* allfiles;
	FILE* infile, *outfile;
	FILE* intxt = fopen(v[2], "rb+");
	


	if (!intxt) {
		perror("Can't open CSV");
		return 1;
	}

	while (fscanf(intxt, "%X,%i,\"%63[^\"]\",\"%255[^\"]\"", &stack, &trackof, name, filename) != EOF) {
		filenum++;
	}
	fseek(intxt, 0, SEEK_SET);

	allfiles = calloc(sizeof(VFSFILE), filenum);
	fnlist = calloc(sizeof(FILENAME), filenum);
	start_byte = (((sizeof(VFSFILE) * filenum) + 12) % addr_size);
	start_pad = addr_size - start_byte;
	if (start_pad == addr_size) {
		start_pad = 0;
	}
	start_sect = (start_pad + start_byte) / addr_size;
	cur_sector = start_sect;

	while (fscanf(intxt, "%X,%i,\"%63[^\"]\",\"%255[^\"]\"", &stack, &trackof, name, filename) != EOF) {
		
		infile = fopen(filename, "rb+");
		if (!infile) {
			perror("File Size Error");
		}
		//realloc(allfiles, sizeof(VFSFILE) * (filenum + 1));
		//realloc(fnlist, sizeof(FILENAME) * (filenum + 1));
		allfiles[f].stack = stack;
		fseek(infile, 0, SEEK_END);
		if (trackof == -1) {
			fnlist[f].fs = ftell(infile);
			//printf("%i %s\n",fnlist[f].fs,filename);
			fnlist[f].pad = addr_size - (fnlist[f].fs % addr_size);
			if (fnlist[f].pad == addr_size) {
				fnlist[f].pad = 0;
			}
			fnlist[f].sectors = (fnlist[f].pad + fnlist[f].fs) / addr_size;
		} else {
			fnlist[f].fs = 0;
			fnlist[f].pad = 0;
			fnlist[f].sectors = 0;
		}
		strcpy(fnlist[f].fn, filename);
		strcpy(allfiles[f].name, name);
		fnlist[f].source = trackof;
		fclose(infile);
		allfiles[f].size = fnlist[f].fs;
		allfiles[f].sector_size = fnlist[f].sectors;
		allfiles[f].addr = cur_sector;
		allfiles[f].byte_addr = cur_sector * addr_size;
		cur_sector += fnlist[f].sectors;
		//printf("%i %i %i %s %i\n", allfiles[f].sector_size, allfiles[f].addr, allfiles[f].size, allfiles[f].name, allfiles[f].stack);
		f++;
	}
	
	for (f = 0; f < filenum; f++) {
		if (fnlist[f].source != -1) {
			allfiles[f].addr = allfiles[fnlist[f].source].addr;
			allfiles[f].byte_addr = allfiles[fnlist[f].source].byte_addr;
			allfiles[f].size = allfiles[fnlist[f].source].size;
			allfiles[f].sector_size = allfiles[fnlist[f].source].sector_size;
		}
		//printf("%i %i %i %s %i\n", allfiles[f].sector_size, allfiles[f].addr, allfiles[f].size, allfiles[f].name, allfiles[f].stack);
	}
	fclose(intxt);
	outfile = fopen(v[3], "wb+");

	fwrite(magic, sizeof(magic), 1, outfile);
	fwrite(&filenum, 1, 4, outfile);
	fwrite(&start_sect, 1, 4, outfile);
	fwrite(allfiles, filenum, sizeof(VFSFILE), outfile);
	fwrite(padder, 1, start_pad, outfile);
	
	for (f = 0; f < filenum; f++) {
		infile = fopen(fnlist[f].fn, "rb+");
		if (!infile) {
			perror("Couldn't open input file");
			printf("Failed file: %s\n", fnlist[f].fn);
			return 1;
		}
		buffer_left = fnlist[f].fs;
		while (buffer_left > 0) {
			//printf("%s has %i bytes left!\n", fnlist[f].fn, buffer_left);
			if (buffer_left > buffer_max) {
				buffer = malloc(buffer_max);
				fread(buffer, 1, buffer_max, infile);
				fwrite(buffer, 1, buffer_max, outfile);
				buffer_left -= buffer_max;

			}
			else {
				buffer = malloc(buffer_left);
				fread(buffer, 1, buffer_left, infile);
				fwrite(buffer, 1, buffer_left, outfile);
				buffer_left = 0;
			}
			free(buffer);
		}

		fwrite(padder, 1, fnlist[f].pad, outfile);
		fclose(infile);
	}
	fclose(outfile);
	printf("VFS saved from CSV!\n");
	return 0;
}

int splitvfs(int c, char* v[]) {
	int i, j;
	char header[4];
	char fn_sp[70];
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
		strcpy(fn_sp, readtmp->name);
		for (j = 0; j < (sizeof(extstk) / sizeof(extstk[0])); j++) {
			if (readtmp->stack == extstk[j]) {
				sprintf(fn_sp,"%s%s",readtmp->name,extn[j]);
			}
		}
		outvfs = fopen(fn_sp, "wb+");
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
	int oldsize, padbytes, fnlen, i, j, buffer_left, hdrpad, fnstart, fnend;
	char *buffer, *fnext, *lowerstr;
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
			writetmp->byte_addr = writetmp->addr * addr_size;
			oldsize = writetmp->size + padbytes;
			writetmp->sector_size = oldsize / addr_size;
		}
		else {
			writetmp->addr = (oldsize / addr_size) + lastaddr;
			writetmp->byte_addr = writetmp->addr * addr_size;
			oldsize = writetmp->size + padbytes;
			writetmp->sector_size = oldsize / addr_size;
		}

		lastaddr = writetmp->addr;
		fnlen = strlen(v[i]);
		fnstart = 0;
		fnend = fnlen;
		for (j = 0; j < fnlen; j++) {
			switch (v[i][j]) {
			case '.':
				fnend = j;
				break;
			case '/':
			case '\\':
				fnstart = j + 1;
				break;
			default:
				break;
			}
		}
		if (fnend - fnstart > 63) {
			fnend = fnstart + 63;
			/*
			for (j = fnstart; j < fnstart + 63; j++) {
				writetmp->name[j] = v[i][j];
			}
			*/
		}
		for (j = fnstart; j < fnend; j++) {
			writetmp->name[j - fnstart] = v[i][j];
		}
		fnext = strrchr(v[i], '.');
		
		if (fnext) {
			//printf("%i %s, ", strlen(fnext) + 2, fnext);
			lowerstr = malloc(strlen(fnext) + 2);
			for (j = 0; j <= strlen(fnext) + 1; j++) {
				lowerstr[j] = tolower(fnext[j]);
			}
			//printf("%s\n", lowerstr);
			writetmp->stack = 0x801FFFF0;
			for (j = 0; j < sizeof(extstk) / sizeof(extstk[0]); j++) {
				//printf("%s, %s\n", lowerstr, extn[j]);
				if (!strcmp(lowerstr, extn[j])) {
					writetmp->stack = extstk[j];
				}
			}
			free(lowerstr);
		}
		else {
			writetmp->stack = 0x801FFFF0;
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
	free(filelist);
	printf("Packing complete!\n");
	return 0;
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		printf("Usage: %s [options] outfile infile [infile2] [infile3]...\n", argv[0]);
		printf("Option -x extracts all resources from the first file passed as an argument.\n");
		printf("Option -c makes a VFS (file arg 3) from the first CSV file argument.\n");
		printf("Otherwise, VFSTool will pack all other specified files into the first file.\n");
		return 0;
	}
	else if (!strcmp(argv[1], "-x")) {
		return splitvfs(argc, argv);
	}
	else if (argc == 4 && !strcmp(argv[1], "-c")) {
		return txttovfs(argc, argv);
	}
	else {
		return makevfs(argc, argv);
	}

}