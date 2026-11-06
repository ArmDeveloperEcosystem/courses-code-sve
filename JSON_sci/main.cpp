/// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates
// <open-source-office@arm.com> SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "JSON.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using std::cerr;
using std::cout;
using std::ifstream;
using std::istream;
using std::ostream;
using std::string;

void usage(ostream &out, const char *prog) {
    out << "Usage: " << prog << " [JSON_file]\n";
    out << "Options:\n"
        << "  --help, -h    Show this help message\n";
    out << "If no JSON_file is provided, the program will read from standard "
           "input.\n";
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
        usage(cerr, argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        const auto option = string(argv[1]);
        if (option == "-h" || option == "--help") {
            usage(cout, argv[0]);
            return EXIT_SUCCESS;
        }
    }

    ifstream file;
    istream *input = &std::cin;
    if (argc == 2) {
        const auto json_file = argv[1];
        file.open(json_file);
        if (!file) {
            cerr << "Error: Could not open file " << json_file << '\n';
            return EXIT_FAILURE;
        }
        input = &file;
    }

    const auto json = string((std::istreambuf_iterator<char>(*input)),
                             std::istreambuf_iterator<char>());
    cout << "Input JSON:\n" << json << "\n\n";

    const auto indices = JSON::SCI(json).get_ref();

    if (indices.empty()) {
        cout << "No structural characters found.\n";
        return EXIT_SUCCESS;
    }

    cout << "Structural characters found at indices:\n";
    for (const auto idx : indices) {
        cout << idx << ' ';
    }
    cout << '\n';

    return EXIT_SUCCESS;
}
