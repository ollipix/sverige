#include <iostream>
#include <string>
#include <map>
#include <cstdlib>
#include <array>
#include <memory>
#include <cstdio>
#include <sys/wait.h> // Required for WIFEXITED and WEXITSTATUS

struct PackageMeta {
    std::string url;
    std::string expected_sha256;
};

const std::string SOURCE_DIR = "/mnt/sverige/sources";

const std::map<std::string, PackageMeta> manifest = {
    {"Acl", {"https://download.savannah.gnu.org/releases/acl/acl-2.3.2.tar.gz", ""}},
    {"Attr", {"https://download.savannah.gnu.org/releases/attr/attr-2.5.2.tar.gz", ""}},
    {"Bash", {"https://ftp.gnu.org/gnu/bash/bash-5.2.21.tar.gz", ""}},
    {"Bc", {"https://ftp.gnu.org/gnu/bc/bc-1.07.1.tar.gz", ""}},
    {"Binutils", {"https://ftp.gnu.org/gnu/binutils/binutils-2.43.1.tar.xz", ""}},
    {"Bison", {"https://ftp.gnu.org/gnu/bison/bison-3.8.2.tar.xz", ""}},
    {"Bzip2", {"https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz", ""}},
    {"Coreutils", {"https://ftp.gnu.org/gnu/coreutils/coreutils-9.5.tar.xz", ""}},
    {"D-Bus", {"https://dbus.freedesktop.org/releases/dbus/dbus-1.14.10.tar.xz", ""}},
    {"Diffutils", {"https://ftp.gnu.org/gnu/diffutils/diffutils-3.10.tar.xz", ""}},
    {"E2fsprogs", {"https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.1/e2fsprogs-1.47.1.tar.xz", ""}},
    {"File", {"https://astron.com/pub/file/file-5.45.tar.gz", ""}},
    {"Findutils", {"https://ftp.gnu.org/gnu/findutils/findutils-4.10.0.tar.xz", ""}},
    {"Flex", {"https://github.com/westes/flex/releases/download/v2.6.4/flex-2.6.4.tar.gz", ""}},
    {"GCC", {"https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz", ""}},
    {"Gawk", {"https://ftp.gnu.org/gnu/gawk/gawk-5.3.0.tar.xz", ""}},
    {"Glibc", {"https://ftp.gnu.org/gnu/glibc/glibc-2.40.tar.xz", ""}},
    {"GMP", {"https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz", ""}},
    {"GRUB", {"https://ftp.gnu.org/gnu/grub/grub-2.12.tar.xz", ""}},
    {"Grep", {"https://ftp.gnu.org/gnu/grep/grep-3.11.tar.xz", ""}},
    {"Gzip", {"https://ftp.gnu.org/gnu/gzip/gzip-1.13.tar.xz", ""}},
    {"IPRoute2", {"https://www.kernel.org/pub/linux/utils/net/iproute2/iproute2-6.10.0.tar.xz", ""}},
    {"IPUtils", {"https://github.com/iputils/iputils/archive/refs/tags/20240117.tar.gz", ""}},
    {"Kmod", {"https://www.kernel.org/pub/linux/utils/kernel/kmod/kmod-33.tar.xz", ""}},
    {"Libcap", {"https://www.kernel.org/pub/linux/libs/security/linux-privs/libcap2/libcap-2.70.tar.xz", ""}},
    {"Linux-PAM", {"https://github.com/linux-pam/linux-pam/releases/download/v1.6.1/Linux-PAM-1.6.1.tar.xz", ""}},
    {"Linux-firmware", {"https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/snapshot/linux-firmware-20240909.tar.gz", ""}},
    {"Linux-headers", {"https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.tar.xz", ""}},
    {"M4", {"https://ftp.gnu.org/gnu/m4/m4-1.4.19.tar.xz", ""}},
    {"MPC", {"https://ftp.gnu.org/gnu/mpc/mpc-1.3.1.tar.gz", ""}},
    {"MPFR", {"https://ftp.gnu.org/gnu/mpfr/mpfr-4.2.1.tar.xz", ""}},
    {"Make", {"https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz", ""}},
    {"Ncurses", {"https://invisible-mirror.net/archives/ncurses/ncurses-6.5.tar.gz", ""}},
    {"Patch", {"https://ftp.gnu.org/gnu/patch/patch-2.7.6.tar.xz", ""}},
    {"Perl", {"https://www.cpan.org/src/5.0/perl-5.40.0.tar.gz", ""}},
    {"Procps-ng", {"https://gitlab.com/procps-ng/procps/-/archive/v4.0.4/procps-v4.0.4.tar.gz", ""}},
    {"Psmisc", {"https://gitlab.com/psmisc/psmisc/-/archive/v23.7/psmisc-v23.7.tar.gz", ""}},
    {"Python", {"https://www.python.org/ftp/python/3.12.0/Python-3.12.0.tar.xz", ""}},
    {"Readline", {"https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz", ""}},
    {"Sed", {"https://ftp.gnu.org/gnu/sed/sed-4.9.tar.xz", ""}},
    {"Shadow", {"https://github.com/shadow-maint/shadow/releases/download/v4.16.0/shadow-4.16.0.tar.xz", ""}},
    {"Tar", {"https://ftp.gnu.org/gnu/tar/tar-1.35.tar.xz", ""}},
    {"Texinfo", {"https://ftp.gnu.org/gnu/texinfo/texinfo-7.1.tar.xz", ""}},
    {"Util-linux", {"https://www.kernel.org/pub/linux/utils/util-linux/v2.40/util-linux-2.40.2.tar.xz", ""}},
    {"XZ", {"https://github.com/tukaani-project/xz/releases/download/v5.6.2/xz-5.6.2.tar.xz", ""}},
    {"Zlib", {"https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.xz", ""}},
    {"systemd", {"https://github.com/systemd/systemd-stable/archive/refs/tags/v256.6.tar.gz", ""}}
};

