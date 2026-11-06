// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

package main

import (
	"encoding/base64"
	"fmt"
	"testing"
)

var (
	benchmarkInt32  int32
	benchmarkUint32 uint32
	benchmarkByte   byte
)

func BenchmarkVectorAdd(b *testing.B) {
	for _, size := range benchmarkSizes() {
		x, y, dst := int32Inputs(size)
		name := fmt.Sprintf("size_%d", size)

		b.Run(name+"/ScalarGo", func(b *testing.B) {
			b.SetBytes(int64(size * 3 * 4))
			for b.Loop() {
				vectorAddScalar(dst, x, y)
			}
			benchmarkInt32 = dst[size-1]
		})
		b.Run(name+"/CgoSVE", func(b *testing.B) {
			if !runtimeHasSVE {
				b.Skip("SVE is unavailable; refusing to label the Go fallback as CgoSVE")
			}
			b.SetBytes(int64(size * 3 * 4))
			for b.Loop() {
				VectorAdd(dst, x, y)
			}
			benchmarkInt32 = dst[size-1]
		})
	}
}

func BenchmarkHistogram(b *testing.B) {
	for _, size := range benchmarkSizes() {
		records := histogramInputs(size)
		name := fmt.Sprintf("size_%d", size)

		b.Run(name+"/ScalarGo", func(b *testing.B) {
			histogram := make([]uint32, HistogramBins)
			b.SetBytes(int64(size * 4))
			for b.Loop() {
				histogramScalar(histogram, records)
			}
			benchmarkUint32 = histogram[0]
		})
		b.Run(name+"/SVE2", func(b *testing.B) {
			if !runtimeHasSVE2 {
				b.Skip("SVE2 is unavailable; refusing to benchmark the Go fallback as SVE2")
			}
			histogram := make([]uint32, HistogramBins)
			b.SetBytes(int64(size * 4))
			for b.Loop() {
				Histogram(histogram, records)
			}
			benchmarkUint32 = histogram[0]
		})
	}
}

func BenchmarkBase64Encode(b *testing.B) {
	for _, size := range benchmarkSizes() {
		src := make([]byte, size)
		for i := range src {
			src[i] = byte(i*17 + 29)
		}
		encodedLen := base64.StdEncoding.EncodedLen(size)
		stdlibDst := make([]byte, encodedLen)
		sveDst := make([]byte, encodedLen)
		name := fmt.Sprintf("size_%d", size)

		b.Run(name+"/StdlibGo", func(b *testing.B) {
			b.SetBytes(int64(size))
			b.ReportAllocs()
			for b.Loop() {
				base64.StdEncoding.Encode(stdlibDst, src)
			}
			benchmarkByte = stdlibDst[encodedLen-1]
		})
		b.Run(name+"/CgoSVE", func(b *testing.B) {
			if !runtimeHasSVE {
				b.Skip("SVE is unavailable; refusing to label the Go fallback as CgoSVE")
			}
			b.SetBytes(int64(size))
			b.ReportAllocs()
			for b.Loop() {
				Base64Encode(sveDst, src)
			}
			benchmarkByte = sveDst[encodedLen-1]
		})
	}
}

func benchmarkSizes() []int {
	return []int{16, 256, 4096}
}

func int32Inputs(size int) (x, y, dst []int32) {
	x = make([]int32, size)
	y = make([]int32, size)
	dst = make([]int32, size)
	for i := range x {
		x[i] = int32(i*17 - 29)
		y[i] = int32(71 - i*13)
	}
	return x, y, dst
}

func histogramInputs(size int) []uint32 {
	records := make([]uint32, size)
	for i := range records {
		records[i] = uint32(i*17+29) & (HistogramBins - 1)
	}
	return records
}
