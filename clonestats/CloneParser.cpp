#include <filesystem>
#include <iostream>
#include <fstream>
#include <ctgmath>

#include "CloneParser.h"

namespace fs = std::filesystem;


void CloneListener::startDocument() {
    // NO-OP
}

void CloneListener::whitespace(char c) {
    // NO-OP
}

void CloneListener::key(std::string key) {
    last_key = key;
}

void CloneListener::startObject() {
    if (state == root && last_key == "file_starts") {
        state = file_starts;
    } else if (state == root && last_key == "line_starts") {
        lines.emplace();
        state = line_starts;
    } else if (state == repeats) {
        state = repeat;
    } else if (state == start) {
        state = root;
    } else {
        throw std::runtime_error("Unexpected object start after key " + last_key + " in state " + states[state]);
    }
}

void CloneListener::endObject() {
    if (state == file_starts) {
        state = root;
        postFileStarts();
    } else if (state == line_starts) {
        state = root;
    } else if (state == repeat) {
        state = repeats;
        unsigned long size = current_repeat.text.size();

        if (current_repeat.length >= 0 && size != current_repeat.length) {
            throw std::runtime_error(std::string("Invalid length from parsed JSON: ") +
            " should be " + std::to_string(current_repeat.length) +
            " chars, got " + std::to_string(current_repeat.text.length()));
        }

        onRepeat(current_repeat);

        current_repeat.file_ids.clear();
        current_repeat.occurrences.clear();
        current_repeat.text.clear();
    } else if (state == root) {
        state = done;
    } else {
        throw std::runtime_error("Unexpected object end after key " + last_key + " in state " + states[state]);
    }
}

void CloneListener::value(std::string value) {
    if (state == file_starts) {
        unsigned id = files.size();
        std::string ext = std::filesystem::path(last_key).extension();
        unsigned long file_start = std::stoul(value);
        files[file_start] = {id, last_key, ext};
    } else if (state == line_starts) {
        (*lines)[std::stoul(value)] = std::stoul(last_key);
    } else if (state == positions) {
        unsigned long pos = std::stoul(value);
        FileData &source = (--files.upper_bound(pos))->second;
        current_repeat.occurrences.push_back(source);
        current_repeat.file_ids.insert(source.id);
        current_repeat.positions.push_back(pos);
    } else if (state == repeat && last_key == "text") {
        current_repeat.text = value;
    } else if (state == repeat && last_key == "length") {
        current_repeat.length = std::stol(value);
    } else if (state != root) {
        throw std::runtime_error("Unexpected value after key " + last_key + " in state " + states[state]);
    }
}

void CloneListener::startArray() {
    if (state == root && last_key == "repeats") {
        state = repeats;
    } else if (state == repeat && last_key == "positions") {
        state = positions;
    } else {
        throw std::runtime_error("Unexpected array start after key " + last_key + " in state " + states[state]);
    }
}

void CloneListener::endArray() {
    if (state == repeats) {
        state = root;
    } else if (state == positions) {
        state = repeat;
    } else {
        throw std::runtime_error("Unexpected array end after key " + last_key + " in state " + states[state]);
    }
}

void CloneListener::endDocument() {
    // NO-OP
}

void DistanceMatrixGenerator::printResults(std::ostream &out) {
    std::optional<std::ofstream> connect;

    if (connectivity) {
        connect = std::ofstream(*connectivity);

        if (!*connect) {
            std::cerr << "output file open fails. exit.\n";
            exit(1);
        }
    }

    const auto &concat_end = files.crbegin();  // special last pair
    if (!concat_end->second.name.empty()) throw std::runtime_error("Last file mapping should use the empty string as name");

    unsigned long file_lengths[files.size()];
    unsigned long last_file_start{};

    for (const auto &f : files) {
        unsigned int file_id = f.second.id;
        if (file_id > 0) {
            out << ",";
            file_lengths[file_id - 1] = f.first - last_file_start;
        }
        out << f.second.name;
        last_file_start = f.first;
    }

    out << "\n";

    for (int i = 0; i < similarity_matrix.size() - 1; i++) {    // discard the dummy
        for (int j = 0; j < similarity_matrix.size() - 1; j++) {
            if (j > 0) {   // no separator for the first value of the line
                out << ",";
                if (connect) *connect << ",";
            }
            unsigned long val = similarity_matrix.at(i, j) + similarity_matrix.at(j, i);
            if (i == j) {
                out << 0;
                if (connect) *connect << 1;
            } else if (val == 0) {
                out << 1e2;
                if (connect) *connect << 0;
            } else {
                out << log((double) (file_lengths[i] + file_lengths[j])) - log((double) val);
                if (connect) *connect << 1;
            }
        }
        out << "\n";
        if (connect) *connect << "\n";
    }
}