void printHelp() {
    std::cout << "Usage: saada <command> [package]\n\n"
              << "Commands:\n"
              << "  install <PackageName>   Download upstream source for the specified package\n"
              << "  list                    Show all available packages in the manifest\n"
              << "  --help                  Show this help message\n"
              << "  --version               Show saada version\n";
}

void printList() {
    std::cout << "Available packages:\n"
              << "  Linux (Dynamically fetched latest stable)\n";

    for (const auto& pair : manifest) {
        std::cout << "  " << pair.first << "\n";
    }
}

std::string execCommand(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(cmd, "r"),
        pclose
    );

    if (!pipe) {
        std::cerr << "[saada] Error: popen() failed to execute command: "
                  << cmd << std::endl;
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

bool ensureSourceDirectory() {
    std::string command = "mkdir -p \"" + SOURCE_DIR + "\"";

    int res = std::system(command.c_str());

    if (res == -1) {
        std::cerr << "[saada] Fatal: system() failed while creating source directory."
                  << std::endl;
        return false;
    }

    if (!WIFEXITED(res) || WEXITSTATUS(res) != 0) {
        std::cerr << "[saada] Fatal: Could not create "
                  << SOURCE_DIR << std::endl;
        return false;
    }

    return true;
}

std::string getFilenameFromUrl(const std::string& url) {
    size_t pos = url.find_last_of('/');

    if (pos == std::string::npos || pos + 1 >= url.length()) {
        return "";
    }

    return url.substr(pos + 1);
}

int downloadFile(const std::string& url, const std::string& filename) {
    if (!ensureSourceDirectory()) {
        return 1;
    }

    std::string outputPath = SOURCE_DIR + "/" + filename;

    std::cout << "[saada] Downloading " << url << "..." << std::endl;
    std::cout << "[saada] Destination: " << outputPath << std::endl;

    std::string downloadCmd =
        "curl -fL -o \"" + outputPath + "\" \"" + url + "\"";

    int res = std::system(downloadCmd.c_str());

    if (res == -1) {
        std::cerr << "[saada] Fatal: system() call failed to execute shell."
                  << std::endl;
        return 1;
    }

    if (WIFEXITED(res) && WEXITSTATUS(res) == 0) {
        std::cout << "[saada] Downloaded " << filename
                  << " successfully." << std::endl;
        return 0;
    }

    int exitCode = WIFEXITED(res) ? WEXITSTATUS(res) : -1;

    std::cerr << "[saada] Fatal: Download failed "
              << "(curl exit code or abnormal termination: "
              << exitCode << ")" << std::endl;

    return 1;
}

int downloadLatestLinuxKernel() {
    std::cout
        << "[saada] Querying kernel.org for the latest stable Linux kernel..."
        << std::endl;

    std::string banner =
        execCommand("curl -sSf https://www.kernel.org/finger_banner");

    if (banner.empty()) {
        std::cerr
            << "[saada] Fatal: Could not connect to kernel.org or retrieve banner."
            << std::endl;
        return 1;
    }

    std::string token =
        "The latest stable version of the Linux kernel is:";

    size_t pos = banner.find(token);

    if (pos == std::string::npos) {
        std::cerr
            << "[saada] Fatal: Unexpected response format from kernel.org."
            << std::endl;
        return 1;
    }

    std::string version = banner.substr(pos + token.length());

    size_t start = version.find_first_not_of(" \t\n\r");
    size_t end = version.find_last_not_of(" \t\n\r");

    if (start != std::string::npos) {
        version = version.substr(start, end - start + 1);
    }

    size_t dotPos = version.find('.');

    if (dotPos == std::string::npos) {
        std::cerr
            << "[saada] Fatal: Failed to parse major version from '"
            << version << "'." << std::endl;
        return 1;
    }

    std::string majorVersion = version.substr(0, dotPos);

    std::string url =
        "https://cdn.kernel.org/pub/linux/kernel/v" +
        majorVersion +
        ".x/linux-" +
        version +
        ".tar.xz";

    std::cout
        << "[saada] Found latest version: "
        << version << "\n";

    std::string filename = "linux-" + version + ".tar.xz";

    return downloadFile(url, filename);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "--help" || cmd == "-h") {
        printHelp();
        return 0;
    }

    if (cmd == "--version" || cmd == "-v") {
        std::cout << "saada v0.4" << std::endl;
        return 0;
    }

    if (cmd == "list") {
        printList();
        return 0;
    }

    if (cmd == "install") {
        if (argc < 3) {
            std::cerr
                << "[saada] Error: 'install' requires a package name."
                << std::endl;
            return 1;
        }

        std::string pkg = argv[2];

        if (pkg == "Linux") {
            return downloadLatestLinuxKernel();
        }

        auto it = manifest.find(pkg);

        if (it != manifest.end()) {
            std::string filename = getFilenameFromUrl(it->second.url);

            if (filename.empty()) {
                std::cerr
                    << "[saada] Fatal: Could not determine filename from URL."
                    << std::endl;
                return 1;
            }

            int result = downloadFile(it->second.url, filename);

            if (result == 0 && !it->second.expected_sha256.empty()) {
                std::cout
                    << "[saada] Notice: Checksum validation not yet implemented."
                    << std::endl;
            }

            return result;
        }

        std::cerr
            << "[saada] Fatal: Package '"
            << pkg
            << "' not recognized in repos, you made a spelling mistake probably!"
            << std::endl;

        return 1;
    }

    std::cerr
        << "[saada] Error: Unknown command '"
        << cmd
        << "'\n";

    printHelp();

    return 1;
}
