#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static const std::string VERSION = "0.7";
static const std::string DEFAULT_SOURCE_DIR = "/mnt/sverige/sources";
static const std::string DEFAULT_DATA_DIR = "/var/lib/saada";
static const std::string DEFAULT_CACHE_DIR = "/var/cache/saada";

struct Package {
    std::string name;
    std::string version;
    std::string source;
    std::string archive;
    std::vector<std::string> dependencies;
    std::string sha256;
    int line = 0;
};

struct ResolvedSource {
    std::string version;
    std::string url;
    std::string filename;
};

struct Config {
    std::string sourceDir = DEFAULT_SOURCE_DIR;
    std::string dataDir = DEFAULT_DATA_DIR;
    std::string cacheDir = DEFAULT_CACHE_DIR;
    std::string manifest;
    std::string manifestUrl;
};

struct Options {
    bool force = false;
    bool dryRun = false;
    bool yes = false;
};

static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

static std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> out;
    std::string part;
    for (char c : s) {
        if (c == delimiter) {
            out.push_back(trim(part));
            part.clear();
        } else {
            part += c;
        }
    }
    out.push_back(trim(part));
    return out;
}

static std::vector<std::string> splitComma(const std::string& s) {
    std::vector<std::string> out;
    for (const auto& item : split(s, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

static bool isHex64(const std::string& s) {
    if (s.size() != 64) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isxdigit(c);
    });
}

static std::string normalizeHash(std::string s) {
    s = trim(s);
    if (startsWith(lower(s), "sha256:")) s = s.substr(7);
    return lower(s);
}

static bool isUrl(const std::string& s) {
    return startsWith(s, "http://") || startsWith(s, "https://");
}

static std::string shellQuote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static int runProcess(const std::vector<std::string>& args,
                      std::string* capturedStdout = nullptr) {
    if (args.empty()) return -1;

    int pipefd[2] = {-1, -1};
    if (capturedStdout && pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        if (capturedStdout) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return -1;
    }

    if (pid == 0) {
        if (capturedStdout) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& arg : args)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    if (capturedStdout) {
        close(pipefd[1]);
        std::string output;
        char buffer[4096];
        ssize_t n;
        while ((n = read(pipefd[0], buffer, sizeof(buffer))) > 0)
            output.append(buffer, static_cast<size_t>(n));
        close(pipefd[0]);
        *capturedStdout = output;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 128;
}

static bool commandExists(const std::string& command) {
    return runProcess({"sh", "-c", "command -v " + shellQuote(command)}) == 0;
}

static bool ensureDir(const fs::path& path) {
    std::error_code ec;
    if (fs::exists(path, ec)) return fs::is_directory(path, ec);
    return fs::create_directories(path, ec) || fs::is_directory(path, ec);
}

static std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

static std::string basenameFromUrl(const std::string& url) {
    auto q = url.find_first_of("?#");
    std::string clean = q == std::string::npos ? url : url.substr(0, q);
    auto slash = clean.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= clean.size()) return "";
    return clean.substr(slash + 1);
}

static std::string sanitizeFilename(const std::string& filename) {
    if (filename.empty() || filename == "." || filename == "..") return "";
    if (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)
        return "";
    return filename;
}

static std::optional<std::string> readFile(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool writeFile(const fs::path& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) return false;
    out << content;
    return static_cast<bool>(out);
}

static Config loadConfig() {
    Config cfg;

    const char* home = std::getenv("HOME");
    fs::path configPath;
    if (home) configPath = fs::path(home) / ".config/saada/config";

    if (!configPath.empty()) {
        if (auto text = readFile(configPath)) {
            std::istringstream in(*text);
            std::string line;
            while (std::getline(in, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;

                std::string key = trim(line.substr(0, eq));
                std::string value = trim(line.substr(eq + 1));

                if (key == "source_dir") cfg.sourceDir = value;
                else if (key == "data_dir") cfg.dataDir = value;
                else if (key == "cache_dir") cfg.cacheDir = value;
                else if (key == "manifest") cfg.manifest = value;
                else if (key == "manifest_url") cfg.manifestUrl = value;
            }
        }
    }

    if (const char* v = std::getenv("SAADA_SOURCE_DIR")) cfg.sourceDir = v;
    if (const char* v = std::getenv("SAADA_DATA_DIR")) cfg.dataDir = v;
    if (const char* v = std::getenv("SAADA_CACHE_DIR")) cfg.cacheDir = v;
    if (const char* v = std::getenv("SAADA_MANIFEST")) cfg.manifest = v;
    if (const char* v = std::getenv("SAADA_MANIFEST_URL")) cfg.manifestUrl = v;

    if (cfg.manifest.empty()) {
        const std::vector<fs::path> candidates = {
            "/usr/share/saada/manifest",
            "/usr/local/share/saada/manifest",
            "/opt/saada/share/manifest"
        };
        for (const auto& p : candidates) {
            if (fs::exists(p)) {
                cfg.manifest = p.string();
                break;
            }
        }
    }

    return cfg;
}

static bool loadManifest(const std::string& path,
                         std::map<std::string, Package>& packages,
                         std::vector<std::string>& errors) {
    packages.clear();
    std::ifstream in(path);
    if (!in) {
        errors.push_back("cannot open manifest: " + path);
        return false;
    }

    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        auto fields = split(line, '|');
        if (fields.size() != 6) {
            errors.push_back("line " + std::to_string(lineNo) +
                             ": expected 6 fields, got " + std::to_string(fields.size()));
            continue;
        }

        Package p;
        p.name = fields[0];
        p.version = fields[1];
        p.source = fields[2];
        p.archive = fields[3];
        p.dependencies = splitComma(fields[4]);
        p.sha256 = normalizeHash(fields[5]);
        p.line = lineNo;

        if (p.name.empty()) {
            errors.push_back("line " + std::to_string(lineNo) + ": empty package name");
            continue;
        }
        if (p.version.empty()) {
            errors.push_back("line " + std::to_string(lineNo) + ": empty version/provider");
            continue;
        }
        if (!startsWith(p.version, "dynamic:") && !isUrl(p.source)) {
            errors.push_back("line " + std::to_string(lineNo) +
                             ": static package source must be an http(s) URL");
            continue;
        }
        if (startsWith(p.version, "dynamic:") && p.source.empty()) {
            errors.push_back("line " + std::to_string(lineNo) +
                             ": dynamic package requires provider arguments");
            continue;
        }
        if (!p.sha256.empty() && !isHex64(p.sha256)) {
            errors.push_back("line " + std::to_string(lineNo) +
                             ": invalid SHA-256 checksum");
            continue;
        }

        const std::string key = lower(p.name);
        if (packages.count(key)) {
            errors.push_back("line " + std::to_string(lineNo) +
                             ": duplicate package: " + p.name);
            continue;
        }
        packages[key] = p;
    }

    for (const auto& [key, p] : packages) {
        for (const auto& dep : p.dependencies) {
            if (!packages.count(lower(dep))) {
                errors.push_back("package " + p.name +
                                 " depends on missing package " + dep);
            }
        }
    }

    return errors.empty();
}

static std::optional<Package> findPackage(const std::map<std::string, Package>& packages,
                                          const std::string& name) {
    auto it = packages.find(lower(name));
    if (it == packages.end()) return std::nullopt;
    return it->second;
}

static std::string dynamicProviderName(const Package& p) {
    if (!startsWith(p.version, "dynamic:")) return "";
    return p.version.substr(8);
}

static std::optional<ResolvedSource> resolveSource(const Package& p) {
    if (!startsWith(p.version, "dynamic:")) {
        const std::string filename = sanitizeFilename(
            basenameFromUrl(p.source));
        if (filename.empty()) return std::nullopt;
        return ResolvedSource{p.version, p.source, filename};
    }

    const std::string provider = dynamicProviderName(p);

    if (provider == "kernel.org") {
        std::string banner;
        std::cout << "[saada] Resolving dynamic source: " << provider << "\n";

        const int rc = runProcess(
            {"curl", "-fsSL", "--connect-timeout", "15",
             "https://www.kernel.org/finger_banner"},
            &banner);

        if (rc != 0 || banner.empty()) {
            std::cerr << "[saada] Fatal: could not query kernel.org\n";
            return std::nullopt;
        }

        const std::string marker =
            "The latest stable version of the Linux kernel is:";
        auto pos = banner.find(marker);
        if (pos == std::string::npos) {
            std::cerr << "[saada] Fatal: unexpected kernel.org response\n";
            return std::nullopt;
        }

        std::string version = trim(banner.substr(pos + marker.size()));
        if (version.empty()) {
            std::cerr << "[saada] Fatal: kernel.org returned an empty version\n";
            return std::nullopt;
        }

        for (char c : version) {
            if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.')) {
                std::cerr << "[saada] Fatal: invalid kernel version: " << version << "\n";
                return std::nullopt;
            }
        }

        auto dot = version.find('.');
        if (dot == std::string::npos) {
            std::cerr << "[saada] Fatal: invalid kernel version: " << version << "\n";
            return std::nullopt;
        }

        const std::string major = version.substr(0, dot);
        const std::string url =
            "https://cdn.kernel.org/pub/linux/kernel/v" + major +
            ".x/linux-" + version + ".tar.xz";
        const std::string filename = "linux-" + version + ".tar.xz";

        return ResolvedSource{version, url, filename};
    }

    std::cerr << "[saada] Fatal: unsupported dynamic provider: "
              << provider << "\n";
    return std::nullopt;
}

