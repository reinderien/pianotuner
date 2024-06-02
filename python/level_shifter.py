import functools
from pprint import pprint

import numpy as np
import sympy
from scipy.optimize import (
    differential_evolution, minimize, root,
    Bounds, LinearConstraint, NonlinearConstraint, OptimizeResult,
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
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
) -> float:
    # Solve for first hysteresis transition:
    # High input, output pulled low immediately before transition
    R12 = 1/(1/R1 + 1/R2)
    R56 = R6  # R5 open
    Vn_hi = Vinhi*R56/(R56 + R4)
    Vp_hi = Vdd*R12/(R12 + R7)

    # Converge to comparator inputs matching before low-high output transition
    return Vp_hi/Vn_hi - 1


def node_currents(
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
    Vp: float, Vn: float, Vi: float, Vo: float,
) -> np.ndarray:
    i1 = Vn/R1
    i2 = (Vo - Vn)/R2
    i3 = (Vdd - Vo)/R3
    i4 = (Vp - Vi)/R4
    i5 = 0
    i6 = (Vo - Vp)/R6
    i7 = (Vdd - Vn)/R7
    return np.array((i1, i2, i3, i4, i5, i6, i7))


def kcl(currents: np.ndarray) -> np.ndarray:
    i1, i2, i3, i4, i5, i6, i7 = currents
    lhs = np.array((
        i4 + i5,  # Positive input node
        i7 + i2,  # Negative input node
        i2 + i6,  # Output node
    ))
    rhs = np.array((
        i6,
        i1,
        i3,
    ))
    return rhs/lhs - 1


def system_nodes(
    Vpn_lo: float, Vo_lo: float, Vp_pk: float, Vn_pk: float, Vo_pk: float,
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
) -> tuple[np.ndarray, np.ndarray]:
    # Solve for second hysteresis transition:
    # Low input, output Z immediately before transition
    eq_hyst = kcl(
        currents=node_currents(
            R1=R1, R2=R2, R3=R3, R4=R4, R6=R6, R7=R7,
            Vp=Vpn_lo, Vn=Vpn_lo,
            Vi=Vinlo, Vo=Vo_lo,
        ),
    )

    # Solve for worst-case input:
    # Highest input, highest output, output Z
    eq_hi = kcl(
        currents=node_currents(
            R1=R1, R2=R2, R3=R3, R4=R4, R6=R6, R7=R7,
            Vp=Vp_pk, Vn=Vn_pk,
            Vi=Vimax, Vo=Vo_pk,
        ),
    )

    return eq_hyst, eq_hi


def kcl_errors(
    V: np.ndarray,
    R: np.ndarray,
    dof5: bool,
) -> np.ndarray:
    eq_hyst, eq_hi = system_nodes(*V, *R)

    if dof5:
        # Needed to be compatible with root() shape requirements
        errors = np.array((
            *eq_hyst[:2],
            *eq_hi[:2],
            np.sqrt(eq_hyst[2]**2 + eq_hi[2]**2),
        ))
    else:
        errors = np.concatenate((eq_hyst, eq_hi))

    return errors


