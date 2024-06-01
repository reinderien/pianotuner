import functools
from pprint import pprint

import numpy as np
import sympy
from scipy.optimize import (
    differential_evolution, minimize, Bounds,  LinearConstraint, NonlinearConstraint,
)


'''
Per main.asm,

    ; In practice, the RPI sends an SPI clock of 0.88-3.92V, and a MOSI of
    ; 0-3.28V. With our Vdd=5.2V, these are the PIC input levels:
    ; TTL in: 0.80-2.00
    ;  ST in: 1.04-4.16
    ;    out: 0.60-4.50
    
The comparator is an LM393N (KA393)
    2 V <= Vcc=5.2 V <= 36 V (easy)
    Input common mode voltage < Vcc-2
    Open collector output.
'''

Vdd = 5.2     # to both the PIC and comparator
Vinhi = 2.8   # Schmitt trigger design high transition voltage
Vinlo = 1.2   # Schmitt trigger design low transition voltage
Vimax = 3.92  # from the pi
Isinkmax = 1e-3  # Target output sink for reasonable saturation


def hysteresis_error(
    Vpn_lo: float, Vo_lo: float, Vp_pk: float, Vn_pk: float, Vo_pk: float,
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
) -> float:
    # Solve for first hysteresis transition:
    # High input, output pulled low immediately before transition
    R12 = 1/(1/R1 + 1/R2)
    R56 = R6  # R5 open
    Vn_hi = Vinhi*R56/(R56 + R4)
    Vp_hi = Vdd*R12/(R12 + R7)

    # Converge to comparator inputs matching before low-high output transition
    error = Vn_hi - Vp_hi
    return error**2


def node_currents(
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
    Vp: float, Vn: float,
    Vi: float, Vo: float,
) -> tuple[float, ...]:
    i1 = Vn/R1
    i2 = (Vo - Vn)/R2
    i3 = (Vdd - Vo)/R3
    i4 = (Vp - Vi)/R4
    i5 = 0
    i6 = (Vo - Vp)/R6
    i7 = (Vdd - Vn)/R7
    return i1, i2, i3, i4, i5, i6, i7


def kcl(currents: tuple[float, ...]) -> tuple[float, ...]:
    i1, i2, i3, i4, i5, i6, i7 = currents
    return (
        i4 + i5 - i6,  # Positive input node
        i7 + i2 - i1,  # Negative input node
        i2 + i6 - i3,  # Output node
    )


def kcl_errors(
    Vpn_lo: float, Vo_lo: float, Vp_pk: float, Vn_pk: float, Vo_pk: float,
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
) -> np.ndarray:
    eq = ()

    # Solve for second hysteresis transition:
    # Low input, output Z immediately before transition
    eq += kcl(node_currents(
        R1=R1, R2=R2, R3=R3, R4=R4, R6=R6, R7=R7,
        Vp=Vpn_lo, Vn=Vpn_lo,
        Vi=Vinlo, Vo=Vo_lo,
    ))

    # Solve for worst-case input:
    # Highest input, highest output, output Z
    eq += kcl(node_currents(
        R1=R1, R2=R2, R3=R3, R4=R4, R6=R6, R7=R7,
        Vp=Vp_pk, Vn=Vn_pk,
        Vi=Vimax, Vo=Vo_pk,
    ))

    # Scale currents to microamps
    return np.array(eq) * 1e6


def node_equations(params: np.ndarray) -> np.ndarray:
    return kcl_errors(*params)


def discrete_node_equations(params: np.ndarray, e24_series: np.ndarray) -> np.ndarray:
    V, R_index = np.split(params, (5,))
    R = e24_series[R_index.astype(int)]
    return kcl_errors(*V, *R)


def hysteresis_least_squared_error(params: np.ndarray) -> float:
    return hysteresis_error(*params)


def discrete_hysteresis_least_squared_error(params: np.ndarray, e24_series: np.ndarray) -> float:
    V, R_index = np.split(params, (5,))
    R = e24_series[R_index.astype(int)]
    return hysteresis_error(*V, *R)


