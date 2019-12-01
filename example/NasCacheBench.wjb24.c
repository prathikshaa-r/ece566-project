#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <time.h>
#include <string.h>


extern int errno;

void usage() 
{
  fprintf(stderr, "Usage: Directory[mountdir or nasdir] fileSize (Kb) numberofFiles\n");
  abort();
}

int main(int argc, char *argv[])
{
	if(argc != 4)
	{
		usage();
	}	

	//----------Parsing-----------	

	char directory[9];//up to mountdir/ (10)
	int numberofFiles;
	size_t fileSize;

	sscanf(argv[argc-1], "%d", &numberofFiles);

	int parseRet = sscanf(argv[argc-2], "%lu", &fileSize);
	if(parseRet == EOF)
	{
		fprintf(stderr, "No fileSize parsed ---> Defaulting to 100 Kb.\n");
	}
	else
	{
		fprintf(stderr, "fileSize parsed ---> Setting to %lu Kb.\n", fileSize);
	}

	parseRet = sscanf(argv[argc-3], "%s", directory);
	fprintf(stderr, "Test Directory: %s\n", directory);

	//-------End Parsing-------


	FILE* fileNames[numberofFiles];

	FILE* filePointers[numberofFiles];
	for(int create_index = 0; create_index < numberofFiles; create_index++)
	{
		char* fileName;
		if(strcmp("mountdir", directory) == 0)
		{
			fileName = malloc(100);//see below snprintf comment for length of string 27
			strcpy(fileName, "/home/wjb24/ECE566/ece566-project/example/mountdir/benchFile");

			int numLength = snprintf( NULL, 0, "%d", create_index);
			char* fileNum = malloc(numLength + 1);
			snprintf(fileNum, numLength + 1, "%d", create_index);
			strcat(fileName, fileNum);
			free(fileNum);

			strcat(fileName, ".wjb24");
		}
		else if(strcmp("nasdir", directory) == 0)
		{
			fileName = malloc(100);//see below snprintf comment for length of string 27
			strcpy(fileName, "/home/wjb24/ECE566/ece566-project/example/nasdir/benchFile");

			int numLength = snprintf( NULL, 0, "%d", create_index);
			char* fileNum = malloc(numLength + 1);
			snprintf(fileNum, numLength + 1, "%d", create_index);
			strcat(fileName, fileNum);
			free(fileNum);
			
			strcat(fileName, ".wjb24");
		}
		else
		{
			fprintf(stderr,"Direcotry parse error");
			abort();
		}
		//printf("%s\n", fileName);
		fileNames[create_index] = fileName;
		filePointers[create_index] = fopen(fileName, "w+");
		if(filePointers[create_index] == NULL)
		{
			perror("Error opening file: ");
		}
	}

	//--------Data Source Creation-------

	char* dataSource;

	dataSource = malloc(fileSize*1024);

	for(int byte_index = 0; byte_index < fileSize*1024; byte_index++)//write 1 to 9 repeatedly to dataSource
	{
		int toSet = (byte_index%10);
		if(toSet != 0)
		{
			memset(dataSource+byte_index, toSet+48, 1);//ASCII value of 0 is 48
		}
		else
		{
			memset(dataSource+byte_index, 10, 1);//line break
		}
	}


	//--------------------Begin Testing-------------------


	//---------Write Testing---------
	
	/*
		Files should already be open for writing.
	*/
	struct timespec timerStart, timerEnd; 
	double time_taken;

	double writeSpeeds[numberofFiles];
	for(int write_index = 0; write_index < numberofFiles; write_index++)
	{
		size_t writeRes;
		clock_gettime(CLOCK_MONOTONIC, &timerStart);

		writeRes = fwrite(dataSource, fileSize*1024, 1, filePointers[write_index]);

		clock_gettime(CLOCK_MONOTONIC, &timerEnd);
    	time_taken = (timerEnd.tv_sec - timerStart.tv_sec) * 1e9; 
    	time_taken = (time_taken + (timerEnd.tv_nsec - timerStart.tv_nsec)) * 1e-9; 

		//fprintf(stderr, "Write to file %d took %lf milliseconds\n", write_index, cpuTimeUsed*1000);
		writeSpeeds[write_index] = ((double)(fileSize))/time_taken;

      	fclose(filePointers[write_index]);
	}

	double writeAverage, writeMin, writeMax;
	writeAverage = 0;
	writeMin = writeSpeeds[0];
	writeMax = writeSpeeds[0];
	for(int write_speed_index = 0; write_speed_index < numberofFiles; write_speed_index++)
	{
		writeAverage = writeAverage + (writeSpeeds[write_speed_index]/numberofFiles);
		if(writeMin > writeSpeeds[write_speed_index])
		{
			writeMin = writeSpeeds[write_speed_index];
		}
		if(writeMax < writeSpeeds[write_speed_index])
		{
			writeMax = writeSpeeds[write_speed_index];
		}
	}

	fprintf(stderr, "Write average: %f (Kb/s) | Write minimum: %f (Kb/s) | Write maximum: %f (Kb/s)\n", writeAverage, writeMin, writeMax);

	//-------Cache Removal after Write

	if(strcmp("mountdir", directory) == 0)//need to remove files, and recreate in NAS only so we aren't reading after write to cache
	{
		char* fileName;
		for(int recreate_index = 0; recreate_index < numberofFiles; recreate_index++)
		{	
			char deleteCall[100];
			strcpy(deleteCall, "rm ");
			strcat(deleteCall, (const char*)fileNames[recreate_index]);
			system(deleteCall);//remove file from cache and NAS through mountdir

			fileName = malloc(100);//see below snprintf comment for length of string 27
			strcpy(fileName, "/home/wjb24/ECE566/ece566-project/example/nasdir/benchFile");

			int numLength = snprintf( NULL, 0, "%d", recreate_index);
			char* fileNum = malloc(numLength + 1);
			snprintf(fileNum, numLength + 1, "%d", recreate_index);
			strcat(fileName, fileNum);
			free(fileNum);
			
			strcat(fileName, ".wjb24");

			FILE* reFile = fopen(fileName, "w+");
			free((void*) fileName);
			fwrite(dataSource, fileSize*1024, 1, reFile);
			fclose(reFile);
		}
	}

	//---------Read Testing---------

	//fprintf(stderr, "Clearing OS cache for reads...\n");
	system("sync;sudo sh -c \"echo 1 > /proc/sys/vm/drop_caches\"");

	double readSpeeds[numberofFiles];
	for(int read_index = 0; read_index < numberofFiles; read_index++)
	{
		filePointers[read_index] = fopen((const char *)fileNames[read_index], "r");
		if(filePointers[read_index] == NULL)
		{
			perror("Error opening file: ");
		}

		size_t readRes;
		clock_gettime(CLOCK_MONOTONIC, &timerStart);

		readRes = fread(dataSource, fileSize*1024, 1, filePointers[read_index]);

		clock_gettime(CLOCK_MONOTONIC, &timerEnd);
    	time_taken = (timerEnd.tv_sec - timerStart.tv_sec) * 1e9; 
    	time_taken = (time_taken + (timerEnd.tv_nsec - timerStart.tv_nsec)) * 1e-9; 

		//fprintf(stderr, "Read to file %d took %f milliseconds\n", read_index, cpuTimeUsed*1000);
		readSpeeds[read_index] = ((double)fileSize)/time_taken;

      	fclose(filePointers[read_index]);
	}

	double readAverage, readMin, readMax;
	readAverage = 0;
	readMin = readSpeeds[0];
	readMax = readSpeeds[0];
	for(int read_speed_index = 0; read_speed_index < numberofFiles; read_speed_index++)
	{
		readAverage = readAverage + (readSpeeds[read_speed_index]/numberofFiles);
		if(readMin > readSpeeds[read_speed_index])
		{
			readMin = readSpeeds[read_speed_index];
		}
		if(readMax < readSpeeds[read_speed_index])
		{
			readMax = readSpeeds[read_speed_index];
		}
	}

	fprintf(stderr, "Read average: %f (Kb/s) | Read minimum: %f (Kb/s) | Read maximum: %f (Kb/s)\n", readAverage, readMin, readMax);


	//--------Re-Read Testing----------

	//fprintf(stderr, "Clearing OS cache for Re-Reads...\n");
	system("sync;sudo sh -c \"echo 1 > /proc/sys/vm/drop_caches\"");

	double reReadSpeeds[numberofFiles];
	for(int reRead_index = 0; reRead_index < numberofFiles; reRead_index++)
	{
		filePointers[reRead_index] = fopen((const char *)fileNames[reRead_index], "r");
		if(filePointers[reRead_index] == NULL)
		{
			perror("Error opening file: ");
		}

		size_t reReadRes;
		clock_gettime(CLOCK_MONOTONIC, &timerStart);

		reReadRes = fread(dataSource, fileSize*1024, 1, filePointers[reRead_index]);

		clock_gettime(CLOCK_MONOTONIC, &timerEnd);
    	time_taken = (timerEnd.tv_sec - timerStart.tv_sec) * 1e9; 
    	time_taken = (time_taken + (timerEnd.tv_nsec - timerStart.tv_nsec)) * 1e-9; 

		//fprintf(stderr, "reRead to file %d took %f milliseconds\n", reRead_index, cpuTimeUsed*1000);
		reReadSpeeds[reRead_index] = ((double)fileSize)/time_taken;

      	fclose(filePointers[reRead_index]);
	}

	double reReadAverage, reReadMin, reReadMax;
	reReadAverage = 0;
	reReadMin = reReadSpeeds[0];
	reReadMax = reReadSpeeds[0];
	for(int reRead_speed_index = 0; reRead_speed_index < numberofFiles; reRead_speed_index++)
	{
		reReadAverage = reReadAverage + (reReadSpeeds[reRead_speed_index]/numberofFiles);
		if(reReadMin > reReadSpeeds[reRead_speed_index])
		{
			reReadMin = reReadSpeeds[reRead_speed_index];
		}
		if(reReadMax < reReadSpeeds[reRead_speed_index])
		{
			reReadMax = reReadSpeeds[reRead_speed_index];
		}
	}

	fprintf(stderr, "Re-Read average: %f (Kb/s) | Re-Read minimum: %f (Kb/s) | Re-Read maximum: %f (Kb/s)\n", reReadAverage, reReadMin, reReadMax);

	return 0;
}