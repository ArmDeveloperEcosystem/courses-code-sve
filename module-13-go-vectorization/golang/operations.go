//go:build go1.24

// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

package main

import "encoding/base64"

// Snapshot the operating system's feature report once. Variables also let the
// tests force the unavailable-feature paths without requiring different CPUs.
var (
	runtimeHasSVE  = detectSVE()
	runtimeHasSVE2 = detectSVE2()
)

// HistogramBins is the number of counters required by Histogram.
const HistogramBins = 256

// VectorAdd adds x and y element-wise into dst. It uses the cgo SVE kernel
// when available and otherwise uses scalar Go. Arithmetic has Go's defined
// wrapping int32 semantics.
//
// The three slices must have equal lengths. dst may be exactly the same slice
// as x or y; partially overlapping slices are not supported.
func VectorAdd(dst, x, y []int32) {
	checkVectorLengths(len(dst), len(x), len(y))
	if len(dst) == 0 {
		return
	}
	// Tip: If VectorAdd is called frequently, avoid repeated runtime checks by performing
	// feature detection at a higher level and dispatching directly to the SVE variant.
	// This may also enable additional compiler optimizations.
	if runtimeHasSVE {
		vectorAddSVE(dst, x, y)
		return
	}
	vectorAddScalar(dst, x, y)
}

func vectorAddScalar(dst, x, y []int32) {
	for i := range dst {
		dst[i] = x[i] + y[i]
	}
}

// Base64Encode encodes src with standard padded Base64. It uses the cgo SVE
// kernel when available and otherwise uses encoding/base64. dst must have room
// for ((len(src) + 2) / 3) * 4 bytes and must not overlap src.
func Base64Encode(dst, src []byte) {
	encodedLen := (len(src) + 2) / 3 * 4
	if len(dst) < encodedLen {
		panic("sve: base64 destination too short")
	}
	if encodedLen != 0 {
		if runtimeHasSVE {
			base64EncodeSVE(dst[:encodedLen], src)
			return
		}
		base64EncodeScalar(dst[:encodedLen], src)
	}
}

func base64EncodeScalar(dst, src []byte) {
	base64.StdEncoding.Encode(dst, src)
}

// Histogram adds the frequency of each record to histogram. It uses the cgo
// SVE2 kernel when available and otherwise uses scalar Go. Records are bin
// indices and must be in [0, HistogramBins).
//
// histogram must contain exactly HistogramBins counters and must not overlap
// records. Existing counter values are preserved and incremented.
func Histogram(histogram, records []uint32) {
	checkHistogram(histogram)
	if len(records) == 0 {
		return
	}
	if runtimeHasSVE2 {
		histogramSVE2(histogram, records)
		return
	}
	histogramScalar(histogram, records)
}

func histogramScalar(histogram, records []uint32) {
	for _, record := range records {
		histogram[record]++
	}
}

func checkVectorLengths(dst, x, y int) {
	if dst != x || dst != y {
		panic("sve: vector lengths differ")
	}
}

func checkHistogram(histogram []uint32) {
	if len(histogram) != HistogramBins {
		panic("sve: histogram must contain 256 bins")
	}
}