static std::vector<int> numericVersionParts(std::string version) {
    if (!version.empty() && version[0] == 'v') version.erase(0, 1);
    std::vector<int> parts;
    std::string current;
    for (char c : version) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            current += c;
        } else if (c == '.') {
            if (current.empty()) parts.push_back(0);
            else {
                try { parts.push_back(std::stoi(current)); }
                catch (...) { parts.push_back(0); }
            }
            current.clear();
        } else {
            break;
        }
    }
    if (!current.empty()) {
        try { parts.push_back(std::stoi(current)); }
        catch (...) { parts.push_back(0); }
    }
    return parts;
}

static int compareVersions(const std::string& a, const std::string& b) {
    const auto av = numericVersionParts(a);
    const auto bv = numericVersionParts(b);
    const size_t n = std::max(av.size(), bv.size());
    for (size_t i = 0; i < n; ++i) {
        const int x = i < av.size() ? av[i] : 0;
        const int y = i < bv.size() ? bv[i] : 0;
        if (x < y) return -1;
        if (x > y) return 1;
    }
    return 0;
}

static fs::path dbDir(const Config& cfg) {
    return fs::path(cfg.dataDir) / "installed";
}

static fs::path recordPath(const Config& cfg, const std::string& name) {
    return dbDir(cfg) / (lower(name) + ".meta");
}

