#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>  
#include <sys/wait.h>  
#include <sys/types.h> 
#include "linux_input.h"
#include "linux_input_event_util.h"

#define DEBUG 0

#define VERSION 1

#define ERROR_DATA_FORMAT 1
#define ERROR_IO 2
#define ERROR_UNSUPPORTED_VERSION 3
#define ERROR_ARGUEMENT 4 
#define ERROR_INTR 5

#define DATA_TYPE_SLEEP 0
#define DATA_TYPE_EVENT 1
#define DATA_TYPE_EVENT_SYNC 2
#define DATA_TYPE_EVENT_TOUCH_X 3
#define DATA_TYPE_EVENT_TOUCH_Y 4
#define DATA_HANDLER_COUNT 5

#define SIZE_TIMEVAL 8
#define SIZE_EVENT 8


typedef void (*DataHandler)(FILE*, int);

static void parseArgs(int argc, char* argv[], int fromIndex);
static void scanInput();
static void readFileHeader(FILE *fi);
static void execute();

inline static int readInt(FILE*, char*);
static int openDeviceOrExit(char *path, int flags);
static FILE *openFileOrExit(char *path, char *mode);

inline static int scaleX(int x);
inline static int scaleY(int y);

static void handleDataTypeSleep(FILE*, int);
static void handleDataTypeEvent(FILE*, int);
static void handleDataTypeEventSync(FILE*, int);
static void handleDataTypeEventTouchX(FILE*, int);
static void handleDataTypeEventTouchY(FILE*, int);

static void handleSignalInt();
static void __exit(int i);

static DataHandler dataHandlers[DATA_HANDLER_COUNT] = {
	handleDataTypeSleep,
	handleDataTypeEvent,
	handleDataTypeEventSync,
	handleDataTypeEventTouchX,
	handleDataTypeEventTouchY
};
static char *pathIn = "";
static int deviceFd;
static struct input_event event;
static unsigned char buffer[SIZE_EVENT];
static int recordWidth, recordHeight, currentWidth = 0, currentHeight = 0;


int main(int argc, char *argv[])
{
	signal(SIGINT, handleSignalInt);   
    parseArgs(argc, argv, 1);
    #if DEBUG
    printf("pathIn = %s, deviceFd = %d, currentWidth: %d, currentHeight: %d\n", pathIn, deviceFd, currentWidth, currentWidth);
    #endif
    memset(&event, 0, sizeof(event));
    if(strlen(pathIn) == 0){
    	scanInput();
    }else{
	    execute();
    }
    printf("Finish!\n");
    return 0;                       
}

static void parseArgs(int argc, char* argv[], int fromIndex){
	int i = fromIndex;
	char *nameOrPath = NULL;
	while(i < argc){
		char *arg = argv[i++];
		if(arg[0] != '-'){
			pathIn = arg;
			continue;
		}
		switch(arg[1]){
			case 'w':
				currentWidth = atoi(argv[i++]);
				continue;
			case 'h':
				currentHeight = atoi(argv[i++]);
				continue;
			case 'd':
				nameOrPath = argv[i++];
				continue;
			default:
				fprintf(stderr, "unknown option: %s\n", arg);
				__exit(ERROR_ARGUEMENT);
		}
	}
	if(nameOrPath == NULL){
		fprintf(stderr, "option -d should be set\n");
		__exit(ERROR_ARGUEMENT);
	}
	if(nameOrPath[0] != '/'){
		#if DEBUG
		printf("open_device_by_name: %s\n", nameOrPath);
		#endif
		deviceFd = open_device_by_name("/dev/input", nameOrPath);
	}else{
		deviceFd = openDeviceOrExit(nameOrPath, O_RDWR);
	}
	if(deviceFd < 0){
		fprintf(stderr, "cannot open device: %s\n", nameOrPath);
		__exit(ERROR_IO);
	}
}

static int openDeviceOrExit(char *path, int flags){
	int f = open(path, flags);
	if(f < 0){
		fprintf(stderr, "Error open '%s' with flags %x\n", path, flags);
		__exit(ERROR_IO);
	}
	return f;
}

static FILE *openFileOrExit(char *path, char *mode){
	FILE *f = fopen(path, mode);
	if(f == NULL){
		fprintf(stderr, "Error open '%s' with mode '%s\n", path, mode);
		__exit(ERROR_IO);
	}
	return f;
}

static void execute(){
	FILE *fi = openFileOrExit(pathIn, "rb");
	readFileHeader(fi);
	int fo = deviceFd;
	while(1){
		unsigned char dataType = fgetc(fi);
		if(feof(fi)){
			break;
		}
		if(dataType >= DATA_HANDLER_COUNT){
			fprintf(stderr, "Error: unknown data type: %u\n", dataType);
			__exit(ERROR_DATA_FORMAT);
		}
		#if DEBUG
		printf("handle dataType: %d\n", dataType);
		#endif
		dataHandlers[dataType](fi, fo);
	}
	fclose(fi);
	close(fo);
}