def unpack_vr(params: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    return np.split(params, (5,))


def kcl_errors_packed(params: np.ndarray) -> np.ndarray:
    V, R = unpack_vr(params)
    return kcl_errors(V, R, dof5=False)


def index_to_r(R_index: np.ndarray, e24_series: np.ndarray) -> np.ndarray:
    return e24_series[R_index.astype(int)]


def solve_v(R: np.ndarray) -> OptimizeResult:
    return root(
        fun=functools.partial(
            kcl_errors, R=R, dof5=True,
        ),
        x0=(
            2.2,  # Vpn_lo, inputs for output hi-lo transition
            4.7,  # Vo_lo, output hi-lo transition
            4.2,  # Vp_pk
            2.2,  # Vn_pk
            4.8,  # Vo_pk
        ),
    )


def common_mode(
    Vpn_lo: float, Vo_lo: float, Vp_pk: float, Vn_pk: float, Vo_pk: float,
) -> float:
    return 0.5*(Vp_pk + Vn_pk)


def discrete_common_mode(R_index: np.ndarray, e24_series: np.ndarray) -> float:
    R = index_to_r(R_index, e24_series)
    sol = solve_v(R)
    return common_mode(*sol.x)


def hysteresis_error_packed(params: np.ndarray) -> float:
    V, R = unpack_vr(params)
    error = hysteresis_error(*R)
    return error**2


def discrete_least_squared_error(R_index: np.ndarray, e24_series: np.ndarray) -> float:
    R = index_to_r(R_index=R_index, e24_series=e24_series)
    V_solution = solve_v(R)
    V = V_solution.x
    err_ihyst, err_ihi = system_nodes(*V, *R)
    err_vhist = hysteresis_error(*R)
    error_vector = np.concatenate((
        err_ihyst, err_ihi, (err_vhist,),
    ))
    return error_vector.dot(error_vector)


def print_res(R_index: np.ndarray, convergence: float, e24_series: np.ndarray) -> None:
    R = index_to_r(R_index=R_index, e24_series=e24_series)
    print(f'R={R.round()} conv={convergence:.1e}')


def solve(
    discrete: bool = False,
    pre_baked: bool = False,
    start_approximate: bool = False,
) -> None:
    # Limit R3 to limit sink current. This is an approximation: there will
    # be a contribution from the R7-R2 branch, but R3 will dominate.
    R3min = max(1e3, Vdd/Isinkmax)

    if pre_baked:
        var_voltage = np.array((
            (      0.1, 2.2042490254557170, Vdd - 0.1),  # Vpn_lo, inputs for output hi-lo transition
            (Vdd - 0.5, 4.7010216986522790, Vdd),        # Vo_lo, output hi-lo transition
            (      0.1, 4.1890968857112800, Vdd - 0.1),  # Vp_pk
            (      0.1, 2.2109031142887217, Vdd - 0.1),  # Vn_pk
            (      0.1, 4.8581279065631060, Vdd),        # Vo_pk
        ))
        var_resistance = ((
            (  1e3,  3878., 1e7),  # R1
            (  1e3, 52390., 1e7),  # R2
            (R3min,  5513., 1e7),  # R3
            (  1e3, 23435., 1e7),  # R4
                                   # R5 left open
            (  1e3, 58265., 1e7),  # R6
            (  1e3,  5754., 1e7),  # R7
        ))
    elif start_approximate:
        var_resistance = np.array((
            ( 7400,  7400,  7400),  # R1
            (100e3, 100e3, 100e3),  # R2
            (R3min,   1e4,   1e4),  # R3
            ( 40e3,  40e3,  40e3),  # R4
                                    # R5 left open
            (100e3, 100e3, 100e3),  # R6
            ( 11e3,  11e3,  11e3),  # R7
        ))
    else:
        var_voltage = np.array((
            (      0.1,   0.5*Vdd, Vdd - 0.1),  # Vpn_lo, inputs for output hi-lo transition
            (Vdd - 0.5, Vdd - 0.3, Vdd),        # Vo_lo, output hi-lo transition
            (      0.1,   0.5*Vdd, Vdd - 0.1),  # Vp_pk
            (      0.1,   0.5*Vdd, Vdd - 0.1),  # Vn_pk
            (      0.1, Vdd - 0.3, Vdd),        # Vo_pk
        ))
        var_resistance = np.array((
            (  1e3,  6000, 1e7),  # R1
            (  1e3, 50000, 1e7),  # R2
            (R3min,  6000, 1e7),  # R3
            (  1e3, 25000, 1e7),  # R4
                                  # R5 left open
            (  1e3, 50000, 1e7),  # R6
            (  1e3,  5000, 1e7),  # R7
        ))

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
        decisions = e24_series.searchsorted(v=var_resistance)

        common_mode_constraint = NonlinearConstraint(
            fun=functools.partial(discrete_common_mode, e24_series=e24_series),
            lb=0, ub=Vdd - 2,
        )
    else:
        node_constraint = NonlinearConstraint(
            fun=kcl_errors_packed, lb=0, ub=0,
        )
        common_mode_constraint = LinearConstraint(
            A=(
                0, 0, 0.5, 0.5, 0,
                0, 0, 0, 0, 0, 0,
            ),
            lb=0, ub=Vdd - 2,
        )
        decisions = np.vstack((var_voltage, var_resistance))

    xmin, x0, xmax = decisions.T

    if discrete:
        result = differential_evolution(
            func=discrete_least_squared_error,
            args=(e24_series,),
            bounds=Bounds(lb=xmin, ub=xmax),
            integrality=np.ones(shape=6, dtype=np.uint8),
            constraints=common_mode_constraint,
            x0=x0, seed=0,

            # todo - decrease this, set disp to false, and print whenever a better solution is found
            tol=1, disp=True,
            # callback=functools.partial(print_res, e24_series=e24_series),

            # not possible due to inner fsolve
            # vectorized=True, updating='deferred',
        )
    else:
        result = minimize(
            fun=hysteresis_error_packed,
            bounds=Bounds(lb=xmin, ub=xmax),
            x0=x0,
            constraints=(
                common_mode_constraint,
                node_constraint,
            ),
            options={'maxiter': 1000},
            tol=1e-9,
        )

    print()
    print(result.message)
    print(f'Total error: {result.fun:.2e}')
    print(f'Iterations: {result.nit}')
    print(f'Evaluations: {result.nfev}')

    if discrete:
        R = index_to_r(R_index=result.x, e24_series=e24_series)
        V = solve_v(R).x
    else:
        V, R = unpack_vr(result.x)
    Vpn_lo, Vo_lo, Vp_pk, Vn_pk, Vo_pk = V

    print(f'Hysteresis transition error: {hysteresis_error(*R):.2%}')
    print(f'Highest common-mode voltage: {common_mode(*V):.2f}')
    print(f'Vpn_lo: {Vpn_lo:.3f}')
    print(f' Vo_lo: {Vo_lo:.3f}')
    print(f' Vp_pk: {Vp_pk:.3f}')
    print(f' Vn_pk: {Vn_pk:.3f}')
    print(f' Vo_pk: {Vo_pk:.3f}')
    print('Resistances:', R.round())
    print('KCL node error:')
    pprint(system_nodes(*V, *R))


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
    # solve(discrete=False, pre_baked=True)
    solve(discrete=True)
    # print_symbolic()

'''
Example output:
...
differential_evolution step 57: f(x)= 2.1602129007184598e-05

Optimization terminated successfully.
Total error: 2.16e-05
Iterations: 57
Evaluations: 2979
Hysteresis transition error: 0.06%
Highest common-mode voltage: 3.12
Vpn_lo: 2.089
 Vo_lo: 4.642
 Vp_pk: 4.159
 Vn_pk: 2.084
 Vo_pk: 4.843
Resistances: [  12000. 5100000.  510000. 1500000. 4300000.   18000.]
KCL node error:
(array([0.00222194, 0.00402005, 0.00040838]),
 array([-9.67262272e-06,  8.61215982e-06, -6.97556323e-06]))
'''
