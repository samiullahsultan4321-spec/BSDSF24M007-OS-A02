/*
 * Programming Assignment 02: lsv1.2.0
 * Adds: -l (long listing format) using getopt()           (v1.1.0)
 *       Default output in columns, "down then across"      (v1.2.0)
 *
 * Usage:
 *       $ lsv1.2.0
 *       $ lsv1.2.0 /home
 *       $ lsv1.2.0 -l
 *       $ lsv1.2.0 -l /home/kali/ /etc/
 */
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
#include <sys/ioctl.h>
#include <stdlib.h>  // qsort()
#include <string.h>  // strcmp()
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

#define COLOR_RESET    "\033[0m"
#define COLOR_BLUE     "\033[0;34m"
#define COLOR_GREEN    "\033[0;32m"
#define COLOR_RED      "\033[0;31m"
#define COLOR_PINK     "\033[0;35m"
#define COLOR_REVERSE  "\033[7m"
#define DEFAULT_TERM_WIDTH 80 /* fallback if ioctl() is unavailable */
#define COLUMN_SPACING 2      /* spaces between columns */

enum DisplayMode
{
    MODE_VERTICAL,
    MODE_HORIZONTAL,
    MODE_LONG
};

enum DisplayMode display_mode = MODE_VERTICAL;
void print_horizontal(const char* dir,char **names, size_t count, size_t max_len);
void do_ls(const char *dir);
void do_ls_long(const char *dir);
void print_long_entry(const char *dir, const char *name);
void format_mode(mode_t mode, char out[11]);
int get_terminal_width(void);
void print_columns(const char* dir,char **names, size_t count, size_t max_len);


int main(int argc, char *argv[])
{
    int opt;

    /* Parse command-line options */
    while ((opt = getopt(argc, argv, "lx")) != -1)
    {
        switch (opt)
        {
        case 'l':
            display_mode = MODE_LONG;
            break;

        case 'x':
            display_mode = MODE_HORIZONTAL;
            break;

        default:
            fprintf(stderr, "Usage: %s [-l | -x] [directory...]\n",
                    argv[0]);
            return 1;
        }
    }

    /* Use current directory if none is provided */
    if (optind >= argc)
    {
        if (display_mode == MODE_LONG)
            do_ls_long(".");
        else
            do_ls(".");
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s:\n", argv[i]);

            if (display_mode == MODE_LONG)
                do_ls_long(argv[i]);
            else
                do_ls(argv[i]);

            puts("");
        }
    }

    return 0;
}

int compare_names(const void *a, const void *b)
{
    const char *name_a = *(const char **)a;
    const char *name_b = *(const char **)b;

    return strcmp(name_a, name_b);
}

/* Task 2 (v1.2.0): default listing.
 * Read ALL entries into a dynamically allocated array of strings first,
 * tracking the longest filename, then hand them to print_columns(). */

 int is_tarball(const char *name)
{
    size_t len = strlen(name);

    return (len >= 4 && strcmp(name + len - 4, ".tar") == 0) ||
           (len >= 3 && strcmp(name + len - 3, ".gz") == 0) ||
           (len >= 4 && strcmp(name + len - 4, ".zip") == 0);
}

void print_colored_name(const char *name, mode_t mode)
{
    const char *color = COLOR_RESET;

    if (S_ISLNK(mode))
        color = COLOR_PINK;
    else if (S_ISDIR(mode))
        color = COLOR_BLUE;
    else if (S_ISCHR(mode) || S_ISBLK(mode) ||
             S_ISFIFO(mode) || S_ISSOCK(mode))
        color = COLOR_REVERSE;
    else if (is_tarball(name))
        color = COLOR_RED;
    else if (mode & (S_IXUSR | S_IXGRP | S_IXOTH))
        color = COLOR_GREEN;

    printf("%s%s%s", color, name, COLOR_RESET);
}

void print_colored_entry(const char *dir, const char *name)
{
    char path[PATH_MAX];
    struct stat st;

    snprintf(path, sizeof(path), "%s/%s", dir, name);

    if (lstat(path, &st) == -1)
    {
        perror(path);
        printf("%s", name);
        return;
    }

    print_colored_name(name, st.st_mode);
}
void do_ls(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    char **names = NULL;   /* dynamic array of filenames */
    size_t count = 0;      /* number of names stored */
    size_t capacity = 0;   /* number of slots allocated */
    size_t max_len = 0;    /* length of the longest filename */

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        /* grow the array when it is full (doubling strategy) */
        if (count == capacity)
        {
            size_t new_cap = (capacity == 0) ? 16 : capacity * 2;
            char **tmp = realloc(names, new_cap * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc failed");
                goto cleanup;
            }
            names = tmp;
            capacity = new_cap;
        }

        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            perror("strdup failed");
            goto cleanup;
        }

        size_t len = strlen(entry->d_name);
        if (len > max_len)
            max_len = len;

        count++;
        errno = 0; /* strdup/realloc may have changed errno; reset for readdir */
    }

    if (errno != 0)
{
    perror("readdir failed");
    goto cleanup;
}

    
    /* Sort filenames alphabetically. */
    qsort(names, count, sizeof(names[0]), compare_names);

    /* Display filenames according to the selected mode. */
    if (display_mode == MODE_LONG)
    {
        do_ls_long(dir);
    }
    else if (display_mode == MODE_HORIZONTAL)
    {
        print_horizontal(dir, names, count, max_len);
    }
    else
    {
        print_columns(dir, names, count, max_len);
    }
    cleanup:
    for (size_t i = 0; i < count; i++)
        free(names[i]);
    free(names);
    closedir(dp);
}

