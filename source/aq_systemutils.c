/*
 * Copyright (c) 2017 Shaun Feakes - All rights reserved
 *
 * You may use redistribute and/or modify this code under the terms of
 * the GNU General Public License version 2 as published by the
 * Free Software Foundation. For the terms of this license,
 * see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 *  https://github.com/sfeakes/aqualinkd
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/wait.h>

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

bool copy_file(const char *source_path, const char *destination_path)
{

    bool ro_root = remount_root_ro(false);

    FILE *source_file = fopen(source_path, "rb");
    if (source_file == NULL)
    {
        LOG(AQUA_LOG, LOG_ERR, "Error opening source file: %s\n", source_path);
        remount_root_ro(ro_root);
        return false;
    }

    FILE *destination_file = fopen(destination_path, "wb");
    if (destination_file == NULL)
    {
        LOG(AQUA_LOG, LOG_ERR, "Error opening source file: %s\n", destination_path);
        fclose(source_file);
        remount_root_ro(ro_root);
        return false;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0)
    {
        if (fwrite(buffer, 1, bytes_read, destination_file) != bytes_read)
        {
            LOG(AQUA_LOG, LOG_ERR, "Error writing to destination file: %s\n", destination_path);
            fclose(source_file);
            fclose(destination_file);
            remount_root_ro(ro_root);
            return false;
        }
    }

    if (ferror(source_file))
    {
        LOG(AQUA_LOG, LOG_ERR, "Error reading from source: %s\n", source_path);
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

bool run_aqualinkd_upgrade(bool onlycheck)
{
    int pipe_curl_to_bash[2];
    pid_t pid_curl, pid_bash;
    //char *curl_args[] = {"curl", "-fsSl", "http://tiger/scratch/remote_install.sh", NULL};
    //char *curl_args[] = {"curl", "-fsSl", "-H",  "Accept: application/vnd.github.raw", "https://api.github.com/repos/sfeakes/AqualinkD/contents/release/remote_install.sh", NULL};
    char *curl_args[] = {"curl", "-fsSl", "-H",  "Accept: application/vnd.github.raw", "https://api.github.com/repos/AqualinkD/AqualinkD/contents/release/remote_install.sh", NULL};
    char *bash_args[] = {"bash", "-s", "--", "check", NULL};
    int status_curl, status_bash;

    if (!onlycheck) {
        bash_args[3] = NULL;
    }

    if (pipe(pipe_curl_to_bash) == -1)
    {
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, opening pipe");
        return false;
    }

    // Fork for curl
    pid_curl = fork();
    if (pid_curl == -1)
    {
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, fork (curl)");
        return false;
    }

    if (pid_curl == 0)
    { // Child process (curl)
        close(pipe_curl_to_bash[0]);
        dup2(pipe_curl_to_bash[1], STDOUT_FILENO);
        close(pipe_curl_to_bash[1]);
        execvp("curl", curl_args);
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, execvp (curl)");
        return false;
    }

    // Fork for bash
    pid_bash = fork();
    if (pid_bash == -1)
    {
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, fork (bash)");
        return false;
    }

    if (pid_bash == 0)
    { // Child process (bash)
        close(pipe_curl_to_bash[1]);
        dup2(pipe_curl_to_bash[0], STDIN_FILENO);
        close(pipe_curl_to_bash[0]);
        execvp("bash", bash_args);
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, execvp (bash)");
        return false;
    }

    // Parent process
    close(pipe_curl_to_bash[0]);
    close(pipe_curl_to_bash[1]);

    // Wait for curl and get its exit status
    if (waitpid(pid_curl, &status_curl, 0) == -1)
    {
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, waitpid (curl)");
        return false;
    }

    // Wait for bash and get its exit status
    if (waitpid(pid_bash, &status_bash, 0) == -1)
    {
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, waitpid (bash)");
        return false;
    }

    // Check the exit status of curl
    if (WIFEXITED(status_curl))
    {
        //printf("curl exited with status: %d\n", WEXITSTATUS(status_curl));
        if (WEXITSTATUS(status_curl) != 0) {
            LOG(AQUA_LOG, LOG_ERR,"Upgrade error, curl exited with status: %d\n", WEXITSTATUS(status_curl));
            return false;
        }
    }
    else if (WIFSIGNALED(status_curl))
    {
        //printf("curl terminated by signal: %d\n", WTERMSIG(status_curl));
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, curl terminated by signal: %d\n", WTERMSIG(status_curl));
        return false;
    }

    // Check the exit status of bash
    if (WIFEXITED(status_bash))
    {
        //printf("bash exited with status: %d\n", WEXITSTATUS(status_bash));
        if (WEXITSTATUS(status_bash) != 0) {
            LOG(AQUA_LOG, LOG_ERR,"Upgrade error, bash exited with status: %d\n", WEXITSTATUS(status_bash));
            return false;
        }
    }
    else if (WIFSIGNALED(status_bash))
    {
        //printf("bash terminated by signal: %d\n", WTERMSIG(status_bash));
        LOG(AQUA_LOG, LOG_ERR,"Upgrade error, bash terminated by signal: %d\n", WTERMSIG(status_bash));
        return false;
    }

    //printf("Command execution complete.\n");
    LOG(AQUA_LOG, LOG_NOTICE, "AqualinkD is upgrading!");
    return true;
}