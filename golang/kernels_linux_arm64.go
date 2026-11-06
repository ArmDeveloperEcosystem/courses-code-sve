//go:build linux && arm64 && cgo

// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

package main

/*
#cgo CXXFLAGS: -O3 -std=c++20 -Wall -Wextra -march=armv8-a
#cgo LDFLAGS: -lstdc++
#cgo noescape sve_vector_addition_i32
#cgo nocallback sve_vector_addition_i32
#cgo noescape sve2_histogram_u32
#cgo nocallback sve2_histogram_u32
#cgo noescape base64_sve_encode
#cgo nocallback base64_sve_encode
#include "vector_kernels.h"
*/
import "C"

import (
	"unsafe"

	"golang.org/x/sys/cpu"
)

func detectSVE() bool {
	return cpu.ARM64.HasSVE
}

func detectSVE2() bool {
	return cpu.ARM64.HasSVE2
}

func vectorAddSVE(dst, x, y []int32) {
	C.sve_vector_addition_i32(
		(*C.int32_t)(unsafe.Pointer(&dst[0])),
		(*C.int32_t)(unsafe.Pointer(&x[0])),
		(*C.int32_t)(unsafe.Pointer(&y[0])),
		C.size_t(len(dst)),
	)
}

func histogramSVE2(histogram, records []uint32) {
	C.sve2_histogram_u32(
		(*C.uint32_t)(unsafe.Pointer(&histogram[0])),
		(*C.uint32_t)(unsafe.Pointer(&records[0])),
		C.size_t(len(records)),
	)
}

func base64EncodeSVE(dst, src []byte) {
	C.base64_sve_encode(
		(*C.uint8_t)(unsafe.Pointer(&src[0])),
		C.size_t(len(src)),
		(*C.char)(unsafe.Pointer(&dst[0])),
	)
}
