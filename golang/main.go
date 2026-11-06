//go:build go1.24

// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

package main

import (
	"encoding/base64"
	"fmt"
)

func main() {
	fmt.Println("System CPU features")
	fmt.Println()
	fmt.Printf("SVE available:  %t\n", runtimeHasSVE)
	fmt.Printf("SVE2 available: %t\n", runtimeHasSVE2)

	x := []int32{1, 2, 3, 4}
	y := []int32{10, 20, 30, 40}

	scalar := make([]int32, len(x))
	sveResult := make([]int32, len(x))
	vectorAddScalar(scalar, x, y)
	VectorAdd(sveResult, x, y)

	fmt.Println("\nRunning Vector Addition of ", x, " + ", y)
	fmt.Printf("Scalar Go: %v\n", scalar)
	fmt.Printf("%-11s %v\n", sveBackend()+":", sveResult)

	source := []byte("Hello, SVE from Go!")
	fmt.Println("\nRunning Base64 Encoding of \"Hello, SVE from Go!\"")
	encoded := make([]byte, base64.StdEncoding.EncodedLen(len(source)))
	Base64Encode(encoded, source)
	fmt.Printf("Base64 (%s): %s\n", sveBackend(), encoded)
}

func sveBackend() string {
	if runtimeHasSVE {
		return "Cgo SVE"
	}
	return "Go fallback"
}
