// clang-format off
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear
// clang-format on
#include "VSS.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using std::cerr;
using std::cout;
using std::ostream;
using std::size_t;
using std::span;
using std::string;
using std::string_view;
using std::vector;

using VSS::Embedding;
using VSS::make_embedding;
using VSS::Metric;
using VSS::similarity_ref;

string progname{"VSS"};

struct MetricResults {
    using Ty = Metric::ResultTy;
    Ty euclidean;
    Ty dot;
    Ty cosine;
};

bool is_help_argument(string_view arg) {
    return arg == "-h" || arg == "--help";
}

bool is_supported_dimension(size_t size) {
    return size == 8 || size == 384 || size == 768 || size == 1024 ||
           size == 3072;
}

[[noreturn]] void usage(ostream &out, int exit_code,
                        string_view error_msg = "") {
    if (!error_msg.empty())
        out << "Error: " << error_msg << '\n';

    out << "Usage: " << progname << " \"vector1 string\"  \"vector2 string\"\n";
    out << "Compute the similarity between 2 vectors using all metrics: "
           "euclidean distance, dot product and cosine similarity.\n";
    out << "Options:\n"
        << "  --help, -h    Show this help message\n";
    out << '\n';
    out << "Each vector string is delimited by opening and closing quotes and "
           "contains space-separated floating point numbers. The 2 vectors "
           "must have the same length. Currently only sizes 8, 384, 768, "
           "1024 and 3072 are supported\n";
    exit(exit_code);
}

template <typename Ty> Ty parse_token(const string &token) {
    size_t parsed_chars = 0;
    try {
        static_assert(std::is_same_v<Ty, float>,
                      "Only float is supported for now");
        const auto value = std::stof(token, &parsed_chars);
        if (parsed_chars != token.size())
            usage(cerr, EXIT_FAILURE, "Invalid number in vector: " + token);
        return value;
    } catch (const std::invalid_argument &) {
        usage(cerr, EXIT_FAILURE, "Invalid number in vector: " + token);
    } catch (const std::out_of_range &) {
        usage(cerr, EXIT_FAILURE, "Number out of range in vector: " + token);
    }
}

template <typename Ty> vector<Ty> parse_vector(string_view str) {
    vector<Ty> vec;
    std::istringstream input{string(str)};
    string token;
    while (input >> token)
        vec.push_back(parse_token<Ty>(token));

    if (!is_supported_dimension(vec.size()))
        usage(cerr, EXIT_FAILURE,
              "Invalid vector size: " + std::to_string(vec.size()));

    return vec;
}

template <size_t N, typename Ty>
void display_vector(string_view name, const Embedding<N, Ty> &embedding) {
    cout << name << '[' << embedding.size() << "]: [";
    for (size_t i = 0; i < embedding.size(); ++i) {
        cout << embedding[i];
        if (i < embedding.size() - 1)
            cout << ", ";
    }
    cout << "]\n";
}

template <size_t N, typename Ty>
MetricResults compute_metrics(span<const Ty> vector1, span<const Ty> vector2) {
    const auto embedding1 = make_embedding<N, Ty>(vector1);
    const auto embedding2 = make_embedding<N, Ty>(vector2);

    display_vector("Vector1", embedding1);
    display_vector("Vector2", embedding2);

    return {
        .euclidean = similarity_ref<N, Ty, Metric::EUCLIDEAN_DISTANCE>(
            embedding1, embedding2),
        .dot =
            similarity_ref<N, Ty, Metric::DOT_PRODUCT>(embedding1, embedding2),
        .cosine = similarity_ref<N, Ty, Metric::COSINE_SIMILARITY>(embedding1,
                                                                   embedding2),
    };
}

int main(int argc, char *argv[]) {
    progname = string(argv[0]);

    if (argc == 2 && is_help_argument(argv[1]))
        usage(cout, EXIT_SUCCESS);

    if (argc != 3)
        usage(cerr, EXIT_FAILURE, "Invalid number of arguments");

    const auto vector1 = parse_vector<float>(argv[1]);
    const auto vector2 = parse_vector<float>(argv[2]);
    if (vector1.size() != vector2.size())
        usage(cerr, EXIT_FAILURE, "Vectors must have the same length");

    const auto size = vector1.size();
    MetricResults metrics;
    switch (size) {
    case 8:
        metrics = compute_metrics<8, float>(vector1, vector2);
        break;
    case 384:
        metrics = compute_metrics<384, float>(vector1, vector2);
        break;
    case 768:
        metrics = compute_metrics<768, float>(vector1, vector2);
        break;
    case 1024:
        metrics = compute_metrics<1024, float>(vector1, vector2);
        break;
    case 3072:
        metrics = compute_metrics<3072, float>(vector1, vector2);
        break;
    default:
        usage(cerr, EXIT_FAILURE,
              "Unsupported vector size: " + std::to_string(size));
    }

    cout << "Euclidean distance: " << metrics.euclidean << '\n';
    cout << "Dot product: " << metrics.dot << '\n';
    cout << "Cosine similarity: " << metrics.cosine << '\n';

    return EXIT_SUCCESS;
}
