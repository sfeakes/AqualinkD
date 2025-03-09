
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/statvfs.h>

#include "aqualink.h"
#include "aq_scheduler.h"

bool remount_root_ro(bool readonly)
{

#ifdef AQ_CONTAINER
    // In container this is pointless
    return false;
#endif

    if (readonly)
    {
        LOG(AQUA_LOG, LOG_INFO, "reMounting root RO\n");
        mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
        return true;
    }
    else
    {
        struct statvfs fsinfo;
        statvfs("/", &fsinfo);
        if ((fsinfo.f_flag & ST_RDONLY) == 0) // We are readwrite, ignore
            return false;

        LOG(AQUA_LOG, LOG_INFO, "reMounting root RW\n");
        mount(NULL, "/", NULL, MS_REMOUNT, NULL);
        return true;
    }
}

FILE *aq_open_file(char *filename, bool *ro_root, bool *created_file)
{
    FILE *fp;

    *ro_root = remount_root_ro(false);

    if (access(filename, F_OK) == 0)
    {
        *created_file = true;
    }

    fp = fopen(filename, "w");
    if (fp == NULL)
    {
        remount_root_ro(*ro_root);
    }

    return fp;
}

bool aq_close_file(FILE *file, bool ro_root)
{
    if (file != NULL)
      fclose(file);

    return remount_root_ro(ro_root);
}

#define BUFFER_SIZE 4096

bool copy_file(const char *source_path, const char *destination_path) {


    bool ro_root = remount_root_ro(false);

    FILE *source_file = fopen(source_path, "rb");
    if (source_file == NULL) {
        LOG(AQUA_LOG,LOG_ERR, "Error opening source file: %s\n",source_path);
        remount_root_ro(ro_root);
        return false;
    }

    FILE *destination_file = fopen(destination_path, "wb");
    if (destination_file == NULL) {
        LOG(AQUA_LOG,LOG_ERR, "Error opening source file: %s\n",destination_path);
        fclose(source_file);
        remount_root_ro(ro_root);
        return false;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        if (fwrite(buffer, 1, bytes_read, destination_file) != bytes_read) {
            LOG(AQUA_LOG,LOG_ERR, "Error writing to destination file: %s\n",destination_path);
            fclose(source_file);
            fclose(destination_file);
            remount_root_ro(ro_root);
            return false;
        }
    }

    if (ferror(source_file)) {
       LOG(AQUA_LOG,LOG_ERR, "Error reading from source: %s\n",source_path);
       fclose(source_file);
       fclose(destination_file);
       remount_root_ro(ro_root);
       return false;
    }
    
    fclose(source_file);
    fclose(destination_file);
    remount_root_ro(ro_root);
    return true;
}