static void readFileHeader(FILE *fi){
	if(readInt(fi, "FileHeader") != 0x00B87B6D){
		fprintf(stderr, "FileHeader Format error.\n");
		__exit(ERROR_DATA_FORMAT);
	}
	int v = readInt(fi, "Version");
	#if DEBUG
	printf(".auto file version: %d\n", v);
	#endif
	if(v > VERSION){
		fprintf(stderr, "Only support .auto file version <= %d\n", VERSION);
		__exit(ERROR_UNSUPPORTED_VERSION);
	}
	recordWidth = readInt(fi, "recordWidth");
	recordHeight = readInt(fi, "recordHeight");
	#if DEBUG
	printf("recordWidth: %d, recordHeight: %d\n", recordWidth, recordHeight);
	#endif
	fseek(fi, 240, SEEK_CUR);
}


static inline writeEvent(){
	if(write(deviceFd, &event, sizeof(event)) != sizeof(event)){
		fprintf(stderr, "Error write event.\n");
		__exit(ERROR_IO);
	}
}

static void handleDataTypeSleep(FILE *fi, int fo){
	int n = readInt(fi, "handleDataTypeSleep");
	#if DEBUG
	printf("sleep(%d)\n", n);
	#endif
	usleep(n * 1000L);
}

static void handleDataTypeEvent(FILE *fi, int fo){
	if(fread(buffer, SIZE_EVENT, 1, fi) != 1){
		fprintf(stderr, "handleDataTypeEvent: Format error.\n");
		__exit(ERROR_DATA_FORMAT);
	}
	event.type = buffer[1] | (buffer[0] << 8);
	event.code = buffer[3] | (buffer[2] << 8);
	event.value = buffer[7] | (buffer[6] << 8) | (buffer[5] << 16) | (buffer[4] << 24);
	if(event.type == 3){
		if(event.code == 0x35){
			event.value = scaleX(event.value);
		}else if(event.code == 0x36){
			event.value = scaleY(event.value);
		}
	}
	#if DEBUG
	printf("handleDataTypeEvent: type=%d code=%d value=%d\n", event.type, event.code, event.value);
	#endif
	writeEvent();
}

static void handleDataTypeEventSync(FILE *fi, int fo){
	#if DEBUG
	printf("write sync report\n");
	#endif
	event.type = 0;
	event.code = 0;
	event.value = 0;
	writeEvent();
}

static void handleDataTypeEventTouchX(FILE *fi, int fo){
	event.type = 3;
	event.code = 0x35;
	event.value = scaleX(readInt(fi, "Read touch x"));
	#if DEBUG
	printf("write touch x: %d\n", event.value);
	#endif
	writeEvent();
}

static void handleDataTypeEventTouchY(FILE *fi, int fo){
	event.type = 3;
	event.code = 0x36;
	event.value = scaleY(readInt(fi, "Read touch y"));
	#if DEBUG
	printf("write touch y: %d\n", event.value);
	#endif
	writeEvent();
}

inline static int readInt(FILE *fi, char *msg){
	if(fread(buffer, 4, 1, fi) != 1){
		fprintf(stderr, "%s: Format error.\n", msg);
		__exit(ERROR_DATA_FORMAT);
	}
	return buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
}

inline static int scaleX(int x){
	if(currentWidth == 0 || currentWidth == recordWidth){
		return x;
	}
	return x * currentWidth / recordWidth;
}

inline static int scaleY(int y){
	if(currentHeight == 0 || currentHeight == recordHeight){
		return y;
	}
	return y * currentHeight / recordWidth;
}

static void scanInput(){
	while(1){
		if(scanf("%hu %hu %d", &event.type, &event.code, &event.value) != 3){
			fprintf(stderr, "Data format error.\n");
			__exit(ERROR_DATA_FORMAT);
		}
		if(event.type == 0xffff && event.code == 0xffff && event.value == 0xefefefef){
			__exit(0);
		}
		if(write(deviceFd, &event, sizeof(event)) != sizeof(event)){
			fprintf(stderr, "Error write event.\n");
			__exit(ERROR_IO);
		}
	}
}

static void __exit(int i){
	printf("Exit code: %d\n", i);
	printf("Error!\n");
	exit(i);
}

static void handleSignalInt(int sig){
	if(deviceFd >= 0){
		close(deviceFd);
	}
	__exit(ERROR_INTR);
}