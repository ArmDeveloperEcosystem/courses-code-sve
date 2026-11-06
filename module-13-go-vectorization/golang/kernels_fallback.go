//go:build !linux || !arm64 || !cgo

// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

package main

func detectSVE() bool {
	return false
}

func detectSVE2() bool {
	return false
}

func vectorAddSVE(dst, x, y []int32) {
	vectorAddScalar(dst, x, y)
}

func histogramSVE2(histogram, records []uint32) {
	histogramScalar(histogram, records)
}

func base64EncodeSVE(dst, src []byte) {
	base64EncodeScalar(dst, src)
}