static bool saveRecord(const Config& cfg, const Package& p,
                       const ResolvedSource& r, const fs::path& archivePath) {
    if (!ensureDir(dbDir(cfg))) return false;

    std::ostringstream out;
    out << "name=" << p.name << "\n";
    out << "version=" << r.version << "\n";
    out << "url=" << r.url << "\n";
    out << "filename=" << r.filename << "\n";
    out << "path=" << archivePath.string() << "\n";
    out << "sha256=" << p.sha256 << "\n";
    out << "installed_at=" << timestamp() << "\n";
    return writeFile(recordPath(cfg, p.name), out.str());
}

static std::map<std::string, std::string> readRecord(const fs::path& path) {
    std::map<std::string, std::string> values;
    if (auto text = readFile(path)) {
        std::istringstream in(*text);
        std::string line;
        while (std::getline(in, line)) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            values[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
        }
    }
    return values;
}

static std::map<std::string, std::map<std::string, std::string>>
loadInstalled(const Config& cfg) {
    std::map<std::string, std::map<std::string, std::string>> installed;
    std::error_code ec;
    if (!fs::exists(dbDir(cfg), ec)) return installed;

    for (const auto& entry : fs::directory_iterator(dbDir(cfg), ec)) {
        if (ec || !entry.is_regular_file()) continue;
        if (entry.path().extension() != ".meta") continue;

        auto record = readRecord(entry.path());
        if (record.count("name"))
            installed[lower(record["name"])] = record;
    }
    return installed;
}

static std::optional<std::string> sha256sum(const fs::path& file) {
    if (!commandExists("sha256sum")) return std::nullopt;
    std::string output;
    const int rc = runProcess({"sha256sum", file.string()}, &output);
    if (rc != 0) return std::nullopt;

    std::istringstream in(output);
    std::string hash;
    in >> hash;
    if (!isHex64(hash)) return std::nullopt;
    return lower(hash);
}

static bool verifyChecksum(const fs::path& file, const std::string& expected) {
    if (expected.empty()) {
        std::cout << "[saada] Warning: no SHA-256 checksum supplied; archive is not verified.\n";
        return true;
    }

    auto actual = sha256sum(file);
    if (!actual) {
        std::cerr << "[saada] Fatal: sha256sum is required for checksum verification.\n";
        return false;
    }

    if (lower(normalizeHash(expected)) != *actual) {
        std::cerr << "[saada] Fatal: SHA-256 mismatch for " << file << "\n";
        std::cerr << "[saada] Expected: " << normalizeHash(expected) << "\n";
        std::cerr << "[saada] Actual:   " << *actual << "\n";
        return false;
    }

    std::cout << "[saada] SHA-256 verified.\n";
    return true;
}

