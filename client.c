#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

void
die(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

ssize_t
write_full(int fd, const void *buf, size_t count)
{
    size_t done = 0;
    ssize_t r;

    while (done < count)
    {
        if ((r = write(fd, buf + done, count - done)) < 0)
            return r;
        done += r;
    }

    return done;
}

void
write_cmd(int fd, char *op, char *arg)
{
    if (op != NULL && arg != NULL)
    {
        if (write_full(fd, op, strlen(op)) == -1 ||
            write_full(fd, arg, strlen(arg)) == -1 ||
            write_full(fd, "\0", 1) == -1)
        {
            die(__NAME__": Could not write to socket");
        }
    }
}

int
main(int argc, char **argv)
{
    bool hold = false;
    char cwd[PATH_MAX] = "";
    char *class = NULL;
    char *name = NULL;
    char *title = NULL;
    char *socket_suffix = "main";
    char dummy_buf[1] = "";
    int fd, i, try, max_tries = 10, exec_argi = argc;
    struct sockaddr_un addr = {0};

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-class") == 0 && i < argc - 1)
            class = argv[++i];
        else if (strcmp(argv[i], "-hold") == 0)
            hold = true;
        else if (strcmp(argv[i], "-name") == 0 && i < argc - 1)
            name = argv[++i];
        else if (strcmp(argv[i], "-title") == 0 && i < argc - 1)
            title = argv[++i];
        else if (strcmp(argv[i], "--suffix") == 0 && i < argc - 1)
            socket_suffix = argv[++i];
        else if (strcmp(argv[i], "-e") == 0 && i < argc - 1)
        {
            exec_argi = ++i;
            break;
        }
        else
        {
            fprintf(stderr, __NAME__": Invalid arguments, check manpage\n");
            exit(EXIT_FAILURE);
        }
    }

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        die(__NAME__": Could not create socket");

    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "/tmp/xiate-%d/%s",
             getuid(), socket_suffix);

    for (try = 0; try < max_tries; try++)
    {
        if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != -1)
            break;

        perror(__NAME__": Could not connect to server, retrying");
        sleep(1);
    }

    if (try == max_tries)
        die(__NAME__": Could not connect to server, giving up");

    if (getcwd(cwd, sizeof cwd) == NULL)
        die(__NAME__": getcwd()");

    /* We don't want to be killed by the kernel if the server closes the
     * connection while we're writing. We do handle socket errors. */
    signal(SIGPIPE, SIG_IGN);

    write_cmd(fd, "S", "");

    write_cmd(fd, "OC", cwd);
    write_cmd(fd, "Oc", class);
    write_cmd(fd, "On", name);
    write_cmd(fd, "Ot", title);

    if (hold)
        write_cmd(fd, "H", "");

    for (/* nop */; exec_argi < argc; exec_argi++)
        write_cmd(fd, "A", argv[exec_argi]);

    write_cmd(fd, "F", "");

    /* Try to read. This just blocks until the connection is closed,
     * which will happen when the terminal window is closed, or until we
     * get an error. */
    read(fd, &dummy_buf, 1);
    close(fd);

    exit(EXIT_SUCCESS);
}
