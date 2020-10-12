## Hardware

The RPI 4 has a:
 
- BCM2711: Quad-core Cortex-A72 ARM v8-A 64-bit (Requires upgrading from
  Raspbian for 64-bit support)
- NEON SIMD extensions are mandatory per core
- VFPv4 Floating Point Unit onboard (per core)
- 32 64-bit FPU registers
- fused multiply-accumulate instructions


## Math

                          N-n-1
We need to compute r(n) = sum_k x(k)*x(k + n)
                          k=0

As the dot product,

    r(n) = x[0:N-n] dot x[n:N]

If there are 2048 samples, an S16 will overflow if the mean is above

    sqrt(2^15 / 2048) = 4

so it can't be done in-place. An equivalent S32 won't overflow until a mean of

    sqrt(2^31 / 2048) = 1024

but overflow becomes a non-issue for BLAS, which requires floats.

## BLAS

We aren't using anything in GSL that BLAS doesn't already have, so use the
latter. The input type is float. The output type can be float or double.

For the dot-product method, vectors will be aliased (overlap). GSL says:

> If the arguments will not be modified (for example, if a function prototype 
> declares them as const arguments) then overlapping or aliased memory regions 
> can be safely used.

BLAS "scalar" dot product is level 1 (vector*vector); we have to loop N times
calling `cblas_sdot` and offsetting the second vector every time.

Level 2 (matrix*vector) would use an "anti-triangular" matrix that is symmetric,
calling `cblas_ssymv`. Unfortunately `lda` does not permit row pitch hacks that
would allow us to reuse a single vector.

Alternatively, for the reversed-triangular case, we could use `cblas_strmv`, but
the same pitch restriction applies. This means that we can't have a row-aliased
matrix, because it doesn't use array-of-array format; it uses a contiguous 
block.