/* Task 3 (v1.2.0): terminal width via ioctl(TIOCGWINSZ), fallback to 80 */
int get_terminal_width(void)
{
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
        return DEFAULT_TERM_WIDTH;
    return ws.ws_col;
}

/* Task 3 + 4 (v1.2.0): compute the layout and print "down then across".
 *
 *   col_width = max_len + spacing
 *   num_cols  = term_width / col_width          (at least 1)
 *   num_rows  = ceil(count / num_cols)
 *
 * Item at (row, col) is names[row + col * num_rows], so the list reads
 * top-to-bottom within a column, then continues in the next column. */
void print_columns(const char *dir, char **names, size_t count, size_t max_len)
{
    if (count == 0)
        return;

    size_t col_width = max_len + COLUMN_SPACING;
    size_t term_width = (size_t)get_terminal_width();

    size_t num_cols = term_width / col_width;
    if (num_cols < 1)
        num_cols = 1; /* a filename wider than the terminal: one per line */

    size_t num_rows = (count + num_cols - 1) / num_cols; /* ceiling division */

    for (size_t row = 0; row < num_rows; row++)
    {
        for (size_t col = 0; col < num_cols; col++)
        {
            size_t idx = row + col * num_rows;
            if (idx >= count)
                break; /* last column can be shorter than the others */

            /* pad every item except the last one on the line, so lines
               have no trailing spaces but columns still line up */
            size_t next_idx = row + (col + 1) * num_rows;
            print_colored_entry(dir, names[idx]);

            if (col + 1 < num_cols && next_idx < count)
            {
                size_t padding = col_width - strlen(names[idx]);
                printf("%*s", (int)padding, "");
            }
        }
        putchar('\n');
    }
}


/* Print filenames horizontally: across, then down */
void print_horizontal(const char *dir, char **names,size_t count, size_t max_len)
{
    if (count == 0)
        return;

    size_t col_width = max_len + COLUMN_SPACING;
    size_t term_width = (size_t)get_terminal_width();
    size_t current_pos = 0;

    for (size_t i = 0; i < count; i++)
    {
        if (current_pos > 0 &&
            current_pos + col_width > term_width)
        {
            putchar('\n');
            current_pos = 0;
        }

        print_colored_entry(dir, names[i]);

        if (i + 1 < count)
        {
            size_t padding = col_width - strlen(names[i]);
            printf("%*s", (int)padding, "");
            current_pos += col_width;
        }
        else
        {
            current_pos += strlen(names[i]);
        }
    }

    putchar('\n');
}



/* Long listing of a directory (-l), unchanged from v1.1.0 */

void do_ls_long(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);

    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory: %s\n", dir);
        return;
    }

    char **names = NULL;
    size_t count = 0;
    size_t capacity = 0;

    errno = 0;

    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        if (count == capacity)
        {
            size_t new_cap = (capacity == 0) ? 16 : capacity * 2;
            char **tmp = realloc(names, new_cap * sizeof(char *));

            if (tmp == NULL)
            {
                perror("realloc failed");
                goto cleanup;
            }

            names = tmp;
            capacity = new_cap;
        }

        names[count] = strdup(entry->d_name);

        if (names[count] == NULL)
        {
            perror("strdup failed");
            goto cleanup;
        }

        count++;
        errno = 0;
    }

    if (errno != 0)
    {
        perror("readdir failed");
        goto cleanup;
    }

    /* Sort filenames alphabetically. */
    qsort(names, count, sizeof(names[0]), compare_names);

    /* Print long listing in sorted order. */
    for (size_t i = 0; i < count; i++)
    {
        print_long_entry(dir, names[i]);
    }

cleanup:
    for (size_t i = 0; i < count; i++)
    {
        free(names[i]);
    }

    free(names);
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
        printf("%s %3lu %-8s %-8s %8lld %.12s ",
            perms,
            (unsigned long)st.st_nlink,
            pw ? pw->pw_name : uid_str,
            gr ? gr->gr_name : gid_str,
            (long long)st.st_size,
            timestr ? timestr + 4 : "????????????");
        print_colored_name(name, st.st_mode);
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