void DistanceMatrixGenerator::onRepeat(const RepeatData &repeat) {
    auto size = repeat.text.size();

    for (const FileData &source_file : repeat.occurrences) {
        // similarity[x, y] += (size of the repeat) * (combined number of occurrences in x and y)
        for (unsigned file_id : repeat.file_ids) {
            similarity_matrix.at(source_file.id, file_id) += size;
        }
    }
}

void DistanceMatrixGenerator::postFileStarts() {
    similarity_matrix = SimpleMatrix<unsigned long>(files.size());
}

void CountMatrixGenerator::onRepeat(const RepeatData &repeat) {
    for (const FileData &source_file : repeat.occurrences) {
        // similarity[x, y] += (size of the repeat) * (combined number of occurrences in x and y)
        for (unsigned file_id : repeat.file_ids) {
            count_matrix.at(source_file.id, file_id) += 1;
        }
    }
}

void CountMatrixGenerator::printResults(std::ostream &out) {
    const auto &concat_end = files.crbegin();  // special last pair
    if (!concat_end->second.name.empty()) throw std::runtime_error("Last file mapping should use the empty string as name");

    unsigned long file_count = files.size();
    unsigned long file_lengths[file_count];
    unsigned long last_file_start{};

    for (const auto &f : files) {
        unsigned int file_id = f.second.id;
        if (file_id < file_count - 1) {
            out << "File name,";
        }
        if (file_id > 0) {
            file_lengths[file_id - 1] = f.first - last_file_start;
        }
        out << f.second.name;
        last_file_start = f.first;
    }

    out << "\n";
    auto it = files.begin();

    for (int i = 0; i < count_matrix.size() - 1; i++) {    // discard the dummy
        out << it->second.name;
        ++it;
        for (int j = 0; j < count_matrix.size() - 1; j++) {
            out << ",";
            out << count_matrix.at(i, j) + count_matrix.at(j, i);
        }
        out << "\n";
    }
}

void CountMatrixGenerator::postFileStarts() {
    count_matrix = SimpleMatrix<unsigned long>(files.size());
}

void OccurrenceCsvGenerator::printResults(std::ostream &out) {
    out << "File extension,Size,Occurrence(s),unique sequence(s)\n";
    for (const auto &ext_entry : occurrences) {
        for (const auto &size_entry : ext_entry.second) {
            out << (ext_entry.first.empty() ? "(none)" : ext_entry.first) << ',';
            out << size_entry.first << "," << size_entry.second.total << ",";
            out << size_entry.second.unique;
            out << "\n";
        }
    }
}

void OccurrenceCsvGenerator::onRepeat(const RepeatData &repeat) {
    unsigned size = repeat.text.size();
    unsigned count = 0;
    std::unordered_set<extension> encountered_exts;
    std::unordered_map<unsigned, unsigned> repeats_per_file;

    for (const FileData &source_file : repeat.occurrences) {
        auto &occ = occurrences[source_file.ext][size];
        count++;
        occ.total++;
        repeats_per_file[source_file.id]++;

        // Only works if the entries in the JSON are unique themselves (no 2 entries with the same text)
        if (encountered_exts.insert(source_file.ext).second) {
            occ.unique++;
        }
    }
}

void TestCsvGenerator::onRepeat(const RepeatData &repeat) {
    if (!lines) throw std::runtime_error("Missing line mappings");

    unsigned long size = repeat.positions.size();
    for (int i = 0; i < size-1; i++) {
        for (int j = i+1; j < size; j++) {
            const auto &pos = repeat.positions[i];
            FileData &source = (--files.upper_bound(pos))->second;
            fs::path p1(source.name);
            unsigned long line = (--lines->upper_bound(pos))->second;
            std::cout << p1.parent_path().filename() << "," << p1.filename() << "," << line;
            const auto &pos2 = repeat.positions[j];
            FileData &source2 = (--files.upper_bound(pos2))->second;
            fs::path p2(source2.name);
            unsigned long line2 = (--lines->upper_bound(pos2))->second;
            std::cout << p2.parent_path().filename() << "," << p2.filename() << "," << line2;
        }
    }
}

void TestCsvGenerator::printResults(std::ostream &out) {
    // TODO
    throw std::runtime_error("TODO");
}
