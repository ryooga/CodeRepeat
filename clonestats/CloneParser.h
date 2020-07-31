#pragma once

#include <string>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <vector>
#include "json/JsonListener.h"

using extension = std::string;
using fileid = unsigned;

struct OccurrenceCounter {
    unsigned unique;
    unsigned total;
};

struct FileData {
    unsigned id;
    std::string name;
    std::string ext;
};

struct RepeatDigest {
    std::string text;
    unsigned long occurrences;
};

template<typename T>
class SimpleMatrix {
private:
    std::unique_ptr<T[]> matrix;
    unsigned n{};

    [[nodiscard]] T &_at(unsigned x, unsigned y) const {
        if (x >= n || y >= n)
            throw std::length_error(
                    std::string("Invalid index [") + std::to_string(x) + ", " + std::to_string(y) + "]");

        return matrix[(x * n + y)];
    }

public:
    explicit SimpleMatrix(unsigned size) : n(size), matrix(new T[size * size]) {};

    [[nodiscard]] T &at(unsigned x, unsigned y) {
        return _at(x, y);
    }

    [[nodiscard]] const T &at(unsigned x, unsigned y) const {
        return _at(x, y);
    }

    [[nodiscard]] unsigned size() const {
        return n;
    }
};

struct Statistics {
    // extension -> repeat size -> number of occurrences
    std::unordered_map<std::string, std::map<unsigned long, OccurrenceCounter>> occurrences;
    // subtext -> number of occurrences
    std::vector<RepeatDigest> repeats;
    // [file id, file id] -> similarity
    SimpleMatrix<unsigned long> similarity_matrix{0};
    // [file id, file id] -> number of clones
    SimpleMatrix<unsigned long> count_matrix{0};
};

enum State {
    start, root, file_starts, line_starts, repeats, repeat, positions, done
};

static const char *states[] = {"start", "root", "file starts", "line starts", "repeats", "repeat", "positions", "done"};

struct RepeatData {
    std::string text;
    long length{-1};
    std::vector<FileData> occurrences{};
    std::unordered_set<unsigned> file_ids{};
};

class CloneListener : public JsonListener {
private:
    State state{start};
    RepeatData current_repeat{};
    std::string last_key{};
    // position in the concatenated file -> extension+id of the source file
    // will be empty if the "file_starts" object is not parsed before the "repeats"!
    std::map<unsigned long, FileData> &files;
    // position in the concatenated file -> line number in the source file
    // may be empty if the "line_starts" object does not exist
    std::optional<std::map<unsigned long, unsigned long>> &lines;
    Statistics &statistics;

public:
    CloneListener(std::map<unsigned long, FileData> &files, Statistics &statistics,
                  std::optional<std::map<unsigned long, unsigned long>> &lines) :
            files(files), statistics(statistics), lines(lines) {};

    void whitespace(char c) override;

    void startDocument() override;

    void key(std::string key) override;

    void value(std::string value) override;

    void endArray() override;

    void endObject() override;

    void endDocument() override;

    void startArray() override;

    void startObject() override;
};
