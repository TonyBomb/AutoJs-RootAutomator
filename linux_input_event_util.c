#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/limits.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <errno.h>
#include "linux_input_event_util.h"
static struct pollfd *ufds = NULL;
static char **device_names = NULL;
static int nfds;

static int device_name_equals(int deviceFd, const char *device_name)
{
    char name[80];
    name[sizeof(name) - 1] = '\0';
    if(ioctl(deviceFd, EVIOCGNAME(sizeof(name) - 1), &name) < 1) {
        fprintf(stderr, "could not get device name for %d, %s, %s\n", deviceFd, device_name, strerror(errno));
        return -1;
    }
    //printf("device name: %s\n", name);
    if(strcmp(name, device_name) == 0){
        return 1;
    }
    return 0;
}

int open_device_by_name(const char *dirname, char *device_name)
{
    ufds = calloc(1, sizeof(ufds[0]));
    nfds = 1;
    char devname[PATH_MAX];
    char *filename;
    DIR *dir;
    struct dirent *de;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;
    strcpy(devname, dirname);
    filename = devname + strlen(devname);
    *filename++ = '/';
    while((de = readdir(dir))) {
        if(de->d_name[0] == '.' &&
           (de->d_name[1] == '\0' ||
            (de->d_name[1] == '.' && de->d_name[2] == '\0')))
            continue;
        strcpy(filename, de->d_name);
        int fd = open(devname, O_RDWR);
        if(fd < 0) {
            fprintf(stderr, "could not open %s, %s\n", devname, strerror(errno));
            return -1;
        }
        //printf("open device: %s\n", devname);
        if(device_name_equals(fd, device_name) == 1){
            return fd;
        }else{
            close(fd);
        }
    }
    closedir(dir);
    return -1;
}