static bool download(const Config& cfg, const ResolvedSource& r,
                     const std::string& expectedHash, const Options& options,
                     fs::path* finalPathOut = nullptr) {
    if (!ensureDir(cfg.sourceDir) || !ensureDir(cfg.cacheDir)) {
        std::cerr << "[saada] Fatal: could not create source/cache directories.\n";
        return false;
    }

    const fs::path destination = fs::path(cfg.sourceDir) / r.filename;
    const fs::path cache = fs::path(cfg.cacheDir) / r.filename;
    const fs::path part = destination.string() + ".part";

    if (finalPathOut) *finalPathOut = destination;

    if (fs::exists(destination) && !options.force) {
        std::cout << "[saada] Archive already exists: " << destination << "\n";
        if (!verifyChecksum(destination, expectedHash)) return false;
        return true;
    }

    if (options.dryRun) {
        std::cout << "[saada] DRY-RUN: would download " << r.url << "\n";
        std::cout << "[saada] DRY-RUN: destination " << destination << "\n";
        return true;
    }

    std::error_code ec;
    fs::remove(part, ec);

    std::cout << "[saada] Downloading " << r.url << "\n";
    std::cout << "[saada] Destination: " << destination << "\n";

    int rc = runProcess({
        "curl", "-fL", "--retry", "3", "--retry-delay", "1",
        "--connect-timeout", "15", "--output", part.string(), r.url
    });

    if (rc != 0) {
        fs::remove(part, ec);
        std::cerr << "[saada] Fatal: curl failed with exit code " << rc << "\n";
        return false;
    }

    if (!verifyChecksum(part, expectedHash)) {
        fs::remove(part, ec);
        return false;
    }

    fs::remove(cache, ec);
    fs::copy_file(part, cache, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "[saada] Warning: could not populate cache: " << ec.message() << "\n";
    }

    fs::remove(destination, ec);
    fs::rename(part, destination, ec);
    if (ec) {
        std::cerr << "[saada] Fatal: could not finalize download: "
                  << ec.message() << "\n";
        fs::remove(part, ec);
        return false;
    }

    std::cout << "[saada] Downloaded " << r.filename << " successfully.\n";
    return true;
}

static bool confirm(const std::string& question, const Options& options) {
    if (options.yes) return true;
    std::cout << question << " [y/N] ";
    std::string answer;
    std::getline(std::cin, answer);
    return lower(trim(answer)) == "y" || lower(trim(answer)) == "yes";
}

static bool resolveDependencies(const std::map<std::string, Package>& packages,
                                const Package& root,
                                std::vector<Package>& order,
                                std::string& error) {
    std::set<std::string> visiting;
    std::set<std::string> visited;

    std::function<bool(const Package&)> visit = [&](const Package& p) {
        const std::string key = lower(p.name);
        if (visiting.count(key)) {
            error = "dependency cycle detected at " + p.name;
            return false;
        }
        if (visited.count(key)) return true;

        visiting.insert(key);
        for (const auto& depName : p.dependencies) {
            auto it = packages.find(lower(depName));
            if (it == packages.end()) {
                error = p.name + " depends on missing package " + depName;
                return false;
            }
            if (!visit(it->second)) return false;
        }
        visiting.erase(key);
        visited.insert(key);
        order.push_back(p);
        return true;
    };

    return visit(root);
}

static void printHelp() {
    std::cout
        << "Saada - source package manager for sverige Linux\n\n"
        << "Usage: saada <command> [package] [options]\n\n"
        << "Commands:\n"
        << "  install <pkg>       Resolve dependencies and download a package\n"
        << "  remove <pkg>        Remove a Saada-managed source archive and record\n"
        << "  update              Refresh the configured manifest from its URL\n"
        << "  upgrade [pkg]       Upgrade one package, or all managed packages\n"
        << "  list                List packages in the manifest\n"
        << "  installed           List Saada-managed packages\n"
        << "  search <text>       Search package names and metadata\n"
        << "  info <pkg>          Show package metadata and resolved source\n"
        << "  deps <pkg>          Show dependency tree\n"
        << "  verify <pkg>        Verify an installed archive\n"
        << "  clean               Remove partial downloads and stale cache entries\n"
        << "  doctor              Check Saada and its configuration\n"
        << "  config              Show active configuration\n"
        << "  init                Create the user configuration file\n"
        << "  --help              Show this help\n"
        << "  --version           Show Saada version\n\n"
        << "Options:\n"
        << "  --force, -f         Redownload/rewrite existing data\n"
        << "  --dry-run           Show actions without changing files\n"
        << "  --yes, -y           Skip confirmation prompts\n";
}

static void printList(const std::map<std::string, Package>& packages) {
    std::cout << "Available packages:\n";
    for (const auto& [key, p] : packages) {
        std::cout << "  " << p.name;
        if (startsWith(p.version, "dynamic:"))
            std::cout << " [" << p.version << "]";
        else
            std::cout << " " << p.version;
        std::cout << "\n";
    }
}

