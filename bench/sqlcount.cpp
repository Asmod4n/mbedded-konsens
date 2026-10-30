#include <dlfcn.h>
#include <sqlite3.h>
#include <unistd.h>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
namespace {
std::mutex m;
std::map<std::string, unsigned long>* counts = new std::map<std::string, unsigned long>;
struct dump {
    ~dump()
    {
        if (counts->empty()) return;
        const char* d = getenv("SQLCOUNT_DIR");
        std::string p = std::string(d ? d : "/tmp") + "/sqlcount." + std::to_string(getpid());
        FILE* f = fopen(p.c_str(), "w");
        for (auto& [k, v] : *counts) fprintf(f, "%lu\t%s\n", v, k.c_str());
        fclose(f);
    }
} at_exit;
}
extern "C" int sqlite3_step(sqlite3_stmt* s)
{
    static auto real = reinterpret_cast<int (*)(sqlite3_stmt*)>(dlsym(RTLD_NEXT, "sqlite3_step"));
    { std::lock_guard l(m); const char* q = sqlite3_sql(s); (*counts)[q ? q : "?"] += 1; }
    return real(s);
}
