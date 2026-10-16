#include <iostream>
#include <system_error>
#include <new>

// Entry point for Haiku.

int haiku_main(int argc, char **argv);

int main(int argc, char **argv)
{
    try {
        return haiku_main(argc, argv);
    }
    catch (std::bad_alloc &badalloc) {
        std::cout << "haiku: out-of-memory during initialisation" << std::endl;
        return 1;
    }
    catch (std::system_error &syserr) {
        std::cout << "haiku: unexpected system error during initialisation: " << syserr.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cout << "haiku: unexpected error during initialisation" << std::endl;
        return 1;
    }
}
