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

## GSL/BLAS

The input type prefix will be `gsl_block_short`. If the output is integral, it
will be `gsl_block_long`. However, BLAS only supports float or double.

For the dot-product method, vectors will be aliased (overlap). GSL says:

> If the arguments will not be modified (for example, if a function prototype 
> declares them as const arguments) then overlapping or aliased memory regions 
> can be safely used.

"scalar" dot product is level 1 (vector-vector); we'd have to loop N times
calling `gsl_blas_sdot` and offsetting the second vector every time.

    float result;
    gsl_blas_sdot(&v1, &v2, &result);

Level 2 (matrix-vector) would use an "anti-triangular" matrix that is
symmetric.

    gsl_blas_ssymv(
        CBLAS_UPLO_t Uplo,          CblasUpper
        float alpha,                1
        const gsl_matrix_float * A,
        const gsl_vector_float * x, Sample vector
        float beta,                 1
        gsl_vector_float * y        Output
    )

Direct access to BLAS would use this for the symmetric case:

    SS(Y|B|P)MV(
        UPLO = U,
        N,
        [K = band width,]
        ALPHA = 1,
        A = matrix,
        LDA?,
        X = sample vector,
        INCX = 1,
        BETA = 1,
        Y = output vector,
        INCY = 1,
    )

Alternatively, for the reversed-triangular in-place case, we can use
[`strmv`](https://www.gnu.org/software/gsl/doc/html/blas.html#c.gsl_blas_strmv):

    STRMV(
        UPLO = L,   lower
        TRANS = N,  no transpose
        DIAG = N,   non-unit-diagonal
        N,          size
        A,          Input matrix, triangular
        LDA=N,      A first dimension
        X,          sample vector, forward
        INCX=1??    input order of x
    )

    Regular: upper triangle not referenced <---
    Banded: diagonally packed
    Packed: top-to-bottom, then left-to-right, packed storage

Unfortunately, we can't have a row-aliased matrix, because it doesn't use array-
of-array format; it uses a contiguous block. Maybe one could get the same effect
using a row pitch (`tda`/`lda`) equal to 1, but that seems like a bad idea.
