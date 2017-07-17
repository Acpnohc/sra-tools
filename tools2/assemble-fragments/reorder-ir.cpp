/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include "vdb.hpp"
#include "writer.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

template <typename T>
static std::ostream &write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::RawData const &in)
{
    return out.value(cid, in.elements, (T const *)in.data);
}

template <typename T>
static std::ostream &write(VDB::Writer const &out, unsigned const cid, VDB::Cursor::Data const *in)
{
    return out.value(cid, in->elements, (T const *)in->data());
}

static int process(VDB::Writer const &out, VDB::Database const &inDb, int64_t const *const beg, int64_t const *const end)
{
    static char const *const FLDS[] = { "READ_GROUP", "FRAGMENT", "READNO", "SEQUENCE", "REFERENCE", "STRAND", "POSITION", "CIGAR" };
    auto const in = inDb["RAW"].read(8, FLDS);
    auto const range = in.rowRange();
    if (end - beg != range.second - range.first) {
        std::cerr << "index size doesn't match input table" << std::endl;
        return -1;
    }
    auto const freq = (range.second - range.first) / 100.0;
    auto nextReport = 1;
    uint64_t written = 0;
    size_t const bufferSize = 4ul * 1024ul * 1024ul * 1024ul;
    auto buffer = malloc(bufferSize);
    auto bufEnd = (void const *)((uint8_t const *)buffer + bufferSize);
    auto blockSize = 0;
    
    std::cerr << "info: processing " << (range.second - range.first) << " records" << std::endl;
    std::cerr << "info: record storage is " << bufferSize / 1024 / 1024 << "MB" << std::endl;
    for (auto i = beg; i != end; ) {
    AGAIN:
        auto j = i + (blockSize == 0 ? 1000000 : blockSize);
        if (j > end)
            j = end;
        auto m = std::map<int64_t, unsigned>();
        {
            auto cp = buffer;
            auto v = std::vector<int64_t>(i, j);
            unsigned count = 0;
            
            std::sort(v.begin(), v.end());
            // std::cerr << "prog: processing " << v.size() << " records (" << v[0] << "-" << v[v.size() - 1] << ")" << std::endl;
            for (auto r : v) {
                auto const tmp = ((uint8_t const *)cp - (uint8_t const *)buffer) / 4;
                for (auto i = 0; i < 8; ++i) {
                    auto const data = in.read(r, i + 1);
                    auto p = data.copy(cp, bufEnd);
                    if (p == nullptr) {
                        std::cerr << "warn: filled buffer; reducing block size and restarting!" << std::endl;
                        blockSize = 0.99 * count;
                        std::cerr << "block size: " << blockSize << std::endl;
                        goto AGAIN;
                    }
                    cp = p->end();
                }
                ++count;
                m[r] = (unsigned)tmp;
            }
            if (blockSize == 0) {
                auto const avg = double(((uint8_t *)cp - (uint8_t *)buffer)) / count;
                std::cerr << "info: average record size is " << size_t(avg + 0.5) << " bytes" << std::endl;
                blockSize = 0.95 * (double(((uint8_t *)bufEnd - (uint8_t *)buffer)) / avg);
                std::cerr << "block size: " << blockSize << std::endl;
            }
        }
        for (auto k = i; k != j; ++k) {
            auto data = (VDB::Cursor::Data const *)((uint8_t const *)buffer + m[*k] * 4);
            
            write<char   >(out, 1, data);
            write<char   >(out, 2, data = data->next());
            write<int32_t>(out, 3, data = data->next());
            write<char   >(out, 4, data = data->next());
            write<char   >(out, 5, data = data->next());
            write<char   >(out, 6, data = data->next());
            write<int32_t>(out, 7, data = data->next());
            write<char   >(out, 8, data = data->next());
            out.closeRow(1);
            ++written;
            if (nextReport * freq <= written) {
                std::cerr << "prog: processed " << nextReport << "%" << std::endl;
                ++nextReport;
            }
        }
        i = j;
    }
    std::cerr << "Done" << std::endl;

    return 0;
}

static ssize_t fsize(int const fd)
{
    struct stat stat = {0};
    if (fstat(fd, &stat) == 0)
        return stat.st_size;
    return -1;
}

static int process(char const *const irdb, char const *const indexFile)
{
    int64_t const *index;
    size_t rows;
    {
        auto const fd = open(indexFile, O_RDONLY);
        if (fd < 0) {
            perror("failed to open index file");
            exit(1);
        }
        auto const size = fsize(fd);
        auto const map = mmap(0, size, PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);
        close(fd);
        if (map == MAP_FAILED) {
            perror("failed to mmap index file");
            exit(1);
        }
        index = (int64_t *)map;
        rows = size / sizeof((*index));
    }
#if 0
    auto const writer = VDB::Writer(std::cout);
#else
    auto devNull = std::ofstream("/dev/null");
    auto const writer = VDB::Writer(devNull);
#endif
    
    writer.destination("IR.vdb");
    writer.schema("aligned-ir.schema.text", "NCBI:db:IR:raw");
    writer.info("reorder-ir", "1.0.0");
    
    writer.openTable(1, "RAW");
    writer.openColumn(1, 1, 8, "READ_GROUP");
    writer.openColumn(2, 1, 8, "FRAGMENT");
    writer.openColumn(3, 1, 32, "READNO");
    writer.openColumn(4, 1, 8, "SEQUENCE");
    writer.openColumn(5, 1, 8, "REFERENCE");
    writer.openColumn(6, 1, 8, "STRAND");
    writer.openColumn(7, 1, 32, "POSITION");
    writer.openColumn(8, 1, 8, "CIGAR");
    
    writer.beginWriting();
    
    auto const mgr = VDB::Manager();
    auto const result = process(writer, mgr[irdb], index, index + rows);
    
    writer.endWriting();
    
    return result;
}

int main(int argc, char *argv[])
{
    if (argc == 3)
        return process(argv[1], argv[2]);
    else {
        std::cerr << "usage: " << VDB::programNameFromArgv0(argv[0]) << " <ir db> <clustering index>" << std::endl;
        return 1;
    }
}
