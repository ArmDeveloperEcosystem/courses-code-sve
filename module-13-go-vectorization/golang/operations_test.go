// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

package main

import (
	"encoding/base64"
	"math"
	"slices"
	"strconv"
	"testing"
)

func TestVectorAdd(t *testing.T) {
	implementations := []struct {
		name string
		add  func(dst, x, y []int32)
	}{
		{"Scalar", vectorAddScalar},
		{"Automatic", VectorAdd},
	}

	for _, size := range testSizes() {
		x := make([]int32, size)
		y := make([]int32, size)
		for i := range x {
			x[i] = int32(i*17 - 91)
			y[i] = int32(37 - i*11)
		}

		want := make([]int32, size)
		for i := range want {
			want[i] = x[i] + y[i]
		}

		for _, implementation := range implementations {
			t.Run(implementation.name+"/"+testName(size), func(t *testing.T) {
				got := make([]int32, size)
				implementation.add(got, x, y)
				if !slices.Equal(got, want) {
					t.Fatalf("addition = %v, want %v", got, want)
				}
			})
		}
	}
}

func TestVectorAddWraps(t *testing.T) {
	x := []int32{math.MaxInt32, math.MinInt32, -1}
	y := []int32{1, -1, math.MinInt32}
	want := []int32{math.MinInt32, math.MaxInt32, math.MaxInt32}

	implementations := map[string]func(dst, x, y []int32){
		"Scalar":    vectorAddScalar,
		"Automatic": VectorAdd,
	}
	for name, add := range implementations {
		t.Run(name, func(t *testing.T) {
			got := make([]int32, len(x))
			add(got, x, y)
			if !slices.Equal(got, want) {
				t.Fatalf("addition = %v, want %v", got, want)
			}
		})
	}
}

func TestVectorAddInPlace(t *testing.T) {
	y := []int32{10, 20, 30, 40}
	want := []int32{11, 22, 33, 44}

	implementations := map[string]func(dst, x, y []int32){
		"Scalar":    vectorAddScalar,
		"Automatic": VectorAdd,
	}
	for name, add := range implementations {
		t.Run(name, func(t *testing.T) {
			x := []int32{1, 2, 3, 4}
			add(x, x, y)
			if !slices.Equal(x, want) {
				t.Fatalf("in-place addition = %v, want %v", x, want)
			}
		})
	}
}

func TestVectorAddLengthMismatchPanics(t *testing.T) {
	defer func() {
		if recover() == nil {
			t.Fatal("expected length mismatch panic")
		}
	}()
	VectorAdd(make([]int32, 1), make([]int32, 2), make([]int32, 1))
}

func TestHistogram(t *testing.T) {
	for _, records := range [][]uint32{nil, {0, 3, 3, 3, 255}} {
		want := make([]uint32, HistogramBins)
		want[3] = 7
		got := slices.Clone(want)

		histogramScalar(want, records)
		Histogram(got, records)
		if !slices.Equal(got, want) {
			t.Fatalf("Histogram() = %v, want %v", got, want)
		}
	}
}

func TestHistogramLengthPanics(t *testing.T) {
	defer func() {
		if recover() == nil {
			t.Fatal("expected histogram length panic")
		}
	}()
	Histogram(make([]uint32, HistogramBins-1), nil)
}

func TestBase64Encode(t *testing.T) {
	for _, size := range testSizes() {
		t.Run(testName(size), func(t *testing.T) {
			src := make([]byte, size)
			for i := range src {
				src[i] = byte(i*17 + 29)
			}
			want := make([]byte, base64.StdEncoding.EncodedLen(size))
			base64.StdEncoding.Encode(want, src)
			got := make([]byte, len(want)+1)
			got[len(want)] = 0xa5

			Base64Encode(got[:len(want)], src)
			if !slices.Equal(got[:len(want)], want) {
				t.Fatalf("Base64Encode() = %q, want %q", got[:len(want)], want)
			}
			if got[len(want)] != 0xa5 {
				t.Fatal("Base64Encode() wrote past the destination")
			}
		})
	}
}

func TestBase64EncodeShortDestinationPanics(t *testing.T) {
	defer func() {
		if recover() == nil {
			t.Fatal("expected short destination panic")
		}
	}()
	Base64Encode(make([]byte, 3), []byte("SVE"))
}

func TestRuntimeFeatureFallbacks(t *testing.T) {
	oldSVE, oldSVE2 := runtimeHasSVE, runtimeHasSVE2
	runtimeHasSVE, runtimeHasSVE2 = false, false
	t.Cleanup(func() {
		runtimeHasSVE, runtimeHasSVE2 = oldSVE, oldSVE2
	})

	x := []int32{1, 2, 3, 4}
	y := []int32{10, 20, 30, 40}
	gotVector := make([]int32, len(x))
	VectorAdd(gotVector, x, y)
	wantVector := []int32{11, 22, 33, 44}
	if !slices.Equal(gotVector, wantVector) {
		t.Fatalf("VectorAdd fallback = %v, want %v", gotVector, wantVector)
	}

	src := []byte("runtime fallback")
	wantBase64 := make([]byte, base64.StdEncoding.EncodedLen(len(src)))
	base64.StdEncoding.Encode(wantBase64, src)
	gotBase64 := make([]byte, len(wantBase64))
	Base64Encode(gotBase64, src)
	if !slices.Equal(gotBase64, wantBase64) {
		t.Fatalf("Base64Encode fallback = %q, want %q", gotBase64, wantBase64)
	}

	records := []uint32{1, 1, 3}
	wantHistogram := make([]uint32, HistogramBins)
	histogramScalar(wantHistogram, records)
	gotHistogram := make([]uint32, HistogramBins)
	Histogram(gotHistogram, records)
	if !slices.Equal(gotHistogram, wantHistogram) {
		t.Fatal("Histogram fallback differs from scalar Go")
	}
}

func testSizes() []int {
	return []int{0, 1, 2, 3, 7, 16, 17, 63, 64, 65, 257}
}

func testName(size int) string {
	return "size_" + strconv.Itoa(size)
}