static int commandInstall(const Config& cfg,
                          const std::map<std::string, Package>& packages,
                          const std::string& name, const Options& options) {
    auto root = findPackage(packages, name);
    if (!root) {
        std::cerr << "[saada] Fatal: package '" << name << "' not found.\n";
        return 1;
    }

    std::vector<Package> order;
    std::string error;
    if (!resolveDependencies(packages, *root, order, error)) {
        std::cerr << "[saada] Fatal: " << error << "\n";
        return 1;
    }

    const auto installed = loadInstalled(cfg);

    for (const auto& p : order) {
        auto resolved = resolveSource(p);
        if (!resolved) return 1;

        auto old = installed.find(lower(p.name));
        if (old != installed.end() && old->second.count("version") &&
            compareVersions(old->second.at("version"), resolved->version) == 0 &&
            !options.force) {
            const fs::path oldPath =
                old->second.count("path") ? old->second.at("path") : "";
            if (!oldPath.empty() && fs::exists(oldPath)) {
                std::cout << "[saada] " << p.name << " "
                          << resolved->version << " is already managed.\n";
                continue;
            }
        }

        fs::path archivePath;
        if (!download(cfg, *resolved, p.sha256, options, &archivePath)) return 1;

        if (!options.dryRun) {
            if (!saveRecord(cfg, p, *resolved, archivePath)) {
                std::cerr << "[saada] Fatal: could not write package record.\n";
                return 1;
            }
        }
    }

    return 0;
}

static int commandRemove(const Config& cfg, const std::string& name,
                         const Options& options) {
    const auto installed = loadInstalled(cfg);
    auto it = installed.find(lower(name));
    if (it == installed.end()) {
        std::cerr << "[saada] Package '" << name << "' is not managed by Saada.\n";
        return 1;
    }

    const auto& record = it->second;
    const std::string packageName =
        record.count("name") ? record.at("name") : name;
    const std::string path = record.count("path") ? record.at("path") : "";

    std::cout << "[saada] Saada removes its managed source archive and metadata only.\n";

    if (!confirm("[saada] Remove " + packageName + "?", options)) {
        std::cout << "[saada] Cancelled.\n";
        return 0;
    }

    if (options.dryRun) {
        std::cout << "[saada] DRY-RUN: would remove " << path << "\n";
        std::cout << "[saada] DRY-RUN: would remove " << recordPath(cfg, packageName) << "\n";
        return 0;
    }

    std::error_code ec;
    if (!path.empty()) {
        const fs::path p(path);
        if (fs::exists(p)) fs::remove(p, ec);
        if (ec) {
            std::cerr << "[saada] Fatal: could not remove archive: "
                      << ec.message() << "\n";
            return 1;
        }
    }

    fs::remove(recordPath(cfg, packageName), ec);
    if (ec) {
        std::cerr << "[saada] Fatal: could not remove package record: "
                  << ec.message() << "\n";
        return 1;
    }

    std::cout << "[saada] Removed " << packageName << ".\n";
    return 0;
}

static int commandUpdate(const Config& cfg, const Options& options) {
    if (cfg.manifestUrl.empty()) {
        std::cerr << "[saada] Fatal: manifest_url is not configured.\n";
        std::cerr << "[saada] Set SAADA_MANIFEST_URL or add manifest_url= to config.\n";
        return 1;
    }
    if (!isUrl(cfg.manifestUrl)) {
        std::cerr << "[saada] Fatal: manifest_url is not an HTTP(S) URL.\n";
        return 1;
    }

    if (options.dryRun) {
        std::cout << "[saada] DRY-RUN: would update " << cfg.manifest << "\n";
        return 0;
    }

    if (cfg.manifest.empty()) {
        std::cerr << "[saada] Fatal: no local manifest path configured.\n";
        return 1;
    }

    const fs::path target(cfg.manifest);
    if (!ensureDir(target.parent_path())) {
        std::cerr << "[saada] Fatal: cannot create manifest directory.\n";
        return 1;
    }

    const fs::path part = target.string() + ".part";
    std::error_code ec;
    fs::remove(part, ec);

    std::cout << "[saada] Updating manifest from " << cfg.manifestUrl << "\n";
    const int rc = runProcess({
        "curl", "-fL", "--retry", "3", "--retry-delay", "1",
        "--connect-timeout", "15", "--output", part.string(), cfg.manifestUrl
    });
    if (rc != 0) {
        fs::remove(part, ec);
        std::cerr << "[saada] Fatal: manifest download failed.\n";
        return 1;
    }

    std::map<std::string, Package> packages;
    std::vector<std::string> errors;
    if (!loadManifest(part.string(), packages, errors)) {
        fs::remove(part, ec);
        std::cerr << "[saada] Fatal: downloaded manifest failed validation.\n";
        for (const auto& e : errors) std::cerr << "  " << e << "\n";
        return 1;
    }

    fs::remove(target, ec);
    fs::rename(part, target, ec);
    if (ec) {
        std::cerr << "[saada] Fatal: could not replace manifest: "
                  << ec.message() << "\n";
        return 1;
    }

    std::cout << "[saada] Manifest updated. " << packages.size()
              << " packages available.\n";
    return 0;
}

