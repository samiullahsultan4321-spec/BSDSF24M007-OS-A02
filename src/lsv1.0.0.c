// /*
// * Programming Assignment 02: lsv1.0.0
// * This is the source file of version 1.0.0
// * Read the write-up of the assignment to add the features to this base version
// * Usage:
// *       $ lsv1.0.0 
// *       % lsv1.0.0  /home
// *       $ lsv1.0.0  /home/kali/   /etc/
// */
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <dirent.h>
// #include <string.h>
// #include <errno.h>
// #include <sys/stat.h>

// extern int errno;

// void do_ls(const char *dir);

// int main(int argc, char const *argv[])
// {
//     if (argc == 1)
//     {
//         do_ls(".");
//     }
//     else
//     {
//         for (int i = 1; i < argc; i++)
//         {
//             printf("Directory listing of %s : \n", argv[i]);
//             do_ls(argv[i]);
// 	    puts("");
//         }
//     }
//     return 0;
// }

// void do_ls(const char *dir)
// {
//     struct dirent *entry;
//     DIR *dp = opendir(dir);
//     if (dp == NULL)
//     {
//         fprintf(stderr, "Cannot open directory : %s\n", dir);
//         return;
//     }
//     errno = 0;
//     while ((entry = readdir(dp)) != NULL)
//     {
//         if (entry->d_name[0] == '.')
//             continue;
//         printf("%s\n", entry->d_name);
//     }

//     if (errno != 0)
//     {
//         perror("readdir failed");
//     }

//     closedir(dp);
// }


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>

void do_ls(const char *dir);
void do_ls_long(const char *dir);
void print_long_entry(const char *dir, const char *name);
void format_mode(mode_t mode, char out[11]);

int main(int argc, char *argv[])
{
    int long_format = 0;
    int opt;

    /* Task 2: detect -l with getopt() */
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_format = 1;
            break;
        default: /* '?' : unknown option */
            fprintf(stderr, "Usage: %s [-l] [directory...]\n", argv[0]);
            return 1;
        }
    }

    /* After getopt, optind is the index of the first non-option argument */
    if (optind >= argc)
    {
        if (long_format)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            if (long_format)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);
            puts("");
        }
    }
    return 0;
}

/* Original simple listing: one name per line, hidden files skipped */
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        printf("%s\n", entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/* Task 3: long listing of a directory */
void do_ls_long(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        print_long_entry(dir, entry->d_name);
    }

    if (errno != 0)
    {
        perror("readdir failed");
    }

    closedir(dp);
}

/* Print one line:  perms links owner group size mtime name [-> target] */
void print_long_entry(const char *dir, const char *name)
{
    char path[PATH_MAX];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s", dir, name);

    /* lstat() so symbolic links are described, not followed */
    if (lstat(path, &st) == -1)
    {
        perror(path);
        return;
    }

    char perms[11];
    format_mode(st.st_mode, perms);

    /* Resolve numeric IDs to names; fall back to the number if unknown */
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);
    char uid_str[32], gid_str[32];
    if (pw == NULL)
        snprintf(uid_str, sizeof(uid_str), "%u", (unsigned)st.st_uid);
    if (gr == NULL)
        snprintf(gid_str, sizeof(gid_str), "%u", (unsigned)st.st_gid);

    /* ctime() returns "Wed Jun 30 21:49:08 1993\n"; skip the weekday
       (4 chars) and keep "Jun 30 21:49" (12 chars) like the real ls */
    char *timestr = ctime(&st.st_mtime);

    printf("%s %3lu %-8s %-8s %8lld %.12s %s",
           perms,
           (unsigned long)st.st_nlink,
           pw ? pw->pw_name : uid_str,
           gr ? gr->gr_name : gid_str,
           (long long)st.st_size,
           timestr ? timestr + 4 : "????????????",
           name);

    /* For symlinks, also show where they point */
    if (S_ISLNK(st.st_mode))
    {
        char target[PATH_MAX];
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n != -1)
        {
            target[n] = '\0';
            printf(" -> %s", target);
        }
    }
    putchar('\n');
}

/* Convert st_mode into a string like "-rwxr-xr-x" (handles setuid,
   setgid and sticky bits). out must hold 11 bytes. */
void format_mode(mode_t mode, char out[11])
{
    /* File type */
    if (S_ISREG(mode))
        out[0] = '-';
    else if (S_ISDIR(mode))
        out[0] = 'd';
    else if (S_ISLNK(mode))
        out[0] = 'l';
    else if (S_ISCHR(mode))
        out[0] = 'c';
    else if (S_ISBLK(mode))
        out[0] = 'b';
    else if (S_ISFIFO(mode))
        out[0] = 'p';
    else if (S_ISSOCK(mode))
        out[0] = 's';
    else
        out[0] = '?';

    /* Owner */
    out[1] = (mode & S_IRUSR) ? 'r' : '-';
    out[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID)
        out[3] = (mode & S_IXUSR) ? 's' : 'S';
    else
        out[3] = (mode & S_IXUSR) ? 'x' : '-';

    /* Group */
    out[4] = (mode & S_IRGRP) ? 'r' : '-';
    out[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID)
        out[6] = (mode & S_IXGRP) ? 's' : 'S';
    else
        out[6] = (mode & S_IXGRP) ? 'x' : '-';

    /* Others */
    out[7] = (mode & S_IROTH) ? 'r' : '-';
    out[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX)
        out[9] = (mode & S_IXOTH) ? 't' : 'T';
    else
        out[9] = (mode & S_IXOTH) ? 'x' : '-';

    out[10] = '\0';
}