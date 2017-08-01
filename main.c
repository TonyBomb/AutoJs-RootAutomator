#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/ioctl.h>
//#include <linux/input.h> // this does not compile
#include <errno.h>
// from <linux/input.h>
struct input_event {
	struct timeval time;
	__u16 type;
	__u16 code;
	__s32 value;
};
#define EVIOCGVERSION		_IOR('E', 0x01, int)			/* get driver version */
#define EVIOCGID		_IOR('E', 0x02, struct input_id)	/* get device ID */
#define EVIOCGKEYCODE		_IOR('E', 0x04, int[2])			/* get keycode */
#define EVIOCSKEYCODE		_IOW('E', 0x04, int[2])			/* set keycode */
#define EVIOCGNAME(len)		_IOC(_IOC_READ, 'E', 0x06, len)		/* get device name */
#define EVIOCGPHYS(len)		_IOC(_IOC_READ, 'E', 0x07, len)		/* get physical location */
#define EVIOCGUNIQ(len)		_IOC(_IOC_READ, 'E', 0x08, len)		/* get unique identifier */
#define EVIOCGKEY(len)		_IOC(_IOC_READ, 'E', 0x18, len)		/* get global keystate */
#define EVIOCGLED(len)		_IOC(_IOC_READ, 'E', 0x19, len)		/* get all LEDs */
#define EVIOCGSND(len)		_IOC(_IOC_READ, 'E', 0x1a, len)		/* get all sounds status */
#define EVIOCGSW(len)		_IOC(_IOC_READ, 'E', 0x1b, len)		/* get all switch states */
#define EVIOCGBIT(ev,len)	_IOC(_IOC_READ, 'E', 0x20 + ev, len)	/* get event bits */
#define EVIOCGABS(abs)		_IOR('E', 0x40 + abs, struct input_absinfo)		/* get abs value/limits */
#define EVIOCSABS(abs)		_IOW('E', 0xc0 + abs, struct input_absinfo)		/* set abs value/limits */
#define EVIOCSFF		_IOC(_IOC_WRITE, 'E', 0x80, sizeof(struct ff_effect))	/* send a force effect to a force feedback device */
#define EVIOCRMFF		_IOW('E', 0x81, int)			/* Erase a force effect */
#define EVIOCGEFFECTS		_IOR('E', 0x84, int)			/* Report number of effects playable at the same time */
#define EVIOCGRAB		_IOW('E', 0x90, int)			/* Grab/Release device */
// end <linux/input.h>

#define DEBUG 0

#define ERROR_DATA_FORMAT 1
#define ERROR_IO 2

#define DATA_TYPE_SLEEP 0
#define DATA_TYPE_EVENT 1
#define DATA_HANDLER_COUNT 2

#define SIZE_TIMEVAL 8
#define SIZE_EVENT 8

typedef struct Args {
	char* path;
	char* pathOut;
} Args;

typedef void (*DataHandler)(FILE*, int);

void parseArgs(int argc, char* argv[], int fromIndex, Args *args);
char *getOrDefault(char *argv[], int i, int argc, char *defaultValue);
int openDeviceOrExit(char *path, int flags);
FILE *openFileOrExit(char *path, char *mode);
void writeInputEvents(char*, char*);

void handleDataTypeSleep(FILE*, int);
void handleDataTypeEvent(FILE*, int);

static DataHandler dataHandlers[DATA_HANDLER_COUNT] = {
	handleDataTypeSleep,
	handleDataTypeEvent
};
static struct input_event event;
static unsigned char buffer[SIZE_EVENT];




int main(int argc, char *argv[])
{
    printf("Parsing args...\n");
    Args args;
    parseArgs(argc, argv, 1, &args);
    printf("pathIn = %s, pathOut = %s\n", args.path, args.pathOut);
    printf("writing input event...\n");
    writeInputEvents(args.path, args.pathOut);;
	printf("Complete!\n");
    return 0;                       
}

void parseArgs(int argc, char* argv[], int fromIndex, Args *args){
	int i = fromIndex;
	args->path = getOrDefault(argv, i++, argc, "/sdcard/input.autojs");
	args->pathOut = getOrDefault(argv, i++, argc, "/dev/input/event1");
}

char *getOrDefault(char *argv[], int i, int argc, char *defaultValue){
	if(i >= argc){
		return defaultValue;
	}
	return argv[i];
}

int openDeviceOrExit(char *path, int flags){
	int f = open(path, flags);
	if(f < 0){
		printf("Error open '%s' with flags '%d'\n", path, flags);
		exit(ERROR_IO);
	}
	return f;
}


FILE *openFileOrExit(char *path, char *mode){
	FILE *f = fopen(path, mode);
	if(f == NULL){
		printf("Error open '%s' with mode '%s\n", path, mode);
		exit(ERROR_IO);
	}
	return f;
}

void writeInputEvents(char *pathIn, char *pathOut){
    memset(&event, 0, sizeof(event));
	FILE *fi = openFileOrExit(pathIn, "rb");
	int fo = openDeviceOrExit(pathOut, O_RDWR);
	while(1){
		unsigned char dataType = fgetc(fi);
		if(feof(fi)){
			break;
		}
		if(dataType >= DATA_HANDLER_COUNT){
			printf("Error: unknown data type: %u\n", dataType);
			exit(ERROR_DATA_FORMAT);
		}
		#if DEBUG
		printf("handle dataType: %d\n", dataType);
		#endif
		dataHandlers[dataType](fi, fo);
	}
	fclose(fi);
	close(fo);
}


void handleDataTypeSleep(FILE *fi, int fo){
	if(fread(buffer, 4, 1, fi) != 1){
		printf("handleDataTypeSleep: Format error.\n");
		exit(ERROR_DATA_FORMAT);
	}
	int n = buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
	#if DEBUG
	printf("sleep(%d) %x\n", n, n);
	#endif
	usleep(n * 1000L);
}

void handleDataTypeEvent(FILE *fi, int fo){
	if(fread(buffer, SIZE_EVENT, 1, fi) != 1){
		printf("handleDataTypeEvent: Format error.\n");
		exit(ERROR_DATA_FORMAT);
	}
	event.type = buffer[1] | (buffer[0] << 8);
	event.code = buffer[3] | (buffer[2] << 8);
	event.value = buffer[7] | (buffer[6] << 8) | (buffer[5] << 16) | (buffer[4] << 24);
	#if DEBUG
	printf("handleDataTypeEvent: type=%d code=%d value=%d\n", event.type, event.code, event.value);
	#endif
	if(write(fo, &event, sizeof(event)) != sizeof(event)){
		printf("handleDataTypeEvent: Write error.\n");
		exit(ERROR_IO);
	}
}