static int commandUpgrade(const Config& cfg,
                          const std::map<std::string, Package>& packages,
                          const std::optional<std::string>& requested,
                          const Options& options) {
    auto installed = loadInstalled(cfg);
    std::vector<std::string> names;

    if (requested) {
        names.push_back(*requested);
    } else {
        for (const auto& [key, record] : installed)
            if (record.count("name")) names.push_back(record.at("name"));
    }

    if (names.empty()) {
        std::cout << "[saada] No Saada-managed packages to upgrade.\n";
        return 0;
    }

    int failures = 0;
    for (const auto& name : names) {
        auto p = findPackage(packages, name);
        if (!p) {
            std::cerr << "[saada] Warning: " << name
                      << " is installed but absent from the manifest.\n";
            ++failures;
            continue;
        }

        auto recordIt = installed.find(lower(name));
        if (recordIt == installed.end() || !recordIt->second.count("version")) {
            ++failures;
            continue;
        }

        auto resolved = resolveSource(*p);
        if (!resolved) {
            ++failures;
            continue;
        }

        const std::string oldVersion = recordIt->second.at("version");
        const int cmp = compareVersions(oldVersion, resolved->version);

        if (cmp >= 0 && !options.force) {
            std::cout << "[saada] " << p->name << " is up to date ("
                      << oldVersion << ").\n";
            continue;
        }

        std::cout << "[saada] Upgrade " << p->name << " "
                  << oldVersion << " -> " << resolved->version << "\n";

        if (!download(cfg, *resolved, p->sha256, options)) {
            ++failures;
            continue;
        }

        if (!options.dryRun) {
            fs::path archivePath = fs::path(cfg.sourceDir) / resolved->filename;
            if (!saveRecord(cfg, *p, *resolved, archivePath)) {
                ++failures;
                std::cerr << "[saada] Fatal: could not update package record.\n";
            }
        }
    }

    return failures ? 1 : 0;
}

static void printDepsRecursive(const std::map<std::string, Package>& packages,
                               const Package& p, int depth,
                               std::set<std::string>& seen) {
    std::cout << std::string(depth * 2, ' ') << p.name;
    if (!startsWith(p.version, "dynamic:"))
        std::cout << " " << p.version;
    std::cout << "\n";

    if (seen.count(lower(p.name))) return;
    seen.insert(lower(p.name));

    for (const auto& dep : p.dependencies) {
        auto it = packages.find(lower(dep));
        if (it != packages.end())
            printDepsRecursive(packages, it->second, depth + 1, seen);
    }
}

static int commandInfo(const std::map<std::string, Package>& packages,
                       const std::string& name) {
    auto p = findPackage(packages, name);
    if (!p) {
        std::cerr << "[saada] Package not found: " << name << "\n";
        return 1;
    }

    std::cout << "Name:         " << p->name << "\n";
    std::cout << "Version:      " << p->version << "\n";
    std::cout << "Source:       " << p->source << "\n";
    std::cout << "Archive:      " << p->archive << "\n";
    std::cout << "Dependencies: "
              << (p->dependencies.empty() ? "(none)" : "") ;
    if (!p->dependencies.empty()) {
        for (size_t i = 0; i < p->dependencies.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << p->dependencies[i];
        }
    }
    std::cout << "\n";
    std::cout << "SHA-256:      "
              << (p->sha256.empty() ? "(not supplied)" : p->sha256) << "\n";

    auto resolved = resolveSource(*p);
    if (resolved) {
        std::cout << "Resolved URL: " << resolved->url << "\n";
        std::cout << "Filename:     " << resolved->filename << "\n";
        std::cout << "Resolved ver: " << resolved->version << "\n";
    }

    return 0;
}

