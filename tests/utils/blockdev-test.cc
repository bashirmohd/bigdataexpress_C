// Get some details about the given block device.

#include <iostream>

#include <sys/ioctl.h>
#include <linux/fs.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *device = "/dev/sda1";

    if (argc > 1)
    {
        device = argv[1];
    }

    int fd = open(device, O_RDONLY);

    if (fd == -1)
    {
        std::cerr << "Error opening " << device << ": "
                  << strerror(errno) << "\n";
        exit(1);
    }

    size_t bs = 0;
    int    rc = ioctl(fd, BLKSSZGET, &bs);

    if (rc != 0)
    {
        std::cerr << "Error in ioctl(BLKSSZGET): " << strerror(errno) << "\n";
    }
    else
    {
        std::cout << "ioctl(" << device << ", BLKSSZGET) => " << bs << "\n";
    }

    rc = ioctl(fd, BLKBSZGET, &bs);

    if (rc != 0)
    {
        std::cerr << "Error in ioctl(BLKSSZGET): " << strerror(errno) << "\n";
    }
    else
    {
        std::cout << "ioctl(" << device << ", BLKBSZGET) => " << bs << "\n";
    }

    rc = ioctl(fd, BLKFRAGET, &bs);

    if (rc != 0)
    {
        std::cerr << "Error in ioctl(BLKFRAGET): " << strerror(errno) << "\n";
    }
    else
    {
        std::cout << "ioctl(" << device << ", BLKFRAGET) => " << bs << "\n";
    }

    close(fd);
}

// Local Variables:
// mode: c++
// c-basic-offset: 4
// End:
