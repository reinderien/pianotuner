import functools

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
Ibias = 65e-9  # comparator spec, into both inputs


def hysteresis_error(
    R1: float, R2: float, R3: float, R4: float, R6: float, R7: float,
) -> float:
    # Solve for first hysteresis transition:
    # High input, output pulled low immediately before transition

    """
    I7 = Vo/R12 + Ibias   i.
    I7 = (Vdd - Vo)/R7    ii.
    Vo = (Vdd - R7*Ibias)R12/(R12 + R7)
    """
    R12 = 1/(1/R1 + 1/R2)
    Vn_hi = (Vdd - R7*Ibias)*R12/(R12 + R7)

    R56 = R6  # R5 open
    Vp_hi = (Vinhi - R4*Ibias)*R56/(R56 + R4)

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
    i5 = np.zeros_like(i4)
    i6 = (Vo - Vp)/R6
    i7 = (Vdd - Vn)/R7
    return np.array((i1, i2, i3, i4, i5, i6, i7))


def kcl(currents: np.ndarray) -> np.ndarray:
    i1, i2, i3, i4, i5, i6, i7 = currents
    lhs = np.array((
        i4 + i5 + Ibias,  # Positive input node
        i7 + i2,  # Negative input node
        i2 + i6,  # Output node
    ))
    rhs = np.array((
        i6,
        i1 + Ibias,
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


def kcl_errors_lstsq(V: np.ndarray, R: np.ndarray) -> np.ndarray:
    V = V.reshape((5, -1))
    return kcl_errors(V=V, R=R, dof5=True).ravel()


def unpack_vr(params: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    return np.split(params, (5,))


def kcl_errors_packed(params: np.ndarray) -> np.ndarray:
    V, R = unpack_vr(params)
    return kcl_errors(V, R, dof5=False)


def index_to_r(R_index: np.ndarray, e24_series: np.ndarray) -> np.ndarray:
    return e24_series[R_index.astype(int)]


def solve_v(R: np.ndarray) -> OptimizeResult:
    x0 = np.array((
        2.2,  # Vpn_lo, inputs for output hi-lo transition
        4.7,  # Vo_lo, output hi-lo transition
        4.2,  # Vp_pk
        2.2,  # Vn_pk
        4.8,  # Vo_pk
    ))
    if len(R.shape) == 2:
        x0 = x0.repeat(R.shape[1])

    result = root(
        fun=kcl_errors_lstsq,
        args=(R,),
        x0=x0,
    )
    return result


def common_mode(
    Vpn_lo: float, Vo_lo: float, Vp_pk: float, Vn_pk: float, Vo_pk: float,
) -> float:
    return 0.5*(Vp_pk + Vn_pk)


def discrete_common_mode(R_index: np.ndarray, e24_series: np.ndarray) -> float:
    R = index_to_r(R_index, e24_series)
    sol = solve_v(R).x.reshape((5, -1))
    vcm = common_mode(*sol)
    if len(R.shape) == 2:
        vcm = vcm[np.newaxis, :]
    return vcm


def discrete_pullup(R_index: np.ndarray, e24_series: np.ndarray) -> float:
    R = index_to_r(R_index, e24_series)
    V = solve_v(R).x.reshape((5, -1))
    Vpn_lo, Vo_lo, Vp_pk, Vn_pk, Vo_pk = V
    if len(R.shape) == 2:
        Vo_lo = Vo_lo[np.newaxis, :]
    return Vo_lo


def hysteresis_error_packed(params: np.ndarray) -> float:
    V, R = unpack_vr(params)
    error = hysteresis_error(*R)
    return error**2


def discrete_least_squared_error(R_index: np.ndarray, e24_series: np.ndarray) -> float:
    R = index_to_r(R_index=R_index, e24_series=e24_series)
    V_solution = solve_v(R)
    V = V_solution.x.reshape((5, -1))
    err_ihyst, err_ihi = system_nodes(*V, *R)
    err_vhist = hysteresis_error(*R)
    error_vector = np.concatenate((
        err_ihyst, err_ihi, (err_vhist,),
    ))
    if len(error_vector.shape) == 1:
        sqerr = error_vector.dot(error_vector)
    else:
        sqerr = np.einsum('ij,ij->j', error_vector, error_vector)
    return sqerr


def dump(
    result: OptimizeResult,
    e24_series: np.ndarray,
    discrete: bool,
) -> None:
    # print(result.message)
    print(f'Total error: {result.fun:.2e}')
    print(f'Iterations: {result.nit}')
    print(f'Evaluations: {result.nfev}')

    if discrete:
        R = index_to_r(R_index=result.x, e24_series=e24_series)
        V = solve_v(R).x
    else:
        V, R = unpack_vr(result.x)
    Vpn_lo, Vo_lo, Vp_pk, Vn_pk, Vo_pk = V
    eq_hyst, eq_hi = system_nodes(*V, *R)

    print(f'Hysteresis transition error: {hysteresis_error(*R):.2%}')
    print(f'Highest common-mode voltage: {common_mode(*V):.2f}')
    print(f'Vpn_lo: {Vpn_lo:.3f}')
    print(f' Vo_lo: {Vo_lo:.3f}')
    print(f' Vp_pk: {Vp_pk:.3f}')
    print(f' Vn_pk: {Vn_pk:.3f}')
    print(f' Vo_pk: {Vo_pk:.3f}')
    print('Resistances:', R.round())
    print('KCL node error:')
    print(eq_hyst)
    print(eq_hi)
    print()


def print_better(
    intermediate_result: OptimizeResult,
    e24_series: np.ndarray,
    solution_state: dict,
) -> None:
    old = solution_state.get('old')
    if old is None or old.fun > intermediate_result.fun:
        solution_state['old'] = intermediate_result
        dump(intermediate_result, e24_series=e24_series, discrete=True)


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
        pullup_constraint = NonlinearConstraint(
            fun=functools.partial(discrete_pullup, e24_series=e24_series),
            lb=4.5, ub=Vdd,
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
        solution_state = {}

        def callback(intermediate_result: OptimizeResult) -> None:
            print_better(
                intermediate_result=intermediate_result,
                e24_series=e24_series, solution_state=solution_state,
            )

        result = differential_evolution(
            func=discrete_least_squared_error,
            args=(e24_series,),
            bounds=Bounds(lb=xmin, ub=xmax),
            integrality=np.ones(shape=6, dtype=np.uint8),
            constraints=(
                common_mode_constraint,
                pullup_constraint,
            ),
            x0=x0, seed=0,
            vectorized=True, updating='deferred',
            tol=0.5, maxiter=125,
            callback=callback,
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
    dump(result, e24_series=e24_series, discrete=discrete)


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

Total error: 1.42e-02
Iterations: 31
Evaluations: 33
Hysteresis transition error: 2.51%
Highest common-mode voltage: 3.14
Vpn_lo: 2.039
 Vo_lo: 4.627
 Vp_pk: 4.109
 Vn_pk: 2.179
 Vo_pk: 4.807
Resistances: [  62000. 1300000.  160000.  510000. 1600000.   91000.]
KCL node error:
[-0.05375866 -0.10290087 -0.00854464]
[ 0.00044485 -0.00026804 -0.00045755]

Total error: 1.85e-04
Iterations: 36
Evaluations: 38
Hysteresis transition error: -0.82%
Highest common-mode voltage: 3.12
Vpn_lo: 2.087
 Vo_lo: 4.824
 Vp_pk: 4.139
 Vn_pk: 2.100
 Vo_pk: 4.937
Resistances: [  24000. 2000000.  150000.  820000. 2400000.   36000.]
KCL node error:
[-0.00540004 -0.00937301 -0.0006252 ]
[-9.78346784e-06  7.35118785e-06  1.15541537e-06]

Total error: 1.29e-04
Iterations: 49
Evaluations: 51
Hysteresis transition error: 1.01%
Highest common-mode voltage: 3.09
Vpn_lo: 2.078
 Vo_lo: 4.906
 Vp_pk: 4.106
 Vn_pk: 2.074
 Vo_pk: 4.963
Resistances: [  82000. 2200000.  160000. 1800000. 5100000.  130000.]
KCL node error:
[0.00267151 0.00437091 0.00024693]
[1.53179961e-07 5.32302581e-05 1.57891433e-05]

Total error: 6.00e-05
Iterations: 71
Evaluations: 73
Hysteresis transition error: -0.56%
Highest common-mode voltage: 3.05
Vpn_lo: 2.066
 Vo_lo: 4.640
 Vp_pk: 4.037
 Vn_pk: 2.062
 Vo_pk: 4.697
Resistances: [  56000. 1100000.  200000. 2200000. 5600000.   91000.]
KCL node error:
[0.0027404  0.00448257 0.0004342 ]
[ 6.57815128e-06  2.59759108e-06 -1.54980265e-05]

Total error: 3.23e-05
Iterations: 110
Evaluations: 112
Hysteresis transition error: 0.38%
Highest common-mode voltage: 3.19
Vpn_lo: 2.223
 Vo_lo: 4.755
 Vp_pk: 4.139
 Vn_pk: 2.233
 Vo_pk: 4.865
Resistances: [ 300000. 2200000.  240000. 1600000. 3600000.  470000.]
KCL node error:
[-0.0010384  -0.00160569 -0.00011408]
[-4.75070319e-06 -1.67146250e-05 -3.61469911e-07]

Total error: 2.82e-05
Iterations: 117
Evaluations: 119
Hysteresis transition error: 0.27%
Highest common-mode voltage: 3.16
Vpn_lo: 2.187
 Vo_lo: 4.819
 Vp_pk: 4.131
 Vn_pk: 2.198
 Vo_pk: 4.915
Resistances: [ 300000. 2700000.  240000. 1800000. 4300000.  470000.]
KCL node error:
[-0.00243131 -0.00382453 -0.00025618]
[-1.14120050e-05  1.77891666e-05  3.16916313e-06]

Maximum number of iterations has been exceeded.
Total error: 2.82e-05
Iterations: 125
Evaluations: 127
Hysteresis transition error: 0.27%
Highest common-mode voltage: 3.16
Vpn_lo: 2.187
 Vo_lo: 4.819
 Vp_pk: 4.131
 Vn_pk: 2.198
 Vo_pk: 4.915
Resistances: [ 300000. 2700000.  240000. 1800000. 4300000.  470000.]
KCL node error:
[-0.00243131 -0.00382453 -0.00025618]
[-1.14120050e-05  1.77891666e-05  3.16916313e-06]

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