static int commandVerify(const Config& cfg,
                         const std::map<std::string, Package>& packages,
                         const std::string& name) {
    auto p = findPackage(packages, name);
    if (!p) {
        std::cerr << "[saada] Package not found: " << name << "\n";
        return 1;
    }

    const auto installed = loadInstalled(cfg);
    auto it = installed.find(lower(name));
    if (it == installed.end()) {
        std::cerr << "[saada] Package is not managed: " << p->name << "\n";
        return 1;
    }

    if (!it->second.count("path")) {
        std::cerr << "[saada] Package record has no archive path.\n";
        return 1;
    }

    const fs::path path = it->second.at("path");
    if (!fs::exists(path)) {
        std::cerr << "[saada] Archive is missing: " << path << "\n";
        return 1;
    }

    const std::string expected =
        p->sha256.empty() && it->second.count("sha256")
            ? it->second.at("sha256") : p->sha256;

    return verifyChecksum(path, expected) ? 0 : 1;
}

static int commandClean(const Config& cfg, const Options& options) {
    size_t removed = 0;
    std::error_code ec;

    for (const auto& root : {fs::path(cfg.sourceDir), fs::path(cfg.cacheDir)}) {
        if (!fs::exists(root, ec)) continue;
        for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            const auto name = entry.path().filename().string();
            if (endsWith(name, ".part")) {
                if (options.dryRun)
                    std::cout << "[saada] DRY-RUN: would remove " << entry.path() << "\n";
                else if (fs::remove(entry.path(), ec)) ++removed;
            }
        }
    }

    const auto installed = loadInstalled(cfg);
    std::set<std::string> managedFiles;
    for (const auto& [key, record] : installed) {
        if (record.count("filename")) managedFiles.insert(record.at("filename"));
    }

    if (fs::exists(cfg.cacheDir, ec)) {
        for (const auto& entry : fs::directory_iterator(cfg.cacheDir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file()) continue;
            const auto filename = entry.path().filename().string();
            if (!managedFiles.count(filename) && !endsWith(filename, ".part")) {
                if (options.dryRun)
                    std::cout << "[saada] DRY-RUN: would remove stale cache "
                              << entry.path() << "\n";
                else if (fs::remove(entry.path(), ec)) ++removed;
            }
        }
    }

    if (!options.dryRun)
        std::cout << "[saada] Cleaned " << removed << " stale/partial files.\n";
    return 0;
}

static int commandDoctor(const Config& cfg,
                         const std::map<std::string, Package>& packages,
                         const std::vector<std::string>& manifestErrors) {
    bool ok = true;

    auto check = [&](const std::string& label, bool result) {
        std::cout << "[saada] " << std::left << std::setw(24) << label
                  << (result ? "OK" : "FAIL") << "\n";
        if (!result) ok = false;
    };

    check("curl", commandExists("curl"));
    check("sha256sum", commandExists("sha256sum"));
    check("manifest path", !cfg.manifest.empty());
    check("manifest readable", !cfg.manifest.empty() && fs::is_regular_file(cfg.manifest));
    check("manifest valid", manifestErrors.empty() && !packages.empty());

    std::error_code ec;
    check("source directory", ensureDir(cfg.sourceDir));
    check("data directory", ensureDir(cfg.dataDir));
    check("cache directory", ensureDir(cfg.cacheDir));

    if (!manifestErrors.empty()) {
        std::cout << "\nManifest errors:\n";
        for (const auto& e : manifestErrors) std::cout << "  " << e << "\n";
    }

    if (cfg.manifestUrl.empty())
        std::cout << "[saada] manifest_url             not configured\n";

    std::cout << "\n";
    if (ok) std::cout << "[saada] Doctor: everything required looks usable.\n";
    else std::cout << "[saada] Doctor: problems were found.\n";
    return ok ? 0 : 1;
}

static int commandInit() {
    const char* home = std::getenv("HOME");
    if (!home) {
        std::cerr << "[saada] Fatal: HOME is not set.\n";
        return 1;
    }

    const fs::path path = fs::path(home) / ".config/saada/config";
    if (fs::exists(path)) {
        std::cout << "[saada] Config already exists: " << path << "\n";
        return 0;
    }

    if (!ensureDir(path.parent_path())) {
        std::cerr << "[saada] Fatal: could not create " << path.parent_path() << "\n";
        return 1;
    }

    std::ostringstream config;
    config << "source_dir=" << DEFAULT_SOURCE_DIR << "\n";
    config << "data_dir=" << DEFAULT_DATA_DIR << "\n";
    config << "cache_dir=" << DEFAULT_CACHE_DIR << "\n";
    config << "manifest=/usr/share/saada/manifest\n";
    config << "manifest_url=\n";

    if (!writeFile(path, config.str())) {
        std::cerr << "[saada] Fatal: could not write " << path << "\n";
        return 1;
    }

    std::cout << "[saada] Created " << path << "\n";
    return 0;
}

