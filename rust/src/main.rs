// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: BSD-3-Clause-Clear

#![feature(portable_simd)]
#![cfg_attr(target_arch = "aarch64", feature(stdarch_aarch64_sve))]

use std::env;
use std::error::Error;
use std::fs;
use std::simd::{Simd, num::SimdFloat};
use std::time::{Duration, Instant};

fn load_columns(path: &str) -> Result<(Vec<f32>, Vec<f32>), Box<dyn Error>> {
    let input = fs::read_to_string(path)?;
    let mut first = Vec::new();
    let mut second = Vec::new();

    for (line_number, line) in input.lines().enumerate() {
        if line.trim().is_empty() {
            continue;
        }

        let mut fields = line.split_whitespace();
        let a = fields
            .next()
            .ok_or_else(|| format!("line {}: expected two columns", line_number + 1))?;
        let b = fields
            .next()
            .ok_or_else(|| format!("line {}: expected two columns", line_number + 1))?;
        if fields.next().is_some() {
            return Err(format!("line {}: expected exactly two columns", line_number + 1).into());
        }

        first.push(
            a.parse::<f32>()
                .map_err(|error| format!("line {}, column 1: {error}", line_number + 1))?,
        );
        second.push(
            b.parse::<f32>()
                .map_err(|error| format!("line {}, column 2: {error}", line_number + 1))?,
        );
    }

    Ok((first, second))
}

#[inline(never)]
fn squared_difference_scalar(first: &[f32], second: &[f32]) -> f32 {
    first
        .iter()
        .zip(second)
        .map(|(&a, &b)| {
            let difference = a - b;
            difference * difference
        })
        .sum()
}

#[inline(never)]
fn squared_difference_simd(first: &[f32], second: &[f32]) -> f32 {
    const LANES: usize = 16;
    let vector_length = first.len() / LANES * LANES;
    let mut sum = Simd::<f32, LANES>::splat(0.0);

    for i in (0..vector_length).step_by(LANES) {
        let difference = Simd::from_slice(&first[i..]) - Simd::from_slice(&second[i..]);
        sum += difference * difference;
    }

    sum.reduce_sum() + squared_difference_scalar(&first[vector_length..], &second[vector_length..])
}

#[cfg(target_arch = "aarch64")]
#[inline(never)]
#[target_feature(enable = "sve")]
unsafe fn squared_difference_sve(first: &[f32], second: &[f32]) -> f32 {
    use std::arch::aarch64::*;

    let mut acc = svdup_n_f32(0.0);
    let mut i = 0;
    while i < first.len() {
        let predicate = svwhilelt_b32_u64(i as u64, first.len() as u64);
        // SAFETY: the predicate disables every lane at or beyond the slice boundary.
        let a = unsafe { svld1_f32(predicate, first.as_ptr().add(i)) };
        let b = unsafe { svld1_f32(predicate, second.as_ptr().add(i)) };
        let difference = svsub_f32_x(predicate, a, b);
        acc = svmla_f32_m(predicate, acc, difference, difference);
        i += svcntw() as usize;
    }

    svaddv_f32(svptrue_b32(), acc)
}

fn compute_timed(
    implementation: &str,
    first: &[f32],
    second: &[f32],
) -> Result<(f32, Duration), String> {
    match implementation {
        "scalar" => {
            let start = Instant::now();
            let result = squared_difference_scalar(first, second);
            Ok((result, start.elapsed()))
        }
        "simd" => {
            let start = Instant::now();
            let result = squared_difference_simd(first, second);
            Ok((result, start.elapsed()))
        }
        "sve" => {
            #[cfg(target_arch = "aarch64")]
            {
                if !std::arch::is_aarch64_feature_detected!("sve") {
                    return Err("SVE is not available on this CPU".into());
                }
                // SAFETY: SVE support was checked immediately above.
                let start = Instant::now();
                let result = unsafe { squared_difference_sve(first, second) };
                return Ok((result, start.elapsed()));
            }
            #[cfg(not(target_arch = "aarch64"))]
            Err("SVE is only available on AArch64".into())
        }
        _ => Err(format!(
            "unknown implementation '{implementation}'; use scalar, simd, or sve"
        )),
    }
}

fn main() -> Result<(), Box<dyn Error>> {
    let mut arguments = env::args();
    let program = arguments
        .next()
        .unwrap_or_else(|| "squared-difference".into());
    let implementation = arguments
        .next()
        .ok_or_else(|| format!("usage: {program} <scalar|simd|sve> <file>"))?;
    let path = arguments
        .next()
        .ok_or_else(|| format!("usage: {program} <scalar|simd|sve> <file>"))?;
    if arguments.next().is_some() {
        return Err(format!("usage: {program} <scalar|simd|sve> <file>").into());
    }

    let (first, second) = load_columns(&path)?;
    let (result, elapsed) = compute_timed(&implementation, &first, &second)?;

    println!("accumulated squared difference: {result}");
    println!("computation time: {elapsed:?}");
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scalar_and_simd_agree() {
        let first: Vec<_> = (0..37).map(|x| x as f32 * 0.5).collect();
        let second: Vec<_> = (0..37).map(|x| x as f32 * 0.25).collect();
        let scalar = squared_difference_scalar(&first, &second);
        let simd = squared_difference_simd(&first, &second);
        assert!((scalar - simd).abs() <= 0.0001);
    }

    #[cfg(target_arch = "aarch64")]
    #[test]
    fn scalar_and_sve_agree() {
        if !std::arch::is_aarch64_feature_detected!("sve") {
            eprintln!("skipping SVE comparison: SVE is not available on this CPU");
            return;
        }

        let first: Vec<_> = (0..37).map(|x| x as f32 * 0.5).collect();
        let second: Vec<_> = (0..37).map(|x| x as f32 * 0.25).collect();
        let scalar = squared_difference_scalar(&first, &second);
        // SAFETY: SVE support was checked immediately above.
        let sve = unsafe { squared_difference_sve(&first, &second) };
        assert!((scalar - sve).abs() <= 0.0001);
    }
}