def solve(
    discrete: bool = False,
) -> None:
    # Limit R3 to limit sink current. This is an approximation: there will
    # be a contribution from the R7-R2 branch, but R3 will dominate.
    R3min = max(1e3, Vdd / Isinkmax)

    pre_baked_decisions = np.array((
        (      0.1, 2.2042490254557170, Vdd - 0.1),  # Vpn_lo, inputs for output hi-lo transition
        (Vdd - 0.5, 4.7010216986522790, Vdd),        # Vo_lo, output hi-lo transition
        (      0.1, 4.1890968857112800, Vdd - 0.1),  # Vp_pk
        (      0.1, 2.2109031142887217, Vdd - 0.1),  # Vn_pk
        (      0.1, 4.8581279065631060, Vdd),        # Vo_pk

        (  1e3,  3878., 1e7),  # R1
        (  1e3, 52390., 1e7),  # R2
        (R3min,  5513., 1e7),  # R3
        (  1e3, 23435., 1e7),  # R4
        # R5 left open
        (  1e3, 58265., 1e7),  # R6
        (  1e3,  5754., 1e7),  # R7
    ))

    example_resistor_approximation = np.array((
        ( 7400,  7400,  7400),  # R1
        (100e3, 100e3, 100e3),  # R2
        (R3min,   1e4,   1e4),  # R3
        ( 40e3,  40e3,  40e3),  # R4
                                # R5 left open
        (100e3, 100e3, 100e3),  # R6
        ( 11e3,  11e3,  11e3),  # R7
    ))

    decisions = np.array((
        (      0.1,   0.5*Vdd, Vdd - 0.1),  # Vpn_lo, inputs for output hi-lo transition
        (Vdd - 0.5, Vdd - 0.3, Vdd),        # Vo_lo, output hi-lo transition
        (      0.1,   0.5*Vdd, Vdd - 0.1),  # Vp_pk
        (      0.1,   0.5*Vdd, Vdd - 0.1),  # Vn_pk
        (      0.1, Vdd - 0.3, Vdd),        # Vo_pk

        (  1e3,  6000, 1e7),  # R1
        (  1e3, 50000, 1e7),  # R2
        (R3min,  6000, 1e7),  # R3
        (  1e3, 25000, 1e7),  # R4
                              # R5 left open
        (  1e3, 50000, 1e7),  # R6
        (  1e3,  5000, 1e7),  # R7

    )).T

    if discrete:
        e24_decade = np.array((
            10, 11, 12, 13, 15, 16,
            18, 20, 22, 24, 27, 30,
            33, 36, 39, 43, 47, 51,
            56, 62, 68, 75, 82, 91,
        ))
        e24_series: np.ndarray = np.outer(
            np.logspace(2, 6, num=5), e24_decade,
        ).ravel()
        decisions[:, 5:] = e24_series.searchsorted(v=decisions[:, 5:])

        node_constraint = NonlinearConstraint(
            fun=functools.partial(discrete_node_equations, e24_series=e24_series),
            lb=0, ub=0,
        )
    else:
        node_constraint = NonlinearConstraint(
            fun=node_equations, lb=0, ub=0,
        )

    common_mode_constraint = LinearConstraint(
        A=(
            0, 0, 0.5, 0.5, 0,
            0, 0, 0, 0, 0, 0,
        ),
        lb=0, ub=Vdd - 2,
    )

    xmin, x0, xmax = decisions

    if discrete:
        # Unhappy. Constraints never satisfy.
        result = differential_evolution(
            func=discrete_hysteresis_least_squared_error,
            args=(e24_series,),
            bounds=Bounds(lb=xmin, ub=xmax),
            integrality=(
                0, 0, 0, 0, 0,
                1, 1, 1, 1, 1, 1,
            ),
            constraints=(
                node_constraint,
                common_mode_constraint,
            ),
            x0=x0, seed=0, disp=True,
            vectorized=True, updating='deferred',
        )
    else:
        result = minimize(
            fun=hysteresis_least_squared_error,
            bounds=Bounds(lb=xmin, ub=xmax),
            x0=x0,
            constraints=(
                common_mode_constraint,
                node_constraint,
            ),
        )

    if not result.success:
        raise ValueError(result.message)

    (
        Vpn_lo, Vo_lo, Vp_pk, Vn_pk, Vo_pk,
        R1, R2, R3, R4, R6, R7,
    ) = result.x

    pprint(locals())
    print('Hysteresis transition error:', hysteresis_least_squared_error(result.x))
    print('Node constraint error:', node_equations(result.x))
    print('Highest common-mode voltage:', common_mode_constraint.A @ result.x)


def print_symbolic() -> None:
    (
        Vpn_lo, Vo_lo, Vp_pk, Vn_pk, Vo_pk,
        R1, R2, R3, R4, R6, R7,
    ) = sympy.symbols(
        names=(
            'Vpn_lo', 'Vo_lo', 'Vp_pk', 'Vn_pk', 'Vo_pk',
            'R1', 'R2', 'R3', 'R4', 'R6', 'R7',
        ),
        real=True, positive=True,
    )

    eq = ()

    # Solve for first hysteresis transition:
    # High input, output pulled low immediately before transition
    R12 = 1/(1/R1 + 1/R2)
    R56 = R6  # R5 open
    Vn_hi = Vinhi*R56/(R56 + R4)
    Vp_hi = Vdd*R12/(R12 + R7)
    eq += (
        # Converge to comparator inputs matching before low-high output transition
        Vn_hi - Vp_hi,
    )

    # Solve for second hysteresis transition:
    # Low input, output Z immediately before transition
    eq += kcl(node_currents(
        R1=R1, R2=R2, R3=R3, R4=R4, R6=R6, R7=R7,
        Vp=Vpn_lo, Vn=Vpn_lo,
        Vi=Vinlo, Vo=Vo_lo,
    ))

    # Solve for worst-case input:
    # Highest input, highest output, output Z
    eq += kcl(node_currents(
        R1=R1, R2=R2, R3=R3, R4=R4, R6=R6, R7=R7,
        Vp=Vp_pk, Vn=Vn_pk,
        Vi=Vimax, Vo=Vo_pk,
    ))

    eq = [
        sympy.Eq(
            lhs=eq[i].args[0],
            rhs=-eq[i].args[1],
        )
        for i in (0, 1, 4, 3, 6, 2, 5)
    ]

    eq.append(
        sympy.Le(
            0.5 * (Vp_pk + Vn_pk),
            Vdd - 2,
        )
    )

    sympy.init_printing(use_unicode=False, use_latex=True, pretty_print=True)
    for e in eq:
        print(sympy.latex(e))
        print()


if __name__ == '__main__':
    # solve(discrete=False)
    solve(discrete=True)
    # print_symbolic()