static void printConfig(const Config& cfg) {
    std::cout << "source_dir=" << cfg.sourceDir << "\n";
    std::cout << "data_dir=" << cfg.dataDir << "\n";
    std::cout << "cache_dir=" << cfg.cacheDir << "\n";
    std::cout << "manifest=" << cfg.manifest << "\n";
    std::cout << "manifest_url=" << cfg.manifestUrl << "\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }

    std::string command;
    std::vector<std::string> positional;
    Options options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--force" || arg == "-f") options.force = true;
        else if (arg == "--dry-run") options.dryRun = true;
        else if (arg == "--yes" || arg == "-y") options.yes = true;
        else if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "--version" || arg == "-v") {
            std::cout << "saada v" << VERSION << "\n";
            return 0;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        printHelp();
        return 1;
    }

    command = positional[0];

    if (command == "init") return commandInit();

    Config cfg = loadConfig();

    std::map<std::string, Package> packages;
    std::vector<std::string> manifestErrors;

    if (!cfg.manifest.empty() && fs::exists(cfg.manifest)) {
        loadManifest(cfg.manifest, packages, manifestErrors);
    } else if (command != "config" && command != "update") {
        manifestErrors.push_back("manifest not found; configure SAADA_MANIFEST or install a manifest");
    }

    if (command == "config") {
        printConfig(cfg);
        return 0;
    }

    if (command == "update")
        return commandUpdate(cfg, options);

    if (command == "doctor")
        return commandDoctor(cfg, packages, manifestErrors);

    if (!manifestErrors.empty() &&
        command != "list" && command != "installed" && command != "clean") {
        std::cerr << "[saada] Manifest errors prevent this command:\n";
        for (const auto& e : manifestErrors) std::cerr << "  " << e << "\n";
        return 1;
    }

    if (command == "list") {
        printList(packages);
        return manifestErrors.empty() ? 0 : 1;
    }

    if (command == "installed") {
        const auto installed = loadInstalled(cfg);
        if (installed.empty()) {
            std::cout << "No Saada-managed packages.\n";
            return 0;
        }
        for (const auto& [key, record] : installed) {
            std::cout << "  " << record.at("name");
            if (record.count("version")) std::cout << " " << record.at("version");
            if (record.count("path")) std::cout << " -> " << record.at("path");
            std::cout << "\n";
        }
        return 0;
    }

    if (command == "clean")
        return commandClean(cfg, options);

    if (command == "install") {
        if (positional.size() < 2) {
            std::cerr << "[saada] Error: install requires a package name.\n";
            return 1;
        }
        return commandInstall(cfg, packages, positional[1], options);
    }

    if (command == "remove") {
        if (positional.size() < 2) {
            std::cerr << "[saada] Error: remove requires a package name.\n";
            return 1;
        }
        return commandRemove(cfg, positional[1], options);
    }

    if (command == "upgrade") {
        std::optional<std::string> package;
        if (positional.size() >= 2) package = positional[1];
        return commandUpgrade(cfg, packages, package, options);
    }

    if (command == "search") {
        if (positional.size() < 2) {
            std::cerr << "[saada] Error: search requires text.\n";
            return 1;
        }
        const std::string needle = lower(positional[1]);
        for (const auto& [key, p] : packages) {
            std::string haystack = lower(
                p.name + " " + p.version + " " + p.source + " " + p.archive);
            for (const auto& dep : p.dependencies) haystack += " " + dep;
            if (haystack.find(needle) != std::string::npos)
                std::cout << p.name << " " << p.version << "\n";
        }
        return 0;
    }

    if (command == "info") {
        if (positional.size() < 2) {
            std::cerr << "[saada] Error: info requires a package name.\n";
            return 1;
        }
        return commandInfo(packages, positional[1]);
    }

    if (command == "deps") {
        if (positional.size() < 2) {
            std::cerr << "[saada] Error: deps requires a package name.\n";
            return 1;
        }
        auto p = findPackage(packages, positional[1]);
        if (!p) {
            std::cerr << "[saada] Package not found: " << positional[1] << "\n";
            return 1;
        }
        std::set<std::string> seen;
        printDepsRecursive(packages, *p, 0, seen);
        return 0;
    }

    if (command == "verify") {
        if (positional.size() < 2) {
            std::cerr << "[saada] Error: verify requires a package name.\n";
            return 1;
        }
        return commandVerify(cfg, packages, positional[1]);
    }

    std::cerr << "[saada] Error: unknown command '" << command << "'\n\n";
    printHelp();
    return 1;
}
